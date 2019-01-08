#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, EchoDeviceCreate)
#pragma alloc_text (PAGE, EchoEvtDeviceSelfManagedIoSuspend)
#endif


// ��EvtDeviceAdd�ص������б�����
// 1 ��ʼ��/ע��pnp��Դ����ص�����
// 2 ��ʼ���豸��������Ժͻ�������
// 3 �����豸���󣨴�������֮��DeviceInit�����ٷ��ʣ�
// 4 �����豸�ӿ�
// 5 ��ʼ������
NTSTATUS
EchoDeviceCreate(
	PWDFDEVICE_INIT DeviceInit
)
{
	WDF_OBJECT_ATTRIBUTES deviceAttributes;				// �豸���������
	PDEVICE_CONTEXT deviceContext;						// �豸����Ļ�������
	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;		// pnp�ص�����
	WDFDEVICE device;
	NTSTATUS status;

	PAGED_CODE();

	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

	// 1 ��ʼ��/ע��pnp��Դ����ص�����

	// �豸����D0״̬ʱ���ã�����Queue������Timer
	pnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EchoEvtDeviceSelfManagedIoStart;
	// �豸�뿪D0״̬ʱ���ã�ֹͣQueue��ֹͣTimer
	pnpPowerCallbacks.EvtDeviceSelfManagedIoSuspend = EchoEvtDeviceSelfManagedIoSuspend;
	// �豸������ʱ����
	pnpPowerCallbacks.EvtDeviceSelfManagedIoRestart = EchoEvtDeviceSelfManagedIoStart;

	// Register the PnP and power callbacks. Power policy related callbacks will be registered later in SotwareInit.
	// ע��pnp�͵�Դ����ص���������Դ������صĻص�������֮��ע��
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	// 2 ��ʼ���豸��������Ժͻ�������
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	// 3 �����豸����
	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

	if (NT_SUCCESS(status)) {

		// 2.x ��ʼ���豸��������Ժͻ�������
		deviceContext = GetDeviceContext(device);
		deviceContext->PrivateDeviceData = 0;

		// 4 �����豸�ӿ�
		status = WdfDeviceCreateDeviceInterface(
			device,
			&GUID_DEVINTERFACE_ECHO,
			NULL			// ReferenceString
		);

		if (NT_SUCCESS(status)) {
			// 5 ��ʼ������
			status = EchoQueueInitialize(device);
		}
	}

	return status;
}

// This event is called by the Framework when the device is started or restarted after a suspend operation.
// This function is not marked pageable because this function is in the device power up path.
// �豸����D0״̬ʱ���ã�����Queue������Timer
NTSTATUS
EchoEvtDeviceSelfManagedIoStart(
	IN  WDFDEVICE Device
)
{
	PQUEUE_CONTEXT queueContext = QueueGetContext(WdfDeviceGetDefaultQueue(Device));
	LARGE_INTEGER DueTime;

	KdPrint(("--> EchoEvtDeviceSelfManagedIoInit\n"));

	// ����Ĭ�϶���
	WdfIoQueueStart(WdfDeviceGetDefaultQueue(Device));

	// ������ʱ������һ�ε�����100ms�Ժ�
	DueTime.QuadPart = WDF_REL_TIMEOUT_IN_MS(100);
	WdfTimerStart(queueContext->Timer, DueTime.QuadPart);

	KdPrint(("<-- EchoEvtDeviceSelfManagedIoInit\n"));

	return STATUS_SUCCESS;

}


// This event is called by the Framework when the device is stopped for resource rebalance or suspended when the system is entering Sx state.
// �豸�뿪D0״̬ʱ���ã�ֹͣQueue��ֹͣTimer
NTSTATUS
EchoEvtDeviceSelfManagedIoSuspend(
	IN  WDFDEVICE Device
)
{
	PQUEUE_CONTEXT queueContext = QueueGetContext(WdfDeviceGetDefaultQueue(Device));

	PAGED_CODE();

	KdPrint(("--> EchoEvtDeviceSelfManagedIoSuspend\n"));

	// �豸����ǰ������������δ��ɵ�I/O�������ִ���ʽ��
	// 1) �ȴ�����δ���������ɺ��ٹ���
	// 2) ����ע��EvtIoStop�ص�������ȷ��֪ͨ��ܿ��Թ������δ���I/O���豸����
	// ����ʹ�õ�һ�ַ���������WdfIoQueueStopSynchronously����ͬ����ʽֹͣ����
	// ����WdfIoQueueStopSynchronously������ַ�ֹͣ�����Խ��գ�ֱ������������ɻ�ȡ���󣬲ŷ���
	WdfIoQueueStopSynchronously(WdfDeviceGetDefaultQueue(Device));

	// ֹͣ��ʱ�����ȴ���ʱ���ص�����ִ�����ŷ���
	WdfTimerStop(queueContext->Timer, TRUE);

	KdPrint(("<-- EchoEvtDeviceSelfManagedIoSuspend\n"));

	return STATUS_SUCCESS;
}
