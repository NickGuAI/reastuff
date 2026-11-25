# Sengoku Rance: Chinese text fix for CrossOver

An MIT-licensed community patch for the **Simplified Chinese Steam version** of
Sengoku Rance on macOS with CrossOver. It corrects clipped, narrow Chinese glyphs
and enables a 170% window scale (1360 × 1020).

This is a standalone patch published in the NickGuAI/reastuff fork. It does not
require REA. The release contains the patch source, installer, test programs, and
a compiled patch DLL. Game executables, translation DLLs, fonts, and saves are
not included.

## Download and install

Download `sengoku-rance-crossover-v0.1.0.zip` from the
[release page](https://github.com/NickGuAI/reastuff/releases/tag/sengoku-rance-crossover-v0.1.0)
and extract it. Use the attached patch ZIP rather than GitHub's automatically
generated repository source archive, which does not contain the compiled DLL.

Requirements:

- macOS, CrossOver installed at `/Applications/CrossOver.app`, and Python 3.9+.
- A legally obtained Simplified Chinese Steam installation that already starts
  successfully in CrossOver. The tested bottle uses `LC_ALL=zh_CN.UTF-8`.
- The exact game and translation files listed under **Supported files** below.
  This version expects the executable after the separate
  [Sengoku Rance Deck Fix startup correction](https://github.com/yuzuatri/Sengoku-Rance-Deck-Fix).
  The display installer does not apply that startup fix or change the bottle locale.
- Enough screen space for a 1360 × 1020 client area plus window decorations if
  using 170%. Smaller screens can use the game's existing 100% or 150% options.

Close the game, open Terminal in the extracted `sengoku-rance-crossover` folder,
and run:

```sh
python3 patch.py status
python3 patch.py install
```

Then launch the game normally through Steam. The default bottle is `Steam`,
with the game in its standard Steam library. Override those locations when needed:

```sh
python3 patch.py install --bottle "My Bottle" --game-dir "/path/to/Sengoku Rance"
```

Unknown game versions or conflicting modifications are rejected before patching.
The 170% setting enlarges the existing 4:3 canvas; it does not add widescreen or
arbitrary resizing by dragging the window border.

## Undo

Close the game and run:

```sh
python3 patch.py uninstall
```

Use the same `--bottle` and `--game-dir` overrides if you used them at installation.
Original files and previous window preferences are saved inside the game folder
under `.rance-display-fix`. Uninstall checks hashes, restores those files, and
retains an archived backup. It refuses to overwrite files changed after installation.
Window preferences changed by the user after installation are preserved.

The patch sets `FullScreen2=0` and `WindowScalePercent=170` under
`HKCU\Software\AliceSoft\System40` in the selected bottle. **Other System40 games
in that bottle share these window preferences.** The DLL hooks run only inside
the patched game. Save files and the earlier startup fix are preserved.

Steam updates or file verification can replace the patched files. The installer
intentionally rejects unfamiliar builds; do not bypass its version checks.

## What was wrong

The installed Chinese `GBText.dll` substitutes a Chinese font and forces its
width to half the requested height. With the tested bottle's Songti/SimSun
substitute, this compresses Chinese characters. Its line height also exceeds the
game's small glyph buffer, clipping the bottom of characters.

The patch restores the font's natural width and uses glyph ink metrics to fit
each character inside its intended cell. It recognizes both square buffers and
the engine's padded buffers: runtime tracing observed an 80 × 73, 8-bit bitmap
for a 72-pixel font with a 104-pixel line height. It preserves the existing CP936
decoder and translated text.

The engine's scale table accepts only 100%, 150%, 200%, and 300%. Changing one
table byte enables 170%, which fit the test display. The built-in fullscreen
path did not fill that Mac's display correctly; the patch uses the larger window.

## Validation and limits

Tested on one macOS/CrossOver setup with Wine `11.0-8726-g2e2f5fca349`, a 64-bit
Steam bottle running the 32-bit game, and its existing Chinese font substitutes.
Native Windows, Proton, other game versions, and other fonts are unverified.

- **11 rendering cases passed:** Chinese and ASCII glyphs at sizes 12/18/24,
  using square or padded 1/8/32-bit GDI bitmaps. Each corrected game-sized buffer
  retained the same number of ink pixels as a double-height reference.
- **6 installer tests passed:** exact restoration and idempotence; rejection of
  unknown executables and untracked DLLs; preservation of later edits and
  preferences; recovery from a simulated failure halfway through installation.
- **Runtime checks passed:** the game loaded the patch, logged both font and
  placement corrections, and launched through Steam with a 1360 × 1020 client area.

For example, `国` at size 18 retained only 296 of 394 pixels in its original
narrow rendering. The corrected natural-width rendering retained 810 of 810.
These are rendering measurements and runtime checks, **not a visual review of
every dialogue page**. Translated-line wrapping was not independently validated.

## Source build and tests

The source lives in this directory. Compiled files are release assets and are
excluded from Git. Building requires LLVM clang, llvm-dlltool, llvm-readobj, and
LLD; no Windows SDK or Windows C runtime is needed.

```sh
python3 build.py
python3 tests/test_glyphs.py
python3 -m unittest discover -s tests -p test_installer.py -v
```

The integration tests require your own matching game files and the default
CrossOver Steam bottle. Installer tests use disposable copies and a fake
registry; the glyph test draws into memory bitmaps in a separate process.

Build defaults use `/opt/homebrew/opt/llvm/bin` and the existing Rust toolchain's
`lld-link`. Override `LLVM_PREFIX`, `CLANG`, `LLVM_DLLTOOL`, `LLVM_READOBJ`,
`LLD_LINK`, or `WINE_DLLS` as needed. Building does not install dependencies.
The PE timestamp is fixed at zero. The installer pins the tested DLL hash;
different compiler versions may produce different bytes, which require review
and testing before deliberately updating that hash.

The build strips stdcall suffixes from DLL import names and checks the final PE
imports against available CrossOver system DLL exports. This prevents the
`LoadLibraryA@4` import error encountered in an early development diagnostic.
`TRACE_GLYPH` is an optional compile-time diagnostic, disabled in the release DLL.

## Supported files

| File | SHA-256 |
| --- | --- |
| Input `Rance7.exe`, after the separate startup fix | `126b8e6fcda30fff82d8fad1e34e27c5cbe3f3257c4255c3a669870b0c550556` |
| Output `Rance7.exe` | `20591ebd68be6ebe6a58d889f91554d92967e541837489b45e69d59ab01d2876` |
| Original `SACT_DLL.inc` | `40863d1f342d39ec7ab22d8595da3838270539d7b2842ccbdd5fb743161aacdf` |
| `GBText.dll` | `8e6640346d83bc757c2955763297d8497b346afcd9719fa02c813298dfa5cefe` |
| `DrawText2.dll` | `946481ca080cbbb3c407ac6276f7ca6c4e85f96d082f0556f7c73e8175e09912` |
| Released `RanceTextFix.dll` | `4f91a85ae31f2b206aa1e70da7a89f28bf15f654615f9f045362a448daf81b72` |

Installation changes byte `0x13bfa` of the recognized executable from `01` to
`00`, copies `RanceTextFix.dll` into `DLL`, and loads it immediately after
`GBText.dll` through `SACT_DLL.inc`. At runtime the DLL validates GBText's export
addresses, instruction signatures, and original function pointers before
replacing its two cached GDI pointers. Unexpected libraries are left untouched.

## License

Patch code, installer, tests, and documentation: [MIT](LICENSE), copyright
2026 Nick Gu. This license covers this patch's original files; the game,
translation, fonts, and separately obtained startup fix retain their own licenses.
