#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, EchoDeviceCreate)
#pragma alloc_text (PAGE, EchoEvtDeviceSelfManagedIoSuspend)
#endif


// 在EvtDeviceAdd回调函数中被调用
// 1 初始化/注册pnp电源管理回调函数
// 2 初始化设备对象的属性和环境变量
// 3 创建设备对象（创建对象之后，DeviceInit不可再访问）
// 4 创建设备接口
// 5 初始化队列
NTSTATUS
EchoDeviceCreate(
	PWDFDEVICE_INIT DeviceInit
)
{
	WDF_OBJECT_ATTRIBUTES deviceAttributes;				// 设备对象的属性
	PDEVICE_CONTEXT deviceContext;						// 设备对象的环境变量
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;		// pnp回调函数
	WDFDEVICE device;
	NTSTATUS status;

	PAGED_CODE();

	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

	// 1 初始化/注册pnp电源管理回调函数

	// 设备进入D0状态时调用，启动Queue，启动Timer
	pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EchoEvtDeviceSelfManagedIoStart;
	// 设备离开D0状态时调用，停止Queue，停止Timer
	pnpPowerCallbacks.EvtDeviceSelfManagedIoSuspend = EchoEvtDeviceSelfManagedIoSuspend;
	// 设备重启动时调用
	pnpPowerCallbacks.EvtDeviceSelfManagedIoRestart = EchoEvtDeviceSelfManagedIoStart;

	// Register the PnP and power callbacks. Power policy related callbacks will be registered later in SotwareInit.
	// 注册pnp和电源管理回调函数，电源策略相关的回调函数在之后注册
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	// 2 初始化设备对象的属性和环境变量
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	// 3 创建设备对象
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

	if (NT_SUCCESS(status)) {

		// 2.x 初始化设备对象的属性和环境变量
		deviceContext = GetDeviceContext(device);
		deviceContext->PrivateDeviceData = 0;

		// 4 创建设备接口
		status = WdfDeviceCreateDeviceInterface(
			device,
			&GUID_DEVINTERFACE_ECHO,
			NULL			// ReferenceString
		);

		if (NT_SUCCESS(status)) {
			// 5 初始化队列
			status = EchoQueueInitialize(device);
		}
	}

	return status;
}

// This event is called by the Framework when the device is started or restarted after a suspend operation.
// This function is not marked pageable because this function is in the device power up path.
// 设备进入D0状态时调用，启动Queue，启动Timer
NTSTATUS
EchoEvtDeviceSelfManagedIoStart(
	IN  WDFDEVICE Device
)
{
	PQUEUE_CONTEXT queueContext = QueueGetContext(WdfDeviceGetDefaultQueue(Device));
	LARGE_INTEGER DueTime;

	KdPrint(("--> EchoEvtDeviceSelfManagedIoInit\n"));

	// 启动默认队列
	WdfIoQueueStart(WdfDeviceGetDefaultQueue(Device));

	// 启动定时器，第一次调用在100ms以后
	DueTime.QuadPart = WDF_REL_TIMEOUT_IN_MS(100);
	WdfTimerStart(queueContext->Timer, DueTime.QuadPart);

	KdPrint(("<-- EchoEvtDeviceSelfManagedIoInit\n"));

	return STATUS_SUCCESS;

}


// This event is called by the Framework when the device is stopped for resource rebalance or suspended when the system is entering Sx state.
// 设备离开D0状态时调用，停止Queue，停止Timer
NTSTATUS
EchoEvtDeviceSelfManagedIoSuspend(
	IN  WDFDEVICE Device
)
{
	PQUEUE_CONTEXT queueContext = QueueGetContext(WdfDeviceGetDefaultQueue(Device));

	PAGED_CODE();

	KdPrint(("--> EchoEvtDeviceSelfManagedIoSuspend\n"));

	// 设备挂起前，队列中仍有未完成的I/O请求，两种处理方式：
	// 1) 等待所有未完成请求完成后再挂起
	// 2) 队列注册EvtIoStop回调函数，确认通知框架可以挂起具有未完成I/O的设备请求
	// 这里使用第一种方法，调用WdfIoQueueStopSynchronously，以同步方式停止队列
	// 调用WdfIoQueueStopSynchronously后，请求分发停止，但仍接收，直到所有请求都完成或取消后，才返回
	WdfIoQueueStopSynchronously(WdfDeviceGetDefaultQueue(Device));

	// 停止定时器，等待定时器回调函数执行完后才返回
	WdfTimerStop(queueContext->Timer, TRUE);

	KdPrint(("<-- EchoEvtDeviceSelfManagedIoSuspend\n"));

	return STATUS_SUCCESS;
}
