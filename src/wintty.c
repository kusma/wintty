#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
