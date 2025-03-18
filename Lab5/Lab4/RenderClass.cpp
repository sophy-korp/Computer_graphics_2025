#include "framework.h"
#include "RenderClass.h"
#include "DDSTextureLoader11.h"
#include <filesystem>
#include <wrl/client.h>

#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

struct Vertex
{
    float x, y, z;
    float u, v;
};

struct SkyboxVertex
{
    float x, y, z;
};

struct MatrixBuffer
{
    XMMATRIX m;
};

struct CameraBuffer
{
    XMMATRIX vp;
};


struct ParallelogramVertex
{
    float x, y, z;
};

struct ColorBuffer
{
    XMFLOAT4 color;
};

HRESULT RenderClass::Init(HWND hWnd, WCHAR szTitle[], WCHAR szWindowClass[])
{
    m_szTitle = szTitle;
    m_szWindowClass = szWindowClass;

    HRESULT hr;

    IDXGIFactory* pFactory = nullptr;
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    IDXGIAdapter* pSelectedAdapter = nullptr;
    if (SUCCEEDED(hr))
    {
        IDXGIAdapter* pAdapter = nullptr;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);

            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                pSelectedAdapter = pAdapter;
                break;
            }

            pAdapter->Release();
            adapterIdx++;
        }
    }

    // Создание устройства DirectX 11
    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(hr))
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);
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
        swapChainDesc.Windowed = true;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = 0;

        hr = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    }

    if (SUCCEEDED(hr))
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;
        hr = ConfigureBackBuffer(width, height);
    }

    if (SUCCEEDED(hr))
    {
        hr = InitBufferShader();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitSkybox();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitParallelogram();
    }

    if (pSelectedAdapter) pSelectedAdapter->Release();
    if (pFactory) pFactory->Release();

    if (FAILED(hr))
    {
        Terminate();
    }

    return hr;
}

HRESULT RenderClass::InitBufferShader()
{
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    HRESULT hr = S_OK;

    ID3DBlob* pVertexCode = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"ColorVertex.vs", &m_pVertexShader, nullptr, &pVertexCode);
    }
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"ColorPixel.ps", nullptr, &m_pPixelShader);
    }

    if (SUCCEEDED(hr))
    {
        hr = m_pDevice->CreateInputLayout(InputDesc, 2, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pLayout);
    }

    if (pVertexCode) pVertexCode->Release();

    static const Vertex vertices[] = {
        { -1.0f, -1.0f,  1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f,  1.0f, 1.0f, 1.0f },
        {  1.0f, -1.0f, -1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, -1.0f, 0.0f, 0.0f },

        { -1.0f,  1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f,  1.0f, -1.0f, 1.0f, 1.0f },
        {  1.0f,  1.0f,  1.0f, 1.0f, 0.0f },
        { -1.0f,  1.0f,  1.0f, 0.0f, 0.0f },

        {  1.0f, -1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f,  1.0f, 1.0f, 1.0f },
        {  1.0f,  1.0f,  1.0f, 1.0f, 0.0f },
        {  1.0f,  1.0f, -1.0f, 0.0f, 0.0f },

        { -1.0f, -1.0f,  1.0f, 0.0f, 1.0f },
        { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f },
        { -1.0f,  1.0f, -1.0f, 1.0f, 0.0f },
        { -1.0f,  1.0f,  1.0f, 0.0f, 0.0f },

        {  1.0f, -1.0f,  1.0f, 0.0f, 1.0f },
        { -1.0f, -1.0f,  1.0f, 1.0f, 1.0f },
        { -1.0f,  1.0f,  1.0f, 1.0f, 0.0f },
        {  1.0f,  1.0f,  1.0f, 0.0f, 0.0f },

        { -1.0f, -1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f, -1.0f, 1.0f, 1.0f },
        {  1.0f,  1.0f, -1.0f, 1.0f, 0.0f },
        { -1.0f,  1.0f, -1.0f, 0.0f, 0.0f }
    };

    WORD indices[] = {
        0, 2, 1, 0, 3, 2,
        4, 6, 5, 4, 7, 6,
        8, 10, 9, 8, 11, 10,
        12, 14, 13, 12, 15, 14,
        16, 18, 17, 16, 19, 18,
        20, 22, 21, 20, 23, 22
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * ARRAYSIZE(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(hr)) return hr;

    bd.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
    if (FAILED(hr)) return hr;

    bd.ByteWidth = sizeof(XMMATRIX);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = m_pDevice->CreateBuffer(&bd, nullptr, &m_pModelBuffer);
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC vpBufferDesc = {};
    vpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpBufferDesc.ByteWidth = sizeof(XMMATRIX);
    vpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_pDevice->CreateBuffer(&vpBufferDesc, nullptr, &m_pVPBuffer);
    if (FAILED(hr)) return hr;

    ID3D11Resource* pTexture = nullptr;
    hr = CreateDDSTextureFromFileEx(m_pDevice, L"cat.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, DDS_LOADER_DEFAULT, &pTexture, &m_pTextureView);
    if (FAILED(hr)) return hr;

    if (pTexture)
    {
        ID3D11Texture2D* pTexture2D = nullptr;
        pTexture->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pTexture2D);
        if (pTexture2D)
        {
            m_pDeviceContext->GenerateMips(m_pTextureView);
            pTexture2D->Release();
        }
        pTexture->Release();
    }

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    sampDesc.MaxAnisotropy = 16;
    hr = m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);

    return hr;
}

