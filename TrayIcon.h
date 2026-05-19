#ifndef TRAYICON_H
#define TRAYICON_H

#include <windows.h>

class TrayIcon {
public:
    static bool Init(HINSTANCE hInstance);
    static void Cleanup();
};

#endif
