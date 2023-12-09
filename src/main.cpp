// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <cstring>
#include <algorithm>
#include <iostream>

#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "../ext/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb/stb_image_write.h"
#define SDF_IMPLEMENTATION
#include "../sdf/sdf.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Data
static ID3D11Device *g_pd3dDevice = nullptr;
static ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
static IDXGISwapChain *g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

inline bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

inline void Log(std::string const &value)
{
    std::cout << value << std::endl;
}

// Main code
int main(int, char **)
{
    // Create application window
    // ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr};
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Image Sdf Baker", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    bool show_demo_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Sdf state
    int radius = 64;
    bool use_channel_r = false;
    bool use_channel_g = false;
    bool use_channel_b = false;
    bool use_channel_a = true;
    std::string sourceFileName;

    unsigned int SizeX, SizeY, Comp, ElementSize, PixelSize;
    unsigned char *charData = nullptr;

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            ImGui::Begin("Sdf Baker");

            // Menu Bar
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open"))
                    {
                        OPENFILENAME ofn;               // common dialog box structure
                        char szFile[MAX_PATH] = {'\0'}; // buffer for file name

                        // Initialize OPENFILENAME
                        ZeroMemory(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFile = szFile;
                        ofn.nMaxFile = sizeof(szFile);
                        ofn.lpstrFilter = "ALL\0*.*\0PNG\0*.png\0TGA\0*.tga\0";
                        ofn.nFilterIndex = 1;
                        ofn.lpstrFileTitle = NULL;
                        ofn.nMaxFileTitle = 0;
                        ofn.lpstrInitialDir = NULL;
                        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                        // Display the Open dialog box.
                        if (GetOpenFileName(&ofn))
                        {
                            sourceFileName = std::string(szFile);
                            std::transform(sourceFileName.begin(), sourceFileName.end(), sourceFileName.begin(), ::tolower);
                        }

                        // free old data
                        if (charData != nullptr)
                        {
                            delete[] charData;
                            charData = nullptr;
                        }

                        // Data Prepare
                        int sizeX, sizeY, comp;
                        unsigned char *SrcCharData = stbi_load(szFile, &sizeX, &sizeY, &comp, 4);
                        SizeX = sizeX;
                        SizeY = sizeY;
                        Comp = comp;
                        ElementSize = SizeX * SizeY * Comp;
                        PixelSize = SizeX * SizeY;
                        charData = new unsigned char[ElementSize];
                        std::memcpy(charData, SrcCharData, ElementSize);
                        stbi_image_free(SrcCharData);
                        Log("Open File: " + sourceFileName);
                    }

                    if (ImGui::MenuItem("Save"))
                    {
                        if (ends_with(sourceFileName, ".png"))
                        {
                            stbi_write_png(sourceFileName.c_str(), SizeX, SizeY, Comp, charData, SizeX * Comp);
                        }
                        else if (ends_with(sourceFileName, ".tga"))
                        {
                            stbi_write_tga(sourceFileName.c_str(), SizeX, SizeY, Comp, charData);
                        }

                        Log("Save File: " + sourceFileName);
                    }

                    if (ImGui::MenuItem("Save As.."))
                    {
                        OPENFILENAME ofn;               // common dialog box structure
                        char szFile[MAX_PATH] = {'\0'}; // buffer for file name

                        // Initialize OPENFILENAME
                        ZeroMemory(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFile = szFile;
                        ofn.nMaxFile = sizeof(szFile);
                        ofn.lpstrFilter = "ALL\0*.*\0PNG\0*.png\0TGA\0*.tga\0";
                        ofn.nFilterIndex = 1;
                        ofn.lpstrFileTitle = NULL;
                        ofn.nMaxFileTitle = 0;
                        ofn.lpstrInitialDir = NULL;
                        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST;

                        // Display the Open dialog box.
                        if (GetSaveFileName(&ofn))
                        {
                            std::string writeFileName(szFile);
                            std::transform(writeFileName.begin(), writeFileName.end(), writeFileName.begin(), ::tolower);
                            if (ends_with(writeFileName, ".png"))
                            {
                                stbi_write_png(writeFileName.c_str(), SizeX, SizeY, Comp, charData, SizeX * Comp);
                            }
                            else if (ends_with(writeFileName, ".tga"))
                            {
                                stbi_write_tga(writeFileName.c_str(), SizeX, SizeY, Comp, charData);
                            }
                            Log("Save As File: " + writeFileName);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            ImGui::Text("Source File: %s", sourceFileName.c_str());
            ImGui::PopStyleColor();
            ImGui::Text("Bake Channel Select:");
            ImGui::Checkbox("Red", &use_channel_r);
            ImGui::Checkbox("Green", &use_channel_g);
            ImGui::Checkbox("Blue", &use_channel_b);
            ImGui::Checkbox("Alpha", &use_channel_a);

            ImGui::Text("Seach Radius: ");
            ImGui::SameLine();
            ImGui::SliderInt("pixels", &radius, 1, 256);

            if (ImGui::Button("Bake Sdf"))
            {
                if (use_channel_r)
                {
                    unsigned char *channelData = new unsigned char[PixelSize];
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        channelData[i] = charData[i * Comp];
                    }
                    sdfBuildDistanceField(channelData, SizeX, radius, radius, channelData, SizeX, SizeY, SizeX);
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        charData[i * Comp] = channelData[i];
                    }
                    delete channelData;
                    Log("Bake Red Channel Success.");
                }
                if (use_channel_g)
                {
                    unsigned char *channelData = new unsigned char[PixelSize];
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        channelData[i] = charData[i * Comp + 1];
                    }
                    sdfBuildDistanceField(channelData, SizeX, radius, radius, channelData, SizeX, SizeY, SizeX);
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        charData[i * Comp + 1] = channelData[i];
                    }
                    delete channelData;
                    Log("Bake Green Channel Success.");
                }
                if (use_channel_b)
                {
                    unsigned char *channelData = new unsigned char[PixelSize];
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        channelData[i] = charData[i * Comp + 2];
                    }
                    sdfBuildDistanceField(channelData, SizeX, radius, radius, channelData, SizeX, SizeY, SizeX);
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        charData[i * Comp + 2] = channelData[i];
                    }
                    delete channelData;
                    Log("Bake Blue Channel Success.");
                }
                if (use_channel_a && Comp == 4)
                {
                    unsigned char *channelData = new unsigned char[PixelSize];
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        channelData[i] = charData[i * Comp + 3];
                    }
                    sdfBuildDistanceField(channelData, SizeX, radius, radius, channelData, SizeX, SizeY, SizeX);
                    for (unsigned int i = 0; i < PixelSize; i++)
                    {
                        charData[i * Comp + 3] = channelData[i];
                    }
                    delete channelData;
                    Log("Bake Alpha Channel Success.");
                }
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
                                     // g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Sdf resource release
    delete[] charData;

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
