#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, EchoQueueInitialize)
#pragma alloc_text (PAGE, EchoTimerCreate)
#endif

// �����ͳ�ʼ������
// 1 ��ʼ��Ĭ�϶��У�WDF_IO_QUEUE_CONFIG�������ô��д���IO����Ļص�����
// 2 ��ʼ�����е����Ժͻ���������ͬ�����͡����ٻص�����������������ʼ����
// 3 ��������
// 4 �����ͳ�ʼ����ʱ��
NTSTATUS
EchoQueueInitialize(
	WDFDEVICE Device
)
{
	WDFQUEUE queue;
	NTSTATUS status;
	PQUEUE_CONTEXT queueContext;
	WDF_IO_QUEUE_CONFIG    queueConfig;
	WDF_OBJECT_ATTRIBUTES  queueAttributes;

	PAGED_CODE();

	// 1 ��ʼ��Ĭ�϶��У�WDF_IO_QUEUE_CONFIG�������ô��д���IO����Ļص�����

	// ��ʼ��Ĭ�϶��У����д���һ��ֻ�ܴ���һ��I/O����
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&queueConfig,
		WdfIoQueueDispatchSequential
	);

	// ���ö��еĶ���д����Ļص�����
	queueConfig.EvtIoRead = EchoEvtIoRead;
	queueConfig.EvtIoWrite = EchoEvtIoWrite;

	// 2 ��ʼ�����е����Ժͻ�������
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, QUEUE_CONTEXT);

	// ����ͬ����ΧΪWdfSynchronizationScopeQueue������������ΪTimer�ĸ�������EchoTimerCreate�����ã�
	// �ڴ�����£����к�Timer�Ļص�����������ͬһ����ͬ��
	queueAttributes.SynchronizationScope = WdfSynchronizationScopeQueue;

	// ���ö�������ʱ�Ļص�����
	queueAttributes.EvtDestroyCallback = EchoEvtIoQueueContextDestroy;

	// 3 ��������
	status = WdfIoQueueCreate(
		Device,
		&queueConfig,
		&queueAttributes,
		&queue
	);

	if (!NT_SUCCESS(status)) {
		KdPrint(("WdfIoQueueCreate failed 0x%x\n", status));
		return status;
	}

	// 2.x ���еĻ���������ʼ��
	queueContext = QueueGetContext(queue);

	queueContext->Buffer = NULL;
	queueContext->Timer = NULL;

	queueContext->CurrentRequest = NULL;
	queueContext->CurrentStatus = STATUS_INVALID_DEVICE_REQUEST;

	// 4 �����ͳ�ʼ����ʱ��
	status = EchoTimerCreate(&queueContext->Timer, TIMER_PERIOD, queue);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Error creating timer 0x%x\n", status));
		return status;
	}

	return status;
}


// �����ͳ�ʼ����ʱ��
NTSTATUS
EchoTimerCreate(
	IN WDFTIMER*       Timer,
	IN ULONG           Period,
	IN WDFQUEUE        Queue
)
{
	NTSTATUS Status;
	WDF_TIMER_CONFIG       timerConfig;
	WDF_OBJECT_ATTRIBUTES  timerAttributes;

	PAGED_CODE();

	// �������ڶ�ʱ������ʱ����2000ms���ص�����EchoEvtTimerFunc
	// WDF_TIMER_CONFIG_INIT_PERIODIC sets AutomaticSerialization to TRUE by default.
	WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, EchoEvtTimerFunc, Period);

	// ���ö�ʱ���ĸ�����ΪĬ�϶���
	WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
	timerAttributes.ParentObject = Queue;			// Synchronize with the I/O Queue

	// ������ʱ������
	Status = WdfTimerCreate(&timerConfig,
		&timerAttributes,
		Timer
	);

	return Status;

}


// ��������ʱ�Ļص�����
// ����������еĻ�������������Ŀռ���Դ
VOID
EchoEvtIoQueueContextDestroy(
	WDFOBJECT Object
)
{
	PQUEUE_CONTEXT queueContext = QueueGetContext(Object);
	
	if (queueContext->Buffer != NULL) {
		ExFreePool(queueContext->Buffer);
	}

	return;
}


