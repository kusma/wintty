#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

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

	MessageBoxA(NULL, temp, NULL, MB_OK | MB_ICONERROR);
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

	MessageBoxA(NULL, temp, NULL, MB_OK | MB_ICONWARNING);
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

static HANDLE hstdout, hstdin;

CRITICAL_SECTION console_cs;
static int console_width = 0;
static int console_height = 0;
static CHAR_INFO *buffer = NULL;
static void update_console(HANDLE hstdout);

void set_console_size(int new_width, int new_height)
{
	COORD size = {new_width, new_height};
	CONSOLE_SCREEN_BUFFER_INFO sbi;

	if (new_width == console_width && new_height == console_height)
		return;

	if (!new_width || !new_height)
		return;

	GetConsoleScreenBufferInfo(hstdout, &sbi);
	if (new_width > sbi.dwSize.X || new_height > sbi.dwSize.Y)
	{
		sbi.dwSize.X = new_width > sbi.dwSize.X ? new_width : sbi.dwSize.X;
		sbi.dwSize.Y = new_height > sbi.dwSize.Y ? new_height : sbi.dwSize.Y;

		if (!SetConsoleScreenBufferSize(hstdout, sbi.dwSize))
			die("Failed to set console buffer size <%d, %d> (%d)", size.X, size.Y, GetLastError());	
	}

	GetConsoleScreenBufferInfo(hstdout, &sbi);
	if (new_width > sbi.dwMaximumWindowSize.X ||
		new_height > sbi.dwMaximumWindowSize.Y) {
		warn("Window size <%dx%d> is too big window! Maximum size is <%dx%d>",
			new_width, new_height,
			sbi.dwMaximumWindowSize.X, sbi.dwMaximumWindowSize.Y);
		return;
	}

	sbi.srWindow.Right = sbi.srWindow.Left + new_width - 1;
	sbi.srWindow.Bottom = sbi.srWindow.Top + new_height - 1;
	if (!SetConsoleWindowInfo(hstdout, TRUE, &sbi.srWindow))
		die("Failed to set console window size");

	console_width = sbi.dwSize.X;
	console_height = sbi.dwSize.Y;
	if (new_width != 0 && new_height != 0) {
		buffer = realloc(buffer, sizeof(CHAR_INFO) * console_width * console_height);
		if (NULL == buffer)
			die("failed to allocate console buffer");
	}

	update_console(hstdout);
}

static HWND main_wnd, sb_wnd;

static void update_console(HANDLE hstdout)
{
	COORD pos = {0, 0}, size = {console_width, console_height};
	CONSOLE_SCREEN_BUFFER_INFO sbi;
	GetConsoleScreenBufferInfo(hstdout, &sbi);

	if (!ReadConsoleOutput(hstdout, buffer, size, pos, &sbi.srWindow)) {
		int y;
		size.Y = 1;
		for (y = 0; y < console_height; ++y) {
			CHAR_INFO *dst = &buffer[y * console_width];
			if (!ReadConsoleOutput(hstdout, dst, size, pos, &sbi.srWindow))
				die("failed to read console output (%d)", GetLastError());
			sbi.srWindow.Top++;
			sbi.srWindow.Bottom = sbi.srWindow.Top;
		}
	}
	InvalidateRect(main_wnd, NULL, FALSE);
}

static DWORD WINAPI monitor(LPVOID param)
{
	do {
		ResetEvent(hstdout);
		Sleep(10); /* don't spend ALL CPU power on redrawing */

		EnterCriticalSection(&console_cs);
		update_console(hstdout);
		LeaveCriticalSection(&console_cs);
	} while (WaitForSingleObject(hstdout, INFINITE) == WAIT_OBJECT_0);

	return EXIT_SUCCESS;
}

static int is_process_alive(HANDLE proc)
{
	DWORD ret;
	return GetExitCodeProcess(proc, &ret) && STILL_ACTIVE == ret;
}

static HWND get_console_wnd()
{
	WCHAR old_title[1024];
	HWND ret;

	static HWND (WINAPI *GetConsoleWindow)(VOID) = NULL;
	if (NULL == GetConsoleWindow) GetConsoleWindow = (HWND (WINAPI *)(VOID))GetProcAddress(LoadLibraryA("KERNEL32.dll"), "GetConsoleWindow");
	if (NULL != GetConsoleWindow) GetConsoleWindow();

	GetConsoleTitleW(old_title, sizeof(old_title));
	SetConsoleTitleA("wintty-hideme");
	ret = FindWindowA(NULL, "wintty-hideme");
	SetConsoleTitleW(old_title);
	return ret;
}