HRESULT RenderClass::InitSkybox()
{
    ID3DBlob* pVertexCode = nullptr;
    HRESULT hr = CompileShader(L"SkyboxVertex.vs", &m_pSkyboxVS, nullptr, &pVertexCode);
    if (SUCCEEDED(hr))
    {
        hr = CompileShader(L"SkyboxPixel.ps", nullptr, &m_pSkyboxPS);
    }

    D3D11_INPUT_ELEMENT_DESC skyboxLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (SUCCEEDED(hr))
    {
        hr = m_pDevice->CreateInputLayout(skyboxLayout, 1, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pSkyboxLayout);
    }

    SkyboxVertex SkyboxVertices[] = {
        { -1.0f, -1.0f, -1.0f },
        { -1.0f,  1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f },
        {  1.0f, -1.0f, -1.0f },

        {  1.0f, -1.0f,  1.0f },
        {  1.0f,  1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f },
        {  1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f },
        { -1.0f, -1.0f,  1.0f },

        { -1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f },
        { -1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        { -1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f, -1.0f },

        {  1.0f, -1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f },
        {  1.0f,  1.0f,  1.0f },
        {  1.0f, -1.0f, -1.0f },
        {  1.0f,  1.0f,  1.0f },
        {  1.0f, -1.0f,  1.0f },

        { -1.0f,  1.0f, -1.0f },
        { -1.0f,  1.0f,  1.0f },
        {  1.0f,  1.0f,  1.0f },
        { -1.0f,  1.0f, -1.0f },
        {  1.0f,  1.0f,  1.0f },
        {  1.0f,  1.0f, -1.0f },

        { -1.0f, -1.0f,  1.0f },
        { -1.0f, -1.0f, -1.0f },
        {  1.0f, -1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        {  1.0f, -1.0f, -1.0f },
        {  1.0f, -1.0f,  1.0f }
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SkyboxVertex) * ARRAYSIZE(SkyboxVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = SkyboxVertices;
    hr = m_pDevice->CreateBuffer(&bd, &initData, &m_pSkyboxVB);
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC vpBufferDesc = {};
    vpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpBufferDesc.ByteWidth = sizeof(XMMATRIX);
    vpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_pDevice->CreateBuffer(&vpBufferDesc, nullptr, &m_pSkyboxVPBuffer);
    if (FAILED(hr)) return hr;

    hr = CreateDDSTextureFromFile(m_pDevice, L"skybox.dds", nullptr, &m_pSkyboxSRV);
    if (FAILED(hr)) return hr;

    return S_OK;
}

void RenderClass::Terminate()
{
    TerminateBufferShader();
    TerminateSkybox();
    TerminateParallelogram();

    

    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pSwapChain)
    {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }

    if (m_pColorBuffer)
    {
        m_pColorBuffer->Release();
        m_pColorBuffer = nullptr;
    }

    if (m_pDeviceContext)
    {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Release();
        m_pDeviceContext = nullptr;
    }

    if (m_pDevice)
    {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

}

void RenderClass::TerminateBufferShader()
{
    if (m_pLayout) m_pLayout->Release();
    if (m_pPixelShader) m_pPixelShader->Release();
    if (m_pVertexShader) m_pVertexShader->Release();
    if (m_pIndexBuffer) m_pIndexBuffer->Release();
    if (m_pVertexBuffer) m_pVertexBuffer->Release();
    if (m_pModelBuffer) m_pModelBuffer->Release();
    if (m_pVPBuffer) m_pVPBuffer->Release();
    if (m_pTextureView) m_pTextureView->Release();
    if (m_pSamplerState) m_pSamplerState->Release();
}

void RenderClass::TerminateSkybox()
{
    if (m_pSkyboxVB) m_pSkyboxVB->Release();
    if (m_pSkyboxSRV) m_pSkyboxSRV->Release();
    if (m_pSkyboxVPBuffer) m_pSkyboxVPBuffer->Release();
    if (m_pSkyboxLayout) m_pSkyboxLayout->Release();
    if (m_pSkyboxVS) m_pSkyboxVS->Release();
    if (m_pSkyboxPS) m_pSkyboxPS->Release();
}

std::wstring Extension(const std::wstring& path)
{
    size_t dotPos = path.find_last_of(L".");
    if (dotPos == std::wstring::npos || dotPos == 0)
    {
        return L"";
    }
    return path.substr(dotPos + 1);
}

HRESULT RenderClass::CompileShader(const std::wstring& path, ID3D11VertexShader** ppVertexShader, ID3D11PixelShader** ppPixelShader, ID3DBlob** pCodeShader)
{
    std::wstring extension = Extension(path);

    std::string platform = "";
    if (extension == L"vs")
    {
        platform = "vs_5_0";
    }
    else if (extension == L"ps")
    {
        platform = "ps_5_0";
    }

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pCode = nullptr;
    ID3DBlob* pErr = nullptr;

    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", platform.c_str(), 0, 0, &pCode, &pErr);
    if (FAILED(hr) && pErr != nullptr)
    {
        OutputDebugStringA((const char*)pErr->GetBufferPointer());
    }
    if (pErr) pErr->Release();

    if (SUCCEEDED(hr))
    {
        if (extension == L"vs" && ppVertexShader)
        {
            hr = m_pDevice->CreateVertexShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppVertexShader);
            if (FAILED(hr))
            {
                pCode->Release();
                return hr;
            }
        }
        else if (extension == L"ps" && ppPixelShader)
        {
            hr = m_pDevice->CreatePixelShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), nullptr, ppPixelShader);
            if (FAILED(hr))
            {
                pCode->Release();
                return hr;
            }
        }
    }

    if (pCodeShader)
    {
        *pCodeShader = pCode;
    }
    else
    {
        pCode->Release();
    }
    return hr;
}

