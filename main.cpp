#include <windows.h>
#include <string>
#include <shellapi.h>
#include "TaskReg.h"
#include "ScancodeMap.h"
#include "TrayIcon.h"
#include "KeyHook.h"
#include "SplashWnd.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool install   = false;
    bool uninstall = false;

    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if      (arg == L"-install")   install   = true;
            else if (arg == L"-uninstall") uninstall = true;
        }
        LocalFree(argv);
    }

    // --------------------------------------------------------------------
    // Uninstall: remove Scancode Map + Task Scheduler entry, then quit
    // --------------------------------------------------------------------
    if (uninstall) {
        if (!TaskReg::IsAdmin()) {
            TaskReg::RunAsAdmin(L"-uninstall");
            return 0;
        }
        ScancodeMap::Uninstall();
        TaskReg::Uninstall();
        // 常駐中のインスタンスに終了スプラッシュ付きで終了させる
        HWND hWnd = FindWindowW(L"f1copy_HiddenWnd", NULL);
        if (hWnd) PostMessageW(hWnd, WM_CLOSE, 0, 0);
        return 0;
    }

    // --------------------------------------------------------------------
    // Install: write Scancode Map + register Task Scheduler entry
    // --------------------------------------------------------------------
    if (install) {
        if (!TaskReg::IsAdmin()) {
            TaskReg::RunAsAdmin(L"-install");
            return 0;
        }
        ScancodeMap::Install();
        TaskReg::Install();
        // After install, fall through to start resident operation.
    }

    // --------------------------------------------------------------------
    // Resident operation (normal launch or post-install)
    // --------------------------------------------------------------------
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\f1copy_mutex_2026");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    if (!TrayIcon::Init(hInstance)) {
        CloseHandle(hMutex);
        return 1;
    }

    KeyHook::Install();
    SplashWnd::Show(hInstance, true);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KeyHook::Uninstall();
    TrayIcon::Cleanup();
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