static PROCESS_INFORMATION pi;
static int run_process(char *argv[], int argc)
{
	int i;
	static STARTUPINFOA si;
	HANDLE hthread;
	MSG msg;
	char *cmd = strdup(argv[0]);

	/* concatenate argv */
	for (i = 1; i < argc; ++i) {
		cmd = realloc(cmd, strlen(cmd) + strlen(argv[i]) + 2);
		strcat(cmd, " ");
		strcat(cmd, argv[i]);
	}

	si.cb = sizeof(si);
	si.lpTitle = "ttywin32";

	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
		CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP, NULL, NULL,
		&si, &pi))
		die("CreateProcess failed!\n");

	hthread = CreateThread(NULL, 0, monitor, NULL, 0, NULL);
	if (!hthread)
		die("Failed to launch monitor thread");

	Sleep(100); /* HACK: wait for monitor thread to be operational */

	ResumeThread(pi.hThread);
/*	ShowWindow(get_console_wnd(), SW_HIDE); */

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (WM_QUIT == msg.message || !is_process_alive(pi.hProcess))
			break;
	}

	if (is_process_alive(pi.hProcess))
		TerminateProcess(pi.hProcess, EXIT_SUCCESS);

	TerminateThread(hthread, EXIT_SUCCESS);

	return EXIT_SUCCESS;
}

static LRESULT CALLBACK main_wnd_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	int y;
	PAINTSTRUCT ps;
	INPUT_RECORD ir = {0};
	int sb_widths[] = { 64 };
	HDC hdc;
	DWORD dummy;

	switch (msg) {
	case WM_CREATE:
		sb_wnd = CreateWindowEx(0, STATUSCLASSNAME, NULL,
					SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD,
					0, 0, 0, 0,
					wnd, NULL, GetModuleHandle(0), NULL);

		SendMessage(sb_wnd, SB_SETPARTS, sizeof(sb_widths) / sizeof(int), (LPARAM)sb_widths);
		SendMessageA(sb_wnd, SB_SETTEXTA, 0, (LPARAM)"-");
		SendMessageA(sb_wnd, SB_SETTEXTA, 1, (LPARAM)"-");
		break;

	case WM_SIZE:
		{
			char temp[16];
			RECT sb_rect;
			int width  = LOWORD(lparam), height = HIWORD(lparam), sb_height;

			GetClientRect(sb_wnd, &sb_rect);
			sb_height = sb_rect.bottom - sb_rect.top;

			EnterCriticalSection(&console_cs);
			set_console_size((width + 7) / 8, (height - sb_height + 14) / 15);
			LeaveCriticalSection(&console_cs);

			MoveWindow(sb_wnd, 0, height - sb_height, width, sb_height, TRUE);

			_snprintf(temp, 16, "%dx%d", (width + 7) / 8, (height - sb_height + 14) / 15);
			SendMessageA(sb_wnd, SB_SETTEXTA, 0, (LPARAM)temp);
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(wnd, &ps);

		SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

		/* ps.rcPaint */
		SetBkMode(hdc, OPAQUE);
		for (y = 0; y < console_height; ++y) {
			int x;
			CHAR_INFO *src = &buffer[y * console_width];
			for (x = 0; x < console_width; ++x) {
				if (!x || src[x].Attributes != src[x-1].Attributes) {
					SetTextColor(hdc, palette[src[x].Attributes & 15]);
					SetBkColor(hdc, palette[(src[x].Attributes >> 4) & 15]);
				}
				TextOutW(hdc, x * 8, y * 15, &src[x].Char.UnicodeChar, 1);
			}
		}
		EndPaint(wnd, &ps);
		return 0;
		break;

	case WM_KEYDOWN:
		if ('C' == wparam && GetKeyState(VK_CONTROL))
			GenerateConsoleCtrlEvent(CTRL_C_EVENT, (DWORD)pi.hProcess);
		switch (wparam) {
			case VK_UP:
			case VK_DOWN:
			case VK_LEFT:
			case VK_RIGHT:
				ir.EventType = KEY_EVENT;
				ir.Event.KeyEvent.bKeyDown = !(lparam & (1UL<<31));
				ir.Event.KeyEvent.wRepeatCount = lparam & ((1<<16)-1);
				ir.Event.KeyEvent.wVirtualKeyCode = wparam;
				ir.Event.KeyEvent.wVirtualScanCode = 0;
				ir.Event.KeyEvent.uChar.AsciiChar = 0;
				ir.Event.KeyEvent.dwControlKeyState = 0;

				WriteConsoleInput(hstdin, &ir, 1, NULL);
				break;
		}
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
	WNDCLASSEXW wc;
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
	wc.lpszClassName = L"MainWindow";
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassExW(&wc))
		die("Failed to register window class");

	if (!AllocConsole())
		die("Failed to allocate console");

	hstdout = CreateFileA("CONOUT$", GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	hstdin = CreateFileA("CONIN$", GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);

	InitializeCriticalSection(&console_cs);

/*	ShowWindow(get_console_wnd(), SW_HIDE); */

	main_wnd = CreateWindowExW(0, L"MainWindow", L"WinTTY",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, inst, NULL);

	ShowWindow(main_wnd, TRUE);
	UpdateWindow(main_wnd);

	ret = run_process(argv + 1, argc - 1);

	DestroyWindow(main_wnd);

	return ret;
}