void RenderClass::MoveCamera(float dx, float dy, float dz)
{
    m_CameraPosition.x += dx * m_CameraSpeed;
    m_CameraPosition.y += dy * m_CameraSpeed;
    m_CameraPosition.z += dz * m_CameraSpeed;
}

void RenderClass::RotateCamera(float lrAngle, float udAngle)
{
    m_LRAngle += lrAngle;
    m_UDAngle -= udAngle;

    if (m_LRAngle > XM_2PI) m_LRAngle -= XM_2PI;
    if (m_LRAngle < -XM_2PI) m_LRAngle += XM_2PI;

    if (m_UDAngle > XM_PIDIV2) m_UDAngle = XM_PIDIV2;
    if (m_UDAngle < -XM_PIDIV2) m_UDAngle = -XM_PIDIV2;
}

void RenderClass::Render() {
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);

    const float backgroundColor[4] = { 0.48f, 0.57f, 0.48f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, backgroundColor);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    XMMATRIX rotationY = XMMatrixRotationY(m_LRAngle);
    XMMATRIX rotationX = XMMatrixRotationX(m_UDAngle);
    XMMATRIX combinedRotation = (m_CameraPosition.z <= 0)
        ? rotationY * rotationX
        : rotationX * rotationY;

    XMVECTOR cameraPosition = XMLoadFloat3(&m_CameraPosition);
    XMVECTOR forwardVector = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR lookDirection = XMVector3TransformNormal(forwardVector, combinedRotation);
    XMVECTOR lookAtPoint = XMVectorAdd(cameraPosition, lookDirection);
    XMMATRIX viewMatrix = XMMatrixLookAtLH(
        cameraPosition,
        lookAtPoint,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    RECT clientRect;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &clientRect);
    float width = static_cast<float>(clientRect.right - clientRect.left);
    float height = static_cast<float>(clientRect.bottom - clientRect.top);
    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        width / height,
        0.1f,
        100.0f
    );

    RenderSkybox(projectionMatrix);
    RenderCubes(viewMatrix, projectionMatrix);
    RenderParallelogram();

    m_pSwapChain->Present(1, 0);
}

