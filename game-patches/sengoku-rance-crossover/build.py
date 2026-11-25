#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the self-contained 32-bit Windows DLL and the glyph test program."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
LLVM = Path(os.environ.get("LLVM_PREFIX", "/opt/homebrew/opt/llvm/bin"))
CLANG = os.environ.get("CLANG", str(LLVM / "clang"))
DLLTOOL = os.environ.get("LLVM_DLLTOOL", str(LLVM / "llvm-dlltool"))
READOBJ = os.environ.get("LLVM_READOBJ", str(LLVM / "llvm-readobj"))
WINE_DLLS = Path(os.environ.get("WINE_DLLS", "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib/wine/i386-windows"))
RUST_LINKER = Path.home() / ".rustup/toolchains/stable-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link"
LINKER = os.environ.get("LLD_LINK", shutil.which("lld-link") or str(RUST_LINKER))


def run(*args):
    subprocess.run([str(x) for x in args], check=True)


def verify_imports(binary):
    """Check the final PE imports, not just linker symbols, before execution."""
    imported = subprocess.check_output([READOBJ, "--coff-imports", str(binary)], text=True)
    for block in re.findall(r"Import \{(.*?)\n\}", imported, re.S):
        dll = re.search(r"Name: (\S+)", block).group(1)
        names = re.findall(r"Symbol: (\S+) ", block)
        if any("@" in name for name in names):
            raise RuntimeError(f"Decorated Win32 imports in {binary}: {names}")
        target = WINE_DLLS / dll.lower()
        if target.is_file():
            exports = subprocess.check_output([READOBJ, "--coff-exports", str(target)], text=True)
            available = set(re.findall(r"  Name: (\S+)", exports))
            missing = set(names) - available
            if missing:
                raise RuntimeError(f"Unresolved imports in {binary}: {dll}: {sorted(missing)}")


def main():
    for exe in (CLANG, DLLTOOL, LINKER, READOBJ):
        if not Path(exe).is_file() and not shutil.which(exe):
            raise SystemExit(f"Build tool not found: {exe}")
    output = ROOT / "bin"
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="sengoku-build-") as temp:
        tmp = Path(temp)
        for library in ("kernel32", "gdi32", "user32"):
            # Keep stdcall decorations in linker symbols, strip them from
            # names placed in the PE import table (Windows exports are bare).
            run(DLLTOOL, "-m", "i386", "-k", "-d", ROOT / "src" / (library + ".def"),
                "-l", tmp / (library + ".lib"))
        flags = ("--target=i686-pc-windows-msvc", "-O2", "-ffreestanding", "-fno-stack-protector")
        run(CLANG, *flags, "-c", ROOT / "src/RanceTextFix.c", "-o", tmp / "fix.obj")
        run(LINKER, "/dll", "/machine:x86", "/nodefaultlib", "/entry:DllMain@12", "/timestamp:0",
            "/implib:" + str(tmp / "RanceTextFix.lib"),
            "/out:" + str(output / "RanceTextFix.dll"), tmp / "fix.obj",
            tmp / "kernel32.lib", tmp / "gdi32.lib")
        run(CLANG, *flags, "-c", ROOT / "tests/glyph_probe.c", "-o", tmp / "probe.obj")
        run(LINKER, "/subsystem:console", "/machine:x86", "/nodefaultlib", "/timestamp:0",
            "/entry:mainCRTStartup", "/out:" + str(output / "glyph-probe.exe"),
            tmp / "probe.obj", tmp / "kernel32.lib", tmp / "gdi32.lib", tmp / "user32.lib")
        verify_imports(output / "RanceTextFix.dll")
        verify_imports(output / "glyph-probe.exe")
    print("Built and verified imports:", output / "RanceTextFix.dll", "and glyph-probe.exe")


if __name__ == "__main__":
    main()
