	.set noreorder

#include "pspstub.s"

	STUB_START "VramExt_driver",0x00090000,0x00010005
	STUB_FUNC  0xA5853E1E,VramSetSize
	STUB_END
