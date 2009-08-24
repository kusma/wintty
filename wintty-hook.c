#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

BOOL APIENTRY DllMain(HANDLE hinst, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
		printf("Hello from hook-dll!\n");
}