void RenderClass::SetMVPBuffer()
{
    XMMATRIX rotLR = XMMatrixRotationY(m_LRAngle);
    XMMATRIX rotUD = XMMatrixRotationX(m_UDAngle);
    XMMATRIX totalRot = rotLR * rotUD;

    XMVECTOR defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR direction = XMVector3TransformNormal(defaultForward, totalRot);

    XMVECTOR eyePos = XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 0.0f);
    XMVECTOR focusPoint = XMVectorAdd(eyePos, direction);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, focusPoint, upDir);

    RECT rc;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &rc);
    float aspect = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    XMMATRIX rotLRSky = XMMatrixRotationY(-m_LRAngle);
    XMMATRIX rotUDSky = XMMatrixRotationX(-m_UDAngle);
    XMMATRIX viewSkybox = rotLRSky * rotUDSky;
    XMMATRIX vpSkybox = XMMatrixTranspose(viewSkybox * proj);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_pDeviceContext->Map(m_pSkyboxVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &vpSkybox, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pSkyboxVPBuffer, 0);
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ID3D11DepthStencilState* pDSStateSkybox = nullptr;
    m_pDevice->CreateDepthStencilState(&dsDesc, &pDSStateSkybox);
    m_pDeviceContext->OMSetDepthStencilState(pDSStateSkybox, 0);

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_FRONT;
    rsDesc.FrontCounterClockwise = false;
    ID3D11RasterizerState* pSkyboxRS = nullptr;
    if (SUCCEEDED(m_pDevice->CreateRasterizerState(&rsDesc, &pSkyboxRS)))
    {
        m_pDeviceContext->RSSetState(pSkyboxRS);
    }

    UINT stride = sizeof(SkyboxVertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVB, &stride, &offset);
    m_pDeviceContext->IASetInputLayout(m_pSkyboxLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pDeviceContext->VSSetShader(m_pSkyboxVS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pSkyboxVPBuffer);

    m_pDeviceContext->PSSetShader(m_pSkyboxPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkyboxSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    m_pDeviceContext->Draw(36, 0);

    pDSStateSkybox->Release();
    if (pSkyboxRS)
    {
        pSkyboxRS->Release();
        m_pDeviceContext->RSSetState(nullptr);
    }

    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);

    m_CubeAngle += 0.01f;
    if (m_CubeAngle > XM_2PI) m_CubeAngle -= XM_2PI;
    XMMATRIX model = XMMatrixRotationY(m_CubeAngle);

    XMMATRIX vp = view * proj;
    XMMATRIX mT = XMMatrixTranspose(model);
    XMMATRIX vpT = XMMatrixTranspose(vp);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &mT, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &vpT, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);
}

HRESULT RenderClass::ConfigureBackBuffer(UINT width, UINT height)
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return hr;

    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D32_FLOAT;;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2D* pDepthStencil = nullptr;
    hr = m_pDevice->CreateTexture2D(&descDepth, nullptr, &pDepthStencil);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr, &m_pDepthView);
    pDepthStencil->Release();
    if (FAILED(hr)) return hr;

    return hr;
}

void RenderClass::Resize(HWND hWnd)
{
    if (m_pRenderTargetView)
    {
        m_pRenderTargetView->Release();
        m_pRenderTargetView = nullptr;
    }

    if (m_pDepthView)
    {
        m_pDepthView->Release();
        m_pDepthView = nullptr;
    }

    if (m_pSwapChain)
    {
        HRESULT hr;

        RECT rc;
        GetClientRect(hWnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        hr = m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(hr))
        {
            MessageBox(nullptr, L"ResizeBuffers failed.", L"Error", MB_OK);
            return;
        }

        HRESULT resultBack = ConfigureBackBuffer(width, height);
        if (FAILED(resultBack))
        {
            MessageBox(nullptr, L"Configure back buffer failed.", L"Error", MB_OK);
            return;
        }

        m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);

        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_pDeviceContext->RSSetViewports(1, &vp);
    }
}

