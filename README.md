# WINDOWS_DRIVER_SAMPLES

windows driver samples with kmdf.



QueueSample

	将输入的数字（0~9）转为中文（零~九）输出
	非默认队列和定时器的使用示例

		* 创建非默认手工队列和一个一次性定时器
		* 收到IOControl请求后，不对请求处理，而将其压入非默认手工队列，并启动3秒的定时器
		* 在定时器回调例程中取出IO请求，将数字转换为对应的中文

	sys/: 驱动程序

	exe/: 应用测试程序