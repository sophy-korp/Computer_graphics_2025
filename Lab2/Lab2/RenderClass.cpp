#include "framework.h"
#include "RenderClass.h"

#include <filesystem>
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

struct Vertex
{
    float x, y, z;
    COLORREF color;
};

HRESULT RenderClass::Init(HWND hWnd)
{
    HRESULT hr = S_OK;

    IDXGIFactory* dxgiFactory = nullptr;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory));

    IDXGIAdapter* chosenAdapter = nullptr;
    if (SUCCEEDED(hr))
    {
        IDXGIAdapter* currentAdapter = nullptr;
        UINT adapterIndex = 0;
        while (SUCCEEDED(dxgiFactory->EnumAdapters(adapterIndex, &currentAdapter)))
        {
            DXGI_ADAPTER_DESC adapterDesc;
            currentAdapter->GetDesc(&adapterDesc);

            if (wcscmp(adapterDesc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                chosenAdapter = currentAdapter;
                break;
            }
            currentAdapter->Release();
            ++adapterIndex;
        }
    }

    // Создание DirectX 11 устройства
    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(hr))
    {
        UINT createFlags = 0;
#ifdef _DEBUG
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(chosenAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            createFlags, featureLevels, 1, D3D11_SDK_VERSION, &m_pDevice, &featureLevel, &m_pDeviceContext);
    }

    if (SUCCEEDED(hr))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        hr = dxgiFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    }

    if (SUCCEEDED(hr))
    {
        hr = ConfigureBackBuffer();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitBufferShader();
    }

    if (chosenAdapter)
        chosenAdapter->Release();
    if (dxgiFactory)
        dxgiFactory->Release();

    if (FAILED(hr))
    {
        Terminate();
    }

    return hr;
}

void RenderClass::Terminate()
{
    TerminateBufferShader();

    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Release();
    }
    if (m_pRenderTargetView)
        m_pRenderTargetView->Release();
    if (m_pSwapChain)
        m_pSwapChain->Release();
    if (m_pDevice)
        m_pDevice->Release();
}

HRESULT RenderClass::InitBufferShader()
{
    static const Vertex vertices[] = {
        {  0.0f,  0.5f, 0.0f, RGB(122, 0, 160) },
        {  0.5f, -0.5f, 0.0f, RGB(56, 56, 56) },
        { -0.5f, -0.5f, 0.0f, RGB(0, 56, 200) }
    };

    static const USHORT indices[] = { 2, 0, 1 };

    static const D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    HRESULT hr = S_OK;

    // Создание вершинного буфера
    if (SUCCEEDED(hr))
    {
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = sizeof(vertices);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = 0;
        vbDesc.MiscFlags = 0;
        vbDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = vertices;
        vbData.SysMemPitch = sizeof(vertices);
        vbData.SysMemSlicePitch = 0;

        hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer);
        if (FAILED(hr))
            return hr;
    }

    // Создание индексного буфера
    if (SUCCEEDED(hr))
    {
        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(indices);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = 0;
        ibDesc.MiscFlags = 0;
        ibDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = indices;
        ibData.SysMemPitch = sizeof(indices);
        ibData.SysMemSlicePitch = 0;

        hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer);
        if (FAILED(hr))
            return hr;
    }

    // Компиляция и установка шейдеров
    ID3DBlob* vertexShaderCode = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"ColorVertex.vs", &vertexShaderCode);
    }
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"ColorPixel.ps");
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pDevice->CreateInputLayout(inputElements, 2,
            vertexShaderCode->GetBufferPointer(), vertexShaderCode->GetBufferSize(), &m_pLayout);
    }

    if (vertexShaderCode)
        vertexShaderCode->Release();

    return hr;
}

void RenderClass::TerminateBufferShader()
{
    if (m_pLayout)
        m_pLayout->Release();
    if (m_pPixelShader)
        m_pPixelShader->Release();
    if (m_pVertexShader)
        m_pVertexShader->Release();
    if (m_pIndexBuffer)
        m_pIndexBuffer->Release();
    if (m_pVertexBuffer)
        m_pVertexBuffer->Release();
}

std::wstring Extension(const std::wstring& filePath)
{
    size_t pos = filePath.find_last_of(L".");
    if (pos == std::wstring::npos || pos == 0)
        return L"";
    return filePath.substr(pos + 1);
}

HRESULT RenderClass::CompileShader(const std::wstring& filePath, ID3DBlob** pCodeShader)
{
    std::wstring ext = Extension(filePath);
    std::string shaderModel;

    if (ext == L"vs")
        shaderModel = "vs_5_0";
    else if (ext == L"ps")
        shaderModel = "ps_5_0";

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* compiledCode = nullptr;
    ID3DBlob* errorMsg = nullptr;

    HRESULT hr = D3DCompileFromFile(filePath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", shaderModel.c_str(), 0, 0, &compiledCode, &errorMsg);
    if (!SUCCEEDED(hr) && errorMsg)
    {
        OutputDebugStringA((const char*)errorMsg->GetBufferPointer());
    }
    if (errorMsg)
        errorMsg->Release();

    if (SUCCEEDED(hr))
    {
        if (ext == L"vs")
        {
            hr = m_pDevice->CreateVertexShader(compiledCode->GetBufferPointer(), compiledCode->GetBufferSize(), nullptr, &m_pVertexShader);
            if (FAILED(hr))
            {
                compiledCode->Release();
                return hr;
            }
        }
        else if (ext == L"ps")
        {
            hr = m_pDevice->CreatePixelShader(compiledCode->GetBufferPointer(), compiledCode->GetBufferSize(), nullptr, &m_pPixelShader);
            if (FAILED(hr))
            {
                compiledCode->Release();
                return hr;
            }
        }
    }

    if (pCodeShader)
        *pCodeShader = compiledCode;
    else
        compiledCode->Release();

    return hr;
}

void RenderClass::Render()
{
    float clearColor[4] = { 0.78f, 0.29f, 0.98f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    m_pDeviceContext->Draw(3, 0);
    m_pSwapChain->Present(0, 0);
}

HRESULT RenderClass::ConfigureBackBuffer()
{
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&backBuffer));
    if (FAILED(hr))
        return hr;

    hr = m_pDevice->CreateRenderTargetView(backBuffer, nullptr, &m_pRenderTargetView);
    backBuffer->Release();

    return hr;
}

void RenderClass::Resize(HWND hWnd)
{
    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pSwapChain)
    {
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        UINT width = clientRect.right - clientRect.left;
        UINT height = clientRect.bottom - clientRect.top;

        HRESULT hr = m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(hr))
        {
            MessageBox(nullptr, L"ResizeBuffers failed.", L"Error", MB_OK);
            return;
        }

        hr = ConfigureBackBuffer();
        if (FAILED(hr))
        {
            MessageBox(nullptr, L"Configure back buffer failed.", L"Error", MB_OK);
            return;
        }

        m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<FLOAT>(width);
        viewport.Height = static_cast<FLOAT>(height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        m_pDeviceContext->RSSetViewports(1, &viewport);
    }
}
