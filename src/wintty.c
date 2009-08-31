#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void die(char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}

#define LI 128
#define MI 192
#define HI 255
static COLORREF palette[16] = {
	/* intensity off */
	RGB( 0,  0,  0),
	RGB( 0,  0, LI),
	RGB( 0, LI,  0),
	RGB( 0, LI, LI),
	RGB(LI,  0,  0),
	RGB(LI,  0, LI),
	RGB(LI, LI,  0),
	RGB(MI, MI, MI),

	/* intensity on */
	RGB(LI, LI, LI),
	RGB( 0,  0, HI),
	RGB( 0, HI,  0),
	RGB( 0, HI, HI),
	RGB(HI,  0,  0),
	RGB(HI,  0, HI),
	RGB(HI, HI,  0),
	RGB(HI, HI, HI)
};

static CHAR_INFO buffer[80 * 25];
static HWND main_wnd;

static void update_console(HANDLE hstdout)
{
	COORD pos = {0, 0}, size = {80, 25};
	CONSOLE_SCREEN_BUFFER_INFO ci;

	GetConsoleScreenBufferInfo(hstdout, &ci);
	ReadConsoleOutput(hstdout, buffer, size, pos, &ci.srWindow);
	InvalidateRect(main_wnd, NULL, FALSE);
}

static DWORD WINAPI monitor(LPVOID param)
{
	HANDLE hstdout = hstdout = CreateFile("CONOUT$", GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);

	while (1) {
		DWORD res = WaitForSingleObject(hstdout, INFINITE);
		if (WAIT_OBJECT_0 == res) {
			Sleep(10);
			update_console(hstdout);
			ResetEvent(hstdout);
		}
	}
	return EXIT_SUCCESS;
}

static int is_process_alive(HANDLE proc)
{
	DWORD ret;
	return GetExitCodeProcess(proc, &ret) && STILL_ACTIVE == ret;
}

static int run_process(char *argv[], int argc)
{
	static STARTUPINFO si;
	PROCESS_INFORMATION pi;
	MSG msg;

	si.cb = sizeof(si);
	si.lpTitle = "ttywin32";

	if (!CreateProcess(NULL, argv[0], NULL, NULL, FALSE,
		CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		die("CreateProcess failed!\n");

	CreateThread(NULL, 0, monitor, NULL, 0, NULL);
	Sleep(100); /* HACK: wait for monitor thread to be operational */

	ResumeThread(pi.hThread);
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (WM_QUIT == msg.message || !is_process_alive(pi.hProcess))
			break;
	}
	return EXIT_SUCCESS;
}

static LRESULT CALLBACK main_wnd_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_PAINT) {
		int y;
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(wnd, &ps);

		SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

		/* ps.rcPaint */
		SetBkMode(hdc, OPAQUE);
		for (y = 0; y < 25; ++y) {
			int x;
			CHAR_INFO *src = &buffer[y * 80];
			for (x = 0; x < 80; ++x) {
				SetTextColor(hdc, palette[src[x].Attributes & 15]);
				SetBkColor(hdc, palette[(src[x].Attributes >> 4) & 15]);
				TextOut(hdc, x * 8, y * 15, &src[x].Char.AsciiChar, 1);
			}
		}
		EndPaint(wnd, &ps);
		return 0;
	} else
		return DefWindowProc(wnd, msg, wparam, lparam);
}

int main(int argc, char *argv[])
{
	WNDCLASSEX wc;
	HINSTANCE inst;
	int ret;

	if (argc < 2)
		die("usage: wintty.exe shell.exe\n");

	inst = GetModuleHandle(NULL);

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = main_wnd_proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = inst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
	wc.hbrBackground = (HBRUSH)0;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "MainWindow";
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
		die("Failed to register window class");

	main_wnd = CreateWindowEx(0, "MainWindow", "WinTTY",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, inst, NULL);

	ShowWindow(main_wnd, TRUE);
	UpdateWindow(main_wnd);

	ret = run_process(argv + 1, argc - 1);

	DestroyWindow(main_wnd);

	return ret;
}
