// C++, Windows 11 x86 CLI injector for toker_cs16.dll
// File: loader/loader.cpp
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

#if defined(_WIN64)
#error "This loader must be built as x86 (i686) because hl.exe is a 32-bit process."
#endif

static std::wstring exe_dir() {
    wchar_t buf[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    wchar_t* slash = wcsrchr(buf, L'\\');
    if (slash) *slash = 0;
    return std::wstring(buf);
}

static DWORD get_pid(const wchar_t* proc_name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(proc_name, pe.szExeFile) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// true when the target is a 32-bit process (what our x86 DLL needs)
static bool target_is_x86(HANDLE h) {
    typedef BOOL(WINAPI* Wow64Process2Fn)(HANDLE, USHORT*, USHORT*);
    static Wow64Process2Fn fn = (Wow64Process2Fn)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2");
    if (!fn) return true; // pre-Win10: assume x86, hl.exe always is

    USHORT proc_machine = 0, native_machine = 0;
    if (!fn(h, &proc_machine, &native_machine)) return true;
    return proc_machine == IMAGE_FILE_MACHINE_I386;
}

static bool inject(DWORD pid, const std::wstring& dll_path) {
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                              PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                              FALSE, pid);
    if (!proc) {
        std::wcout << L"[-] OpenProcess failed: " << GetLastError() << L"\n";
        return false;
    }

    if (!target_is_x86(proc)) {
        std::wcout << L"[-] Target is not a 32-bit process; cannot inject an x86 DLL.\n";
        CloseHandle(proc);
        return false;
    }

    size_t bytes = (dll_path.length() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        std::wcout << L"[-] VirtualAllocEx failed: " << GetLastError() << L"\n";
        CloseHandle(proc);
        return false;
    }

    if (!WriteProcessMemory(proc, remote, dll_path.c_str(), bytes, nullptr)) {
        std::wcout << L"[-] WriteProcessMemory failed: " << GetLastError() << L"\n";
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    auto load_library = (LPTHREAD_START_ROUTINE)GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!load_library) {
        std::wcout << L"[-] LoadLibraryW not found.\n";
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, load_library, remote, 0, nullptr);
    if (!thread) {
        std::wcout << L"[-] CreateRemoteThread failed: " << GetLastError() << L"\n";
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD module_base = 0;
    GetExitCodeThread(thread, &module_base);
    CloseHandle(thread);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (!module_base) {
        std::wcout << L"[-] Remote LoadLibraryW returned NULL (DLL load failed).\n";
        return false;
    }
    return true;
}

int main() {
    std::wcout << L"[Toker] CS 1.6 Internal Loader\n";

    std::wstring dll = exe_dir() + L"\\toker_cs16.dll";
    if (GetFileAttributesW(dll.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wcout << L"[-] Missing: " << dll << L"\n";
        std::wcout << L"    Put toker_cs16.dll next to toker_loader.exe.\n";
        system("pause");
        return 1;
    }

    std::wcout << L"[*] Waiting for hl.exe / CsOyna.exe...\n";
    DWORD pid = 0;
    while (!pid) {
        pid = get_pid(L"hl.exe");
        if (!pid) pid = get_pid(L"CsOyna.exe");
        if (!pid) Sleep(500);
    }

    std::wcout << L"[+] Target found. PID: " << pid << L"\n";
    std::wcout << L"[*] Injecting " << dll << L"...\n";

    if (inject(pid, dll)) {
        std::wcout << L"[+] Injected. Press INSERT in-game to open the menu.\n";
        system("pause");
        return 0;
    }

    std::wcout << L"[-] Injection failed.\n";
    system("pause");
    return 1;
}
