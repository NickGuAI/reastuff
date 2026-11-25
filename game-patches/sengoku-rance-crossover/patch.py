#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Version-checked, reversible display patch for this Simplified Chinese build."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
BOTTLES = Path.home() / "Library/Application Support/CrossOver/Bottles"
WINE = Path("/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine")
GAME_REL = "drive_c/Program Files (x86)/Steam/steamapps/common/Sengoku Rance"
KEY = r"HKCU\Software\AliceSoft\System40"
BACKUP = ".rance-display-fix"
SHIM = "DLL/RanceTextFix.dll"
BEFORE = {
    "Rance7.exe": "126b8e6fcda30fff82d8fad1e34e27c5cbe3f3257c4255c3a669870b0c550556",
    "SACT_DLL.inc": "40863d1f342d39ec7ab22d8595da3838270539d7b2842ccbdd5fb743161aacdf",
}
DEPENDENCIES = {
    "DLL/GBText.dll": "8e6640346d83bc757c2955763297d8497b346afcd9719fa02c813298dfa5cefe",
    "DLL/DrawText2.dll": "946481ca080cbbb3c407ac6276f7ca6c4e85f96d082f0556f7c73e8175e09912",
}
SHIM_HASH = "4f91a85ae31f2b206aa1e70da7a89f28bf15f654615f9f045362a448daf81b72"
SCALE_OFFSET = 0x13BFA
SCALE_EXE_HASH = "20591ebd68be6ebe6a58d889f91554d92967e541837489b45e69d59ab01d2876"
SETTINGS = {"FullScreen2": 0, "WindowScalePercent": 170}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    return sha(path.read_bytes()) if path.is_file() else None


