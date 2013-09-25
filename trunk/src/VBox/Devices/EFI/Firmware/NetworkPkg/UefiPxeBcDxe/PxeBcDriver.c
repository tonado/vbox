/** @file
  Driver Binding functions implementationfor for UefiPxeBc Driver.

  Copyright (c) 2007 - 2011, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "PxeBcImpl.h"


EFI_DRIVER_BINDING_PROTOCOL gPxeBcDriverBinding = {
  PxeBcDriverBindingSupported,
  PxeBcDriverBindingStart,
  PxeBcDriverBindingStop,
  0xa,
  NULL,
  NULL
};

/**
  Get the Nic handle using any child handle in the IPv4 stack.

  @param[in]  ControllerHandle    Pointer to child handle over IPv4.

  @return NicHandle               The pointer to the Nic handle.

**/
EFI_HANDLE
PxeBcGetNicByIp4Children (
  IN EFI_HANDLE                 ControllerHandle
  )
{
  EFI_HANDLE                    NicHandle;

  NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiArpProtocolGuid);
  if (NicHandle == NULL) {
    NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiIp4ProtocolGuid);
    if (NicHandle == NULL) {
      NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiUdp4ProtocolGuid);
      if (NicHandle == NULL) {
        NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiDhcp4ProtocolGuid);
        if (NicHandle == NULL) {
          NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiMtftp4ProtocolGuid);
          if (NicHandle == NULL) {
            return NULL;
          }
        }
      }
    }
  }

  return NicHandle;
}


/**
  Get the Nic handle using any child handle in the IPv6 stack.

  @param[in]  ControllerHandle    Pointer to child handle over IPv6.

  @return NicHandle               The pointer to the Nic handle.

**/
EFI_HANDLE
PxeBcGetNicByIp6Children (
  IN EFI_HANDLE                  ControllerHandle
  )
{
  EFI_HANDLE                     NicHandle;

  NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiIp6ProtocolGuid);
  if (NicHandle == NULL) {
    NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiUdp6ProtocolGuid);
    if (NicHandle == NULL) {
      NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiDhcp6ProtocolGuid);
      if (NicHandle == NULL) {
        NicHandle = NetLibGetNicHandle (ControllerHandle, &gEfiMtftp6ProtocolGuid);
        if (NicHandle == NULL) {
          return NULL;
        }
      }
    }
  }

  return NicHandle;
}


/**
  Destroy the opened instances based on IPv4.

  @param[in]  This              Pointer to the EFI_DRIVER_BINDING_PROTOCOL.
  @param[in]  Private           Pointer to PXEBC_PRIVATE_DATA.

**/
VOID
PxeBcDestroyIp4Children (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN PXEBC_PRIVATE_DATA           *Private
  )
{
  ASSERT(Private != NULL);

  if (Private->ArpChild != NULL) {
    //
    // Close Arp for PxeBc->Arp and destroy the instance.
    //
    gBS->CloseProtocol (
           Private->ArpChild,
           &gEfiArpProtocolGuid,
           This->DriverBindingHandle,
           Private->Controller
           );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiArpServiceBindingProtocolGuid,
      Private->ArpChild
      );
  }

  if (Private->Ip4Child != NULL) {
    //
    // Close Ip4 for background ICMP error message and destroy the instance.
    //
    gBS->CloseProtocol (
           Private->Ip4Child,
           &gEfiIp4ProtocolGuid,
           This->DriverBindingHandle,
           Private->Controller
           );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiIp4ServiceBindingProtocolGuid,
      Private->Ip4Child
      );
  }

  if (Private->Udp4WriteChild != NULL) {
    //
    // Close Udp4 for PxeBc->UdpWrite and destroy the instance.
    //
    gBS->CloseProtocol (
           Private->Udp4WriteChild,
           &gEfiUdp4ProtocolGuid,
           This->DriverBindingHandle,
           Private->Controller
           );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiUdp4ServiceBindingProtocolGuid,
      Private->Udp4WriteChild
      );
  }

  if (Private->Udp4ReadChild != NULL) {
    //
    // Close Udp4 for PxeBc->UdpRead and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Udp4ReadChild,
          &gEfiUdp4ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiUdp4ServiceBindingProtocolGuid,
      Private->Udp4ReadChild
      );
  }

  if (Private->Mtftp4Child != NULL) {
    //
    // Close Mtftp4 for PxeBc->Mtftp4 and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Mtftp4Child,
          &gEfiMtftp4ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiMtftp4ServiceBindingProtocolGuid,
      Private->Mtftp4Child
      );
  }

  if (Private->Dhcp4Child != NULL) {
    //
    // Close Dhcp4 for PxeBc->Dhcp4 and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Dhcp4Child,
          &gEfiDhcp4ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiDhcp4ServiceBindingProtocolGuid,
      Private->Dhcp4Child
      );
  }

  if (Private->Ip4Nic != NULL) {
    //
    // Close PxeBcPrivate from the parent Nic handle and destroy the virtual handle.
    //
    gBS->CloseProtocol (
           Private->Controller,
           &gEfiCallerIdGuid,
           This->DriverBindingHandle,
           Private->Ip4Nic->Controller
           );

    gBS->UninstallMultipleProtocolInterfaces (
           Private->Ip4Nic->Controller,
           &gEfiDevicePathProtocolGuid,
           Private->Ip4Nic->DevicePath,
           &gEfiLoadFileProtocolGuid,
           &Private->Ip4Nic->LoadFile,
           &gEfiPxeBaseCodeProtocolGuid,
           &Private->PxeBc,
           NULL
           );

    if (Private->Snp != NULL) { 
      //
      // Close SNP from the child virtual handle
      //
      gBS->CloseProtocol (
             Private->Ip4Nic->Controller,
             &gEfiSimpleNetworkProtocolGuid,
             This->DriverBindingHandle,
             Private->Ip4Nic->Controller
             );
             
      gBS->UninstallProtocolInterface (
             Private->Ip4Nic->Controller,
             &gEfiSimpleNetworkProtocolGuid,
             Private->Snp
             );
    }
    FreePool (Private->Ip4Nic);
  }

  Private->ArpChild         = NULL;
  Private->Ip4Child         = NULL;
  Private->Udp4WriteChild   = NULL;
  Private->Udp4ReadChild    = NULL;
  Private->Mtftp4Child      = NULL;
  Private->Dhcp4Child       = NULL;
  Private->Ip4Nic           = NULL;
}