// I/O����ȡ��ʱ�Ļص�����
// I/O��������Ҫ������Ϊ��ȡ��������WdfRequestMarkCancelable����Ȼ��ȷʵ��ȡ���ˣ��Ż���øûص�����
VOID
EchoEvtRequestCancel(
	IN WDFREQUEST Request
)
{
	PQUEUE_CONTEXT queueContext = QueueGetContext(WdfRequestGetIoQueue(Request));

	KdPrint(("EchoEvtRequestCancel called on Request 0x%p\n", Request));

	WdfRequestCompleteWithInformation(Request, STATUS_CANCELLED, 0L);

	ASSERT(queueContext->CurrentRequest == Request);
	queueContext->CurrentRequest = NULL;

	return;
}


// IoRead�Ļص�����
VOID
EchoEvtIoRead(
	IN WDFQUEUE   Queue,
	IN WDFREQUEST Request,
	IN size_t     Length
)
/*++

Routine Description:

	This event is called when the framework receives IRP_MJ_READ request.
	It will copy the content from the queue-context buffer to the request buffer.
	If the driver hasn't received any write request earlier, the read returns zero.

Arguments:

	Queue -  Handle to the framework queue object that is associated with the
			 I/O request.

	Request - Handle to a framework request object.

	Length  - number of bytes to be read.
			  The default property of the queue is to not dispatch
			  zero lenght read & write requests to the driver and
			  complete is with status success. So we will never get
			  a zero length request.

Return Value:

	VOID

--*/
{
	NTSTATUS Status;
	PQUEUE_CONTEXT queueContext = QueueGetContext(Queue);
	WDFMEMORY memory;

	_Analysis_assume_(Length > 0);

	KdPrint(("EchoEvtIoRead Called! Queue 0x%p, Request 0x%p Length %Iu\n", Queue, Request, Length));

	// û�пɶ�ȡ������
	if ((queueContext->Buffer == NULL)) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, (ULONG_PTR)0L);
		return;
	}

	_Analysis_assume_(queueContext->Length > 0);
	if (queueContext->Length < Length) {
		Length = queueContext->Length;
	}

	// ��ȡrequest�Ĵ洢��ַ
	Status = WdfRequestRetrieveOutputMemory(Request, &memory);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("EchoEvtIoRead Could not get request memory buffer 0x%x\n", Status));
		//WdfVerifierDbgBreakPoint();
		WdfRequestCompleteWithInformation(Request, Status, 0L);
		return;
	}

	// ��queueContext->Buffer��ȡ���ݵ�request�Ĵ洢��ַ��
	Status = WdfMemoryCopyFromBuffer(
		memory,				// destination
		0,					// offset into the destination memory
		queueContext->Buffer,
		Length);

	if (!NT_SUCCESS(Status)) {
		KdPrint(("EchoEvtIoRead: WdfMemoryCopyFromBuffer failed 0x%x\n", Status));
		WdfRequestComplete(Request, Status);
		return;
	}

	WdfRequestSetInformation(Request, (ULONG_PTR)Length);

	// ���ø�request���Ա�ȡ����ȡ���Ļص�����ΪEchoEvtRequestCancel
	WdfRequestMarkCancelable(Request, EchoEvtRequestCancel);

	// �洢��ǰrequest�����еĻ���������
	queueContext->CurrentRequest = Request;
	queueContext->CurrentStatus = Status;

	return;
}


