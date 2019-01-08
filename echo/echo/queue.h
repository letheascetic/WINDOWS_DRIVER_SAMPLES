#pragma once

// Set max write length for testing
#define MAX_WRITE_LENGTH 1024*40

// Set timer period in ms
#define TIMER_PERIOD 1000*2

// 定义默认队列对象的环境变量
typedef struct _QUEUE_CONTEXT {

	PVOID Buffer;
	ULONG Length;
	WDFTIMER Timer;
	WDFREQUEST CurrentRequest;
	NTSTATUS CurrentStatus;

} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)

NTSTATUS
EchoQueueInitialize(
	WDFDEVICE hDevice
);

EVT_WDF_IO_QUEUE_CONTEXT_DESTROY_CALLBACK EchoEvtIoQueueContextDestroy;
EVT_WDF_IO_QUEUE_IO_READ EchoEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE EchoEvtIoWrite;
EVT_WDF_REQUEST_CANCEL EchoEvtRequestCancel;

NTSTATUS
EchoTimerCreate(
	IN WDFTIMER*       pTimer,
	IN ULONG           Period,
	IN WDFQUEUE        Queue
);

EVT_WDF_TIMER EchoEvtTimerFunc;