/**
  Destroy the opened instances based on IPv6.

  @param[in]  This              Pointer to the EFI_DRIVER_BINDING_PROTOCOL.
  @param[in]  Private           Pointer to PXEBC_PRIVATE_DATA.

**/
VOID
PxeBcDestroyIp6Children (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN PXEBC_PRIVATE_DATA           *Private
  )
{
  ASSERT(Private != NULL);

  if (Private->Ip6Child != NULL) {
    //
    // Close Ip6 for Ip6->Ip6Config and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Ip6Child,
          &gEfiIp6ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiIp6ServiceBindingProtocolGuid,
      Private->Ip6Child
      );
  }

  if (Private->Udp6WriteChild != NULL) {
    //
    // Close Udp6 for PxeBc->UdpWrite and destroy the instance.
    //
    gBS->CloseProtocol (
           Private->Udp6WriteChild,
           &gEfiUdp6ProtocolGuid,
           This->DriverBindingHandle,
           Private->Controller
           );
    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiUdp6ServiceBindingProtocolGuid,
      Private->Udp6WriteChild
      );
  }

  if (Private->Udp6ReadChild != NULL) {
    //
    // Close Udp6 for PxeBc->UdpRead and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Udp6ReadChild,
          &gEfiUdp6ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );
    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiUdp6ServiceBindingProtocolGuid,
      Private->Udp6ReadChild
      );
  }

  if (Private->Mtftp6Child != NULL) {
    //
    // Close Mtftp6 for PxeBc->Mtftp and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Mtftp6Child,
          &gEfiMtftp6ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiMtftp6ServiceBindingProtocolGuid,
      Private->Mtftp6Child
      );
  }

  if (Private->Dhcp6Child != NULL) {
    //
    // Close Dhcp6 for PxeBc->Dhcp and destroy the instance.
    //
    gBS->CloseProtocol (
          Private->Dhcp6Child,
          &gEfiDhcp6ProtocolGuid,
          This->DriverBindingHandle,
          Private->Controller
          );

    NetLibDestroyServiceChild (
      Private->Controller,
      This->DriverBindingHandle,
      &gEfiDhcp6ServiceBindingProtocolGuid,
      Private->Dhcp6Child
      );
  }

  if (Private->Ip6Nic != NULL) {
    //
    // Close PxeBcPrivate from the parent Nic handle and destroy the virtual handle.
    //
    gBS->CloseProtocol (
           Private->Controller,
           &gEfiCallerIdGuid,
           This->DriverBindingHandle,
           Private->Ip6Nic->Controller
           );

    gBS->UninstallMultipleProtocolInterfaces (
           Private->Ip6Nic->Controller,
           &gEfiDevicePathProtocolGuid,
           Private->Ip6Nic->DevicePath,
           &gEfiLoadFileProtocolGuid,
           &Private->Ip6Nic->LoadFile,
           &gEfiPxeBaseCodeProtocolGuid,
           &Private->PxeBc,
           NULL
           );
    if (Private->Snp != NULL) {
      //
      // Close SNP from the child virtual handle
      //
      gBS->CloseProtocol (
             Private->Ip6Nic->Controller,
             &gEfiSimpleNetworkProtocolGuid,
             This->DriverBindingHandle,
             Private->Ip6Nic->Controller
             );
      gBS->UninstallProtocolInterface (
             Private->Ip6Nic->Controller,
             &gEfiSimpleNetworkProtocolGuid,
             Private->Snp
             );
    }
    FreePool (Private->Ip6Nic);
  }

  Private->Ip6Child           = NULL;
  Private->Udp6WriteChild     = NULL;
  Private->Udp6ReadChild      = NULL;
  Private->Mtftp6Child        = NULL;
  Private->Dhcp6Child         = NULL;
  Private->Ip6Nic             = NULL;
  Private->Mode.Ipv6Available = FALSE;
}


