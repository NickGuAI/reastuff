# Sengoku Rance: Chinese font fix for CrossOver

An MIT-licensed community patch for the **Simplified Chinese Steam version** of
Sengoku Rance on macOS with CrossOver. Version **0.1.1** replaces the faulty
v0.1.0 character-positioning workaround with a shared font correction for
dialogue and menu text. The package also retains the 170% window option
(1360 × 1020) from the first release.

This standalone patch lives in the NickGuAI/reastuff fork and does not require
REA. Game files, translation libraries, fonts, and saves are not redistributed.

## Download and upgrade

Download `sengoku-rance-crossover-v0.1.1.zip` from the
[release page](https://github.com/NickGuAI/reastuff/releases/tag/sengoku-rance-crossover-v0.1.1)
and extract it. Use the attached patch ZIP; GitHub's automatic repository source
archives do not include the compiled DLL.

If v0.1.0 is already installed, **close the game**, open Terminal in the extracted
`sengoku-rance-crossover` directory, and run:

```sh
python3 patch.py upgrade
python3 patch.py status
```

Then start the game normally through Steam. Upgrade replaces only the font DLL
and its installation record. It retains the previous DLL in the existing backup
and preserves the executable, load list, window preferences, and saves.
`install` also upgrades an existing recognized v0.1.0 installation.

## First installation

Requirements:

- macOS, CrossOver at `/Applications/CrossOver.app`, and Python 3.9+.
- The Mac's **Heiti SC** font family available to Wine. No font is bundled or
  installed by this patch. At runtime, the patch verifies the selected family
  and line height before accepting it; an unavailable or incompatible font
  leaves the original font unchanged.
- A Simplified Chinese Steam installation that already starts in CrossOver.
  The tested bottle uses `LC_ALL=zh_CN.UTF-8`.
- The recognized files listed below, including the executable after the separate
  [Sengoku Rance Deck Fix startup correction](https://github.com/yuzuatri/Sengoku-Rance-Deck-Fix).
  This installer does not apply that startup correction or change the bottle locale.

Close the game and run these commands in the extracted directory:

```sh
python3 patch.py status
python3 patch.py install
```

The default bottle is `Steam` with the standard Steam library location. All
installer commands accept overrides:

```sh
python3 patch.py install --bottle "My Bottle" --game-dir "/path/to/Sengoku Rance"
```

A first installation enables 170% scale, requiring room for a 1360 × 1020 client
area plus window decorations. Smaller displays can use the game's existing
100% or 150% options. This enlarges the existing 4:3 canvas; it does not provide
widescreen or arbitrary resizing by dragging the border.

## Undo

Close the game and run:

```sh
python3 patch.py uninstall
```

Use the same bottle and directory overrides if applicable. The installer restores
original files from `.rance-display-fix` and retains an archived backup. It
refuses to overwrite later file edits and preserves window preferences changed
by the user after installation. The earlier startup correction is retained.

Fresh installation sets `FullScreen2=0` and `WindowScalePercent=170` under
`HKCU\Software\AliceSoft\System40`. Other System40 games in the same bottle share
these preferences. **Upgrading v0.1.0 does not change registry settings.** The
font hook runs only inside this game. Steam updates or file verification may
replace patched files; unfamiliar builds are rejected.

## Why v0.1.0 was replaced

The Chinese `GBText.dll` substitutes a font and forces its requested average
width to half its height. On the tested bottle, its Songti/SimSun substitute
produced compressed glyphs and a 104-pixel line height for a 72-pixel glyph
buffer. The game's renderers assume that the requested font fits that buffer.

Version 0.1.0 restored the natural width, then moved individual glyphs to fit.
That workaround produced uneven dialogue characters and did not cover direct
Unicode drawing. The broken dialogue and menu samples were reproduced with the
released DLL in a real GDI rendering probe.

Version 0.1.1 fixes font creation instead: it selects Heiti SC, removes GBText's
forced average width, and verifies cell-sized line metrics. Both ANSI and direct
Unicode callers receive the same corrected font. There is no text-position
hook, no per-glyph offset, and no change to the CP936 decoder. Requested sizes
and weights are retained.

The executable's one-byte window-scale correction is unchanged. It allows 170%
in addition to the engine's existing scale choices; it is independent of the
font correction.

## Validation and limits

Tested in the same macOS/CrossOver Steam bottle as v0.1.0, with Wine
`11.0-8726-g2e2f5fca349` and the recognized 32-bit game.

- **6,156 raster comparisons passed:** 57 characters from the reported dialogue
  and menus, sizes 12/18/24, normal and bold, square and padded 1/8/32-bit
  buffers, and ANSI/Unicode drawing. Each glyph matches an uncropped reference
  pixel for pixel at the same baseline, with no ink outside its cell. This also
  covers the game's Unicode call that includes a trailing NUL for a two-byte
  Chinese source character.
- The same test **rejects the released v0.1.0 DLL** and reproduces the reported
  uneven dialogue and sliced menu labels. Memory-rendered replacement samples
  were visually checked.
- **12 installer tests passed:** install/restore, upgrade/restore, idempotence,
  preservation of later preferences and saves, refusal of conflicting edits,
  rollback on write failure, and resuming an interrupted DLL upgrade.
- Final PE imports are checked against CrossOver's exports. A real-game launch
  in an isolated fixture confirmed that the replacement font hook initializes.

These checks cover the reported rendering defect; they are not a visual review
of every game screen or translated-line wrapping. Other game versions, native
Windows, Proton, and machines without the matching native font are unverified.

## Build and test

Building requires LLVM clang, llvm-dlltool, llvm-readobj and LLD. It does not
install dependencies or require a Windows SDK or Windows C runtime.

```sh
python3 build.py
python3 tests/test_glyphs.py
python3 -m unittest discover -s tests -p test_installer.py -v
```

The integration test uses your own matching `GBText.dll`, the compiled patch,
and the CrossOver bottle. It draws into memory in a separate process and never
launches the game. Installer tests use disposable copies and a fake registry.

```sh
python3 tests/test_glyphs.py --bottle "My Bottle" --game-dir "/path/to/Sengoku Rance"
python3 tests/test_glyphs.py --preview-dir "/tmp/rance-font-check"
```

The optional preview is a PGM image generated by the GDI probe, not a game
screenshot. `--dll /path/to/RanceTextFix.dll` can test a previous build.

Build defaults use `/opt/homebrew/opt/llvm/bin` and the existing Rust toolchain's
`lld-link`. Override `LLVM_PREFIX`, `CLANG`, `LLVM_DLLTOOL`, `LLVM_READOBJ`,
`LLD_LINK`, or `WINE_DLLS` as needed. PE timestamps are fixed at zero. The
installer pins the tested DLL hash; a different compiler's output must be
reviewed and tested before deliberately updating that hash. Import-library
stdcall suffixes are stripped from final PE import names, avoiding the
`LoadLibraryA@4` error encountered in an early diagnostic.

## Supported files

| File | SHA-256 |
| --- | --- |
| Input `Rance7.exe`, after the separate startup correction | `126b8e6fcda30fff82d8fad1e34e27c5cbe3f3257c4255c3a669870b0c550556` |
| Display-patched `Rance7.exe` | `20591ebd68be6ebe6a58d889f91554d92967e541837489b45e69d59ab01d2876` |
| Original `SACT_DLL.inc` | `40863d1f342d39ec7ab22d8595da3838270539d7b2842ccbdd5fb743161aacdf` |
| `GBText.dll` | `8e6640346d83bc757c2955763297d8497b346afcd9719fa02c813298dfa5cefe` |
| `DrawText2.dll` | `946481ca080cbbb3c407ac6276f7ca6c4e85f96d082f0556f7c73e8175e09912` |
| v0.1.1 `RanceTextFix.dll` | `4e5244a0f0245ce91a24c02aef77a5b50eb2c5380fc1fc540ef78699a51b1552` |

A fresh install changes byte `0x13bfa` in the recognized executable from `01`
to `00`, copies the patch DLL, and loads it immediately after `GBText.dll`
through `SACT_DLL.inc`. The DLL validates GBText's font-hook export address,
instruction signature and original function pointer before replacing its
cached `CreateFontIndirectW` pointer. Unexpected modules are left untouched.

## License

Patch code, installer, tests and documentation: [MIT](LICENSE), copyright
2026 Nick Gu. The game, translation, fonts, and separately obtained startup
correction retain their own licenses.
