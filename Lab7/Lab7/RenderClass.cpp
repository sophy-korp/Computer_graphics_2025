#include "framework.h"
#include "RenderClass.h"
#include "DDSTextureLoader11.h"
#include <filesystem>
#include <vector>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")


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
        hr = Init2DArray();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitSkybox();
    }

    if (SUCCEEDED(hr))
    {
        InitImGui(hWnd);
    }

    if (SUCCEEDED(hr))
    {
        hr = InitParallelogram();
    }

    if (SUCCEEDED(hr))
    {
        hr = InitFullScreenTriangle();
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
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, xyz),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, uv),     D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    HRESULT result = S_OK;

    ID3DBlob* pVertexCode = nullptr;
    if (SUCCEEDED(result))
    {
        result = CompileShader(L"ColorVertex.vs", &m_pVertexShader, nullptr, &pVertexCode);
    }
    if (SUCCEEDED(result))
    {
        result = CompileShader(L"ColorPixel.ps", nullptr, &m_pPixelShader);
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateInputLayout(layout, 3, pVertexCode->GetBufferPointer(), pVertexCode->GetBufferSize(), &m_pLayout);
    }

    if (pVertexCode)
        pVertexCode->Release();

    if (SUCCEEDED(result))
    {
        result = CompileShader(L"LightPixel.ps", nullptr, &m_pLightPixelShader);
    }

    static const Vertex vertices[] =
    {
        { {-1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f,  1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 1.0f} },
        { { 1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {1.0f, 0.0f} },
        { {-1.0f, -1.0f, -1.0f}, { 0.0f,  -1.0f,  0.0f}, {0.0f, 0.0f} },

        { {-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 1.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },

        { { 1.0f, -1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },

        { {-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f} },
        { {-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f} },
        { {-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f} },

        { { 1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f} },
        { {-1.0f, -1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f} },
        { {-1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f} },
        { { 1.0f,  1.0f,  1.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f} },

        { {-1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 1.0f} },
        { { 1.0f, -1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 1.0f} },
        { { 1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {1.0f, 0.0f} },
        { {-1.0f,  1.0f, -1.0f}, { 0.0f,  0.0f,  -1.0f}, {0.0f, 0.0f} },
    };

    WORD indices[] =
    {
        0, 2, 1,
        0, 3, 2,

        4, 6, 5,
        4, 7, 6,

        8, 10, 9,
        8, 11, 10,

        12, 14, 13,
        12, 15, 14,

        16, 18, 17,
        16, 19, 18,

        20, 22, 21,
        20, 23, 22
    };

    D3D11_BUFFER_DESC lightBufferDesc = {};
    lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDesc.ByteWidth = sizeof(PointLight) * 3;
    lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&lightBufferDesc, nullptr, &m_pLightBuffer);
    if (FAILED(result))
        return result;


    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * ARRAYSIZE(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    initData.pSysMem = indices;
    result = m_pDevice->CreateBuffer(&bd, &initData, &m_pIndexBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(XMMATRIX);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&bd, nullptr, &m_pModelBuffer);
    if (FAILED(result))
        return result;

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(InstanceData) * MaxInst;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    result = m_pDevice->CreateBuffer(&bd, nullptr, &m_pModelBufferInst);
    if (FAILED(result))
        return result;

    // Init instances cubes
    const int innerCount = 10;
    const int outerCount = 12;
    const float innerRadius = 4.0f;
    const float outerRadius = 9.5f;

    InstanceData modelBuf;
    modelBuf.model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale);
    modelBuf.texInd = 0;
    m_modelInstances.push_back(modelBuf);

    for (int i = 0; i < innerCount; i++)
    {
        float angle = XM_2PI * i / innerCount;
        XMFLOAT3 position = { innerRadius * cosf(angle), 0.0f, innerRadius * sinf(angle) };

        InstanceData modelBuf;
        modelBuf.model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
            XMMatrixTranslation(position.x, position.y, position.z);
        modelBuf.texInd = i % 2;
        m_modelInstances.push_back(modelBuf);
    }

    for (int i = 0; i < outerCount; i++)
    {
        float angle = XM_2PI * i / outerCount;
        XMFLOAT3 position = { outerRadius * cosf(angle), 0.0f, outerRadius * sinf(angle) };

        InstanceData modelBuf;
        modelBuf.model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
            XMMatrixTranslation(position.x, position.y, position.z);
        modelBuf.texInd = i % 2;
        m_modelInstances.push_back(modelBuf);
    }

    m_pDeviceContext->UpdateSubresource(m_pModelBufferInst, 0, nullptr, m_modelInstances.data(), 0, 0);

    D3D11_BUFFER_DESC vpBufferDesc = {};
    vpBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpBufferDesc.ByteWidth = sizeof(CameraBuffer);
    vpBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    result = m_pDevice->CreateBuffer(&vpBufferDesc, nullptr, &m_pVPBuffer);
    if (FAILED(result))
        return result;

    result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cube_normal.dds", nullptr, &m_pNormalMapView);
    if (FAILED(result))
        return result;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    result = m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);
    return result;
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
    vpBufferDesc.ByteWidth = sizeof(CameraBuffer);
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

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

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
    if (m_pLightBuffer) m_pLightBuffer->Release();
    if (m_pLightPixelShader) m_pLightPixelShader->Release();
    if (m_pNormalMapView) m_pNormalMapView->Release();

    if (m_pModelBufferInst) m_pModelBufferInst->Release();
    if (m_pPostProcessTexture) m_pPostProcessTexture->Release();
    if (m_pPostProcessRTV) m_pPostProcessRTV->Release();
    if (m_pPostProcessSRV) m_pPostProcessSRV->Release();
    if (m_pPostProcessVS) m_pPostProcessVS->Release();
    if (m_pPostProcessPS) m_pPostProcessPS->Release();
    if (m_pFullScreenVB) m_pFullScreenVB->Release();
    if (m_pFullScreenLayout) m_pFullScreenLayout->Release();

    m_modelInstances.clear();
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
    ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
    m_pDeviceContext->PSSetShaderResources(0, 1, nullSRVs);
    m_pDeviceContext->VSSetShaderResources(0, 1, nullSRVs);


    const float backgroundColor[4] = { 0.48f, 0.57f, 0.48f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pPostProcessRTV, backgroundColor);
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, backgroundColor);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    m_pDeviceContext->OMSetRenderTargets(1, &m_pPostProcessRTV, m_pDepthView);

    XMMATRIX rotationY = XMMatrixRotationY(m_LRAngle);
    XMMATRIX rotationX = XMMatrixRotationX(m_UDAngle);
    XMMATRIX combinedRotation = (m_CameraPosition.z <= 0)
        ? rotationY * rotationX
        : rotationX * rotationY;

    XMVECTOR cameraPosition = XMLoadFloat3(&m_CameraPosition);
    XMVECTOR forwardVector = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR lookDirection = XMVector3TransformNormal(forwardVector, combinedRotation);
    XMVECTOR lookAtPoint = XMVectorAdd(cameraPosition, XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), combinedRotation));
    XMMATRIX viewMatrix = XMMatrixLookAtLH(
        cameraPosition,
        lookAtPoint,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );

    RECT clientRect;
    GetClientRect(FindWindow(m_szWindowClass, m_szTitle), &clientRect);
    float aspect = static_cast<float>(clientRect.right - clientRect.left) / (clientRect.bottom - clientRect.top);

    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        aspect,
        0.1f,
        100.0f
    );

    RenderSkybox(projectionMatrix);
    RenderCubes(viewMatrix, projectionMatrix);
    RenderParallelogram();

    m_pDeviceContext->PSSetShaderResources(0, 1, nullSRVs);
    m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

    if (m_useNegative)
    {
        m_pDeviceContext->VSSetShader(m_pPostProcessVS, nullptr, 0);
        m_pDeviceContext->PSSetShader(m_pPostProcessPS, nullptr, 0);
        m_pDeviceContext->IASetInputLayout(m_pFullScreenLayout);

        m_pDeviceContext->PSSetShaderResources(0, 1, &m_pPostProcessSRV);
        m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

        UINT stride = sizeof(FullScreenVertex);
        UINT offset = 0;
        m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pFullScreenVB, &stride, &offset);
        m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pDeviceContext->Draw(3, 0);
    }
    else
    {
        ID3D11Resource* srcResource = nullptr;
        m_pPostProcessSRV->GetResource(&srcResource);

        ID3D11Resource* dstResource = nullptr;
        m_pRenderTargetView->GetResource(&dstResource);

        if (srcResource && dstResource)
        {
            m_pDeviceContext->CopyResource(dstResource, srcResource);
        }

        if (srcResource) srcResource->Release();
        if (dstResource) dstResource->Release();
    }

    RenderImGui();

    m_pSwapChain->Present(1, 0);
    m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_pDeviceContext->PSSetShaderResources(0, 1, nullSRVs);
}

