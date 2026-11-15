// Gamesense menu rebuild - Dear ImGui / Direct3D 11, original pixel-matched appearance.
// Text: GDI ClearType-natural glyphs rendered one at a time, alpha = gamesense's (54R+183G+19B)>>8 of the RGB
// coverage, glyph cell clipped to abcB (matches the reference capture to +-1 per channel).
// Run: gsmenu.exe [--capture out.bmp]   (test hooks: GS_EVENTS input replay, GS_RESIZE backbuffer size)
// Build: build.bat. Regression check: node diff.js out.bmp ref/misc.bgra
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <algorithm>

#include <wincodec.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <map>
#include <cmath>
#include <string>
#include "renderer_dx11.h"
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

static const int WW = 669, WH = 564;

// DPI: "DPI scale" index -> factor table 0x434745cc {1, 1.25, 1.5, 1.75, 2}, current factor DAT_4346d490.
// Every scaled constant goes through FUN_433cc0d6 = cvtss2si(v * dpi): round to nearest, ties to even.
static const float DPI_TABLE[5] = {1.f, 1.25f, 1.5f, 1.75f, 2.f};
static float dpi = 1.f;
static int S(float v) { return (int)lrintf(v * dpi); }

// Drawing and texture creation are provided by renderer_dx11.h.
struct Image { ID3D11ShaderResourceView* tex = nullptr; int w = 0, h = 0; };
#include "assets_data.h"
static Image loadPng(const char* name) {  // decodes an embedded PNG (assets_data.h) - nothing is read from disk
    Image im;
    const EmbeddedAsset* a = nullptr;
    for (auto& e : EMBEDDED_ASSETS) if (!strcmp(e.name, name)) a = &e;
    if (!a) { fprintf(stderr, "missing embedded asset %s\n", name); return im; }
    IWICImagingFactory* f = nullptr; IWICStream* st = nullptr; IWICBitmapDecoder* d = nullptr; IWICBitmapFrameDecode* fr = nullptr; IWICFormatConverter* cv = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
    if (f && SUCCEEDED(f->CreateStream(&st)) && SUCCEEDED(st->InitializeFromMemory((BYTE*)a->data, a->size)) &&
        SUCCEEDED(f->CreateDecoderFromStream(st, nullptr, WICDecodeMetadataCacheOnLoad, &d)) &&
        SUCCEEDED(d->GetFrame(0, &fr)) && SUCCEEDED(f->CreateFormatConverter(&cv)) &&
        SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
        UINT w, h; cv->GetSize(&w, &h);
        std::vector<uint32_t> px(w * h);
        cv->CopyPixels(nullptr, w * 4, w * h * 4, (BYTE*)px.data());
        im.tex = makeTex(w, h, px.data()); im.w = w; im.h = h;
    } else {
        fprintf(stderr, "bad embedded asset %s\n", name);
    }
    if (cv) cv->Release(); if (fr) fr->Release(); if (d) d->Release(); if (st) st->Release(); if (f) f->Release();
    return im;
}

// 4x4 dither tile used for the window background, phase is absolute to the window origin
static ID3D11ShaderResourceView* bgTile;
static int bgOX = 0, bgOY = 0;  // pattern origin = menu position (texture is locked to the menu)
static void texBg(int x, int y, int w, int h) {
    drawList->AddCallback(setWrapSampler, nullptr);
    quad((float)x, (float)y, (float)w, (float)h, 0xffffffff, bgTile, (x - bgOX) / 4.f, (y - bgOY) / 4.f, (x - bgOX + w) / 4.f, (y - bgOY + h) / 4.f);
    drawList->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
}