HRESULT RenderClass::InitParallelogram() {
    ID3DBlob* pVertexCode = nullptr;

    HRESULT result = CompileShader(L"ParallelogramVertex.vs", &m_pParallelogramVS, nullptr, &pVertexCode);
    if (FAILED(result)) 
        return result;

    result = CompileShader(L"ParallelogramPixel.ps", nullptr, &m_pParallelogramPS);
    if (FAILED(result)) 
        return result;

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    result = m_pDevice->CreateInputLayout(layout, 1, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pParallelogramLayout);
    if (FAILED(result)) 
        return result;

    ParallelogramVertex ParallelogramVertices[] = {
        {-0.75f, -0.75f, 0.0f}, 
        {-0.75f, 0.75f, 0.0f},
        {0.75f, 0.75f, 0.0f}, 
        {0.75f, -0.75f, 0.0f}
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ParallelogramVertex) * ARRAYSIZE(ParallelogramVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = ParallelogramVertices;
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_ParallelogramVertexBuffer);
    if (FAILED(result))
        return result;

    WORD indices[] = { 0, 1, 2, 0, 2, 3 };
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pParallelogramIndexBuffer);

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(ColorBuffer);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pColorBuffer);

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    result = m_pDevice->CreateBlendState(&blendDesc, &m_pBlendState);

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    result = m_pDevice->CreateDepthStencilState(&dsDesc, &m_pStateParallelogram);

    return result;
}

void RenderClass::TerminateParallelogram() {
    if (m_ParallelogramVertexBuffer) m_ParallelogramVertexBuffer->Release();
    if (m_pParallelogramIndexBuffer) m_pParallelogramIndexBuffer->Release();
    if (m_pParallelogramPS) m_pParallelogramPS->Release();
    if (m_pParallelogramVS) m_pParallelogramVS->Release();
    if (m_pParallelogramLayout) m_pParallelogramLayout->Release();
    if (m_pBlendState) m_pBlendState->Release();
    if (m_pStateParallelogram) m_pStateParallelogram->Release();
}

void RenderClass::RenderSkybox(XMMATRIX projectionMatrix) {
    XMMATRIX rotationY = XMMatrixRotationY(-m_LRAngle);
    XMMATRIX rotationX = XMMatrixRotationX(-m_UDAngle);
    XMMATRIX skyboxView = rotationY * rotationX;
    XMMATRIX skyboxViewProjection = XMMatrixTranspose(skyboxView * projectionMatrix);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(m_pDeviceContext->Map(m_pSkyboxVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        memcpy(mappedResource.pData, &skyboxViewProjection, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pSkyboxVPBuffer, 0);
    }

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    ID3D11DepthStencilState* depthStencilState = nullptr;
    m_pDevice->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
    m_pDeviceContext->OMSetDepthStencilState(depthStencilState, 0);

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_FRONT;
    rasterizerDesc.FrontCounterClockwise = false;

    ID3D11RasterizerState* rasterizerState = nullptr;
    if (SUCCEEDED(m_pDevice->CreateRasterizerState(&rasterizerDesc, &rasterizerState))) {
        m_pDeviceContext->RSSetState(rasterizerState);
    }

    UINT vertexStride = sizeof(SkyboxVertex);
    UINT vertexOffset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVB, &vertexStride, &vertexOffset);
    m_pDeviceContext->IASetInputLayout(m_pSkyboxLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pDeviceContext->VSSetShader(m_pSkyboxVS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pSkyboxVPBuffer);

    m_pDeviceContext->PSSetShader(m_pSkyboxPS, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkyboxSRV);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    m_pDeviceContext->Draw(36, 0);

    if (depthStencilState) {
        depthStencilState->Release();
    }
    if (rasterizerState) {
        rasterizerState->Release();
        m_pDeviceContext->RSSetState(nullptr);
    }
}

void RenderClass::RenderCubes(XMMATRIX viewMatrix, XMMATRIX projectionMatrix) {
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthView);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    m_CubeAngle += 0.01f;
    if (m_CubeAngle > XM_2PI) {
        m_CubeAngle -= XM_2PI;
    }

    XMMATRIX cubeModel = XMMatrixRotationY(m_CubeAngle);
    XMMATRIX viewProjection = viewMatrix * projectionMatrix;
    XMMATRIX transposedModel = XMMatrixTranspose(cubeModel);
    XMMATRIX transposedViewProjection = XMMatrixTranspose(viewProjection);

    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &transposedModel, 0, 0);

    D3D11_MAPPED_SUBRESOURCE mappedBuffer;
    HRESULT result = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuffer);
    if (SUCCEEDED(result)) {
        memcpy(mappedBuffer.pData, &transposedViewProjection, sizeof(XMMATRIX));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }

    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    UINT vertexStride = sizeof(Vertex);
    UINT vertexOffset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &vertexStride, &vertexOffset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureView);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    m_pDeviceContext->DrawIndexed(36, 0, 0);

    static float orbitAngle = 0.0f;
    orbitAngle += 0.01f;
    XMMATRIX orbitModel = XMMatrixRotationY(orbitAngle) * XMMatrixTranslation(5.0f, 0.0f, 0.0f) * XMMatrixRotationY(-orbitAngle);
    XMMATRIX transposedOrbitModel = XMMatrixTranspose(orbitModel);
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &transposedOrbitModel, 0, 0);
    m_pDeviceContext->DrawIndexed(36, 0, 0);
}