HRESULT RenderClass::ConfigureBackBuffer(UINT width, UINT height)
{
    if (m_pPostProcessTexture) m_pPostProcessTexture->Release();
    if (m_pPostProcessRTV) m_pPostProcessRTV->Release();
    if (m_pPostProcessSRV) m_pPostProcessSRV->Release();
    if (m_pRenderTargetView) m_pRenderTargetView->Release();
    if (m_pDepthView) m_pDepthView->Release();

    m_pPostProcessTexture = nullptr;
    m_pPostProcessRTV = nullptr;
    m_pPostProcessSRV = nullptr;
    m_pRenderTargetView = nullptr;
    m_pDepthView = nullptr;

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

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pPostProcessTexture);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateRenderTargetView(m_pPostProcessTexture, nullptr, &m_pPostProcessRTV);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateShaderResourceView(m_pPostProcessTexture, nullptr, &m_pPostProcessSRV);
    if (FAILED(hr)) return hr;

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_pDeviceContext->RSSetViewports(1, &vp);

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

    if (depthStencilState) depthStencilState->Release();

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


    if (rasterizerState) {
        rasterizerState->Release();
        m_pDeviceContext->RSSetState(nullptr);
    }
}

void RenderClass::RenderCubes(XMMATRIX view, XMMATRIX proj)
{
    m_pDeviceContext->OMSetRenderTargets(1, &m_pPostProcessRTV, m_pDepthView);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    CameraBuffer cameraBuffer;
    cameraBuffer.vp = XMMatrixTranspose(view * proj);
    cameraBuffer.cameraPos = m_CameraPosition;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_pDeviceContext->Map(m_pVPBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &cameraBuffer, sizeof(CameraBuffer));
        m_pDeviceContext->Unmap(m_pVPBuffer, 0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetInputLayout(m_pLayout);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    //m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBuffer);
    m_pDeviceContext->VSSetConstantBuffers(1, 1, &m_pVPBuffer);

    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureView);
    m_pDeviceContext->PSSetShaderResources(1, 1, &m_pNormalMapView);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

    UpdateFrustum(view * proj);

    m_CubeAngle += 0.01f;
    if (m_CubeAngle > XM_2PI) m_CubeAngle -= XM_2PI;

    std::vector<InstanceData> visibleInstances;
    visibleInstances.reserve(m_modelInstances.size());
    m_visibleCubes = 0;

    for (int i = 0; i < m_modelInstances.size(); i++) {
        XMFLOAT3 position;
        XMStoreFloat3(&position, m_modelInstances[i].model.r[3]);

        m_modelInstances[i].model = XMMatrixScaling(m_fixedScale, m_fixedScale, m_fixedScale) *
            XMMatrixRotationY(m_CubeAngle) *
            XMMatrixTranslation(position.x, position.y, position.z);

        float size = m_fixedScale * 0.95f;
        if (IsAABBInFrustum(position, size))
        {
            InstanceData data;
            data.model = XMMatrixTranspose(m_modelInstances[i].model);
            data.texInd = m_modelInstances[i].texInd;
            visibleInstances.push_back(data);
            m_visibleCubes++;
        }
    }

    if (!visibleInstances.empty())
    {
        m_pDeviceContext->UpdateSubresource(
            m_pModelBufferInst,
            0,
            nullptr,
            visibleInstances.data(),
            0,
            sizeof(InstanceData) * m_visibleCubes
        );

        m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBufferInst);

        m_pDeviceContext->DrawIndexedInstanced(36, visibleInstances.size(), 0, 0, 0);
    }

    static float orbitLight = XM_PI / 2;
    orbitLight += 0.01f;
    if (orbitLight > XM_2PI) orbitLight -= XM_2PI;

    static float orbitLight2 = 0.0f;
    orbitLight2 += 0.01f;
    if (orbitLight2 > XM_2PI) orbitLight2 -= XM_2PI;

    PointLight lights[3];
    float radius = 2.0f;

    lights[0].Position = XMFLOAT3(0.0f, radius * cosf(orbitLight), radius * sinf(-orbitLight));
    lights[0].Range = 3.0f;
    lights[0].Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lights[0].Intensity = 1.0f;

    lights[1].Position = XMFLOAT3(radius * cos(orbitLight2), 0.0f, radius * sin(orbitLight2));
    lights[1].Range = 3.0f;
    lights[1].Color = XMFLOAT3(1.0f, 1.0f, 0.13f);
    lights[1].Intensity = 1.0f;

    radius = 8.0f;
    lights[2].Position = XMFLOAT3(radius * cosf(orbitLight), 0.0f, radius * sinf(-orbitLight));
    lights[2].Range = 5.0f;
    lights[2].Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
    lights[2].Intensity = 1.0f;

    D3D11_MAPPED_SUBRESOURCE mappedResourceLight;
    hr = m_pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceLight);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResourceLight.pData, lights, sizeof(PointLight) * 3);
        m_pDeviceContext->Unmap(m_pLightBuffer, 0);
    }
    m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pLightBuffer);

    m_pDeviceContext->PSSetShader(m_pLightPixelShader, nullptr, 0);
    for (int i = 0; i < 3; i++)
    {
        XMMATRIX lightModel = XMMatrixScaling(0.1f, 0.1f, 0.1f) * XMMatrixTranslation(lights[i].Position.x, lights[i].Position.y, lights[i].Position.z);
        XMMATRIX lightModelT = XMMatrixTranspose(lightModel);
        m_pDeviceContext->UpdateSubresource(m_pModelBufferInst, 0, nullptr, &lightModelT, 0, 0);
        m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pModelBufferInst);

        XMFLOAT4 lightColor = XMFLOAT4(lights[i].Color.x, lights[i].Color.y, lights[i].Color.z, 1.0f);
        m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &lightColor, 0, 0);

        m_pDeviceContext->DrawIndexed(36, 0, 0);
    }
}