/**
  Create the opened instances based on IPv4.

  @param[in]  This              Pointer to EFI_DRIVER_BINDING_PROTOCOL.
  @param[in]  ControllerHandle  Handle of the child to destroy.
  @param[in]  Private Handle    Pointer to PXEBC_PRIVATE_DATA.

  @retval EFI_SUCCESS           The instances based on IPv4 were all created successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
PxeBcCreateIp4Children (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN PXEBC_PRIVATE_DATA           *Private
  )
{
  EFI_STATUS                      Status;
  IPv4_DEVICE_PATH                Ip4Node;
  EFI_PXE_BASE_CODE_MODE          *Mode;
  EFI_UDP4_CONFIG_DATA            *Udp4CfgData;
  EFI_IP4_CONFIG_DATA             *Ip4CfgData;
  EFI_IP4_MODE_DATA               Ip4ModeData;
  PXEBC_PRIVATE_PROTOCOL          *Id;
  EFI_SIMPLE_NETWORK_PROTOCOL     *Snp;

  if (Private->Ip4Nic != NULL) {
    //
    // Already created before.
    //
    return EFI_SUCCESS;
  }

  //
  // Create Dhcp4 child and open Dhcp4 protocol for PxeBc->Dhcp.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiDhcp4ServiceBindingProtocolGuid,
             &Private->Dhcp4Child
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Dhcp4Child,
                  &gEfiDhcp4ProtocolGuid,
                  (VOID **) &Private->Dhcp4,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Mtftp4 child and open Mtftp4 protocol for PxeBc->Mtftp.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiMtftp4ServiceBindingProtocolGuid,
             &Private->Mtftp4Child
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Mtftp4Child,
                  &gEfiMtftp4ProtocolGuid,
                  (VOID **) &Private->Mtftp4,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Udp4 child and open Udp4 protocol for PxeBc->UdpRead.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiUdp4ServiceBindingProtocolGuid,
             &Private->Udp4ReadChild
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Udp4ReadChild,
                  &gEfiUdp4ProtocolGuid,
                  (VOID **) &Private->Udp4Read,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Udp4 child and open Udp4 protocol for PxeBc->UdpWrite.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiUdp4ServiceBindingProtocolGuid,
             &Private->Udp4WriteChild
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Udp4WriteChild,
                  &gEfiUdp4ProtocolGuid,
                  (VOID **) &Private->Udp4Write,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Arp child and open Arp protocol for PxeBc->Arp.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiArpServiceBindingProtocolGuid,
             &Private->ArpChild
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->ArpChild,
                  &gEfiArpProtocolGuid,
                  (VOID **) &Private->Arp,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Ip4 child and open Ip4 protocol for background ICMP packets.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiIp4ServiceBindingProtocolGuid,
             &Private->Ip4Child
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Ip4Child,
                  &gEfiIp4ProtocolGuid,
                  (VOID **) &Private->Ip4,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Get max packet size from Ip4 to calculate block size for Tftp later.
  //
  Status = Private->Ip4->GetModeData (Private->Ip4, &Ip4ModeData, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Private->Ip4MaxPacketSize = Ip4ModeData.MaxPacketSize;

  Private->Ip4Nic = AllocateZeroPool (sizeof (PXEBC_VIRTUAL_NIC));
  if (Private->Ip4Nic == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Ip4Nic->Private   = Private;
  Private->Ip4Nic->Signature = PXEBC_VIRTUAL_NIC_SIGNATURE;

  //
  // Create a device path node for Ipv4 virtual nic, and append it.
  //
  ZeroMem (&Ip4Node, sizeof (IPv4_DEVICE_PATH));
  Ip4Node.Header.Type     = MESSAGING_DEVICE_PATH;
  Ip4Node.Header.SubType  = MSG_IPv4_DP;
  Ip4Node.StaticIpAddress = FALSE;

  SetDevicePathNodeLength (&Ip4Node.Header, sizeof (Ip4Node));

  Private->Ip4Nic->DevicePath = AppendDevicePathNode (Private->DevicePath, &Ip4Node.Header);

  if (Private->Ip4Nic->DevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  CopyMem (
    &Private->Ip4Nic->LoadFile,
    &gLoadFileProtocolTemplate,
    sizeof (EFI_LOAD_FILE_PROTOCOL)
    );

  //
  // Create a new handle for IPv4 virtual nic,
  // and install PxeBaseCode, LoadFile and DevicePath protocols.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Ip4Nic->Controller,
                  &gEfiDevicePathProtocolGuid,
                  Private->Ip4Nic->DevicePath,
                  &gEfiLoadFileProtocolGuid,
                  &Private->Ip4Nic->LoadFile,
                  &gEfiPxeBaseCodeProtocolGuid,
                  &Private->PxeBc,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  if (Private->Snp != NULL) {
    //
    // Install SNP protocol on purpose is for some OS loader backward
    // compatibility consideration.
    //
    Status = gBS->InstallProtocolInterface (
                    &Private->Ip4Nic->Controller,
                    &gEfiSimpleNetworkProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    Private->Snp
                    );
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }

    //
    // Open SNP on the child handle BY_DRIVER. It will prevent any additionally 
    // layering to perform the experiment.
    //
    Status = gBS->OpenProtocol (
                    Private->Ip4Nic->Controller,
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **) &Snp,
                    This->DriverBindingHandle,
                    Private->Ip4Nic->Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }
  }

  //
  // Open PxeBaseCodePrivate protocol by child to setup a parent-child relationship between
  // real NIC handle and the virtual IPv4 NIC handle.
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &Id,
                  This->DriverBindingHandle,
                  Private->Ip4Nic->Controller,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Set default configure data for Udp4Read and Ip4 instance.
  //
  Mode                            = Private->PxeBc.Mode;
  Udp4CfgData                     = &Private->Udp4CfgData;
  Ip4CfgData                      = &Private->Ip4CfgData;

  Udp4CfgData->AcceptBroadcast    = FALSE;
  Udp4CfgData->AcceptAnyPort      = TRUE;
  Udp4CfgData->AllowDuplicatePort = TRUE;
  Udp4CfgData->TypeOfService      = Mode->ToS;
  Udp4CfgData->TimeToLive         = Mode->TTL;
  Udp4CfgData->ReceiveTimeout     = PXEBC_DEFAULT_LIFETIME;
  Udp4CfgData->TransmitTimeout    = PXEBC_DEFAULT_LIFETIME;

  Ip4CfgData->AcceptIcmpErrors    = TRUE;
  Ip4CfgData->DefaultProtocol     = EFI_IP_PROTO_ICMP;
  Ip4CfgData->TypeOfService       = Mode->ToS;
  Ip4CfgData->TimeToLive          = Mode->TTL;
  Ip4CfgData->ReceiveTimeout      = PXEBC_DEFAULT_LIFETIME;
  Ip4CfgData->TransmitTimeout     = PXEBC_DEFAULT_LIFETIME;

  return EFI_SUCCESS;

ON_ERROR:
  PxeBcDestroyIp4Children (This, Private);
  return Status;
}


/**
  Create the opened instances based on IPv6.

  @param[in]  This              Pointer to EFI_DRIVER_BINDING_PROTOCOL.
  @param[in]  ControllerHandle  Handle of the child to destroy.
  @param[in]  Private Handle    Pointer to PXEBC_PRIVATE_DATA.

  @retval EFI_SUCCESS           The instances based on IPv6 were all created successfully.
  @retval Others                An unexpected error occurred.

**/
EFI_STATUS
PxeBcCreateIp6Children (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN PXEBC_PRIVATE_DATA           *Private
  )
{
  EFI_STATUS                      Status;
  IPv6_DEVICE_PATH                Ip6Node;
  EFI_UDP6_CONFIG_DATA            *Udp6CfgData;
  EFI_IP6_CONFIG_DATA             *Ip6CfgData;
  EFI_IP6_MODE_DATA               Ip6ModeData;
  PXEBC_PRIVATE_PROTOCOL          *Id;
  EFI_SIMPLE_NETWORK_PROTOCOL     *Snp;

  if (Private->Ip6Nic != NULL) {
    //
    // Already created before.
    //
    return EFI_SUCCESS;
  }

  Private->Ip6Nic = AllocateZeroPool (sizeof (PXEBC_VIRTUAL_NIC));

  if (Private->Ip6Nic == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Ip6Nic->Private   = Private;
  Private->Ip6Nic->Signature = PXEBC_VIRTUAL_NIC_SIGNATURE;

  //
  // Create Dhcp6 child and open Dhcp6 protocol for PxeBc->Dhcp.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiDhcp6ServiceBindingProtocolGuid,
             &Private->Dhcp6Child
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Dhcp6Child,
                  &gEfiDhcp6ProtocolGuid,
                  (VOID **) &Private->Dhcp6,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Mtftp6 child and open Mtftp6 protocol for PxeBc->Mtftp.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiMtftp6ServiceBindingProtocolGuid,
             &Private->Mtftp6Child
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Mtftp6Child,
                  &gEfiMtftp6ProtocolGuid,
                  (VOID **) &Private->Mtftp6,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Udp6 child and open Udp6 protocol for PxeBc->UdpRead.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiUdp6ServiceBindingProtocolGuid,
             &Private->Udp6ReadChild
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Udp6ReadChild,
                  &gEfiUdp6ProtocolGuid,
                  (VOID **) &Private->Udp6Read,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Udp6 child and open Udp6 protocol for PxeBc->UdpWrite.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiUdp6ServiceBindingProtocolGuid,
             &Private->Udp6WriteChild
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Udp6WriteChild,
                  &gEfiUdp6ProtocolGuid,
                  (VOID **) &Private->Udp6Write,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create Ip6 child and open Ip6 protocol for background ICMP6 packets.
  //
  Status = NetLibCreateServiceChild (
             ControllerHandle,
             This->DriverBindingHandle,
             &gEfiIp6ServiceBindingProtocolGuid,
             &Private->Ip6Child
             );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Status = gBS->OpenProtocol (
                  Private->Ip6Child,
                  &gEfiIp6ProtocolGuid,
                  (VOID **) &Private->Ip6,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Get max packet size from Ip6 to calculate block size for Tftp later.
  //
  Status = Private->Ip6->GetModeData (Private->Ip6, &Ip6ModeData, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  Private->Ip6MaxPacketSize = Ip6ModeData.MaxPacketSize;

  //
  // Locate Ip6->Ip6Config and store it for set IPv6 address.
  //
  Status = gBS->HandleProtocol (
                  ControllerHandle,
                  &gEfiIp6ConfigProtocolGuid,
                  (VOID **) &Private->Ip6Cfg
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Create a device path node for Ipv6 virtual nic, and append it.
  //
  ZeroMem (&Ip6Node, sizeof (IPv6_DEVICE_PATH));
  Ip6Node.Header.Type     = MESSAGING_DEVICE_PATH;
  Ip6Node.Header.SubType  = MSG_IPv6_DP;
  Ip6Node.PrefixLength    = IP6_PREFIX_LENGTH;

  SetDevicePathNodeLength (&Ip6Node.Header, sizeof (Ip6Node));

  Private->Ip6Nic->DevicePath = AppendDevicePathNode (Private->DevicePath, &Ip6Node.Header);

  if (Private->Ip6Nic->DevicePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto ON_ERROR;
  }

  CopyMem (
    &Private->Ip6Nic->LoadFile,
    &gLoadFileProtocolTemplate,
    sizeof (EFI_LOAD_FILE_PROTOCOL)
    );

  //
  // Create a new handle for IPv6 virtual nic,
  // and install PxeBaseCode, LoadFile and DevicePath protocols.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Private->Ip6Nic->Controller,
                  &gEfiDevicePathProtocolGuid,
                  Private->Ip6Nic->DevicePath,
                  &gEfiLoadFileProtocolGuid,
                  &Private->Ip6Nic->LoadFile,
                  &gEfiPxeBaseCodeProtocolGuid,
                  &Private->PxeBc,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }
  
  if (Private->Snp != NULL) {
    //
    // Install SNP protocol on purpose is for some OS loader backward
    // compatibility consideration.
    //
    Status = gBS->InstallProtocolInterface (
                    &Private->Ip6Nic->Controller,
                    &gEfiSimpleNetworkProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    Private->Snp
                    );
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }

    //
    // Open SNP on the child handle BY_DRIVER. It will prevent any additionally 
    // layering to perform the experiment.
    //
    Status = gBS->OpenProtocol (
                    Private->Ip6Nic->Controller,
                    &gEfiSimpleNetworkProtocolGuid,
                    (VOID **) &Snp,
                    This->DriverBindingHandle,
                    Private->Ip6Nic->Controller,
                    EFI_OPEN_PROTOCOL_BY_DRIVER
                    );
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }
  }

  //
  // Open PxeBaseCodePrivate protocol by child to setup a parent-child relationship between
  // real NIC handle and the virtual IPv6 NIC handle.
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &Id,
                  This->DriverBindingHandle,
                  Private->Ip6Nic->Controller,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    goto ON_ERROR;
  }

  //
  // Set IPv6 avaiable flag and set default configure data for
  // Udp6Read and Ip6 instance.
  //
  Private->Mode.Ipv6Available     = TRUE;
  Udp6CfgData                     = &Private->Udp6CfgData;
  Ip6CfgData                      = &Private->Ip6CfgData;

  Udp6CfgData->AcceptAnyPort      = TRUE;
  Udp6CfgData->AllowDuplicatePort = TRUE;
  Udp6CfgData->HopLimit           = PXEBC_DEFAULT_HOPLIMIT;
  Udp6CfgData->ReceiveTimeout     = PXEBC_DEFAULT_LIFETIME;
  Udp6CfgData->TransmitTimeout    = PXEBC_DEFAULT_LIFETIME;

  Ip6CfgData->AcceptIcmpErrors    = TRUE;
  Ip6CfgData->DefaultProtocol     = IP6_ICMP;
  Ip6CfgData->HopLimit            = PXEBC_DEFAULT_HOPLIMIT;
  Ip6CfgData->ReceiveTimeout      = PXEBC_DEFAULT_LIFETIME;
  Ip6CfgData->TransmitTimeout     = PXEBC_DEFAULT_LIFETIME;

  return EFI_SUCCESS;

ON_ERROR:
  PxeBcDestroyIp6Children (This, Private);
  return Status;
}


