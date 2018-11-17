#pragma warning(disable:4200)  //
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

#include <ntddk.h>
#include <wdf.h>

#include "public.h"

#ifndef _H
#define _H

NTSTATUS
DriverEntry(
	IN PDRIVER_OBJECT  DriverObject,
	IN PUNICODE_STRING RegistryPath
);

NTSTATUS
CharSample_EvtDeviceAdd(
	IN WDFDRIVER       Driver,
	IN PWDFDEVICE_INIT DeviceInit
);

VOID
CharSample_EvtIoDeviceControl(
	IN WDFQUEUE   Queue,
	IN WDFREQUEST Request,
	IN size_t     OutputBufferLength,
	IN size_t     InputBufferLength,
	IN ULONG      IoControlCode
);

#endif