def atomic_write(path, data):
    mode = path.stat().st_mode & 0o777 if path.exists() else 0o644
    fd, name = tempfile.mkstemp(prefix=path.name + ".tmp-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(name, mode)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def ensure_stopped():
    processes = subprocess.check_output(["ps", "-axo", "pid=,comm="], text=True)
    if re.search(r"(?im)[/\\]Rance7(?:[-_][^\r\n]*)?\.exe\s*$", processes):
        raise RuntimeError("Close Sengoku Rance before applying or removing this patch.")


class Registry:
    def __init__(self, bottle):
        self.bottle = bottle

    def run(self, *args):
        result = subprocess.run([str(WINE), "--bottle", self.bottle, "--cx-app",
                                 "C:/windows/system32/reg.exe", *args],
                                capture_output=True, timeout=30)
        if result.returncode:
            raise RuntimeError("CrossOver registry command failed: " +
                               (result.stdout + result.stderr).decode(errors="replace"))
        return result.stdout.decode(errors="replace")

    def read(self):
        output = self.run("query", KEY)
        return {key: int(value, 16) for key, value in
                re.findall(r"^\s*(\w+)\s+REG_DWORD\s+(0x[0-9a-fA-F]+)", output, re.M)}

    def set(self, key, value):
        if value is None:
            if key in self.read():
                self.run("delete", KEY, "/v", key, "/f")
        else:
            self.run("add", KEY, "/v", key, "/t", "REG_DWORD", "/d", str(value), "/f")


def make_changes(game):
    for name, expected in {**BEFORE, **DEPENDENCIES}.items():
        if digest(game / name) != expected:
            raise RuntimeError(f"Unsupported or modified file: {name}. No files changed.")
    shim = (ROOT / "bin/RanceTextFix.dll").read_bytes()
    if sha(shim) != SHIM_HASH:
        raise RuntimeError("The bundled patch DLL differs from the tested build.")
    exe = bytearray((game / "Rance7.exe").read_bytes())
    if exe[SCALE_OFFSET] != 1:
        raise RuntimeError("Unexpected window-scale table.")
    exe[SCALE_OFFSET] = 0
    if sha(exe) != SCALE_EXE_HASH:
        raise RuntimeError("Unexpected patched executable checksum.")
    include = (game / "SACT_DLL.inc").read_bytes()
    anchor = b'"GBText.dll",\r\n'
    if include.count(anchor) != 1:
        raise RuntimeError("Unexpected DLL load list.")
    include = include.replace(anchor, anchor + b'"RanceTextFix.dll",\r\n')
    return {"Rance7.exe": bytes(exe), "SACT_DLL.inc": include, SHIM: shim}


def read_manifest(game):
    manifest = json.loads((game / BACKUP / "manifest.json").read_text())
    if manifest.get("format") != 1 or manifest.get("before") != BEFORE:
        raise RuntimeError("Unrecognized backup manifest; leaving it untouched.")
    if set(manifest.get("after", {})) != {*BEFORE, SHIM}:
        raise RuntimeError("Invalid backup manifest file list.")
    if manifest.get("settings_applied") != SETTINGS:
        raise RuntimeError("Invalid backup manifest settings.")
    return manifest


def save_manifest(game, manifest):
    atomic_write(game / BACKUP / "manifest.json",
                 (json.dumps(manifest, indent=2) + "\n").encode())


def restore(game, registry, manifest):
    """Also handles an interrupted install; validate everything before writing."""
    for name, expected in BEFORE.items():
        if digest(game / BACKUP / (name + ".original")) != expected:
            raise RuntimeError(f"Backup is missing or changed: {name}")
        if digest(game / name) not in (expected, manifest["after"][name]):
            raise RuntimeError(f"{name} changed after installation; refusing to overwrite it.")
    if digest(game / SHIM) not in (None, manifest["after"][SHIM]):
        raise RuntimeError("The installed text DLL changed; refusing to overwrite it.")
    current_settings = registry.read()
    for name in BEFORE:
        atomic_write(game / name, (game / BACKUP / (name + ".original")).read_bytes())
    if (game / SHIM).exists():
        (game / SHIM).unlink()
    for key, applied in SETTINGS.items():
        # Preserve settings the user changed after installation.
        if current_settings.get(key) == applied:
            registry.set(key, manifest["settings_before"].get(key))
    manifest["state"] = "uninstalled"
    save_manifest(game, manifest)
    suffix = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    archive = game / (BACKUP + ".uninstalled-" + suffix)
    (game / BACKUP).rename(archive)
    return archive


def install(game, registry):
    ensure_stopped()
    if (game / BACKUP).exists():
        manifest = read_manifest(game)
        if manifest["state"] == "installed" and all(
                digest(game / name) == expected for name, expected in manifest["after"].items()):
            print("Patch is already installed. Keeping current window preferences.")
            return
        raise RuntimeError("An incomplete or changed patch exists. Use uninstall to restore it safely.")
    if (game / SHIM).exists():
        raise RuntimeError("A text patch DLL already exists without this installer's backup.")
    changed = make_changes(game)
    original_settings = registry.read()
    manifest = {
        "format": 1, "state": "installing", "before": BEFORE,
        "after": {name: sha(data) for name, data in changed.items()},
        "settings_before": {key: original_settings.get(key) for key in SETTINGS},
        "settings_applied": SETTINGS,
    }
    (game / BACKUP).mkdir()
    for name in BEFORE:
        shutil.copy2(game / name, game / BACKUP / (name + ".original"))
    save_manifest(game, manifest)
    try:
        # DLL first, include last, with the game stopped throughout installation.
        for name in (SHIM, "Rance7.exe", "SACT_DLL.inc"):
            atomic_write(game / name, changed[name])
        for key, value in SETTINGS.items():
            registry.set(key, value)
        manifest["state"] = "installed"
        save_manifest(game, manifest)
    except Exception as error:
        try:
            archive = restore(game, registry, manifest)
        except Exception as recovery:
            raise RuntimeError(f"Install failed: {error}. Recovery also failed: {recovery}. "
                               f"Originals remain in {game / BACKUP}.") from error
        raise RuntimeError(f"Install failed and was rolled back: {error}. Backup: {archive}") from error
    print("Installed Chinese text fix and 170% window scale (1360 x 1020).")
    print("Original files and settings:", game / BACKUP)


def status(game, registry):
    if (game / BACKUP).exists():
        manifest = read_manifest(game)
        print("Patch state:", manifest["state"])
        for name, expected in {**manifest["after"], **DEPENDENCIES}.items():
            print(("OK      " if digest(game / name) == expected else "CHANGED ") + name)
    else:
        print("Display patch is not installed.")
        print("Supported game build:", all(digest(game / n) == h for n, h in {**BEFORE, **DEPENDENCIES}.items()))
    values = registry.read()
    print("Window scale:", values.get("WindowScalePercent"), "%")
    print("Fullscreen:", values.get("FullScreen2"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("install", "status", "uninstall"))
    parser.add_argument("--bottle", default="Steam")
    parser.add_argument("--game-dir", type=Path)
    args = parser.parse_args()
    game = (args.game_dir or BOTTLES / args.bottle / GAME_REL).resolve()
    registry = Registry(args.bottle)
    try:
        if args.action == "install":
            install(game, registry)
        elif args.action == "status":
            status(game, registry)
        else:
            ensure_stopped()
            if not (game / BACKUP).exists():
                print("Display patch is not installed; nothing to remove.")
            else:
                archive = restore(game, registry, read_manifest(game))
                print("Restored the previous game files and unchanged window preferences.")
                print("Backup retained at:", archive)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))


if __name__ == "__main__":
    main()