struct RenderObject {
    XMMATRIX transform;
    XMFLOAT4 color;
    float minDepth; // минимальное расстояние до камеры
};

float ComputeMinDepth(const RenderObject& obj, XMVECTOR cameraPosition) {
    XMFLOAT3 vertices[4];

    XMMATRIX modelMatrix = obj.transform;

    XMVECTOR localVertices[4] = {
        XMVectorSet(-0.75f, -0.75f, 0.0f, 1.0f),
        XMVectorSet(-0.75f,  0.75f, 0.0f, 1.0f),
        XMVectorSet(0.75f,  0.75f, 0.0f, 1.0f),
        XMVectorSet(0.75f, -0.75f, 0.0f, 1.0f),
    };

    float minDepth = FLT_MAX;

    for (int i = 0; i < 4; ++i) {
        XMVECTOR worldPos = XMVector3Transform(localVertices[i], modelMatrix);
        XMVECTOR diff = worldPos - cameraPosition;
        float distance = XMVectorGetX(XMVector3Length(diff));
        if (distance < minDepth) {
            minDepth = distance;
        }
    }

    return minDepth;
}

void RenderClass::RenderParallelogram() {
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;

    ID3D11RasterizerState* rasterState = nullptr;
    m_pDevice->CreateRasterizerState(&rasterDesc, &rasterState);
    m_pDeviceContext->RSSetState(rasterState);

    if (rasterState) rasterState->Release();

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

    std::vector<RenderObject> parallelograms = {
        { XMMatrixTranslation(sinf(rotationAngle) * 2.0f, 1.0f, -2.0f), { 0.2f, 0.0f, 0.7f, 0.5f } },
        { XMMatrixTranslation(-sinf(rotationAngle) * 2.0f, 1.0f, -3.0f), { 0.7f, 0.0f, 0.5f, 0.5f } }
    };

    XMVECTOR cameraPosition = XMLoadFloat3(&m_CameraPosition);

    for (auto& obj : parallelograms) {
        obj.minDepth = ComputeMinDepth(obj, cameraPosition);
    }

    std::sort(parallelograms.begin(), parallelograms.end(), [](const RenderObject& a, const RenderObject& b) {
        return a.minDepth > b.minDepth;
        });

    for (const auto& obj : parallelograms) {
        DrawParallelogram(XMMatrixTranspose(obj.transform), obj.color);
    }

}

