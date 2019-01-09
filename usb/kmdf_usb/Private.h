#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <usbdi.h>
#include <usbdlib.h>
#include <wdfusb.h>
#include <initguid.h>
#include <usbbusif.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "trace.h"
#include "public.h"

#ifndef _PRIVATE_H_
#define _PRIVATE_H_

#define POOL_TAG (ULONG) 'FRSO'
#define _DRIVER_NAME_ "KMDFUSB"

#define TEST_BOARD_TRANSFER_BUFFER_SIZE (64*1024)
#define DEVICE_DESC_LENGTH 256

extern const __declspec(selectany) LONGLONG DEFAULT_CONTROL_TRANSFER_TIMEOUT = 5 * -1 * WDF_TIMEOUT_TO_SEC;

//
// Define the vendor commands supported by our device
//
#define USBFX2LK_READ_7SEGMENT_DISPLAY      0xD4
#define USBFX2LK_READ_SWITCHES              0xD6
#define USBFX2LK_READ_BARGRAPH_DISPLAY      0xD7
#define USBFX2LK_SET_BARGRAPH_DISPLAY       0xD8
#define USBFX2LK_IS_HIGH_SPEED              0xD9
#define USBFX2LK_REENUMERATE                0xDA
#define USBFX2LK_SET_7SEGMENT_DISPLAY       0xDB

//
// Define the features that we can clear
//  and set on our device
//
#define USBFX2LK_FEATURE_EPSTALL            0x00
#define USBFX2LK_FEATURE_WAKE               0x01

//
// Order of endpoints in the interface descriptor
//
#define INTERRUPT_IN_ENDPOINT_INDEX    0
#define BULK_OUT_ENDPOINT_INDEX        1
#define BULK_IN_ENDPOINT_INDEX         2

//
// The device context performs the same job as a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT {

	WDFUSBDEVICE                    UsbDevice;

	WDFUSBINTERFACE                 UsbInterface;

	WDFUSBPIPE                      BulkReadPipe;

	WDFUSBPIPE                      BulkWritePipe;

	WDFUSBPIPE                      InterruptPipe;

	WDFWAITLOCK                     ResetDeviceWaitLock;

	UCHAR                           CurrentSwitchState;

	WDFQUEUE                        InterruptMsgQueue;

	ULONG                           UsbDeviceTraits;

	//
	// The following fields are used during event logging to 
	// report the events relative to this specific instance 
	// of the device.
	//

	WDFMEMORY                       DeviceNameMemory;
	PCWSTR                          DeviceName;

	WDFMEMORY                       LocationMemory;
	PCWSTR                          Location;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

//
// This is the context that can be placed per queue and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT {

	ULONG PrivateDeviceData;  // just a placeholder

} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, GetQueueContext)


typedef
NTSTATUS
(*PFN_IO_GET_ACTIVITY_ID_IRP) (
	_In_     PIRP   Irp,
	_Out_    LPGUID Guid
	);

typedef
NTSTATUS
(*PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA) (
	_In_ PUNICODE_STRING    SymbolicLinkName,
	_In_ CONST DEVPROPKEY   *PropertyKey,
	_In_ LCID               Lcid,
	_In_ ULONG              Flags,
	_In_ DEVPROPTYPE        Type,
	_In_ ULONG              Size,
	_In_opt_ PVOID          Data
	);

//
// Global function pointer set in DriverEntry
// Check for NULL before using
//
extern PFN_IO_GET_ACTIVITY_ID_IRP g_pIoGetActivityIdIrp;

extern PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA g_pIoSetDeviceInterfacePropertyData;

//
// Driver.c func
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP KmdfUsbEvtDriverContextCleanup;
EVT_WDF_DEVICE_D0_ENTRY KmdfUsbEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT KmdfUsbEvtDeviceD0Exit;
EVT_WDF_DEVICE_SELF_MANAGED_IO_FLUSH KmdfUsbEvtDeviceSelfManagedIoFlush;

_IRQL_requires_(PASSIVE_LEVEL)
PCHAR
DbgDevicePowerString(
	_In_ WDF_POWER_DEVICE_STATE Type
);

_IRQL_requires_(PASSIVE_LEVEL)
VOID
GetDeviceEventLoggingNames(
	_In_ WDFDEVICE Device
);

//
// Device.c func
//

EVT_WDF_DRIVER_DEVICE_ADD KmdfUsbEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE KmdfUsbEvtDevicePrepareHardware;

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SelectInterfaces(
	_In_ WDFDEVICE Device
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
KmdfUsbSetPowerPolicy(
	_In_ WDFDEVICE Device
);

//
// Interrupt.c func
//

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
KmdfUsbConfigContReaderForInterruptEndPoint(
	_In_ PDEVICE_CONTEXT DeviceContext
);

VOID
KmdfUsbEvtUsbInterruptPipeReadComplete(
	WDFUSBPIPE  Pipe,
	WDFMEMORY   Buffer,
	size_t      NumBytesTransferred,
	WDFCONTEXT  Context
);

BOOLEAN
KmdfUsbEvtUsbInterruptReadersFailed(
	_In_ WDFUSBPIPE Pipe,
	_In_ NTSTATUS Status,
	_In_ USBD_STATUS UsbdStatus
);


//
// Ioctl.c func
//

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL KmdfUsbEvtIoDeviceControl;

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetPipe(
	_In_ WDFUSBPIPE Pipe
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetDevice(
	_In_ WDFDEVICE Device
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ReenumerateDevice(
	_In_ PDEVICE_CONTEXT DevContext
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
GetBarGraphState(
	_In_ PDEVICE_CONTEXT DevContext,
	_Out_ PBAR_GRAPH_STATE BarGraphState
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SetBarGraphState(
	_In_ PDEVICE_CONTEXT DevContext,
	_In_ PBAR_GRAPH_STATE BarGraphState
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
GetSevenSegmentState(
	_In_ PDEVICE_CONTEXT DevContext,
	_Out_ PUCHAR SevenSegment
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SetSevenSegmentState(
	_In_ PDEVICE_CONTEXT DevContext,
	_In_ PUCHAR SevenSegment
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
GetSwitchState(
	_In_ PDEVICE_CONTEXT DevContext,
	_In_ PSWITCH_STATE SwitchState
);

VOID
KmdfUsbIoctlGetInterruptMessage(
	_In_ WDFDEVICE Device,
	_In_ NTSTATUS ReaderStatus
);


//
// Others
//

FORCEINLINE
GUID
RequestToActivityId(
	_In_ WDFREQUEST Request
)
{
	GUID     activity = { 0 };
	NTSTATUS status = STATUS_SUCCESS;

	if (g_pIoGetActivityIdIrp != NULL) {

		//
		// Use activity ID generated by application (or IO manager)
		//    
		status = g_pIoGetActivityIdIrp(WdfRequestWdmGetIrp(Request), &activity);
	}

	if (g_pIoGetActivityIdIrp == NULL || !NT_SUCCESS(status)) {

		//
		// Fall back to using the WDFREQUEST handle as the activity ID
		//
		RtlCopyMemory(&activity, &Request, sizeof(WDFREQUEST));
	}


	return activity;
}

FORCEINLINE
GUID
DeviceToActivityId(
	_In_ WDFDEVICE Device
)
{
	GUID activity = { 0 };
	RtlCopyMemory(&activity, &Device, sizeof(WDFDEVICE));
	return activity;
}

#endif

