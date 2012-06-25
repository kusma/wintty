#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef _MSC_VER
#define strdup _strdup
#endif

void die(char *fmt, ...)
{
	char temp[4096];
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	vsnprintf(temp, sizeof(temp), fmt, va);
	va_end(va);

	MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR);
	exit(EXIT_FAILURE);
}

void warn(char *fmt, ...)
{
	char temp[4096];
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	vsnprintf(temp, sizeof(temp), fmt, va);
	va_end(va);

	MessageBox(NULL, temp, NULL, MB_OK | MB_ICONWARNING);
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

#define CONSOLE_WIDTH 80
#define CONSOLE_HEIGHT 50
static CHAR_INFO buffer[CONSOLE_WIDTH * CONSOLE_HEIGHT];

static HWND main_wnd;

static void update_console(HANDLE hstdout)
{
	COORD pos = {0, 0}, size = {CONSOLE_WIDTH, CONSOLE_HEIGHT};
	CONSOLE_SCREEN_BUFFER_INFO ci;

	GetConsoleScreenBufferInfo(hstdout, &ci);
	ReadConsoleOutput(hstdout, buffer, size, pos, &ci.srWindow);
	InvalidateRect(main_wnd, NULL, FALSE);
}

HANDLE hstdout, hstdin;
static DWORD WINAPI monitor(LPVOID param)
{
	hstdout = CreateFile("CONOUT$", GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	hstdin = CreateFile("CONIN$", GENERIC_WRITE | GENERIC_READ,
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

static HWND get_console_wnd()
{
	char old_title[1024];
	HWND ret;

	static HWND (WINAPI *GetConsoleWindow)(VOID) = NULL;
	if (NULL == GetConsoleWindow) GetConsoleWindow = (HWND (WINAPI *)(VOID))GetProcAddress(LoadLibrary("KERNEL32.dll"), "GetConsoleWindow");
	if (NULL != GetConsoleWindow) GetConsoleWindow();

	GetConsoleTitle(old_title, sizeof(old_title));
	SetConsoleTitle("wintty-hideme");
	ret = FindWindow(NULL, "wintty-hideme");
	SetConsoleTitle(old_title);
	return ret;
}

static int run_process(char *argv[], int argc)
{
	int i;
	static STARTUPINFO si;
	PROCESS_INFORMATION pi;
	MSG msg;
	char *cmd = strdup(argv[0]);
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	COORD size = {CONSOLE_WIDTH, CONSOLE_HEIGHT};

	/* concatenate argv */
	for (i = 1; i < argc; ++i) {
		cmd = realloc(cmd, strlen(cmd) + strlen(argv[i]) + 2);
		strcat(cmd, " ");
		strcat(cmd, argv[i]);
	}

	si.cb = sizeof(si);
	si.lpTitle = "ttywin32";

	if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE,
		CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		die("CreateProcess failed!\n");

	CreateThread(NULL, 0, monitor, NULL, 0, NULL);
	Sleep(100); /* HACK: wait for monitor thread to be operational */

	ResumeThread(pi.hThread);
	ShowWindow(get_console_wnd(), SW_HIDE);
	SetConsoleScreenBufferSize(hstdout, size);

	GetConsoleScreenBufferInfo(hstdout, &sbi);
	sbi.srWindow.Left = 0;
	sbi.srWindow.Right = CONSOLE_WIDTH - 1;
	sbi.srWindow.Top = sbi.srWindow.Bottom - CONSOLE_HEIGHT + 1;
	if (!SetConsoleWindowInfo(hstdout, TRUE, &sbi.srWindow))
		warn("Failed to set console window size");

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (WM_QUIT == msg.message || !is_process_alive(pi.hProcess))
			break;
	}

	if (is_process_alive(pi.hProcess))
		TerminateProcess(pi.hProcess, EXIT_SUCCESS);

	return EXIT_SUCCESS;
}

static LRESULT CALLBACK main_wnd_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	int y;
	PAINTSTRUCT ps;
	INPUT_RECORD ir = {0};
	HDC hdc;
	DWORD dummy;

	switch (msg) {
	case WM_PAINT:
		hdc = BeginPaint(wnd, &ps);

		SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

		/* ps.rcPaint */
		SetBkMode(hdc, OPAQUE);
		for (y = 0; y < CONSOLE_HEIGHT; ++y) {
			int x;
			CHAR_INFO *src = &buffer[y * CONSOLE_WIDTH];
			for (x = 0; x < CONSOLE_WIDTH; ++x) {
				SetTextColor(hdc, palette[src[x].Attributes & 15]);
				SetBkColor(hdc, palette[(src[x].Attributes >> 4) & 15]);
				TextOut(hdc, x * 8, y * 15, &src[x].Char.AsciiChar, 1);
			}
		}
		EndPaint(wnd, &ps);
		return 0;
		break;

	case WM_CHAR:
		ir.EventType = KEY_EVENT;
		ir.Event.KeyEvent.bKeyDown = !(lparam & (1UL<<31));
		ir.Event.KeyEvent.wRepeatCount = lparam & ((1<<16)-1);
		ir.Event.KeyEvent.wVirtualKeyCode = LOBYTE(VkKeyScan(wparam));
		ir.Event.KeyEvent.wVirtualScanCode = MapVirtualKey(wparam, 0);
		ir.Event.KeyEvent.uChar.AsciiChar = wparam;
		ir.Event.KeyEvent.dwControlKeyState = 0;

		WriteConsoleInput(hstdin, &ir, 1, &dummy);
		return 0;
		break;

	case WM_DESTROY:
		PostQuitMessage(EXIT_SUCCESS);
		return 0;
		break;

	default:
		return DefWindowProc(wnd, msg, wparam, lparam);
	}
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

	AllocConsole();
	ShowWindow(get_console_wnd(), SW_HIDE);

	ret = run_process(argv + 1, argc - 1);

	DestroyWindow(main_wnd);

	return ret;
}