/**
  The entry point for UefiPxeBc driver that installs the driver
  binding and component name protocol on its image.

  @param[in]  ImageHandle          The Image handle of the driver.
  @param[in]  SystemTable          The system table.

  @return EFI_SUCCESS
  @return Others

**/
EFI_STATUS
EFIAPI
PxeBcDriverEntryPoint (
  IN EFI_HANDLE             ImageHandle,
  IN EFI_SYSTEM_TABLE       *SystemTable
  )
{
  return EfiLibInstallDriverBindingComponentName2 (
           ImageHandle,
           SystemTable,
           &gPxeBcDriverBinding,
           ImageHandle,
           &gPxeBcComponentName,
           &gPxeBcComponentName2
           );
}


/**
  Test to see if this driver supports ControllerHandle. This service
  is called by the EFI boot service ConnectController(). In
  order to make drivers as small as possible, there are a few calling
  restrictions for this service. ConnectController() must
  follow these calling restrictions. If any other agent wishes to call
  Supported() it must also follow these calling restrictions.

  @param[in]  This                The pointer to the driver binding protocol.
  @param[in]  ControllerHandle    The handle of device to be tested.
  @param[in]  RemainingDevicePath Optional parameter used to pick a specific child
                                  device to be started.

  @retval EFI_SUCCESS         This driver supports this device.
  @retval EFI_UNSUPPORTED     This driver does not support this device.

**/
EFI_STATUS
EFIAPI
PxeBcDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS                      Ip4Status;
  EFI_STATUS                      Ip6Status;

  //
  // Try to open the Mtftp4 and Dhcp4 protocol to test whether IPv4 stack is ready.
  //
  Ip4Status = gBS->OpenProtocol (
                     ControllerHandle,
                     &gEfiDhcp4ServiceBindingProtocolGuid,
                     NULL,
                     This->DriverBindingHandle,
                     ControllerHandle,
                     EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                     );
  if (!EFI_ERROR (Ip4Status)) {
    Ip4Status = gBS->OpenProtocol (
                       ControllerHandle,
                       &gEfiMtftp4ServiceBindingProtocolGuid,
                       NULL,
                       This->DriverBindingHandle,
                       ControllerHandle,
                       EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                       );
  }

  //
  // Try to open the Mtftp6 and Dhcp6 protocol to test whether IPv4 stack is ready.
  //
  Ip6Status = gBS->OpenProtocol (
                     ControllerHandle,
                     &gEfiDhcp6ServiceBindingProtocolGuid,
                     NULL,
                     This->DriverBindingHandle,
                     ControllerHandle,
                     EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                     );
  if (!EFI_ERROR (Ip6Status)) {
    Ip6Status = gBS->OpenProtocol (
                       ControllerHandle,
                       &gEfiMtftp6ServiceBindingProtocolGuid,
                       NULL,
                       This->DriverBindingHandle,
                       ControllerHandle,
                       EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                       );
  }

  //
  // It's unsupported case if both stack are not ready.
  //
  if (EFI_ERROR (Ip4Status) && EFI_ERROR (Ip6Status)) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}