// ---------------------------------------------------------------- fonts
// ClearType coverage -> alpha. GDI emits 7 levels per channel on the capture machine; those use gamesense's
// FUN_4332f22f weighting, any other level (other ClearType tuning) falls back to Rec.709 luma.
static int ctAlpha(int r, int g, int b) {
    static const int LEVELS[7] = {0, 58, 102, 144, 182, 219, 255};
    auto known = [](int v) { for (int l : LEVELS) if (l == v) return true; return false; };
    if (!known(r) || !known(g) || !known(b)) return (int)(0.2126f * r + 0.7152f * g + 0.0722f * b);
    return (54 * r + 183 * g + 19 * b) >> 8;  // gamesense FUN_4332f22f
}
enum { FX_NONE = 0, FX_SHADOW = 1, FX_OUTLINE = 2 };
struct Glyph { ID3D11ShaderResourceView* tex; int a, b, c, w, h, ox, oy; };
struct Font {
    HFONT hf = nullptr; HDC dc = nullptr; HBITMAP bm = nullptr; HGDIOBJ oldFont = nullptr, oldBitmap = nullptr;
    uint32_t* bits = nullptr; int quality = 0, fx = 0, outlineAlpha = 0, height = 0;
    std::map<wchar_t, Glyph> cache;
    static const int BW = 96, BH = 48, PAD = 16;
    void release() {  // DPI change: the renderer rebuilds its fonts (vtable +0x80 from 0x4339f0ad)
        if (!dc) return;
        for (auto& [ch, g] : cache) if (g.tex) g.tex->Release();
        cache.clear();
        SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
        DeleteObject(hf); DeleteObject(bm); DeleteDC(dc); dc = nullptr; hf = nullptr; bm = nullptr; bits = nullptr;
    }
    void init(const char* face, int h, int weight, int q, int effects, int olA = 0) {
        release();
        quality = q; fx = effects; outlineAlpha = olA;
        dc = CreateCompatibleDC(nullptr);
        BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), BW, -BH, 1, 32}};
        bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
        oldBitmap = SelectObject(dc, bm);
        // mirrors gamesense FUN_4334e194: LOGFONTW zeroed, lfHeight = -size (em px), weight 400/700,
        // quality 3 (non-AA) or 6 (ClearType natural), everything else 0 (ANSI charset, default precision/pitch)
        LOGFONTW lf = {};
        lf.lfHeight = -h; lf.lfWeight = weight; lf.lfQuality = (BYTE)q;
        MultiByteToWideChar(CP_ACP, 0, face, -1, lf.lfFaceName, 32);
        hf = CreateFontIndirectW(&lf);
        SetMapMode(dc, MM_TEXT); SetTextAlign(dc, TA_TOP | TA_LEFT);
        oldFont = SelectObject(dc, hf);
        SetTextColor(dc, 0xffffff); SetBkMode(dc, TRANSPARENT);
        TEXTMETRICA tm; GetTextMetricsA(dc, &tm); height = tm.tmHeight;
    }
    const Glyph& get(wchar_t ch) {
        auto it = cache.find(ch);
        if (it != cache.end()) return it->second;
        ABC abc; GetCharABCWidthsW(dc, ch, ch, &abc);
        memset(bits, 0, BW * BH * 4);
        ExtTextOutW(dc, PAD - abc.abcA, 0, 0, nullptr, &ch, 1, nullptr);
        GdiFlush();
        const int fr = 0;  // glyph cell = abcB exactly (GS keeps no ClearType fringe outside it)
        int gw = abc.abcB + 2 * fr, gh = height, e = fx ? 1 : 0;
        int tw = gw + 2 * e, th = gh + 2 * e;  // shadow/outline grow the cell by 1px
        std::vector<int> al(tw * th, 0);
        for (int y = 0; y < gh; y++)
            for (int x = 0; x < gw; x++) {
                uint32_t p = bits[y * BW + PAD - fr + x];
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                al[(y + e) * tw + x + e] = quality == NONANTIALIASED_QUALITY ? (g ? 255 : 0) : ctAlpha(r, g, b);
            }
        std::vector<uint32_t> px(tw * th);
        for (int i = 0; i < tw * th; i++) px[i] = al[i] ? (al[i] << 24) | 0xffffff : 0;
        // the scaled (ClearType) hotkey font shows its effect as a +1,+1 shadow weighted by coverage, not the flat
        // 8-neighbour outline the 100% bitmap font has (pixel-checked against a GS 125% capture)
        const int eff = (fx == FX_OUTLINE && quality != NONANTIALIASED_QUALITY) ? FX_SHADOW : fx;
        if (eff == FX_SHADOW)  // Source-style: empty pixel takes alpha of (x-1,y-1), colored black
            for (int y = th - 1; y >= 1; y--)
                for (int x = tw - 1; x >= 1; x--)
                    if (!al[y * tw + x] && al[(y - 1) * tw + x - 1]) px[y * tw + x] = (uint32_t)al[(y - 1) * tw + x - 1] << 24;
        if (eff == FX_OUTLINE)
            for (int y = 0; y < th; y++)
                for (int x = 0; x < tw; x++) {
                    if (al[y * tw + x]) continue;
                    bool any = false;  // FUN_4332f22f flag 4: empty pixel next to any glyph pixel -> 0xDC000000 (flat)
                    for (int dy = -1; dy <= 1 && !any; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int X = x + dx, Y = y + dy;
                            if (X >= 0 && Y >= 0 && X < tw && Y < th && al[Y * tw + X]) { any = true; break; }
                        }
                    if (any) px[y * tw + x] = (uint32_t)outlineAlpha << 24;
                }
        Glyph g{gw > 0 ? makeTex(tw, th, px.data()) : nullptr, abc.abcA, (int)abc.abcB, abc.abcC, tw, th, e + fr, e};
        if (eff == FX_SHADOW) { g.ox = fr; g.oy = 0; }  // shadow only grows right/bottom
        return cache[ch] = g;
    }
    int width(const std::wstring& s) { int w = 0; for (wchar_t c : s) { auto& g = get(c); w += g.a + g.b + g.c; } return w; }
    void draw(int x, int y, const std::wstring& s, D3DCOLOR col, int limit = 0x7fffffff) {
        int pen = x;
        for (wchar_t c : s) {
            auto& g = get(c);
            if (limit != 0x7fffffff && pen + g.a + g.b >= limit) break;
            if (g.tex) quad((float)(pen + g.a - g.ox), (float)(y - g.oy), (float)g.w, (float)g.h, col, g.tex);
            pen += g.a + g.b + g.c;
        }
    }
};
static Font fNormal, fBold, fSmall, fValue;