void RenderClass::DrawParallelogram(const XMMATRIX& modelMatrix, const XMFLOAT4& color) {
    m_pDeviceContext->UpdateSubresource(m_pModelBuffer, 0, nullptr, &modelMatrix, 0, 0);
    m_pDeviceContext->UpdateSubresource(m_pColorBuffer, 0, nullptr, &color, 0, 0);
    m_pDeviceContext->DrawIndexed(6, 0, 0);
}

HRESULT RenderClass::Init2DArray()
{
    ID3D11Resource* textureResources[2] = { nullptr, nullptr };

    HRESULT hr = DirectX::CreateDDSTextureFromFile(m_pDevice, L"cat.dds", &textureResources[0], nullptr);
    if (FAILED(hr)) return hr;

    hr = DirectX::CreateDDSTextureFromFile(m_pDevice, L"textile.dds", &textureResources[1], nullptr);
    if (FAILED(hr)) {
        textureResources[0]->Release();
        return hr;
    }

    ID3D11Texture2D* tempTexture = nullptr;
    textureResources[0]->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tempTexture));

    D3D11_TEXTURE2D_DESC textureDesc;
    tempTexture->GetDesc(&textureDesc);
    tempTexture->Release();

    D3D11_TEXTURE2D_DESC arrayTextureDesc = textureDesc;
    arrayTextureDesc.ArraySize = 2;

    ID3D11Texture2D* textureArray = nullptr;
    hr = m_pDevice->CreateTexture2D(&arrayTextureDesc, nullptr, &textureArray);
    if (FAILED(hr)) {
        textureResources[0]->Release();
        textureResources[1]->Release();
        return hr;
    }

    for (UINT i = 0; i < 2; ++i) {
        for (UINT mip = 0; mip < textureDesc.MipLevels; ++mip) {
            if (textureResources[i]) {
                m_pDeviceContext->CopySubresourceRegion(
                    textureArray,
                    D3D11CalcSubresource(mip, i, textureDesc.MipLevels),
                    0, 0, 0,
                    textureResources[i],
                    mip,
                    nullptr
                );
            }
            else {
                OutputDebugString(L"Error: Null texture resource encountered\n");
            }
        }
    }
    textureResources[0]->Release();
    textureResources[1]->Release();

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.ArraySize = 2;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MipLevels = textureDesc.MipLevels;
    srvDesc.Texture2DArray.MostDetailedMip = 0;

    hr = m_pDevice->CreateShaderResourceView(textureArray, &srvDesc, &m_pTextureView);
    textureArray->Release();
    return hr;
}

