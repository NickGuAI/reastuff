// SPDX-License-Identifier: MIT
/* Repair the font at creation, shared by TextOutA and direct TextOutW callers.
 * v0.1.0 moved individual glyphs in one drawing path. That did not correct the
 * font's oversized line metrics and left other rendering paths inconsistent.
 * This version uses the native macOS Chinese family with cell-sized metrics.
 * It never changes text coordinates, decoding, or per-character placement.
 */
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;
typedef int BOOL;
typedef void *HMODULE;
typedef void *HDC;
typedef void *HFONT;
typedef void *HGDIOBJ;
#define WIN __attribute__((stdcall))
#define IMP __declspec(dllimport)

typedef struct {
    LONG height, width, escapement, orientation, weight;
    BYTE italic, underline, strike, charset, out, clip, quality, pitch;
    WCHAR face[32];
} LOGFONTW;
typedef struct {
    LONG height, ascent, descent, internal, external, avg, max, weight;
    LONG overhang, aspX, aspY;
    WCHAR first, last, def, brk;
    BYTE italic, underline, strike, pitch, charset;
} TEXTMETRICW;
_Static_assert(sizeof(LOGFONTW) == 92, "32-bit LOGFONTW layout");
_Static_assert(sizeof(TEXTMETRICW) == 60, "32-bit TEXTMETRICW layout");

typedef HFONT (WIN *CreateFontWProc)(const LOGFONTW *);
IMP HMODULE WIN GetModuleHandleA(const char *);
IMP void *WIN GetProcAddress(HMODULE, const char *);
IMP BOOL WIN DisableThreadLibraryCalls(HMODULE);
IMP void WIN OutputDebugStringA(const char *);
IMP HDC WIN CreateCompatibleDC(HDC);
IMP HGDIOBJ WIN SelectObject(HDC, HGDIOBJ);
IMP BOOL WIN DeleteObject(HGDIOBJ);
IMP BOOL WIN DeleteDC(HDC);
IMP BOOL WIN GetTextMetricsW(HDC, TEXTMETRICW *);
IMP int WIN GetTextFaceW(HDC, int, WCHAR *);

static const WCHAR nativeFace[] = {'H','e','i','t','i',' ','S','C',0};
static const WCHAR localizedFace[] = {0x9ed1,0x4f53,'-',0x7b80,0};
static CreateFontWProc realFont;
static void **fontSlot;
static HMODULE gbModule;
static int loggedFont, loggedUnavailable;

static int equalFace(const WCHAR *a, const WCHAR *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static HFONT WIN fixedFont(const LOGFONTW *font) {
    /* Undo GBText's width substitution and select one family for all glyphs.
       Keep the requested size and weight, so dialogue and menus share metrics. */
    if (font && font->height < 0 && font->height >= -1536 &&
        font->width == (-font->height + 1) / 2) {
        LOGFONTW adjusted = *font;
        TEXTMETRICW metrics = {0};
        WCHAR actualFace[32] = {0};
        HDC dc;
        HFONT result;
        HGDIOBJ previous;
        int i, valid = 0;
        adjusted.width = 0;
        for (i = 0; i < 32; ++i)
            adjusted.face[i] = i < 9 ? nativeFace[i] : 0;
        result = realFont(&adjusted);
        dc = result ? CreateCompatibleDC(0) : 0;
        if (dc) {
            previous = SelectObject(dc, result);
            valid = previous &&
                GetTextFaceW(dc, 32, actualFace) &&
                (equalFace(actualFace, nativeFace) || equalFace(actualFace, localizedFace)) &&
                GetTextMetricsW(dc, &metrics) && metrics.height == -font->height;
            if (previous) SelectObject(dc, previous);
            DeleteDC(dc);
        }
        if (valid) {
            if (!loggedFont) {
                loggedFont = 1;
                OutputDebugStringA("RanceTextFix: native Chinese font metrics applied\n");
            }
            return result;
        }
        if (result) DeleteObject(result);
        if (!loggedUnavailable) {
            loggedUnavailable = 1;
            OutputDebugStringA("RanceTextFix: Heiti SC unavailable or incompatible; original font preserved\n");
        }
    }
    return realFont(font);
}

BOOL WIN DllMain(HMODULE self, DWORD reason, void *reserved) {
    static const BYTE signature[] = {0x57,0x53,0x81,0xec,0xc4,0x00,0x00,0x00};
    if (reason == 1) {
        HMODULE gb = GetModuleHandleA("GBText.dll");
        HMODULE gdi = GetModuleHandleA("gdi32.dll");
        BYTE *hook;
        unsigned int i;
        DisableThreadLibraryCalls(self);
        if (!gb || !gdi) goto incompatible;
        hook = (BYTE *)GetProcAddress(gb, "Hook_CreateFontIndirectA@4");
        if (hook != (BYTE *)gb + 0x2710) goto incompatible;
        for (i = 0; i < sizeof(signature); ++i)
            if (hook[i] != signature[i]) goto incompatible;
        realFont = (CreateFontWProc)GetProcAddress(gdi, "CreateFontIndirectW");
        fontSlot = (void **)((BYTE *)gb + 0x722c);
        if (!realFont || *fontSlot != (void *)realFont) {
            fontSlot = 0;
            goto incompatible;
        }
        gbModule = gb;
        *fontSlot = (void *)fixedFont;
        OutputDebugStringA("RanceTextFix: font hook active\n");
    } else if (reason == 0 && !reserved && gbModule &&
               GetModuleHandleA("GBText.dll") == gbModule) {
        if (fontSlot && *fontSlot == (void *)fixedFont) *fontSlot = (void *)realFont;
    }
    return 1;
incompatible:
    OutputDebugStringA("RanceTextFix: incompatible GBText; no hook installed\n");
    return 1;
}
