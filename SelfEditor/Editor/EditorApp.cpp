#include "EditorApp.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

LRESULT CALLBACK EditorApp::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    EditorApp* app = reinterpret_cast<EditorApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_SIZE:
        if (app && app->m_swapChain && wp != SIZE_MINIMIZED)
        {
            app->destroyRenderTarget();
            app->m_swapChain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            app->createRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool EditorApp::init(HWND hwnd)
{
    if (!createDX11(hwnd)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 日本語フォント読み込み（ひらがな・カタカナ・常用漢字を含む）
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\meiryo.ttc",
        15.0f,
        nullptr,
        io.Fonts->GetGlyphRangesJapanese());

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(m_device, m_context);

    return true;
}

void EditorApp::shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    destroyDX11();
}

void EditorApp::mainLoop()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) return;
        }
        render();
    }
}

bool EditorApp::createDX11(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd       = {};
    sd.BufferCount                = 2;
    sd.BufferDesc.Format          = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage                = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow               = hwnd;
    sd.SampleDesc.Count           = 1;
    sd.Windowed                   = TRUE;
    sd.SwapEffect                 = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_device, nullptr, &m_context);

    if (FAILED(hr)) return false;

    createRenderTarget();
    return true;
}

void EditorApp::destroyDX11()
{
    destroyRenderTarget();
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_context)   { m_context->Release();   m_context   = nullptr; }
    if (m_device)    { m_device->Release();    m_device    = nullptr; }
}

void EditorApp::createRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer)
    {
        m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTarget);
        backBuffer->Release();
    }
}

void EditorApp::destroyRenderTarget()
{
    if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
}

void EditorApp::render()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    m_timeline.render();

    ImGui::Render();

    const float clear[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
    m_context->OMSetRenderTargets(1, &m_renderTarget, nullptr);
    m_context->ClearRenderTargetView(m_renderTarget, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_swapChain->Present(1, 0);
}