// ---------------------------------------------------------------- widgets (geometry measured from ref captures)
struct Accent { int r, g, b; } accent = {158, 202, 42};  // menu color; widgets offset it (fit on GPU: centers-interpolated quads)

#include "ui.h"

// ---------------------------------------------------------------- menu contents: generated from the image's builder (0x4348704a)
static Menu menu;
static void resetMenuLayout() {  // groupbox positions/sizes only; menu size and DPI untouched
    for (Tab* t : menu.tabs) for (Child* c : t->children) c->resetLayout();
    menu.relayout();
}
static void initFonts() {  // renderer font slots; pixel size = (int)(em size * dpi + 0.5) (FUN_4334e194)
    auto px = [](float size) { return (int)(size * dpi + 0.5f); };
    fNormal.init("Verdana", px(9), FW_NORMAL, CLEARTYPE_NATURAL_QUALITY, FX_NONE);
    fBold.init("Verdana", px(9), FW_BOLD, CLEARTYPE_NATURAL_QUALITY, FX_NONE);
    // Hotkey slot: text picks the unscaled set (backend +0x108) at 100% and the rebuilt scaled set (+0x15C) otherwise
    // (0x434172EC). Measured: the unscaled set has the bitmap Small Fonts 7 (exact vs ref), the scaled one ClearType
    // Verdana at (int)(7 * dpi + 0.5) (glyph values match a GS 125% capture to +-2).
    if (dpi == 1.f) fSmall.init("Small Fonts", 7, FW_NORMAL, NONANTIALIASED_QUALITY, FX_OUTLINE, 0xDC);
    else fSmall.init("Verdana", px(7), FW_NORMAL, CLEARTYPE_NATURAL_QUALITY, FX_OUTLINE, 0xDC);
    fValue.init("Verdana", px(9), FW_BOLD, CLEARTYPE_NATURAL_QUALITY, FX_NONE);  // font slot 1 (slider values, flags 0x31): fits ref exactly
}
// "DPI scale" changed (callback 0x433379d4 -> 0x4339f0ad -> per menu 0x433b23fc): new factor, fonts rebuilt, menu size reset to
// S(default), width clamps S(40/200/100), every tab/groupbox/element rescaled (HandleDPI), tabs restacked from y = 20.
static void applyDpi(int step) {
    if (step < 0 || step > 4) step = 0;
    dpiStep = step; dpi = DPI_TABLE[step];
    initFonts();
    menu.minW = menu.sw = S(WW); menu.minH = menu.sh = S(WH);
    int ty = 20;
    for (Tab* t : menu.tabs) { t->HandleDPI(); t->x = t->lp + 6; t->y = ty; ty += t->h; }
    menu.relayout();
}
#include "menu_data.h"
#include "menu_rules.h"
static void buildMenu() {
    for (int i = 0; i < 8; i++) menu.tabs.push_back(new Tab(i, &menu, 6, 20 + 64 * i));
    buildMenuData();
    applyRules();
    E[376]->visible = true;  // shortcut: "Fix animations" is hidden by the builder but shown in the real menu (ref); whatever unhides it at runtime is not found yet
    applyDpi(0);  // first pass at 100% records every element's default size
    on(360, [] { applyDpi(cov(360)); });  // "DPI scale" combo (value at 0x43475a94)
    if (cov(360)) applyDpi(cov(360));
}

