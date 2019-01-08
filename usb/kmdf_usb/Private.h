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

//
// The device context performs the same job as a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
	WDFUSBDEVICE UsbDevice;
	ULONG PrivateDeviceData;  // just a placeholder

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

//
// This is the context that can be placed per queue and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT {

	ULONG PrivateDeviceData;  // just a placeholder

} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, GetQueueContext)


//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD KmdfUsbEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP KmdfUsbEvtDriverContextCleanup;


//
// WDFDEVICE Events
//

// Function to initialize the device's queues and callbacks
NTSTATUS
KmdfUsbCreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
);

// Function to select the device's USB configuration and get a WDFUSBDEVICE handle
EVT_WDF_DEVICE_PREPARE_HARDWARE KmdfUsbEvtDevicePrepareHardware;

//
// WDFQUEUE Events
//

NTSTATUS
KmdfUsbQueueInitialize(
	_In_ WDFDEVICE Device
);

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL KmdfUsbEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP KmdfUsbEvtIoStop;

#endif