// IoWrite�Ļص�����
VOID
EchoEvtIoWrite(
	IN WDFQUEUE   Queue,
	IN WDFREQUEST Request,
	IN size_t     Length
)
/*++

Routine Description:

	This event is invoked when the framework receives IRP_MJ_WRITE request.
	This routine allocates memory buffer, copies the data from the request to it,
	and stores the buffer pointer in the queue-context with the length variable
	representing the buffers length. The actual completion of the request
	is defered to the periodic timer dpc.

Arguments:

	Queue -  Handle to the framework queue object that is associated with the
			 I/O request.

	Request - Handle to a framework request object.

	Length  - number of bytes to be read.
			  The default property of the queue is to not dispatch
			  zero lenght read & write requests to the driver and
			  complete is with status success. So we will never get
			  a zero length request.

Return Value:

	VOID

--*/
{
	NTSTATUS Status;
	WDFMEMORY memory;
	PQUEUE_CONTEXT queueContext = QueueGetContext(Queue);

	_Analysis_assume_(Length > 0);

	KdPrint(("EchoEvtIoWrite Called! Queue 0x%p, Request 0x%p Length %Iu\n", Queue, Request, Length));

	if (Length > MAX_WRITE_LENGTH) {
		KdPrint(("EchoEvtIoWrite Buffer Length to big %Iu, Max is %d\n", Length, MAX_WRITE_LENGTH));
		WdfRequestCompleteWithInformation(Request, STATUS_BUFFER_OVERFLOW, 0L);
		return;
	}

	// ��ȡrequest�Ĵ洢��ַ
	Status = WdfRequestRetrieveInputMemory(Request, &memory);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("EchoEvtIoWrite Could not get request memory buffer 0x%x\n",
			Status));
		WdfVerifierDbgBreakPoint();
		WdfRequestComplete(Request, Status);
		return;
	}

	// ���queueContext->Buffer���������ݣ��������
	if (queueContext->Buffer != NULL) {
		ExFreePool(queueContext->Buffer);
		queueContext->Buffer = NULL;
		queueContext->Length = 0L;
	}

	// ���¿��ٿռ�
	queueContext->Buffer = ExAllocatePoolWithTag(NonPagedPoolNx, Length, 'sam1');
	if (queueContext->Buffer == NULL) {
		KdPrint(("EchoEvtIoWrite: Could not allocate %Iu byte buffer\n", Length));
		WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
		return;
	}

	// ��request�Ĵ洢��ַ��ȡ���ݵ�queueContext->Buffer
	Status = WdfMemoryCopyToBuffer(memory,
		0,						// offset into the source memory
		queueContext->Buffer,
		Length);
	
	if (!NT_SUCCESS(Status)) {
		KdPrint(("EchoEvtIoWrite WdfMemoryCopyToBuffer failed 0x%x\n", Status));
		//WdfVerifierDbgBreakPoint();

		ExFreePool(queueContext->Buffer);
		queueContext->Buffer = NULL;
		queueContext->Length = 0L;
		WdfRequestComplete(Request, Status);
		return;
	}


	queueContext->Length = (ULONG)Length;

	WdfRequestSetInformation(Request, (ULONG_PTR)Length);

	// ��ǰrequest����ȡ���ص�����
	WdfRequestMarkCancelable(Request, EchoEvtRequestCancel);

	// �洢��ǰrequest�����еĻ���������
	queueContext->CurrentRequest = Request;
	queueContext->CurrentStatus = Status;

	return;
}


// ��ʱ���Ļص�����
VOID
EchoEvtTimerFunc(
	IN WDFTIMER     Timer
)
/*++

Routine Description:

	This is the TimerDPC the driver sets up to complete requests.
	This function is registered when the WDFTIMER object is created, and
	will automatically synchronize with the I/O Queue callbacks
	and cancel routine.

Arguments:

	Timer - Handle to a framework Timer object.

Return Value:

	VOID

--*/
{
	NTSTATUS      Status;
	WDFREQUEST     Request;
	WDFQUEUE queue;
	PQUEUE_CONTEXT queueContext;

	queue = WdfTimerGetParentObject(Timer);
	queueContext = QueueGetContext(queue);

	// ��ȡ��ǰҪ�����request
	Request = queueContext->CurrentRequest;
	if (Request != NULL) {

		// ȡ����ǰrequest��ȡ���ص�����
		// �����ɹ����򷵻�STATUS_SUCCESS
		// �����request�Ѿ���ȡ�����򷵻�STATUS_CANCELLED
		Status = WdfRequestUnmarkCancelable(Request);
		if (Status != STATUS_CANCELLED) {

			queueContext->CurrentRequest = NULL;
			Status = queueContext->CurrentStatus;

			KdPrint(("CustomTimerDPC Completing request 0x%p, Status 0x%x \n", Request, Status));

			WdfRequestComplete(Request, Status);
		}
		else {
			// Status == STATUS_CANCELLEDʱ������Ҫ����WdfRequestComplete������
			KdPrint(("CustomTimerDPC Request 0x%p is STATUS_CANCELLED, not completing\n", Request));
		}
	}

	return;
}