// ---------------------------------------------------------------- app
// GS_EVENTS="down:x,y;up:x,y;rdown:x,y;rup:x,y;move:x,y;key:vk;wheel:x,y,delta[,ctrl]" - replay input through the menu before --capture
static void replayEvents() {
    const char* e = getenv("GS_EVENTS"); if (!e) return;
    std::string all(e); size_t p = 0; WPARAM held = 0;
    while (p < all.size()) {
        size_t q = all.find(";", p); std::string t = all.substr(p, q == std::string::npos ? std::string::npos : q - p); p = q == std::string::npos ? all.size() : q + 1;
        int a = 0, b = 0, c = 0, d = 0; char kind[16] = {}; sscanf(t.c_str(), "%15[a-z]:%d,%d,%d,%d", kind, &a, &b, &c, &d);
        std::string k(kind);
        if (k == "down") { held |= MK_LBUTTON; menu.handle({WM_LBUTTONDOWN, held, MAKELPARAM(a, b)}); }
        else if (k == "up") { held &= ~(WPARAM)MK_LBUTTON; menu.handle({WM_LBUTTONUP, held, MAKELPARAM(a, b)}); }
        else if (k == "move") menu.handle({WM_MOUSEMOVE, held, MAKELPARAM(a, b)});
        else if (k == "rdown") { held |= MK_RBUTTON; menu.handle({WM_RBUTTONDOWN, held, MAKELPARAM(a, b)}); }
        else if (k == "rup") { held &= ~(WPARAM)MK_RBUTTON; menu.handle({WM_RBUTTONUP, held, MAKELPARAM(a, b)}); }
        else if (k == "key") menu.handle({WM_KEYDOWN, (WPARAM)a, 0});
        else if (k == "wheel") { POINT pt{a, b}; ClientToScreen(menu.hwnd, &pt); menu.handle({WM_MOUSEWHEEL, MAKEWPARAM(d ? MK_CONTROL : 0, (short)c), MAKELPARAM(pt.x, pt.y)}); }  // wheel lParam is in screen coords
    }
}
#include "app_dx11.h"
