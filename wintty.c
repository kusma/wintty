#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void inject(PROCESS_INFORMATION *pi)
{
	void *mem;
	HANDLE rt;
	LPTHREAD_START_ROUTINE proc;

	char param[MAX_PATH], *c;
	GetModuleFileName(NULL, param, MAX_PATH);
	c = strrchr(param, '\\');
	c[1] = '\0';
	strncat(param, "wintty-hook.dll", MAX_PATH);

	mem = VirtualAllocEx(pi->hProcess, NULL,
		strlen(param) + 1, MEM_COMMIT, PAGE_READWRITE);
	if (!mem) {
		fprintf(stderr, "Failed to allocate memory in remote process\n");
		exit(EXIT_FAILURE);
	}

	if (!WriteProcessMemory(pi->hProcess, mem, param,
		strlen(param) + 1, NULL)) {
		fprintf(stderr, "Failed to write to memory in remote process\n");
		exit(EXIT_FAILURE);
	}

	proc = (LPTHREAD_START_ROUTINE)GetProcAddress(
		GetModuleHandle("KERNEL32"), "LoadLibraryA");
	if (!proc) {
		fprintf(stderr, "Failed get LoadLibrary\n");
		exit(EXIT_FAILURE);
	}

	rt = CreateRemoteThread(pi->hProcess, NULL, 0, proc, mem, 0, NULL);
	if (!rt) {
		fprintf(stderr, "Failed to create thread in remote process\n");
		exit(EXIT_FAILURE);
	}

	if (WaitForSingleObject(rt, 1000) == WAIT_TIMEOUT) {
		fprintf(stderr, "Injection timed out\n");
		exit(EXIT_FAILURE);
	}
}

int run_process(char *argv[], int argc)
{
	static STARTUPINFO si;
	PROCESS_INFORMATION pi;

	si.cb = sizeof(si);
	si.lpTitle = "ttywin32";

	if (!CreateProcess(NULL, argv[0], NULL, NULL, FALSE,
		CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
		fprintf(stderr, "CreateProcess failed!\n");
		exit(EXIT_FAILURE);
	}

	inject(&pi);

	ResumeThread(pi.hThread);
	WaitForSingleObject(pi.hThread, INFINITE);
	CloseHandle(pi.hThread);

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: wintty.exe shell.exe\n");
		return EXIT_FAILURE;
	}

	return run_process(argv + 1, argc - 1);
}