/**
  Start this driver on ControllerHandle. This service is called by the
  EFI boot service ConnectController(). In order to make
  drivers as small as possible, there are a few calling restrictions for
  this service. ConnectController() must follow these
  calling restrictions. If any other agent wishes to call Start() it
  must also follow these calling restrictions.

  @param[in]  This                 The pointer to the driver binding protocol.
  @param[in]  ControllerHandle     The handle of device to be started.
  @param[in]  RemainingDevicePath  Optional parameter used to pick a specific child
                                   device to be started.

  @retval EFI_SUCCESS          This driver is installed to ControllerHandle.
  @retval EFI_ALREADY_STARTED  This driver is already running on ControllerHandle.
  @retval other                This driver does not support this device.

**/
EFI_STATUS
EFIAPI
PxeBcDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  )
{
  PXEBC_PRIVATE_DATA              *Private;
  EFI_STATUS                      Status;
  EFI_STATUS                      Ip4Status;
  EFI_STATUS                      Ip6Status;
  PXEBC_PRIVATE_PROTOCOL          *Id;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &Id,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    //
    // Skip the initialization if the driver has been started already.
    //
    Private = PXEBC_PRIVATE_DATA_FROM_ID (Id);
  } else {
    //
    // If the driver has not been started yet, it should do initialization.
    //
    Private = AllocateZeroPool (sizeof (PXEBC_PRIVATE_DATA));
    if (Private == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    CopyMem (
      &Private->PxeBc,
      &gPxeBcProtocolTemplate,
      sizeof (EFI_PXE_BASE_CODE_PROTOCOL)
      );

    Private->Signature          = PXEBC_PRIVATE_DATA_SIGNATURE;
    Private->Controller         = ControllerHandle;
    Private->Image              = This->ImageHandle;
    Private->PxeBc.Mode         = &Private->Mode;
    Private->Mode.Ipv6Supported = TRUE;
    Private->Mode.AutoArp       = TRUE;
    Private->Mode.TTL           = DEFAULT_TTL;
    Private->Mode.ToS           = DEFAULT_ToS;

    //
    // Open device path to prepare for appending virtual NIC node.
    //
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID **) &Private->DevicePath,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );

    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }

    //
    // Get the NII interface if it exists, it's not required.
    //
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiNetworkInterfaceIdentifierProtocolGuid_31,
                    (VOID **) &Private->Nii,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      Private->Nii = NULL;
    }

    //
    // Install PxeBaseCodePrivate protocol onto the real NIC handler.
    // PxeBaseCodePrivate protocol is only used to keep the relationship between 
    // NIC handle and virtual child handles.
    // gEfiCallerIdGuid will be used as its protocol guid.
    //
    Status = gBS->InstallProtocolInterface (
                    &ControllerHandle,
                    &gEfiCallerIdGuid,
                    EFI_NATIVE_INTERFACE,
                    &Private->Id
                    );
    if (EFI_ERROR (Status)) {
      goto ON_ERROR;
    }

    //
    // Try to locate SNP protocol.
    //
    NetLibGetSnpHandle(ControllerHandle, &Private->Snp);    
  }

  //
  // Try to create virtual NIC handle for IPv4.
  //
  Ip4Status = PxeBcCreateIp4Children (This, ControllerHandle, Private);

  //
  // Try to create virtual NIC handle for IPv6.
  //
  Ip6Status = PxeBcCreateIp6Children (This, ControllerHandle, Private);

  if (EFI_ERROR (Ip4Status) && EFI_ERROR (Ip6Status)) {
    //
    // Failed to start PXE driver if IPv4 and IPv6 stack are both not available.
    //
    Status = EFI_DEVICE_ERROR;
    goto ON_ERROR;
  }

  return EFI_SUCCESS;

