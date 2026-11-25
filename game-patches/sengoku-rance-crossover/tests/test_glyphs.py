#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render real GBText glyphs into the engine-sized DIB, before and after the fix."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GAME = Path.home() / "Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/Sengoku Rance"
WINE = "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"


def winpath(path):
    return "Z:" + str(Path(path).resolve())


def main():
    with tempfile.TemporaryDirectory(prefix="sengoku-glyph-test-") as directory:
        fixture = Path(directory).resolve()
        (fixture / "DLL").mkdir()
        shutil.copyfile(GAME / "DLL/GBText.dll", fixture / "DLL/GBText.dll")
        shutil.copyfile(ROOT / "bin/RanceTextFix.dll", fixture / "DLL/RanceTextFix.dll")
        shutil.copyfile(ROOT / "bin/glyph-probe.exe", fixture / "glyph-probe.exe")
        result = subprocess.run([WINE, "--bottle", "Steam", "--workdir", winpath(fixture),
                                 "--cx-app", winpath(fixture / "glyph-probe.exe")],
                                capture_output=True, text=True, errors="replace", timeout=60, check=True)
    output = result.stdout
    if "GBText only" not in output or "GBText plus patch" not in output:
        raise AssertionError("The real GBText module or the patch could not be loaded:\n" + output)
    before, after = output.split("GBText plus patch", 1)
    pattern = re.compile(r"^(\w+) rows=(\d) size=(\d+) cell=\d+ ink_y=-?\d+\.\.-?\d+ "
                         r"ink_x=(-?\d+)\.\.(-?\d+) total=(\d+)", re.M)
    def results(section):
        return {(name, int(rows)): (int(lo), int(hi), int(total))
                for name, rows, _size, lo, hi, total in pattern.findall(section)}
    baseline, fixed = results(before), results(after)
    names = ("zhong", "guo", "wo", "men", "small", "large", "ascii",
             "padded_guo", "padded_zhong", "padded_ascii", "mono_guo")
    for name in names:
        if fixed[(name, 1)][2] != fixed[(name, 2)][2]:
            raise AssertionError(f"{name}: patched square buffer still loses ink")
    if not any(baseline[(name, 1)][2] < baseline[(name, 2)][2] for name in names):
        raise AssertionError("Control did not reproduce clipped glyphs")
    for name in ("zhong", "guo", "wo", "men"):
        old_width = baseline[(name, 1)][1] - baseline[(name, 1)][0] + 1
        new_width = fixed[(name, 1)][1] - fixed[(name, 1)][0] + 1
        if new_width <= old_width:
            raise AssertionError(f"{name}: expected a wider natural glyph ({old_width} -> {new_width})")
    print("PASS: eleven glyph cases fit square and padded 1/8/32-bit buffers; Chinese glyphs use their natural width.")
    print("Control example (国, size 18):", baseline[("guo", 1)][2], "visible /",
          baseline[("guo", 2)][2], "reference pixels; patched:", fixed[("guo", 1)][2], "/",
          fixed[("guo", 2)][2])


if __name__ == "__main__":
    main()
