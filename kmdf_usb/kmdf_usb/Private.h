#pragma once

#ifndef _PRIVATE_H_
#define _PRIVATE_H_

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


// ****************************************************************************************
#define	 DRIVER_CY001		"CY001: "
#define  POOLTAG			'00YC'

#define  MAX_INSTANCE_NUMBER	8
#define  MAX_INTERFACES			8


//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT {
	WDFUSBDEVICE      UsbDevice;
	WDFUSBINTERFACE   UsbInterface;
	WDFUSBINTERFACE   MulInterfaces[MAX_INTERFACES];

	WDFQUEUE          IoRWQueue;
	WDFQUEUE          IoControlEntryQueue;
	WDFQUEUE          IoControlSerialQueue;
	WDFQUEUE          InterruptManualQueue;
	WDFQUEUE		  AppSyncManualQueue;

	WDFUSBPIPE		  UsbCtlPipe;
	WDFUSBPIPE        UsbIntOutPipe;
	WDFUSBPIPE        UsbIntInPipe;
	WDFUSBPIPE        UsbBulkInPipe;
	WDFUSBPIPE        UsbBulkOutPipe;
	BYTE			  byPreLEDs;
	BYTE			  byPre7Seg;

	ULONG						LastUSBErrorStatusValue;	// 这个值不断被更新，体现最新的错误值
	USB_BUS_INTERFACE_USBDI_V1	busInterface;
	BOOLEAN						bIsDeviceHighSpeed;
	BOOLEAN						bIntPipeConfigured;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// ****************************************************************************************
//
// This is the context that can be placed per queue
// and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT {

	ULONG PrivateDeviceData;  // just a placeholder

} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, GetQueueContext)

// ****************************************************************************************
//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD KmdfUsbEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP KmdfUsbEvtDriverContextCleanup;

// ****************************************************************************************
//
// WDFDEVICE Events
//

//
// Function to initialize the device's queues and callbacks
//
NTSTATUS
KmdfUsbCreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
);

NTSTATUS
ConfigureUsbDevice(
	WDFDEVICE Device,
	PDEVICE_CONTEXT DeviceContext
);

NTSTATUS
GetUsbPipes(
	PDEVICE_CONTEXT DeviceContext
);

NTSTATUS
InitPowerManagement(
	IN WDFDEVICE  Device
);

int
InitSettingPairs(
	IN WDFUSBDEVICE UsbDevice,					// 设备对象
	OUT PWDF_USB_INTERFACE_SETTING_PAIR Pairs,  // 结构体指针。
	IN ULONG NumSettings						// 接口个数
);

char*
PowerName(
	WDF_POWER_DEVICE_STATE PowerState
);

//
// Function to select the device's USB configuration and get a WDFUSBDEVICE
// handle
//
EVT_WDF_DEVICE_PREPARE_HARDWARE KmdfUsbEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE KmdfUsbEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_SURPRISE_REMOVAL KmdfUsbEvtDeviceSurpriseRemoval;
EVT_WDF_DEVICE_D0_ENTRY KmdfUsbEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT KmdfUsbEvtDeviceD0Exit;

// ****************************************************************************************
// 
// WDFDQUEUE Events
//

NTSTATUS
KmdfUsbQueueInitialize(
	_In_ WDFDEVICE Device
);

void
CompleteSyncRequest(
	WDFDEVICE Device,
	DRIVER_SYNC_ORDER_TYPE type,
	int info
);

NTSTATUS
GetOneSyncRequest(
	WDFDEVICE Device,
	WDFREQUEST* pRequest
);

NTSTATUS
InterruptReadStop(
	WDFDEVICE Device
);

void
ClearSyncQueue(
	WDFDEVICE Device
);

WDFUSBPIPE
GetInterruptPipe(
	BOOLEAN bInput,
	WDFDEVICE Device
);

NTSTATUS
SetDigitron(
	IN WDFDEVICE Device,
	IN UCHAR chSet
);

NTSTATUS
GetDigitron(
	IN WDFDEVICE Device,
	IN UCHAR* pchGet
);

NTSTATUS
SetLEDs(
	IN WDFDEVICE Device,
	IN UCHAR chSet
);

NTSTATUS
GetLEDs(
	IN WDFDEVICE Device,
	IN UCHAR* pchGet
);

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL KmdfUsbEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP KmdfUsbEvtIoStop;

// ****************************************************************************************

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DeviceIoControlParallel;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DeviceIoControlSerial;

NTSTATUS
AbortPipe(
	IN WDFDEVICE Device,
	IN ULONG nPipeNum
);

NTSTATUS
GetStringDes(
	USHORT shIndex,
	USHORT shLanID,
	VOID* pBufferOutput,
	ULONG OutputBufferLength,
	ULONG* pulRetLen,
	PDEVICE_CONTEXT pContext
);

NTSTATUS
FirmwareReset(
	IN WDFDEVICE Device,
	IN UCHAR resetBit
);

NTSTATUS
FirmwareUpload(
	WDFDEVICE Device,
	PUCHAR pData,
	ULONG ulLen,
	WORD offset
);

NTSTATUS
ReadRAM(
	WDFDEVICE Device,
	WDFREQUEST request,
	ULONG* pLen
);

NTSTATUS
UsbSetOrClearFeature(
	WDFDEVICE Device,
	WDFREQUEST Request
);

#endif