ON_ERROR:
  gBS->UninstallProtocolInterface (
         ControllerHandle,
         &gEfiCallerIdGuid,
         &Private->Id
         );
  PxeBcDestroyIp4Children (This, Private);
  PxeBcDestroyIp6Children (This, Private);
  FreePool (Private);

  return Status;
}


/**
  Stop this driver on ControllerHandle. This service is called by the
  EFI boot service DisconnectController(). In order to
  make drivers as small as possible, there are a few calling
  restrictions for this service. DisconnectController()
  must follow these calling restrictions. If any other agent wishes
  to call Stop() it must also follow these calling restrictions.

  @param[in]  This              Protocol instance pointer.
  @param[in]  ControllerHandle  Handle of device to stop driver on.
  @param[in]  NumberOfChildren  Number of Handles in ChildHandleBuffer. If number of
                                children is zero stop the entire bus driver.
  @param[in]  ChildHandleBuffer List of Child Handles to Stop.

  @retval EFI_SUCCESS           This driver was removed ControllerHandle.
  @retval EFI_DEVICE_ERROR      An unexpected system or network error occurred.
  @retval Others                This driver was not removed from this device

**/
EFI_STATUS
EFIAPI
PxeBcDriverBindingStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  PXEBC_PRIVATE_DATA              *Private;
  PXEBC_VIRTUAL_NIC               *VirtualNic;
  EFI_LOAD_FILE_PROTOCOL          *LoadFile;
  EFI_STATUS                      Status;
  EFI_HANDLE                      NicHandle;
  BOOLEAN                         IsIpv6;
  PXEBC_PRIVATE_PROTOCOL          *Id;

  Private    = NULL;
  NicHandle  = NULL;
  VirtualNic = NULL;
  LoadFile   = NULL;
  Id         = NULL;
  IsIpv6     = FALSE;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiLoadFileProtocolGuid,
                  (VOID **) &LoadFile,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    //
    // Get the Nic handle by any pass-over service child handle.
    //
    NicHandle = PxeBcGetNicByIp4Children (ControllerHandle);
    if (NicHandle == NULL) {
      NicHandle = PxeBcGetNicByIp6Children (ControllerHandle);
      if (NicHandle == NULL) {
        return EFI_DEVICE_ERROR;
      } else {
        IsIpv6 = TRUE;
      }
    }

    //
    // Try to retrieve the private data by PxeBcPrivate protocol.
    //
    Status = gBS->OpenProtocol (
                    NicHandle,
                    &gEfiCallerIdGuid,
                    (VOID **) &Id,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Private = PXEBC_PRIVATE_DATA_FROM_ID (Id);

  } else {
    //
    // It's a virtual handle with LoadFileProtocol.
    //
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiLoadFileProtocolGuid,
                    (VOID **) &LoadFile,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    VirtualNic = PXEBC_VIRTUAL_NIC_FROM_LOADFILE (LoadFile);
    Private    = VirtualNic->Private;
    NicHandle  = Private->Controller;

    if (Private->Ip6Nic == VirtualNic) {
      IsIpv6   = TRUE;
    }
  }

  //
  // Stop functionality of PXE Base Code protocol
  //
  Status = Private->PxeBc.Stop (&Private->PxeBc);
  if (Status != EFI_SUCCESS && Status != EFI_NOT_STARTED) {
    return Status;
  }


  if (Private->Ip4Nic != NULL && !IsIpv6) {
    PxeBcDestroyIp4Children (This, Private);
  }

  if (Private->Ip6Nic != NULL && IsIpv6) {
    PxeBcDestroyIp6Children (This, Private);
  }

  if (Private->Ip4Nic == NULL && Private->Ip6Nic == NULL) {
    gBS->UninstallProtocolInterface (
           NicHandle,
           &gEfiCallerIdGuid,
           &Private->Id
           );
    FreePool (Private);
  }

  return EFI_SUCCESS;
}
