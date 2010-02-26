/* $Id$ */
/** @file
 * IPRT Testcase - RTProcCreateEx.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/process.h>

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/test.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static char g_szExecName[RTPATH_MAX];


static int tstRTCreateProcEx3Child(void)
{
    int rc = RTR3Init();
    if (rc)
        return RTMsgInitFailure(rc);

    RTStrmPrintf(g_pStdOut, "w"); RTStrmFlush(g_pStdOut);
    RTStrmPrintf(g_pStdErr, "o"); RTStrmFlush(g_pStdErr);
    RTStrmPrintf(g_pStdOut, "r"); RTStrmFlush(g_pStdOut);
    RTStrmPrintf(g_pStdErr, "k"); RTStrmFlush(g_pStdErr);
    RTStrmPrintf(g_pStdOut, "s");

    return 0;
}

static void tstRTCreateProcEx3(void)
{
    RTTestISub("Standard Out+Err");

    RTPIPE hPipeR, hPipeW;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    const char * apszArgs[3] =
    {
        "non-existing-non-executable-file",
        "--testcase-child-3",
        NULL
    };
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipeW;
    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         &Handle, &Handle, NULL, &hProc), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    char    szOutput[_4K];
    size_t  offOutput = 0;
    for (;;)
    {
        size_t cbLeft = sizeof(szOutput) - 1 - offOutput;
        RTTESTI_CHECK(cbLeft > 0);
        if (cbLeft == 0)
            break;

        size_t cbRead;
        int rc = RTPipeReadBlocking(hPipeR, &szOutput[offOutput], cbLeft, &cbRead);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC(rc, VERR_BROKEN_PIPE);
            break;
        }
        offOutput += cbRead;
    }
    szOutput[offOutput] = '\0';
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);
    RTThreadSleep(10);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else if (   offOutput != sizeof("works") - 1
             || strcmp(szOutput, "works"))
        RTTestIFailed("wrong output: \"%s\" (len=%u)", szOutput, offOutput);
    else
        RTTestIPassed(NULL);
}

static int tstRTCreateProcEx2Child(void)
{
    int rc = RTR3Init();
    if (rc)
        return RTMsgInitFailure(rc);

    RTStrmPrintf(g_pStdErr, "howdy");
    RTStrmPrintf(g_pStdOut, "ignore this output\n");

    return 0;
}

static void tstRTCreateProcEx2(void)
{
    RTTestISub("Standard Err");

    RTPIPE hPipeR, hPipeW;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    const char * apszArgs[3] =
    {
        "non-existing-non-executable-file",
        "--testcase-child-2",
        NULL
    };
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipeW;
    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         NULL, &Handle, NULL, &hProc), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    char    szOutput[_4K];
    size_t  offOutput = 0;
    for (;;)
    {
        size_t cbLeft = sizeof(szOutput) - 1 - offOutput;
        RTTESTI_CHECK(cbLeft > 0);
        if (cbLeft == 0)
            break;

        size_t cbRead;
        int rc = RTPipeReadBlocking(hPipeR, &szOutput[offOutput], cbLeft, &cbRead);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC(rc, VERR_BROKEN_PIPE);
            break;
        }
        offOutput += cbRead;
    }
    szOutput[offOutput] = '\0';
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);
    RTThreadSleep(10);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else if (   offOutput != sizeof("howdy") - 1
             || strcmp(szOutput, "howdy"))
        RTTestIFailed("wrong output: \"%s\" (len=%u)", szOutput, offOutput);
    else
        RTTestIPassed(NULL);
}


static int tstRTCreateProcEx1Child(void)
{
    int rc = RTR3Init();
    if (rc)
        return RTMsgInitFailure(rc);
    RTPrintf("it works");
    RTStrmPrintf(g_pStdErr, "ignore this output\n");
    return 0;
}


static void tstRTCreateProcEx1(void)
{
    RTTestISub("Standard Out");

    RTPIPE hPipeR, hPipeW;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    const char * apszArgs[3] =
    {
        "non-existing-non-executable-file",
        "--testcase-child-1",
        NULL
    };
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipeW;
    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         &Handle, NULL, NULL, &hProc), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    char    szOutput[_4K];
    size_t  offOutput = 0;
    for (;;)
    {
        size_t cbLeft = sizeof(szOutput) - 1 - offOutput;
        RTTESTI_CHECK(cbLeft > 0);
        if (cbLeft == 0)
            break;

        size_t cbRead;
        int rc = RTPipeReadBlocking(hPipeR, &szOutput[offOutput], cbLeft, &cbRead);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC(rc, VERR_BROKEN_PIPE);
            break;
        }
        offOutput += cbRead;
    }
    szOutput[offOutput] = '\0';
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else if (   offOutput != sizeof("it works") - 1
             || strcmp(szOutput, "it works"))
        RTTestIFailed("wrong output: \"%s\" (len=%u)", szOutput, offOutput);
    else
        RTTestIPassed(NULL);
}


int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-1"))
        return tstRTCreateProcEx1Child();
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-2"))
        return tstRTCreateProcEx2Child();
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-3"))
        return tstRTCreateProcEx3Child();
    if (argc != 1)
        return 99;

    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTProcCreateEx", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    if (!RTProcGetExecutableName(g_szExecName, sizeof(g_szExecName)))
        RTStrCopy(g_szExecName, sizeof(g_szExecName), argv[0]);

    /*
     * The tests.
     */
    tstRTCreateProcEx1();
    tstRTCreateProcEx2();
    tstRTCreateProcEx3();
    /** @todo Cover files, pszAsUser, arguments with spaces, ++ */

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

