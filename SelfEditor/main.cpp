#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Editor/EditorApp.h"

int main()
{
    WNDCLASSEX wc    = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = EditorApp::wndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = TEXT("SelfEditor");
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(
        TEXT("SelfEditor"), TEXT("SelfEditor - Timeline Event Editor"),
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 760,
        nullptr, nullptr, wc.hInstance, nullptr);

    EditorApp app;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));

    if (!app.init(hwnd))
    {
        DestroyWindow(hwnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    app.mainLoop();

    app.shutdown();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}
