
#include "private.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CharSample_EvtDeviceAdd)
#endif

NTSTATUS
CharSample_EvtDeviceAdd(
	_In_ WDFDRIVER       Driver,
	_Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS			status;
    WDFDEVICE			device;
    WDF_IO_QUEUE_CONFIG	ioQueueConfig;

	//例程的首句PAGED_CODE，表示该例程的代码占用分页内存。
	//只能在PASSIVE_LEVEL中断级别调用该例程，否则会蓝屏。
	//如不说明，则占用系统的非分页内存，要珍惜使用。
    PAGED_CODE();

	//创建设备，没有对象属性和设备对象环境变量结构
    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

	//初始化缺省队列配置，设置I/O请求分发处理方式为串行。
	//对这个实例而言，选择串行或并行都可以，但不能选手工。
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchSequential);

	//设置EvtIoDeviceControl例程，处理应用程序的DeviceIoControl()函数调用
    ioQueueConfig.EvtIoDeviceControl  = CharSample_EvtIoDeviceControl;

	//创建队列
    status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

	//创建设备GUID接口
    status = WdfDeviceCreateDeviceInterface(device, (LPGUID) &CharSample_DEVINTERFACE_GUID, NULL);
    if (!NT_SUCCESS(status)) {
    }

    return status;
}


