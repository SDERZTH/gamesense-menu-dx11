// Dear ImGui draw-list renderer. Coordinates and ARGB colors are those of the
// original menu; the pixel font atlas remains GDI-generated for visual parity.
#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using D3DCOLOR = uint32_t;
static ID3D11Device* dev;
static ID3D11DeviceContext* deviceContext;
static IDXGISwapChain* swapChain;
static ID3D11RenderTargetView* renderTarget;
static ID3D11SamplerState* wrapSampler;
static ImDrawList* drawList;
static int bbW = 669, bbH = 564, pendW = 0, pendH = 0;
static bool softwareRenderer = false;
template<class T> static void releaseCOM(T*& p) { if (p) { p->Release(); p = nullptr; } }

static D3DCOLOR rgb(int r, int g, int b, int a = 255) { return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
static D3DCOLOR gray(int v, int a = 255) { return rgb(v, v, v, a); }
static float globalAlpha = 1.f;
static D3DCOLOR fa(D3DCOLOR c) { return globalAlpha >= 1.f ? c : (c & 0xffffff) | ((D3DCOLOR)((c >> 24) * globalAlpha) << 24); }
static ImU32 imguiColor(D3DCOLOR c) { return (c & 0xff00ff00) | ((c & 0xff) << 16) | ((c >> 16) & 0xff); }
static void quad(float x, float y, float w, float h, D3DCOLOR c, ID3D11ShaderResourceView* texture = nullptr,
                 float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
    if (w <= 0 || h <= 0) return;
    ImU32 col = imguiColor(fa(c));
    if (texture) drawList->AddImage(ImTextureRef((ImTextureID)(uintptr_t)texture), ImVec2(x, y), ImVec2(x + w, y + h), ImVec2(u0, v0), ImVec2(u1, v1), col);
    else drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col);
}
static void rect(int x, int y, int w, int h, D3DCOLOR c) { quad((float)x, (float)y, (float)w, (float)h, c); }
static void outline(int x, int y, int w, int h, D3DCOLOR c) {
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c); rect(x, y + 1, 1, h - 2, c); rect(x + w - 1, y + 1, 1, h - 2, c);
}
static void gradientQuad(int x, int y, int w, int h, ImU32 tl, ImU32 tr, ImU32 bl, ImU32 br) {
    // Match the original triangle strip's diagonal, including interpolation at
    // half-color boundaries. ImGui's regular multicolor rectangle uses the other diagonal.
    ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    drawList->PrimReserve(6, 4);
    unsigned idx = drawList->_VtxCurrentIdx;
    drawList->PrimWriteIdx((ImDrawIdx)idx); drawList->PrimWriteIdx((ImDrawIdx)(idx + 1)); drawList->PrimWriteIdx((ImDrawIdx)(idx + 2));
    drawList->PrimWriteIdx((ImDrawIdx)(idx + 2)); drawList->PrimWriteIdx((ImDrawIdx)(idx + 1)); drawList->PrimWriteIdx((ImDrawIdx)(idx + 3));
    drawList->PrimWriteVtx(ImVec2((float)x, (float)y), uv, tl);
    drawList->PrimWriteVtx(ImVec2((float)(x + w), (float)y), uv, tr);
    drawList->PrimWriteVtx(ImVec2((float)x, (float)(y + h)), uv, bl);
    drawList->PrimWriteVtx(ImVec2((float)(x + w), (float)(y + h)), uv, br);
}
static void gradV(int x, int y, int w, int h, D3DCOLOR top, D3DCOLOR bottom) {
    if (w <= 0 || h <= 0) return;
    ImU32 t = imguiColor(fa(top)), b = imguiColor(fa(bottom));
    gradientQuad(x, y, w, h, t, t, b, b);
}
static void gradH(int x, int y, int w, int h, D3DCOLOR left, D3DCOLOR right) {
    if (w <= 0 || h <= 0) return;
    ImU32 l = imguiColor(fa(left)), r = imguiColor(fa(right));
    gradientQuad(x, y, w, h, l, r, l, r);
}
static ID3D11ShaderResourceView* makeTex(int w, int h, const uint32_t* bgra) {
    if (w <= 0 || h <= 0) return nullptr;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w; desc.Height = h; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{bgra, (UINT)w * 4, 0};
    ID3D11Texture2D* texture = nullptr; ID3D11ShaderResourceView* view = nullptr;
    if (SUCCEEDED(dev->CreateTexture2D(&desc, &data, &texture))) dev->CreateShaderResourceView(texture, nullptr, &view);
    releaseCOM(texture);
    return view;
}
static void setWrapSampler(const ImDrawList*, const ImDrawCmd*) { deviceContext->PSSetSamplers(0, 1, &wrapSampler); }

