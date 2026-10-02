#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

static const wchar_t* kDir = L"C:\\Program Files\\f1copy";
static const wchar_t* kUninstallKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\f1copy";

static std::wstring Join(const wchar_t* name) { return std::wstring(kDir) + L"\\" + name; }

static bool Run(const std::wstring& path, const wchar_t* arguments) {
    std::wstring command = L"\"" + path + L"\" " + arguments;
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(0);
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(path.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &process)) return false;
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return code == 0;
}

static bool StopF1copy() {
    // The app normally exits via WM_CLOSE. Force termination also handles a hung copy.
    HWND window = FindWindowExW(HWND_MESSAGE, nullptr, L"f1copy_HiddenWnd", nullptr);
    if (!window) window = FindWindowW(L"f1copy_HiddenWnd", nullptr);
    if (window) PostMessageW(window, WM_CLOSE, 0, 0);
    Sleep(1000);
    wchar_t systemDir[MAX_PATH];
    if (!GetSystemDirectoryW(systemDir, MAX_PATH)) return false;
    std::wstring taskkill = std::wstring(systemDir) + L"\\taskkill.exe";
    // Image name is fixed, with no user supplied shell text.
    Run(taskkill, L"/F /IM f1copy.exe");
    return true;
}

static bool WriteResource(const std::wstring& path) {
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!resource) return false;
    HGLOBAL data = LoadResource(nullptr, resource);
    if (!data) return false;
    void* bytes = LockResource(data);
    DWORD size = SizeofResource(nullptr, resource);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes, size, &written, nullptr) && written == size;
    CloseHandle(file);
    return ok;
}

static void SetString(HKEY key, const wchar_t* name, const std::wstring& value) {
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool uninstall = argc > 1 && wcscmp(argv[1], L"-uninstall") == 0;
    if (argv) LocalFree(argv);
    if (uninstall) {
        StopF1copy();
        const std::wstring app = Join(L"f1copy.exe");
        if (GetFileAttributesW(app.c_str()) != INVALID_FILE_ATTRIBUTES && !Run(app, L"-uninstall")) {
            MessageBoxW(nullptr, L"設定の削除に失敗しました。", L"f1copy", MB_ICONERROR);
            return 1;
        }
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, kUninstallKey);
        // The running uninstaller cannot delete itself. A detached helper removes the directory after exit.
        wchar_t systemDir[MAX_PATH];
        GetSystemDirectoryW(systemDir, MAX_PATH);
        std::wstring cmd = std::wstring(L"\"") + systemDir + L"\\cmd.exe\" /C "
            L"\"ping 127.0.0.1 -n 3 >nul & rmdir /S /Q \"" + kDir + L"\"\"";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        std::vector<wchar_t> args(cmd.begin(), cmd.end()); args.push_back(0);
        if (CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        }
        return 0;
    }

    StopF1copy();
    if (!CreateDirectoryW(kDir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return 1;
    const std::wstring app = Join(L"f1copy.exe");
    if (!WriteResource(app)) {
        MessageBoxW(nullptr, L"プログラムのコピーに失敗しました。", L"f1copy", MB_ICONERROR);
        return 1;
    }
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::wstring uninstaller = Join(L"Uninstall.exe");
    if (!CopyFileW(self, uninstaller.c_str(), FALSE) && GetLastError() != ERROR_SHARING_VIOLATION) return 1;
    if (!Run(app, L"-install")) {
        MessageBoxW(nullptr, L"自動起動またはキー設定の登録に失敗しました。", L"f1copy", MB_ICONERROR);
        return 1;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, kUninstallKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) == ERROR_SUCCESS) {
        SetString(key, L"DisplayName", L"f1copy");
        SetString(key, L"DisplayVersion", L"0.92");
        SetString(key, L"InstallLocation", kDir);
        SetString(key, L"DisplayIcon", app);
        SetString(key, L"UninstallString", L"\"" + uninstaller + L"\" -uninstall");
        DWORD noModify = 1;
        RegSetValueExW(key, L"NoModify", 0, REG_DWORD, reinterpret_cast<BYTE*>(&noModify), sizeof(noModify));
        RegCloseKey(key);
    }
    return 0;
}
