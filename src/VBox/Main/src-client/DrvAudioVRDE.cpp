/* $Id$ */
/** @file
 * VRDE audio backend for Main.
 */

/*
 * Copyright (C) 2013-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VRDE_AUDIO
#include <VBox/log.h>
#include "DrvAudioVRDE.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include "Logging.h"

#include "../../Devices/Audio/DrvAudio.h"
#include "../../Devices/Audio/AudioMixBuffer.h"

#include <iprt/mem.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>

#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Audio VRDE driver instance data.
 */
typedef struct DRVAUDIOVRDE
{
    /** Pointer to audio VRDE object. */
    AudioVRDE           *pAudioVRDE;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS           pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO        IHostAudio;
    /** Pointer to the VRDP's console object. */
    ConsoleVRDPServer   *pConsoleVRDPServer;
    /** Pointer to the DrvAudio port interface that is above us. */
    PPDMIAUDIOCONNECTOR  pDrvAudio;
} DRVAUDIOVRDE, *PDRVAUDIOVRDE;

typedef struct VRDESTREAMIN
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    /** The PCM properties of this stream. */
    PDMAUDIOPCMPROPS     Props;
    /** Number of samples captured asynchronously in the
     *  onVRDEInputXXX callbacks. */
    uint32_t             cSamplesCaptured;
    /** Critical section. */
    RTCRITSECT           CritSect;
} VRDESTREAMIN, *PVRDESTREAMIN;

typedef struct VRDESTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    /** The PCM properties of this stream. */
    PDMAUDIOPCMPROPS     Props;
    uint64_t             old_ticks;
    uint64_t             cSamplesSentPerSec;
} VRDESTREAMOUT, *PVRDESTREAMOUT;



static int vrdeCreateStreamIn(PPDMIHOSTAUDIO pInterface,
                              PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PVRDESTREAMIN pVRDEStrmIn = (PVRDESTREAMIN)pStream;
    AssertPtrReturn(pVRDEStrmIn, VERR_INVALID_POINTER);

    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pVRDEStrmIn->Props);
    if (RT_SUCCESS(rc))
    {
        if (pCfgAcq)
            pCfgAcq->cSampleBufferSize = _4K; /** @todo Make this configurable. */
    }

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


static int vrdeCreateStreamOut(PPDMIHOSTAUDIO pInterface,
                               PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PVRDESTREAMOUT pVRDEStrmOut = (PVRDESTREAMOUT)pStream;
    AssertPtrReturn(pVRDEStrmOut, VERR_INVALID_POINTER);

    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pVRDEStrmOut->Props);
    if (RT_SUCCESS(rc))
    {
        if (pCfgAcq)
            pCfgAcq->cSampleBufferSize = _4K; /** @todo Make this configurable. */
    }

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


static int vrdeControlStreamOut(PPDMIHOSTAUDIO pInterface,
                                PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(enmStreamCmd);
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturn(pDrv, VERR_INVALID_POINTER);

    PVRDESTREAMOUT pVRDEStrmOut = (PVRDESTREAMOUT)pStream;
    AssertPtrReturn(pVRDEStrmOut, VERR_INVALID_POINTER);

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    AudioMixBufReset(&pStream->MixBuf);

    return VINF_SUCCESS;
}


