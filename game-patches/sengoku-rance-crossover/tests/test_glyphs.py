#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check glyph clipping and baseline consistency through both real GDI paths."""
from pathlib import Path
import argparse
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bottle", default="Steam")
    parser.add_argument("--game-dir", type=Path, default=GAME)
    parser.add_argument("--dll", type=Path, default=ROOT / "bin/RanceTextFix.dll")
    parser.add_argument("--preview-dir", type=Path, help="Keep the memory-rendered PGM sample and log")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="sengoku-glyph-test-") as directory:
        fixture = Path(directory).resolve()
        (fixture / "DLL").mkdir()
        shutil.copyfile(args.game_dir / "DLL/GBText.dll", fixture / "DLL/GBText.dll")
        shutil.copyfile(args.dll, fixture / "DLL/RanceTextFix.dll")
        shutil.copyfile(ROOT / "bin/glyph-probe.exe", fixture / "glyph-probe.exe")
        result = subprocess.run([WINE, "--bottle", args.bottle, "--debugmsg", "-all,+debugstr",
                                 "--workdir", winpath(fixture), "--cx-app",
                                 winpath(fixture / "glyph-probe.exe")], capture_output=True,
                                text=True, errors="replace", timeout=60, check=True)
        if args.preview_dir:
            args.preview_dir.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(fixture / "after.pgm", args.preview_dir / "glyphs.pgm")
            (args.preview_dir / "glyphs.log").write_text(result.stdout + result.stderr)
    pattern = r"^(before|after) cases=(\d+) changed=(\d+) clipped=(\d+) empty=(\d+)$"
    measurements = {phase: tuple(map(int, values)) for phase, *values in
                    re.findall(pattern, result.stdout, re.M)}
    expected_cases = 57 * 3 * 2 * 2 * 3 * 3
    if set(measurements) != {"before", "after"}:
        raise AssertionError("Incomplete GDI probe:\n" + result.stdout + result.stderr)
    if measurements["before"][0] != expected_cases or not measurements["before"][2]:
        raise AssertionError("The control did not reproduce the clipping defect")
    if measurements["after"] != (expected_cases, 0, 0, 0):
        raise AssertionError("Clipping or inconsistent placement remains:\n" +
                             result.stdout + result.stderr)
    print(f"PASS: {expected_cases} raster comparisons, with no clipping, shifted pixels, or empty glyphs.")
    print("57 reported characters; sizes 12/18/24; normal/bold; square/padded 1/8/32-bit buffers;")
    print("ANSI, Unicode, and Unicode with the game's trailing-NUL argument.")


if __name__ == "__main__":
    main()
