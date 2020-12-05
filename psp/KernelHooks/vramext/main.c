/*
Copyright (C) 2010 Crow_bar and MDave.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysevent.h>

PSP_MODULE_INFO("VramExt", 0x1006, 1, 0);

void sceGeEdramSetSize(int);

void VramSetSize(int kb)
{
	int k1 = pspSdkSetK1(0);
    sceGeEdramSetSize(kb*1024);
	pspSdkSetK1(k1);
	printf("Vram Size: %i\n", sceGeEdramGetSize());
}

int module_start(SceSize args, void *argp)
{
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}