HRESULT RenderClass::InitFullScreenTriangle()
{
    FullScreenVertex vertices[3] = {
        { -1.0f, -1.0f, 0, 1,   0.0f, 1.0f },
        { -1.0f,  3.0f, 0, 1,   0.0f,-1.0f },
        {  3.0f, -1.0f, 0, 1,   2.0f, 1.0f }
    };

    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(vertices);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = vertices;

    HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_pFullScreenVB);
    if (FAILED(hr)) return hr;

    // Описание входного макета
    D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    ID3DBlob* vertexShaderBlob = nullptr;
    hr = CompileShader(L"NegativeVertex.vs", &m_pPostProcessVS, nullptr, &vertexShaderBlob);
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateInputLayout(
        inputLayoutDesc, 2, vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(), &m_pFullScreenLayout
    );
    vertexShaderBlob->Release();
    if (FAILED(hr)) return hr;

    return CompileShader(L"NegativePixel.ps", nullptr, &m_pPostProcessPS);
}

void RenderClass::UpdateFrustum(const XMMATRIX& viewProjMatrix)
{
    XMFLOAT4X4 matViewProj;
    XMStoreFloat4x4(&matViewProj, viewProjMatrix);

    m_frustumPlanes[0] = XMVectorSet(
        matViewProj._14 + matViewProj._11,
        matViewProj._24 + matViewProj._21,
        matViewProj._34 + matViewProj._31,
        matViewProj._44 + matViewProj._41
    );

    m_frustumPlanes[1] = XMVectorSet(
        matViewProj._14 - matViewProj._11,
        matViewProj._24 - matViewProj._21,
        matViewProj._34 - matViewProj._31,
        matViewProj._44 - matViewProj._41
    );

    m_frustumPlanes[2] = XMVectorSet(
        matViewProj._14 + matViewProj._12,
        matViewProj._24 + matViewProj._22,
        matViewProj._34 + matViewProj._32,
        matViewProj._44 + matViewProj._42
    );

    m_frustumPlanes[3] = XMVectorSet(
        matViewProj._14 - matViewProj._12,
        matViewProj._24 - matViewProj._22,
        matViewProj._34 - matViewProj._32,
        matViewProj._44 - matViewProj._42
    );

    m_frustumPlanes[4] = XMVectorSet(
        matViewProj._13,
        matViewProj._23,
        matViewProj._33,
        matViewProj._43
    );

    m_frustumPlanes[5] = XMVectorSet(
        matViewProj._14 - matViewProj._13,
        matViewProj._24 - matViewProj._23,
        matViewProj._34 - matViewProj._33,
        matViewProj._44 - matViewProj._43
    );

    for (auto& plane : m_frustumPlanes)
    {
        plane = XMPlaneNormalize(plane);
    }
}

bool RenderClass::IsAABBInFrustum(const XMFLOAT3& center, float boundingRadius) const
{
    for (const auto& plane : m_frustumPlanes)
    {
        float distance = XMVectorGetX(XMPlaneDotCoord(plane, XMLoadFloat3(&center)));
        float radiusOffset = boundingRadius * (
            fabs(XMVectorGetX(plane)) +
            fabs(XMVectorGetY(plane)) +
            fabs(XMVectorGetZ(plane))
            );

        if (distance + radiusOffset < 0)
        {
            return false;
        }
    }
    return true;
}

void RenderClass::InitImGui(HWND hWnd)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);
}

void RenderClass::RenderImGui()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Options");
    ImGui::Checkbox("Negative Effect", &m_useNegative);
    ImGui::End();

    ImGui::Begin("Frustum Culling Info");
    ImGui::Text("Total Cubes: %d", MaxInst);
    ImGui::Text("Visible Cubes: %d", m_visibleCubes);
    ImGui::Text("Culled Cubes: %d", MaxInst - m_visibleCubes);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}