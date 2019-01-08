/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_KMDFUSB,
    0x2f021a6e,0xe822,0x45dd,0xb7,0x78,0x1f,0x01,0xa6,0x8b,0x6f,0xff);
// {2f021a6e-e822-45dd-b778-1f01a68b6fff}
