// Win32 host and Dear ImGui frame/input lifecycle for the original custom widgets.
// Stock ImGui controls would alter the appearance; these widgets intentionally
// retain the original measured geometry, glyphs, rendering and configuration rules.
#pragma once
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static int virtualKey(ImGuiKey key) {
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z) return 'A' + key - ImGuiKey_A;
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) return '0' + key - ImGuiKey_0;
    if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) return VK_F1 + key - ImGuiKey_F1;
    if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) return VK_NUMPAD0 + key - ImGuiKey_Keypad0;
    switch (key) {
    case ImGuiKey_Tab: return VK_TAB; case ImGuiKey_LeftArrow: return VK_LEFT; case ImGuiKey_RightArrow: return VK_RIGHT;
    case ImGuiKey_UpArrow: return VK_UP; case ImGuiKey_DownArrow: return VK_DOWN; case ImGuiKey_PageUp: return VK_PRIOR;
    case ImGuiKey_PageDown: return VK_NEXT; case ImGuiKey_Home: return VK_HOME; case ImGuiKey_End: return VK_END;
    case ImGuiKey_Insert: return VK_INSERT; case ImGuiKey_Delete: return VK_DELETE; case ImGuiKey_Backspace: return VK_BACK;
    case ImGuiKey_Space: return VK_SPACE; case ImGuiKey_Enter: return VK_RETURN; case ImGuiKey_Escape: return VK_ESCAPE;
    case ImGuiKey_LeftCtrl: return VK_LCONTROL; case ImGuiKey_RightCtrl: return VK_RCONTROL;
    case ImGuiKey_LeftShift: return VK_LSHIFT; case ImGuiKey_RightShift: return VK_RSHIFT;
    case ImGuiKey_LeftAlt: return VK_LMENU; case ImGuiKey_RightAlt: return VK_RMENU;
    case ImGuiKey_LeftSuper: return VK_LWIN; case ImGuiKey_RightSuper: return VK_RWIN; case ImGuiKey_Menu: return VK_APPS;
    case ImGuiKey_Apostrophe: return VK_OEM_7; case ImGuiKey_Comma: return VK_OEM_COMMA;
    case ImGuiKey_Minus: return VK_OEM_MINUS; case ImGuiKey_Period: return VK_OEM_PERIOD; case ImGuiKey_Slash: return VK_OEM_2;
    case ImGuiKey_Semicolon: return VK_OEM_1; case ImGuiKey_Equal: return VK_OEM_PLUS;
    case ImGuiKey_LeftBracket: return VK_OEM_4; case ImGuiKey_Backslash: return VK_OEM_5;
    case ImGuiKey_RightBracket: return VK_OEM_6; case ImGuiKey_GraveAccent: return VK_OEM_3;
    case ImGuiKey_CapsLock: return VK_CAPITAL; case ImGuiKey_ScrollLock: return VK_SCROLL; case ImGuiKey_NumLock: return VK_NUMLOCK;
    case ImGuiKey_PrintScreen: return VK_SNAPSHOT; case ImGuiKey_Pause: return VK_PAUSE;
    case ImGuiKey_KeypadDecimal: return VK_DECIMAL; case ImGuiKey_KeypadDivide: return VK_DIVIDE;
    case ImGuiKey_KeypadMultiply: return VK_MULTIPLY; case ImGuiKey_KeypadSubtract: return VK_SUBTRACT;
    case ImGuiKey_KeypadAdd: return VK_ADD; case ImGuiKey_KeypadEnter: return VK_RETURN;
    default: return 0;
    }
}
static void dispatchImGuiInput() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.AppFocusLost) {
        menu.buttons = 0; menu.status &= ~0xcu;
        for (Tab* t : menu.tabs) t->ResetInputState();
        return;
    }
    bool binding = false;
    if (auto* hotkey = dynamic_cast<Hotkey*>(focused)) binding = hotkey->waiting;
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        int vk = virtualKey((ImGuiKey)k);
        if (!vk || !ImGui::IsKeyPressed((ImGuiKey)k)) continue;
        if (!binding && vk == (int)((Hotkey*)E[357])->vk) menu.toggle();
        else menu.handle({WM_KEYDOWN, (WPARAM)vk, 0});
    }
    for (ImWchar ch : io.InputQueueCharacters) menu.handle({WM_CHAR, ch, 0});
    if (io.MousePos.x < -10000 || io.MousePos.y < -10000) return;
    int x = (int)io.MousePos.x, y = (int)io.MousePos.y;
    WPARAM held = (io.MouseDown[0] ? MK_LBUTTON : 0) | (io.MouseDown[1] ? MK_RBUTTON : 0) |
                   (io.MouseDown[2] ? MK_MBUTTON : 0) | (io.KeyCtrl ? MK_CONTROL : 0) | (io.KeyShift ? MK_SHIFT : 0);
    LPARAM pos = MAKELPARAM(x, y);
    const UINT down[] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_XBUTTONDOWN, WM_XBUTTONDOWN};
    const UINT up[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP, WM_XBUTTONUP};
    for (int b = 0; b < 5; ++b) {
        WPARAM w = b >= 3 ? MAKEWPARAM(held, b == 3 ? XBUTTON1 : XBUTTON2) : held;
        if (ImGui::IsMouseClicked(b)) menu.handle({down[b], w, pos});
        if (ImGui::IsMouseReleased(b)) menu.handle({up[b], w, pos});
    }
    if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) menu.handle({WM_MOUSEMOVE, held, pos});
    if (io.MouseWheel != 0) {
        POINT pt{x, y}; ClientToScreen(menu.hwnd, &pt);
        menu.handle({WM_MOUSEWHEEL, MAKEWPARAM(held, (short)(io.MouseWheel * WHEEL_DELTA)), MAKELPARAM(pt.x, pt.y)});
    }
}
static void render(bool interactive = true) {
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    if (interactive) dispatchImGuiInput();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)bbW, (float)bbH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##gamesense", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    drawList = ImGui::GetWindowDrawList();
    drawList->Flags &= ~(ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedFill);
    menu.render();
    ImGui::End(); ImGui::PopStyleVar(2); ImGui::Render();
    const float clear[4] = {0, 0, 0, 1};
    deviceContext->OMSetRenderTargets(1, &renderTarget, nullptr);
    deviceContext->ClearRenderTargetView(renderTarget, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    if (m == WM_SIZE && w != SIZE_MINIMIZED) { pendW = LOWORD(l); pendH = HIWORD(l); return 0; }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (m == WM_SYSCOMMAND && (w & 0xfff0) == SC_KEYMENU) return 0;
    return DefWindowProcW(h, m, w, l);
}
static bool dumpState(const char* path) {
    if (!path) return true;
    FILE* f = _wfopen(widePath(path).c_str(), L"wb"); if (!f) return false;
    fprintf(f, "{\"renderer\":\"Direct3D 11\",\"imgui\":\"%s\",\"software\":%s,\"tab\":%u,\"dpi\":%.2f,\"vertices\":%d,\"elements\":{",
        IMGUI_VERSION, softwareRenderer ? "true" : "false", menu.current, dpi, ImGui::GetDrawData()->TotalVtxCount);
    bool first = true;
    for (int i = 0; i < 439; ++i) if (Element* e = E[i]) {
        int value = 0;
        if (auto* c = dynamic_cast<Checkbox*>(e)) value = c->get();
        else if (auto* combo = dynamic_cast<Combo*>(e)) value = combo->value ? *combo->value : 0;
        else if (auto* s = dynamic_cast<Slider*>(e)) value = s->get();
        else if (auto* h = dynamic_cast<Hotkey*>(e)) value = (int)h->vk;
        else if (auto* w = dynamic_cast<WeaponSelect*>(e)) value = w->value ? *w->value : 0;
        fprintf(f, "%s\"%d\":{\"visible\":%s,\"editable\":%s,\"value\":%d}", first ? "" : ",", i,
            e->visible ? "true" : "false", e->editable ? "true" : "false", value);
        first = false;
    }
    fputs("}}\n", f);
    return fclose(f) == 0;
}
int main(int argc, char** argv) {
    const char *capture = nullptr, *statePath = nullptr;
    int startTab = 4, startDpi = 0; bool warp = false;
    const char* tabs[] = {"rage", "aa", "legit", "visuals", "misc", "skins", "players", "config"};
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--warp")) warp = true;
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc) capture = argv[++i];
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) statePath = argv[++i];
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) {
            const char* value = argv[++i]; startTab = -1;
            for (int t = 0; t < 8; ++t) if (!strcmp(value, tabs[t]) || std::to_string(t) == value) startTab = t;
            if (startTab < 0) return 2;
        } else if (!strcmp(argv[i], "--dpi") && i + 1 < argc) {
            int value = atoi(argv[++i]); startDpi = -1;
            for (int d = 0; d < 5; ++d) if (value == (int)(DPI_TABLE[d] * 100)) startDpi = d;
            if (startDpi < 0) return 2;
        } else return 2;
    }
    ImGui_ImplWin32_EnableDpiAwareness();
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) return 3;
    WNDCLASSW wc{0, wndProc, 0, 0, GetModuleHandleW(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"gsmenu-dx11"};
    RegisterClassW(&wc);
    int width = (int)lrintf(WW * DPI_TABLE[startDpi]), height = (int)lrintf(WH * DPI_TABLE[startDpi]);
    if (const char* size = getenv("GS_RESIZE")) {
        if (sscanf_s(size, "%dx%d", &width, &height) != 2 || width < 200 || height < 200 || width > 8192 || height > 8192) { CoUninitialize(); return 2; }
    }
    RECT r{0, 0, width, height}; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hw = CreateWindowW(wc.lpszClassName, L"gamesense menu", WS_OVERLAPPEDWINDOW, 100, 100,
        r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hw || !createDevice(hw, warp)) {
        if (!capture) MessageBoxW(nullptr, L"Could not initialize Direct3D 11.", L"gamesense menu", MB_OK | MB_ICONERROR);
        cleanupDevice(); if (hw) DestroyWindow(hw); UnregisterClassW(wc.lpszClassName, wc.hInstance); CoUninitialize(); return 3;
    }
    bbW = width; bbH = height; pendW = pendH = 0;
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
    bool platformReady = ImGui_ImplWin32_Init(hw);
    bool rendererReady = platformReady && ImGui_ImplDX11_Init(dev, deviceContext);
    int result = 0;
    if (rendererReady) {
        uint32_t tile[16]; const char* pat = "..x.x.x.x...x.x.";
        for (int i = 0; i < 16; ++i) tile[i] = pat[i] == 'x' ? 0xff0c0c0c : 0xff141414;
        bgTile = makeTex(4, 4, tile);
        initFonts();
        tabIcons = loadPng("tab_icons.png");
        for (int d = 0; d < 5; ++d) for (int i = 0; i < 12; ++i)
            weaponIcons[d][i] = loadPng(("weapons/" + std::to_string(d) + "/" + std::to_string(i) + ".png").c_str());
        bool assetsReady = bgTile && tabIcons.tex;
        for (auto& row : weaponIcons) for (auto& im : row) assetsReady &= im.tex != nullptr;
        menu.hwnd = hw; buildMenu(); menu.current = startTab;
        if (startDpi) { *((Combo*)E[360])->value = startDpi; applyDpi(startDpi); }
        if (!assetsReady) result = 6;
        else if (capture) {
            replayEvents();
            for (int i = 0; i < 3; ++i) render(false);
            result = saveBmp(capture) && dumpState(statePath) ? 0 : 5;
        } else {
            ShowWindow(hw, SW_SHOW); UpdateWindow(hw);
            bool done = false;
            while (!done) {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); if (msg.message == WM_QUIT) done = true; }
                if (done) break;
                if (IsIconic(hw)) { Sleep(20); continue; }
                if (!resetDevice()) { result = 4; break; }
                render();
                HRESULT hr = swapChain->Present(1, 0);
                if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) { result = 4; break; }
                if (hr == DXGI_STATUS_OCCLUDED) Sleep(20);
            }
        }
        for (auto* t : menu.tabs) { for (auto* c : t->children) { for (auto* e : c->items) delete e; delete c; } delete t; }
        menu.tabs.clear(); focused = nullptr;
        fNormal.release(); fBold.release(); fSmall.release(); fValue.release();
        releaseCOM(bgTile); releaseCOM(tabIcons.tex); for (auto& row : weaponIcons) for (auto& im : row) releaseCOM(im.tex);
        ImGui_ImplDX11_Shutdown();
    } else result = 3;
    if (platformReady) ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(); cleanupDevice(); DestroyWindow(hw);
    UnregisterClassW(wc.lpszClassName, wc.hInstance); CoUninitialize();
    return result;
}
