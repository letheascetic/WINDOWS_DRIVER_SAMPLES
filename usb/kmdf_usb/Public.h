/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/


#ifndef _PUBLIC_H_
#define _PUBLIC_H_

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_KMDFUSB,
    0x2f021a6e,0xe822,0x45dd,0xb7,0x78,0x1f,0x01,0xa6,0x8b,0x6f,0xff);
// {2f021a6e-e822-45dd-b778-1f01a68b6fff}

#pragma warning(push)
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

//
// BAR_GRAPH_STATE
//
// BAR_GRAPH_STATE is a bit field structure with each
//  bit corresponding to one of the bar graph on the 
//  OSRFX2 Development Board
//
#include <pshpack1.h>
typedef struct _BAR_GRAPH_STATE {

	union {

		struct {
			//
			// Individual bars starting from the 
			//  top of the stack of bars 
			//
			// NOTE: There are actually 10 bars, 
			//  but the very top two do not light
			//  and are not counted here
			//
			UCHAR Bar1 : 1;
			UCHAR Bar2 : 1;
			UCHAR Bar3 : 1;
			UCHAR Bar4 : 1;
			UCHAR Bar5 : 1;
			UCHAR Bar6 : 1;
			UCHAR Bar7 : 1;
			UCHAR Bar8 : 1;
		};

		//
		// The state of all the bar graph as a single
		// UCHAR
		//
		UCHAR BarsAsUChar;

	};

}BAR_GRAPH_STATE, *PBAR_GRAPH_STATE;

//
// SWITCH_STATE
//
// SWITCH_STATE is a bit field structure with each
//  bit corresponding to one of the switches on the 
//  OSRFX2 Development Board
//
typedef struct _SWITCH_STATE {

	union {
		struct {
			//
			// Individual switches starting from the 
			//  left of the set of switches
			//
			UCHAR Switch1 : 1;
			UCHAR Switch2 : 1;
			UCHAR Switch3 : 1;
			UCHAR Switch4 : 1;
			UCHAR Switch5 : 1;
			UCHAR Switch6 : 1;
			UCHAR Switch7 : 1;
			UCHAR Switch8 : 1;
		};

		//
		// The state of all the switches as a single
		// UCHAR
		//
		UCHAR SwitchesAsUChar;

	};


}SWITCH_STATE, *PSWITCH_STATE;

#include <poppack.h>

#pragma warning(pop)

#define IOCTL_INDEX           0x800
#define FILE_DEVICE_KMDFUSB   65500U

#define IOCTL_KMDFUSB_GET_CONFIG_DESCRIPTOR CTL_CODE(FILE_DEVICE_KMDFUSB,     \
                                                     IOCTL_INDEX,     \
                                                     METHOD_BUFFERED,         \
                                                     FILE_READ_ACCESS)

#define IOCTL_KMDFUSB_RESET_DEVICE  CTL_CODE(FILE_DEVICE_KMDFUSB,     \
                                                     IOCTL_INDEX + 1, \
                                                     METHOD_BUFFERED,         \
                                                     FILE_WRITE_ACCESS)

#define IOCTL_KMDFUSB_REENUMERATE_DEVICE  CTL_CODE(FILE_DEVICE_KMDFUSB, \
                                                    IOCTL_INDEX  + 3,  \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_ACCESS)

#define IOCTL_KMDFUSB_GET_BAR_GRAPH_DISPLAY CTL_CODE(FILE_DEVICE_KMDFUSB,\
                                                    IOCTL_INDEX  + 4, \
                                                    METHOD_BUFFERED, \
                                                    FILE_READ_ACCESS)


#define IOCTL_KMDFUSB_SET_BAR_GRAPH_DISPLAY CTL_CODE(FILE_DEVICE_KMDFUSB,\
                                                    IOCTL_INDEX + 5, \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_ACCESS)


#define IOCTL_KMDFUSB_READ_SWITCHES   CTL_CODE(FILE_DEVICE_KMDFUSB, \
                                                    IOCTL_INDEX + 6, \
                                                    METHOD_BUFFERED, \
                                                    FILE_READ_ACCESS)


#define IOCTL_KMDFUSB_GET_7_SEGMENT_DISPLAY CTL_CODE(FILE_DEVICE_KMDFUSB, \
                                                    IOCTL_INDEX + 7, \
                                                    METHOD_BUFFERED, \
                                                    FILE_READ_ACCESS)


#define IOCTL_KMDFUSB_SET_7_SEGMENT_DISPLAY CTL_CODE(FILE_DEVICE_KMDFUSB, \
                                                    IOCTL_INDEX + 8, \
                                                    METHOD_BUFFERED, \
                                                    FILE_WRITE_ACCESS)

#define IOCTL_KMDFUSB_GET_INTERRUPT_MESSAGE CTL_CODE(FILE_DEVICE_KMDFUSB,\
                                                    IOCTL_INDEX + 9, \
                                                    METHOD_OUT_DIRECT, \
                                                    FILE_READ_ACCESS)

#endif