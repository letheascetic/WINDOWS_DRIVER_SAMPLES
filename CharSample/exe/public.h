
#ifndef _USER_H
#define _USER_H

#include <initguid.h>

DEFINE_GUID(CharSample_DEVINTERFACE_GUID, \
			0xbd083159, 0xeb56, 0x437e, 0xbb, 0x98, 0x17, 0x65, 0xe4, 0x40, 0x81, 0xe);

#define CharSample_IOCTL_800 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif

