#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, EchoQueueInitialize)
#pragma alloc_text (PAGE, EchoTimerCreate)
#endif

// 创建和初始化队列
// 1 初始化默认队列（WDF_IO_QUEUE_CONFIG），设置串行处理，IO请求的回调函数
// 2 初始化队列的属性和环境变量（同步类型、销毁回调函数、环境变量初始化）
// 3 创建队列
// 4 创建和初始化定时器
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

	// 1 初始化默认队列（WDF_IO_QUEUE_CONFIG），设置串行处理，IO请求的回调函数

	// 初始化默认队列，串行处理（一次只能处理一个I/O请求）
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&queueConfig,
		WdfIoQueueDispatchSequential
	);

	// 设置队列的读和写请求的回调函数
	queueConfig.EvtIoRead = EchoEvtIoRead;
	queueConfig.EvtIoWrite = EchoEvtIoWrite;

	// 2 初始化队列的属性和环境变量
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, QUEUE_CONTEXT);

	// 设置同步范围为WdfSynchronizationScopeQueue，并将队列设为Timer的父对象（在EchoTimerCreate中设置）
	// 在此情况下，队列和Timer的回调函数都会由同一个锁同步
	queueAttributes.SynchronizationScope = WdfSynchronizationScopeQueue;

	// 设置队列销毁时的回调函数
	queueAttributes.EvtDestroyCallback = EchoEvtIoQueueContextDestroy;

	// 3 创建队列
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

	// 2.x 队列的环境变量初始化
	queueContext = QueueGetContext(queue);

	queueContext->Buffer = NULL;
	queueContext->Timer = NULL;

	queueContext->CurrentRequest = NULL;
	queueContext->CurrentStatus = STATUS_INVALID_DEVICE_REQUEST;

	// 4 创建和初始化定时器
	status = EchoTimerCreate(&queueContext->Timer, TIMER_PERIOD, queue);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Error creating timer 0x%x\n", status));
		return status;
	}

	return status;
}


// 创建和初始化定时器
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

	// 创建周期定时器，定时周期2000ms，回调函数EchoEvtTimerFunc
	// WDF_TIMER_CONFIG_INIT_PERIODIC sets AutomaticSerialization to TRUE by default.
	WDF_TIMER_CONFIG_INIT_PERIODIC(&timerConfig, EchoEvtTimerFunc, Period);

	// 设置定时器的父对象为默认队列
	WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
	timerAttributes.ParentObject = Queue;			// Synchronize with the I/O Queue

	// 创建定时器对象
	Status = WdfTimerCreate(&timerConfig,
		&timerAttributes,
		Timer
	);

	return Status;

}


// 队列销毁时的回调函数
// 用于清除队列的环境变量中申请的空间资源
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


// I/O请求被取消时的回调函数
// I/O请求首先要被设置为可取消（调用WdfRequestMarkCancelable），然后确实被取消了，才会调用该回调函数
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


// IoRead的回调函数
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

	// 没有可读取的数据
	if ((queueContext->Buffer == NULL)) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, (ULONG_PTR)0L);
		return;
	}

	_Analysis_assume_(queueContext->Length > 0);
	if (queueContext->Length < Length) {
		Length = queueContext->Length;
	}

	// 获取request的存储地址
	Status = WdfRequestRetrieveOutputMemory(Request, &memory);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("EchoEvtIoRead Could not get request memory buffer 0x%x\n", Status));
		//WdfVerifierDbgBreakPoint();
		WdfRequestCompleteWithInformation(Request, Status, 0L);
		return;
	}

	// 从queueContext->Buffer读取数据到request的存储地址中
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

	// 设置该request可以被取消，取消的回调函数为EchoEvtRequestCancel
	WdfRequestMarkCancelable(Request, EchoEvtRequestCancel);

	// 存储当前request到队列的环境变量中
	queueContext->CurrentRequest = Request;
	queueContext->CurrentStatus = Status;

	return;
}


// IoWrite的回调函数
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

	// 获取request的存储地址
	Status = WdfRequestRetrieveInputMemory(Request, &memory);
	if (!NT_SUCCESS(Status)) {
		KdPrint(("EchoEvtIoWrite Could not get request memory buffer 0x%x\n",
			Status));
		WdfVerifierDbgBreakPoint();
		WdfRequestComplete(Request, Status);
		return;
	}

	// 如果queueContext->Buffer中已有数据，则先清除
	if (queueContext->Buffer != NULL) {
		ExFreePool(queueContext->Buffer);
		queueContext->Buffer = NULL;
		queueContext->Length = 0L;
	}

	// 重新开辟空间
	queueContext->Buffer = ExAllocatePoolWithTag(NonPagedPoolNx, Length, 'sam1');
	if (queueContext->Buffer == NULL) {
		KdPrint(("EchoEvtIoWrite: Could not allocate %Iu byte buffer\n", Length));
		WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
		return;
	}

	// 从request的存储地址读取数据到queueContext->Buffer
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

	// 当前request设置取消回调函数
	WdfRequestMarkCancelable(Request, EchoEvtRequestCancel);

	// 存储当前request到队列的环境变量中
	queueContext->CurrentRequest = Request;
	queueContext->CurrentStatus = Status;

	return;
}


// 定时器的回调函数
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

	// 获取当前要处理的request
	Request = queueContext->CurrentRequest;
	if (Request != NULL) {

		// 取消当前request的取消回调函数
		// 操作成功，则返回STATUS_SUCCESS
		// 如果该request已经被取消，则返回STATUS_CANCELLED
		Status = WdfRequestUnmarkCancelable(Request);
		if (Status != STATUS_CANCELLED) {

			queueContext->CurrentRequest = NULL;
			Status = queueContext->CurrentStatus;

			KdPrint(("CustomTimerDPC Completing request 0x%p, Status 0x%x \n", Request, Status));

			WdfRequestComplete(Request, Status);
		}
		else {
			// Status == STATUS_CANCELLED时，不需要调用WdfRequestComplete？？？
			KdPrint(("CustomTimerDPC Request 0x%p is STATUS_CANCELLED, not completing\n", Request));
		}
	}

	return;
}