static bool createRenderTarget() {
    ID3D11Texture2D* buffer = nullptr;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer));
    if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(buffer, nullptr, &renderTarget);
    releaseCOM(buffer);
    return SUCCEEDED(hr);
}
static bool createDevice(HWND hwnd, bool warp) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, levels, 2, D3D11_SDK_VERSION, &desc, &swapChain, &dev, nullptr, &deviceContext);
    if (FAILED(hr) && !warp) return createDevice(hwnd, true);
    if (FAILED(hr)) return false;
    softwareRenderer = warp;
    D3D11_SAMPLER_DESC sampler{}; sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS; sampler.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(dev->CreateSamplerState(&sampler, &wrapSampler)) && createRenderTarget();
}
static bool resetDevice() {
    if (pendW <= 0 || pendH <= 0 || (pendW == bbW && pendH == bbH)) return true;
    deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    releaseCOM(renderTarget);
    HRESULT hr = swapChain->ResizeBuffers(0, pendW, pendH, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr) || !createRenderTarget()) return false;
    bbW = pendW; bbH = pendH; pendW = pendH = 0;
    return true;
}
static void cleanupDevice() {
    if (deviceContext) deviceContext->ClearState();
    releaseCOM(wrapSampler); releaseCOM(renderTarget); releaseCOM(swapChain); releaseCOM(deviceContext); releaseCOM(dev);
}
static std::wstring widePath(const char* path) {
    int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, out.data(), n);
    return out;
}
static bool saveBmp(const char* path) {
    ID3D11Texture2D *buffer = nullptr, *staging = nullptr;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer));
    D3D11_TEXTURE2D_DESC desc{};
    if (SUCCEEDED(hr)) {
        buffer->GetDesc(&desc); desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = desc.MiscFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr = dev->CreateTexture2D(&desc, nullptr, &staging);
    }
    if (SUCCEEDED(hr)) deviceContext->CopyResource(staging, buffer);
    D3D11_MAPPED_SUBRESOURCE map{};
    if (SUCCEEDED(hr)) hr = deviceContext->Map(staging, 0, D3D11_MAP_READ, 0, &map);
    bool ok = false;
    if (SUCCEEDED(hr)) {
        FILE* f = _wfopen(widePath(path).c_str(), L"wb");
        if (f) {
            BITMAPFILEHEADER fh{0x4d42, (DWORD)(54 + desc.Width * desc.Height * 4), 0, 0, 54};
            BITMAPINFOHEADER ih{}; ih.biSize = sizeof(ih); ih.biWidth = (LONG)desc.Width; ih.biHeight = -(LONG)desc.Height; ih.biPlanes = 1; ih.biBitCount = 32;
            ok = fwrite(&fh, sizeof(fh), 1, f) == 1 && fwrite(&ih, sizeof(ih), 1, f) == 1;
            std::vector<uint8_t> row(desc.Width * 4);
            for (UINT y = 0; y < desc.Height; ++y) {
                const uint8_t* src = (const uint8_t*)map.pData + y * map.RowPitch;
                for (UINT x = 0; x < desc.Width; ++x) { row[x * 4] = src[x * 4 + 2]; row[x * 4 + 1] = src[x * 4 + 1]; row[x * 4 + 2] = src[x * 4]; row[x * 4 + 3] = 255; }
                ok = fwrite(row.data(), row.size(), 1, f) == 1 && ok;
            }
            ok = fclose(f) == 0 && ok;
        }
        deviceContext->Unmap(staging, 0);
    }
    releaseCOM(staging); releaseCOM(buffer);
    return ok;
}
