#pragma once

#include "public.h"

// 定义设备对象的环境变量
typedef struct _DEVICE_CONTEXT
{
	ULONG PrivateDeviceData;  // just a placeholder

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

NTSTATUS
EchoDeviceCreate(
	PWDFDEVICE_INIT DeviceInit
);

EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT EchoEvtDeviceSelfManagedIoStart;
EVT_WDF_DEVICE_SELF_MANAGED_IO_SUSPEND EchoEvtDeviceSelfManagedIoSuspend;