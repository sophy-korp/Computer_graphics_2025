#include "framework.h"
#include "Lab1.h"
#include "RenderClass.h"
#include <dxgi.h>
#include <d3d11.h>

#define MAX_LOADSTRING 100
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

const WCHAR WINDOW_TITLE[MAX_LOADSTRING] = L"Lab1 Korpusova";
const WCHAR WINDOW_CLASS[MAX_LOADSTRING] = L"Lab1Class";

RenderClass* globalRenderInstance = nullptr;

ATOM RegisterMyWindowClass(HINSTANCE instance);
BOOL CreateMainWindow(HINSTANCE instance, int cmdShow);
LRESULT CALLBACK HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow) {
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);

    RegisterMyWindowClass(instance);

    if (!CreateMainWindow(instance, cmdShow)) {
        OutputDebugString(_T("Initialization error\n"));
        return FALSE;
    }

    MSG message;
    while (GetMessage(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
        
        if (globalRenderInstance) {
            OutputDebugString(_T("Rendering frame\n"));
            globalRenderInstance->Render();
        }
    }

    globalRenderInstance->Terminate();
    delete globalRenderInstance;

    return (int)message.wParam;
}

ATOM RegisterMyWindowClass(HINSTANCE instance) {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = HandleMessage;
    wc.hInstance = instance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS;

    return RegisterClassExW(&wc);
}

BOOL CreateMainWindow(HINSTANCE instance, int cmdShow) {
    HWND windowHandle = CreateWindowW(WINDOW_CLASS, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                       nullptr, nullptr, instance, nullptr);
                                       
    if (!windowHandle) {
        OutputDebugString(_T("Failed to create window\n"));
        return FALSE;
    }

    globalRenderInstance = new RenderClass();
    if (FAILED(globalRenderInstance->Init(windowHandle))) {
        OutputDebugString(_T("Failed to initialize renderer\n"));
        delete globalRenderInstance;
        return FALSE;
    }

    ShowWindow(windowHandle, cmdShow);
    UpdateWindow(windowHandle);

    return TRUE;
}

LRESULT CALLBACK HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        if (globalRenderInstance && wParam != SIZE_MINIMIZED) {
            globalRenderInstance->Resize(window);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(window, message, wParam, lParam);
    }
    return 0;
}