static int vrdeControlStreamIn(PPDMIHOSTAUDIO pInterface,
                               PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturn(pDrv, VERR_INVALID_POINTER);

    PVRDESTREAMIN pVRDEStrmIn = (PVRDESTREAMIN)pStream;
    AssertPtrReturn(pVRDEStrmIn, VERR_INVALID_POINTER);

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    if (!pDrv->pConsoleVRDPServer)
        return VINF_SUCCESS;

    AudioMixBufReset(&pStream->MixBuf);

    /* Initialize only if not already done. */
    int rc;
    if (enmStreamCmd == PDMAUDIOSTREAMCMD_ENABLE)
    {
        rc = pDrv->pConsoleVRDPServer->SendAudioInputBegin(NULL, pVRDEStrmIn, AudioMixBufSize(&pStream->MixBuf),
                                                           pVRDEStrmIn->Props.uHz,
                                                           pVRDEStrmIn->Props.cChannels, pVRDEStrmIn->Props.cBits);
        if (rc == VERR_NOT_SUPPORTED)
        {
            LogFlowFunc(("No RDP client connected, so no input recording supported\n"));
            rc = VINF_SUCCESS;
        }
    }
    else if (enmStreamCmd == PDMAUDIOSTREAMCMD_DISABLE)
    {
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL /* pvUserCtx */);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvAudioVRDEInit(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
    LogFlowFuncEnter();

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamCapture(PPDMIHOSTAUDIO pInterface,
                                                   PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF2(pvBuf, cbBuf);

    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    PVRDESTREAMIN pVRDEStrmIn = (PVRDESTREAMIN)pStream;

    /** @todo Use CritSect! */

    int rc;

    uint32_t cProcessed = 0;
    if (pVRDEStrmIn->cSamplesCaptured)
    {
        rc = AudioMixBufMixToParent(&pVRDEStrmIn->Stream.MixBuf, pVRDEStrmIn->cSamplesCaptured,
                                    &cProcessed);
    }
    else
        rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
    {
        Assert(pVRDEStrmIn->cSamplesCaptured >= cProcessed);
        pVRDEStrmIn->cSamplesCaptured -= cProcessed;

        if (pcbRead)
            *pcbRead = cProcessed;
    }

    LogFlowFunc(("cSamplesCaptured=%RU32, cProcessed=%RU32 rc=%Rrc\n", pVRDEStrmIn->cSamplesCaptured, cProcessed, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamPlay(PPDMIHOSTAUDIO pInterface,
                                                PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF2(pvBuf, cbBuf);

    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    PDRVAUDIOVRDE  pDrv         = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    PVRDESTREAMOUT pVRDEStrmOut = (PVRDESTREAMOUT)pStream;

    uint32_t cLive = AudioMixBufLive(&pStream->MixBuf);

    uint64_t now = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);
    uint64_t ticks = now  - pVRDEStrmOut->old_ticks;
    uint64_t ticks_per_second = PDMDrvHlpTMGetVirtualFreq(pDrv->pDrvIns);

    /* Minimize the rounding error: samples = int((ticks * freq) / ticks_per_second + 0.5). */
    uint32_t cSamplesPlayed = (int)((2 * ticks * pVRDEStrmOut->Props.uHz + ticks_per_second) / ticks_per_second / 2);

    /* Don't play more than available. */
    if (cSamplesPlayed > cLive)
        cSamplesPlayed = cLive;

    /* Remember when samples were consumed. */
    pVRDEStrmOut->old_ticks = now;

    VRDEAUDIOFORMAT format = VRDE_AUDIO_FMT_MAKE(pVRDEStrmOut->Props.uHz,
                                                 pVRDEStrmOut->Props.cChannels,
                                                 pVRDEStrmOut->Props.cBits,
                                                 pVRDEStrmOut->Props.fSigned);

    int cSamplesToSend = cSamplesPlayed;

    LogFlowFunc(("uFreq=%RU32, cChan=%RU8, cBits=%RU8, fSigned=%RTbool, enmFormat=%ld, cSamplesToSend=%RU32\n",
                 pVRDEStrmOut->Props.uHz,   pVRDEStrmOut->Props.cChannels,
                 pVRDEStrmOut->Props.cBits, pVRDEStrmOut->Props.fSigned,
                 format, cSamplesToSend));

    /*
     * Call the VRDP server with the data.
     */
    uint32_t cReadTotal = 0;

    PPDMAUDIOSAMPLE pSamples;
    uint32_t cRead;
    int rc = AudioMixBufAcquire(&pStream->MixBuf, cSamplesToSend,
                                &pSamples, &cRead);
    if (   RT_SUCCESS(rc)
        && cRead)
    {
        cReadTotal = cRead;
        pDrv->pConsoleVRDPServer->SendAudioSamples(pSamples, cRead, format);

        if (rc == VINF_TRY_AGAIN)
        {
            rc = AudioMixBufAcquire(&pStream->MixBuf, cSamplesToSend - cRead,
                                    &pSamples, &cRead);
            if (RT_SUCCESS(rc))
                pDrv->pConsoleVRDPServer->SendAudioSamples(pSamples, cRead, format);

            cReadTotal += cRead;
        }
    }

    AudioMixBufFinish(&pStream->MixBuf, cSamplesToSend);

    /*
     * Always report back all samples acquired, regardless of whether the
     * VRDP server actually did process those.
     */
    if (pcbWritten)
        *pcbWritten = cReadTotal;

    LogFlowFunc(("cReadTotal=%RU32, rc=%Rrc\n", cReadTotal, rc));
    return rc;
}


static int vrdeDestroyStreamIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pStream);
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturn(pDrv, VERR_INVALID_POINTER);

    if (pDrv->pConsoleVRDPServer)
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);

    return VINF_SUCCESS;
}


static int vrdeDestroyStreamOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pStream);
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturn(pDrv, VERR_INVALID_POINTER);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioVRDEGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    pBackendCfg->cbStreamOut    = sizeof(VRDESTREAMOUT);
    pBackendCfg->cbStreamIn     = sizeof(VRDESTREAMIN);
    pBackendCfg->cMaxStreamsIn  = UINT32_MAX;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;
    pBackendCfg->cSources       = 1;
    pBackendCfg->cSinks         = 1;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvAudioVRDEShutdown(PPDMIHOSTAUDIO pInterface)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturnVoid(pDrv);

    if (pDrv->pConsoleVRDPServer)
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioVRDEGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamCreate(PPDMIHOSTAUDIO pInterface,
                                                  PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = vrdeCreateStreamIn(pInterface,  pStream, pCfgReq, pCfgAcq);
    else
        rc = vrdeCreateStreamOut(pInterface, pStream, pCfgReq, pCfgAcq);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = vrdeDestroyStreamIn(pInterface,  pStream);
    else
        rc = vrdeDestroyStreamOut(pInterface, pStream);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamControl(PPDMIHOSTAUDIO pInterface,
                                                   PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = vrdeControlStreamIn(pInterface,  pStream, enmStreamCmd);
    else
        rc = vrdeControlStreamOut(pInterface, pStream, enmStreamCmd);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvAudioVRDEStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    NOREF(pStream);

    return (  PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED
            | PDMAUDIOSTRMSTS_FLAG_DATA_READABLE | PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for VRDE. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVRDEQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


AudioVRDE::AudioVRDE(Console *pConsole)
    : mpDrv(NULL),
      mParent(pConsole)
{
}


AudioVRDE::~AudioVRDE(void)
{
    if (mpDrv)
    {
        mpDrv->pAudioVRDE = NULL;
        mpDrv = NULL;
    }
}


int AudioVRDE::onVRDEControl(bool fEnable, uint32_t uFlags)
{
    RT_NOREF(fEnable, uFlags);
    LogFlowThisFunc(("fEnable=%RTbool, uFlags=0x%x\n", fEnable, uFlags));

    if (mpDrv == NULL)
        return VERR_INVALID_STATE;

    return VINF_SUCCESS; /* Never veto. */
}


/**
 * Marks the beginning of sending captured audio data from a connected
 * RDP client.
 *
 * @return  IPRT status code.
 * @param   pvContext               The context; in this case a pointer to a
 *                                  VRDESTREAMIN structure.
 * @param   pVRDEAudioBegin         Pointer to a VRDEAUDIOINBEGIN structure.
 */
int AudioVRDE::onVRDEInputBegin(void *pvContext, PVRDEAUDIOINBEGIN pVRDEAudioBegin)
{
    AssertPtrReturn(pvContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pVRDEAudioBegin, VERR_INVALID_POINTER);

    PVRDESTREAMIN pVRDEStrmIn = (PVRDESTREAMIN)pvContext;
    AssertPtrReturn(pVRDEStrmIn, VERR_INVALID_POINTER);

    VRDEAUDIOFORMAT audioFmt = pVRDEAudioBegin->fmt;

    int iSampleHz  = VRDE_AUDIO_FMT_SAMPLE_FREQ(audioFmt);     NOREF(iSampleHz);
    int cChannels  = VRDE_AUDIO_FMT_CHANNELS(audioFmt);        NOREF(cChannels);
    int cBits      = VRDE_AUDIO_FMT_BITS_PER_SAMPLE(audioFmt); NOREF(cBits);
    bool fUnsigned = VRDE_AUDIO_FMT_SIGNED(audioFmt);          NOREF(fUnsigned);

    LogFlowFunc(("cbSample=%RU32, iSampleHz=%d, cChannels=%d, cBits=%d, fUnsigned=%RTbool\n",
                 VRDE_AUDIO_FMT_BYTES_PER_SAMPLE(audioFmt), iSampleHz, cChannels, cBits, fUnsigned));

    return VINF_SUCCESS;
}


int AudioVRDE::onVRDEInputData(void *pvContext, const void *pvData, uint32_t cbData)
{
    PVRDESTREAMIN pVRDEStrmIn = (PVRDESTREAMIN)pvContext;
    AssertPtrReturn(pVRDEStrmIn, VERR_INVALID_POINTER);

    PPDMAUDIOSTREAM pStream = &pVRDEStrmIn->Stream;
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    /** @todo Use CritSect! */

    uint32_t cWritten;
    int rc = AudioMixBufWriteCirc(&pStream->MixBuf, pvData, cbData, &cWritten);
    if (RT_SUCCESS(rc))
        pVRDEStrmIn->cSamplesCaptured += cWritten;

    LogFlowFunc(("cbData=%RU32, cWritten=%RU32, cSamplesCaptured=%RU32, rc=%Rrc\n",
                 cbData, cWritten, pVRDEStrmIn->cSamplesCaptured, rc));
    return rc;
}


int AudioVRDE::onVRDEInputEnd(void *pvContext)
{
    NOREF(pvContext);

    return VINF_SUCCESS;
}


int AudioVRDE::onVRDEInputIntercept(bool fEnabled)
{
    RT_NOREF(fEnabled);
    return VINF_SUCCESS; /* Never veto. */
}


/**
 * Construct a VRDE audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
/* static */
DECLCALLBACK(int) AudioVRDE::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);

    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    LogRel(("Audio: Initializing VRDE driver\n"));
    LogFlowFunc(("fFlags=0x%x\n", fFlags));

    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvAudioVRDEQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvAudioVRDE);

    /*
     * Get the ConsoleVRDPServer object pointer.
     */
    void *pvUser;
    int rc = CFGMR3QueryPtr(pCfg, "ObjectVRDPServer", &pvUser);
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"ObjectVRDPServer\" value, rc=%Rrc\n", rc), rc);

    /* CFGM tree saves the pointer to ConsoleVRDPServer in the Object node of AudioVRDE. */
    pThis->pConsoleVRDPServer = (ConsoleVRDPServer *)pvUser;

    /*
     * Get the AudioVRDE object pointer.
     */
    pvUser = NULL;
    rc = CFGMR3QueryPtr(pCfg, "Object", &pvUser);
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"Object\" value, rc=%Rrc\n", rc), rc);

    pThis->pAudioVRDE = (AudioVRDE *)pvUser;
    pThis->pAudioVRDE->mpDrv = pThis;

    /*
     * Get the interface for the above driver (DrvAudio) to make mixer/conversion calls.
     * Described in CFGM tree.
     */
    pThis->pDrvAudio = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    AssertMsgReturn(pThis->pDrvAudio, ("Configuration error: No upper interface specified!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
/* static */
DECLCALLBACK(void) AudioVRDE::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    LogFlowFuncEnter();

    /*
     * If the AudioVRDE object is still alive, we must clear it's reference to
     * us since we'll be invalid when we return from this method.
     */
    if (pThis->pAudioVRDE)
    {
        pThis->pAudioVRDE->mpDrv = NULL;
        pThis->pAudioVRDE = NULL;
    }
}


/**
 * VRDE audio driver registration record.
 */
const PDMDRVREG AudioVRDE::DrvReg =
{
    PDM_DRVREG_VERSION,
    /* szName */
    "AudioVRDE",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio driver for VRDE backend",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVAUDIOVRDE),
    /* pfnConstruct */
    AudioVRDE::drvConstruct,
    /* pfnDestruct */
    AudioVRDE::drvDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