void RenderClass::RenderParallelogram() {
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;

    ID3D11RasterizerState* rasterState = nullptr;
    m_pDevice->CreateRasterizerState(&rasterDesc, &rasterState);
    m_pDeviceContext->RSSetState(rasterState);

    m_pDeviceContext->OMSetDepthStencilState(m_pStateParallelogram, 0);
    m_pDeviceContext->OMSetBlendState(m_pBlendState, nullptr, 0xFFFFFFFF);

    UINT vertexStride = sizeof(ParallelogramVertex);
    UINT vertexOffset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_ParallelogramVertexBuffer, &vertexStride, &vertexOffset);
    m_pDeviceContext->IASetIndexBuffer(m_pParallelogramIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pParallelogramLayout);

    m_pDeviceContext->VSSetShader(m_pParallelogramVS, nullptr, 0);
    m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    m_pDeviceContext->PSSetShader(m_pParallelogramPS, nullptr, 0);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pColorBuffer);

    static float rotationAngle = 0.0f;
    rotationAngle += 0.015f;

    XMMATRIX transformMatrix1 = XMMatrixTranslation(sinf(rotationAngle) * 2.0f, 1.0f, -2.0f);
    XMMATRIX transposedMatrix1 = XMMatrixTranspose(transformMatrix1);
    XMFLOAT4 color1 = { 0.2f, 0.0f, 0.7f, 0.5f };

    XMMATRIX transformMatrix2 = XMMatrixTranslation(-sinf(rotationAngle) * 2.0f, 1.0f, -3.0f);
    XMMATRIX transposedMatrix2 = XMMatrixTranspose(transformMatrix2);
    XMFLOAT4 color2 = { 0.7f, 0.0f, 0.5f, 0.5f };

    XMFLOAT3 position1, position2;
    XMStoreFloat3(&position1, transformMatrix1.r[3]);
    XMStoreFloat3(&position2, transformMatrix2.r[3]);

    XMVECTOR cameraPosition = XMLoadFloat3(&m_CameraPosition);
    XMVECTOR objectPosition1 = XMLoadFloat3(&position1);
    XMVECTOR objectPosition2 = XMLoadFloat3(&position2);

    float distanceToObject1 = XMVectorGetX(XMVector3Length(XMVectorSubtract(objectPosition1, cameraPosition)));
    float distanceToObject2 = XMVectorGetX(XMVector3Length(XMVectorSubtract(objectPosition2, cameraPosition)));

    if (distanceToObject1 >= distanceToObject2) {
        DrawParallelogram(transposedMatrix1, color1);
        DrawParallelogram(transposedMatrix2, color2);
    }
    else {
        DrawParallelogram(transposedMatrix2, color2);
        DrawParallelogram(transposedMatrix1, color1);
    }

    rasterState->Release();
}

void RenderClass::DrawParallelogram(const XMMATRIX& modelMatrix, const XMFLOAT4& color) {
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &modelMatrix, 0, 0);
    m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &color, 0, 0);
    m_pDeviceContext->DrawIndexed(6, 0, 0);
}