# Dear ImGui / Direct3D 11 port

Verified on 2026-11-15.

## Delivered build

- `gsmenu.exe` — standalone x64 Windows GUI application; also available at `build/gsmenu-dx11.exe`.
- Size: 1,032,704 bytes.
- SHA-256: `5ec0b232a6a5b3531134841fb7f0833e8e5a45703a83f5afc938bff0ee1da0b8`.
- Dear ImGui v1.92.9b, upstream commit `f1cc2ae15e53a861a874c3034aae6798fde194ab`.
- Release build completed with `/W4`, without warnings. The Microsoft C++ runtime is statically linked.
- PE inspection confirms x64, Windows GUI subsystem, and a Direct3D 11 import. The delivered executable has no Direct3D 9 dependency.

## Appearance

The preserved source in `legacy/dx9/` was rebuilt as the reference. The final hardware-rendered DX11 build matched every RGB pixel in all **27 comparison scenes**: eight tabs, five DPI scales, dropdowns and selections, sliders, visibility dependencies, color picker and context menus, hotkey mode/binding, scroll fades and an enlarged client area.

The one-level discrepancies initially seen in alpha gradients were resolved by matching the original triangle-strip diagonal in the ImGui draw list. Original GDI ClearType glyph generation, embedded images, spacing, geometry, gradient colors and window title are retained.

The software WARP fallback also passed its separate smoke check. Its rasterizer can round a color channel one level differently from the hardware renderer; exact pixel parity above applies to the tested hardware adapter and system font configuration.

Run `python tools/check_parity.py` to regenerate `build/parity/report.json` and the paired captures. `ref/misc.png` is an older reference with different visible settings, so the unchanged DX9 source is the port comparison baseline.

## Runtime validation

The final EXE was launched and visually inspected. Native mouse dragging changed the FOV slider from 90 to 107; the Right arrow advanced it to 108. Maximize and restore both succeeded, exercising swap-chain resizing. The slider was restored to 90 and the preview was left open at its original size. A checkbox/visibility dependency was also verified in the first preview build.

## Implementation

Dear ImGui supplies the context, Win32 input queue, draw lists, clipping and official DX11 rendering backend. The existing custom widget logic and drawing are retained to satisfy the exact appearance requirement. This does not replace the controls with the stock ImGui widget style. No game integration or Lua execution was added.

The original data pipeline and generated menu/configuration data remain intact. The old DX9 source and build are retained in `legacy/dx9/` for reference.
