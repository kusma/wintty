#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#define mkptr(ptr, add) ((DWORD)(ptr) + (DWORD)(add))

int (*orig_isatty)(int) = NULL;
int my_isatty(int fd)
{
	printf("Hello from isatty()\n");
	fflush(stdout);
	return orig_isatty(fd);
}

FARPROC (WINAPI *orig_GetProcAddress)(HMODULE, LPCSTR) = NULL;
FARPROC WINAPI my_GetProcAddress(HMODULE hmod, LPCSTR proc)
{
	char module_name[MAX_PATH];
	GetModuleFileName(hmod, module_name, MAX_PATH);
	printf("GetProcAddress '%s', '%s'\n", module_name, proc);
	return orig_GetProcAddress(hmod, proc);
}

void *hook_function(PIMAGE_THUNK_DATA piat, void *func)
{
	DWORD op, np;
	MEMORY_BASIC_INFORMATION mbi;
	DWORD *dst = &piat->u1.Function;
	void *ret;

	if (sizeof(mbi) != VirtualQuery(dst, &mbi, sizeof(mbi))) {
		fprintf(stderr, "failed to query memory-protection\n");
		exit(EXIT_FAILURE);
	}

	np = (mbi.Protect & ~(PAGE_READONLY | PAGE_EXECUTE_READ)) | PAGE_READWRITE;
	if (!VirtualProtect(dst, sizeof(DWORD), np, &op)) {
		fprintf(stderr, "failed to unprotect function pointer\n");
		exit(EXIT_FAILURE);
	}
	ret = (void*)piat->u1.Function;
	piat->u1.Function = (DWORD)func;
	VirtualProtect(dst, sizeof(DWORD), op, &np);
	return ret;
}

BOOL APIENTRY DllMain(HANDLE hinst, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DWORD irva;
		PIMAGE_NT_HEADERS32 nthdr;
		HMODULE hmod = GetModuleHandle(0);
		PIMAGE_DOS_HEADER doshdr = (PIMAGE_DOS_HEADER)hmod;
		PIMAGE_IMPORT_DESCRIPTOR idesc;

		DisableThreadLibraryCalls(hinst);

		nthdr = (PIMAGE_NT_HEADERS32)mkptr(hmod, doshdr->e_lfanew);
		if (nthdr->Signature != IMAGE_NT_SIGNATURE) {
			fprintf(stderr, "image-signature mismatch\n");
			exit(EXIT_FAILURE);
		}

		irva = nthdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		if (!irva) {
			fprintf(stderr, "import RVA not found\n");
			exit(EXIT_FAILURE);
		}

		idesc = (PIMAGE_IMPORT_DESCRIPTOR)mkptr(hmod, irva);
		if (!idesc) {
			fprintf(stderr, "import descriptor not found\n");
			exit(EXIT_FAILURE);
		}

		for (; idesc->FirstThunk; ++idesc) {
			char *dllname = (char *)mkptr(hmod, idesc->Name);
			PIMAGE_THUNK_DATA piat = (PIMAGE_THUNK_DATA)mkptr(hmod, idesc->FirstThunk);
			PIMAGE_THUNK_DATA pint = (PIMAGE_THUNK_DATA)mkptr(hmod, idesc->OriginalFirstThunk);

			for (; piat->u1.Function; ++piat, ++pint) {
				if (!IMAGE_SNAP_BY_ORDINAL(pint->u1.Ordinal)) {
					PIMAGE_IMPORT_BY_NAME iname = (PIMAGE_IMPORT_BY_NAME)mkptr(hmod,
						pint->u1.AddressOfData);

					if (!strcmp(iname->Name, "_isatty")) {
						printf("Hooking '%s' from '%s'\n", iname->Name, dllname);
						orig_isatty = (int (*)(int))hook_function(piat, my_isatty);
					}
					else if (!strcmp(iname->Name, "GetProcAddress")) {
						printf("Hooking '%s' from '%s' (%p)\n", iname->Name, dllname, my_GetProcAddress);
						orig_GetProcAddress = (FARPROC (WINAPI *)(HMODULE, LPCSTR))hook_function(piat, my_GetProcAddress);
					}
				}
				else
				{
					printf("ordinal %d\n", pint->u1.Ordinal);
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	return TRUE;
}
