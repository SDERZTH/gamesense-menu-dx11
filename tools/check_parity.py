"""Compare actual DX9 and DX11 renders, without third-party Python packages.

Build build.bat and legacy/dx9/build.bat first. Captures exercise the original
GS_EVENTS hook; live ImGui input is checked separately in the native preview.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "parity"
REFERENCE = ROOT / "legacy" / "dx9" / "gsmenu.exe"
PORT = ROOT / "build" / "gsmenu-dx11.exe"


def click(x, y, right=False):
    return f"{'rdown' if right else 'down'}:{x},{y};{'rup' if right else 'up'}:{x},{y}"


def bmp(path):
    data = path.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    assert struct.unpack_from("<H", data, 28)[0] == 32
    return width, abs(height), data[offset:]


def compare(a_path, b_path):
    aw, ah, a = bmp(a_path)
    bw, bh, b = bmp(b_path)
    assert (aw, ah) == (bw, bh), "Capture dimensions differ"
    different = over_one = total_delta = max_delta = 0
    for i in range(0, len(a), 4):
        delta = [abs(a[i + c] - b[i + c]) for c in range(3)]
        worst = max(delta)
        different += worst > 0
        over_one += worst > 1
        max_delta = max(max_delta, worst)
        total_delta += sum(delta)
    return dict(width=aw, height=ah, pixels=aw * ah, different_pixels=different,
                max_channel_delta=max_delta, pixels_over_one=over_one,
                mean_channel_delta=total_delta / (aw * ah * 3))


def main():
    if not REFERENCE.exists() or not PORT.exists():
        sys.exit("Run build.bat and legacy\\dx9\\build.bat first.")
    OUT.mkdir(parents=True, exist_ok=True)
    cases = []
    for i, tab in enumerate(["rage", "aa", "legit", "visuals", "misc", "skins", "players", "config"]):
        cases.append((tab, click(40, 50 + 64 * i), None, i))
    cases += [
        ("misc-air-duck-open", click(460, 182), None, 4),
        ("misc-air-duck-selected", click(460, 182) + ";" + click(460, 225), None, 4),
        ("misc-air-strafe", click(440, 129), None, 4),
        ("misc-slider-drag", "down:180,70;move:280,70;up:280,70", None, 4),
        ("misc-slider-ctrl-wheel", "wheel:200,70,120,1", None, 4),
        ("misc-color-picker", click(614, 338), None, 4),
        ("misc-color-context", click(614, 338, True), None, 4),
        ("misc-hotkey-mode", click(614, 321, True), None, 4),
        ("misc-hotkey-bind", click(614, 321) + ";key:75", None, 4),
        ("misc-settings-scroll", ";".join(["wheel:500,500,-120"] * 6), None, 4),
        ("misc-resized", "", "950x750", 4),
        ("rage-weapon-list", click(40, 50) + ";" + click(220, 60), None, 0),
        ("rage-hitboxes", click(40, 50) + ";" + click(200, 213), None, 0),
        ("visuals-scroll", click(40, 242) + ";wheel:220,450,-120;wheel:220,450,-120", None, 3),
    ]
    for step, scale in enumerate([1, 1.25, 1.5, 1.75, 2]):
        events = click(460, 372) + ";" + click(460, 394 + 20 * step)
        cases.append((f"dpi-{int(scale * 100)}", events,
                      f"{round(669 * scale)}x{round(564 * scale)}", 4))
    results = []
    for name, events, size, tab in cases:
        env = {k: v for k, v in os.environ.items() if k not in ("GS_EVENTS", "GS_RESIZE")}
        if events:
            env["GS_EVENTS"] = events
        if size:
            env["GS_RESIZE"] = size
        for kind, executable in [("dx9", REFERENCE), ("dx11", PORT)]:
            args = [str(executable), "--capture", str(OUT / f"{name}-{kind}.bmp")]
            if kind == "dx11":
                args += ["--state", str(OUT / f"{name}.json")]
            result = subprocess.run(args, cwd=ROOT, env=env, timeout=30, capture_output=True)
            if result.returncode:
                raise RuntimeError(f"{name} {kind} failed: {result.returncode}: {result.stderr!r}")
        metrics = compare(OUT / f"{name}-dx9.bmp", OUT / f"{name}-dx11.bmp")
        state = json.loads((OUT / f"{name}.json").read_text())
        assert state["tab"] == tab, (name, state["tab"])
        assert state["renderer"] == "Direct3D 11" and state["vertices"] > 1000
        if name.startswith("dpi-"):
            assert state["dpi"] == int(name[4:]) / 100, (name, state["dpi"])
        if name == "misc-air-strafe":
            assert state["elements"]["337"]["value"] == 1
            assert state["elements"]["338"]["visible"]
        if name == "misc-hotkey-bind":
            assert state["elements"]["357"]["value"] == 75
        passed = metrics["different_pixels"] == 0
        results.append(dict(name=name, passed=passed, **metrics))
        print(f"{'PASS' if passed else 'FAIL'} {name}: {metrics['different_pixels']}/{metrics['pixels']} differing pixels", flush=True)
    # Exercise the software fallback independently of the hardware comparisons.
    result = subprocess.run([str(PORT), "--warp", "--capture", str(OUT / "warp.bmp"),
                             "--state", str(OUT / "warp.json")], cwd=ROOT, timeout=30)
    assert result.returncode == 0
    assert json.loads((OUT / "warp.json").read_text())["software"]
    warp_metrics = compare(OUT / "misc-dx9.bmp", OUT / "warp.bmp")
    # WARP's software rasterizer may round a channel one level differently from
    # the physical GPU. This is a separate fallback check, not hardware parity.
    fallback = dict(passed=warp_metrics["max_channel_delta"] <= 1, **warp_metrics)
    assert fallback["passed"], fallback
    report = dict(reference="original DX9 source, rebuilt locally", port="Dear ImGui 1.92.9b / Direct3D 11",
                  exact_pixel_match=all(r["passed"] for r in results), cases=results, software_fallback=fallback)
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{sum(r['passed'] for r in results)}/{len(results)} exact matches. Report: {OUT / 'report.json'}")
    return 0 if report["exact_pixel_match"] else 1


if __name__ == "__main__":
    sys.exit(main())
