#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include "TimelineEditor.h"

class EditorApp
{
public:
    bool init(HWND hwnd);
    void shutdown();
    void mainLoop();

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    bool createDX11(HWND hwnd);
    void destroyDX11();
    void createRenderTarget();
    void destroyRenderTarget();
    void render();

    ID3D11Device*           m_device       = nullptr;
    ID3D11DeviceContext*    m_context      = nullptr;
    IDXGISwapChain*         m_swapChain    = nullptr;
    ID3D11RenderTargetView* m_renderTarget = nullptr;

    TimelineEditor m_timeline;
};
