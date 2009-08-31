#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

FILE *fp = NULL;
void update_console(HANDLE hstdout)
{
	int y;
	CHAR_INFO buffer[80 * 25];
	COORD pos = {0, 0}, size = {80, 25};
	SMALL_RECT rect = {0, 0, 80, 25};

	if (!fp) {
		fp = fopen("output.txt", "w");
		if (!fp) {
			fprintf(stderr, "failed to open output.txt\n");
			exit(EXIT_FAILURE);
		}
	}

	MessageBeep(-1);
	ReadConsoleOutput(hstdout, buffer, size, pos, &rect);
	for (y = rect.Top; y < rect.Bottom; ++y) {
		int x;
		CHAR_INFO *src = &buffer[y * 80];
		for (x = rect.Left; x < rect.Right; ++x)
			fputc(src[x].Char.AsciiChar, fp);
		fputc('\n', fp);
	}
	fflush(fp);
}

DWORD WINAPI monitor(LPVOID param)
{
	HANDLE hstdout = CreateFile("CONOUT$", GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);

	while (1) {
		DWORD	res = WaitForSingleObject(hstdout, INFINITE);
		if (WAIT_OBJECT_0 == res) {
			Sleep(0);
			update_console(hstdout);
			ResetEvent(hstdout);
		}
	}
	return EXIT_SUCCESS;
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

	CreateThread(NULL, 0, monitor, NULL, 0, NULL);
	Sleep(100); /* HACK: wait for monitor thread to be operational */

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
