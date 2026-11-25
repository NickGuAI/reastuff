// SPDX-License-Identifier: MIT
/*
 * Sengoku Rance Simplified Chinese rendering fix, 32-bit Windows / Wine.
 *
 * The game's GBText.dll translates TextOutA to TextOutW and substitutes a
 * Chinese font, but forces the font's requested width to half its height.
 * DrawText2 renders each glyph at 3x size in a square bitmap; the selected
 * font's line metrics on this CrossOver bottle extend below that bitmap.
 *
 * This module is loaded immediately after the known GBText.dll. It replaces
 * only GBText's cached GDI function pointers. It neither patches Windows nor
 * changes font handling in other processes. The installer checks the exact
 * GBText file hash; the additional runtime checks here fail closed if a
 * different library or another hook is encountered.
 */
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned int UINT;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;
typedef int BOOL;
typedef void *HMODULE;
typedef void *HDC;
typedef void *HFONT;
typedef void *HGDIOBJ;

#define WIN __attribute__((stdcall))
#define IMP __declspec(dllimport)
#define TRUE 1
#define OBJ_BITMAP 7
#define OBJ_FONT 6
#define GGO_METRICS 0
#define GDI_ERROR 0xffffffffUL
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0

/* Win32 layouts, declared here so the small DLL can be built without an SDK. */
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
typedef struct {
    LONG type, width, height, widthBytes;
    unsigned short planes, bitsPixel;
    void *bits;
} BITMAP;
typedef struct { unsigned short fract; short value; } FIXED;
typedef struct { FIXED a, b, c, d; } MAT2;
typedef struct {
    UINT blackX, blackY;
    LONG originX, originY;
    short cellX, cellY;
} GLYPHMETRICS;

typedef BOOL (WIN *TextOutWProc)(HDC, int, int, const WCHAR *, int);
typedef HFONT (WIN *CreateFontWProc)(const LOGFONTW *);

IMP HMODULE WIN GetModuleHandleA(const char *);
IMP void *WIN GetProcAddress(HMODULE, const char *);
IMP BOOL WIN DisableThreadLibraryCalls(HMODULE);
IMP void WIN OutputDebugStringA(const char *);
IMP HGDIOBJ WIN GetCurrentObject(HDC, UINT);
IMP int WIN GetObjectW(HGDIOBJ, int, void *);
IMP BOOL WIN GetTextMetricsW(HDC, TEXTMETRICW *);
IMP DWORD WIN GetGlyphOutlineW(HDC, UINT, UINT, GLYPHMETRICS *, DWORD, void *, const MAT2 *);
#ifdef TRACE_GLYPH
IMP int wsprintfA(char *, const char *, ...);
#endif

static TextOutWProc realText;
static CreateFontWProc realFont;
static void **textSlot, **fontSlot;
static HMODULE gbModule;
static int loggedFont, loggedGlyph;

static int matches(const BYTE *actual, const BYTE *expected, int length) {
    int i;
    for (i = 0; i < length; ++i)
        if (actual[i] != expected[i]) return 0;
    return 1;
}

static HFONT WIN fixedFont(const LOGFONTW *font) {
    /* GBText changes a zero width into ceil(abs(height)/2). Undo that
       particular substitution, allowing GDI to use the font's natural width. */
    if (font && font->height < 0 && font->width > 0 &&
        font->width == (-font->height + 1) / 2) {
        LOGFONTW adjusted = *font;
        adjusted.width = 0;
        if (!loggedFont) {
            loggedFont = 1;
            OutputDebugStringA("RanceTextFix: natural font width applied\n");
        }
        return realFont(&adjusted);
    }
    return realFont(font);
}

static BOOL WIN fixedText(HDC dc, int x, int y, const WCHAR *text, int len) {
#ifdef TRACE_GLYPH
    static int samples;
    if (samples++ < 12) {
        char message[256];
        BITMAP b = {0}; TEXTMETRICW t = {0}; GLYPHMETRICS g = {0}; MAT2 m = {0};
        m.a.value = m.d.value = 1;
        int got = GetObjectW(GetCurrentObject(dc, OBJ_BITMAP), sizeof(b), &b);
        int met = GetTextMetricsW(dc, &t);
        DWORD glyph = (text && len) ? GetGlyphOutlineW(dc, text[0], 0, &g, 0, 0, &m) : GDI_ERROR;
        wsprintfA(message, "RanceTextFix TRACE: len=%d xy=%d,%d bitmap=%d,%d bpp=%d got=%d metrics=%d h=%d ascent=%d glyph=%lu ink=%d,%d origin=%d,%d\n",
                  len,x,y,b.width,b.height,b.bitsPixel,got,met,t.height,t.ascent,glyph,g.blackX,g.blackY,g.originX,g.originY);
        OutputDebugStringA(message);
    }
#endif
    /* The two renderers use square DIBs or padded 8-bit glyph DIBs. The latter
       are 80x73 for a 72-pixel font. Restrict correction to those small buffers
       using the selected font's requested height, not the font's line height. */
    if (dc && text && len == 1 && x == 0 && y == 0) {
        BITMAP bmp = {0};
        TEXTMETRICW tm = {0};
        GLYPHMETRICS gm = {0};
        LOGFONTW font = {0};
        MAT2 mat = {0};
        HGDIOBJ object = GetCurrentObject(dc, OBJ_BITMAP);
        HGDIOBJ selectedFont = GetCurrentObject(dc, OBJ_FONT);
        int cell;
        mat.a.value = mat.d.value = 1;
        if (object && GetObjectW(object, sizeof(bmp), &bmp) == sizeof(bmp) &&
            selectedFont && GetObjectW(selectedFont, sizeof(font), &font) == sizeof(font) &&
            font.height <= -24 && font.height >= -1536 &&
            (cell = -font.height) && bmp.height >= cell && bmp.height <= cell + 1 &&
            bmp.width >= cell && bmp.width <= cell + 15 &&
            GetTextMetricsW(dc, &tm) &&
            GetGlyphOutlineW(dc, text[0], GGO_METRICS, &gm, 0, 0, &mat) != GDI_ERROR) {
            int top = tm.ascent - gm.originY;
            int bottom = top + (int)gm.blackY;
            int right = gm.originX + (int)gm.blackX;
            if (gm.blackY > 0 && gm.blackY <= (UINT)cell) {
                if (tm.height > cell) y = -((tm.height - cell + 1) / 2);
                if (bottom + y > cell) y -= bottom + y - cell;
                if (top + y < 0) y = -top;
            }
            if (gm.blackX > 0 && gm.blackX <= (UINT)cell && right > cell) {
                x = cell - right;
                if (gm.originX + x < 0) x = -gm.originX;
            }
            if ((x || y) && !loggedGlyph) {
                loggedGlyph = 1;
                OutputDebugStringA("RanceTextFix: glyph placement corrected\n");
            }
        }
    }
    return realText(dc, x, y, text, len);
}

BOOL WIN DllMain(HMODULE self, DWORD reason, void *reserved) {
    static const BYTE textSignature[] = {0x55,0x57,0x56,0x53,0x81,0xec,0xfc,0x02,0x00,0x00};
    static const BYTE fontSignature[] = {0x57,0x53,0x81,0xec,0xc4,0x00,0x00,0x00};
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE gb = GetModuleHandleA("GBText.dll");
        HMODULE gdi = GetModuleHandleA("gdi32.dll");
        BYTE *textHook, *fontHook;
        DisableThreadLibraryCalls(self);
        if (!gb || !gdi) goto incompatible;
        textHook = (BYTE *)GetProcAddress(gb, "Hook_TextOutA@20");
        fontHook = (BYTE *)GetProcAddress(gb, "Hook_CreateFontIndirectA@4");
        if (textHook != (BYTE *)gb + 0x2a70 || fontHook != (BYTE *)gb + 0x2710 ||
            !matches(textHook, textSignature, sizeof(textSignature)) ||
            !matches(fontHook, fontSignature, sizeof(fontSignature))) goto incompatible;
        textSlot = (void **)((BYTE *)gb + 0x7224);
        fontSlot = (void **)((BYTE *)gb + 0x722c);
        realText = (TextOutWProc)GetProcAddress(gdi, "TextOutW");
        realFont = (CreateFontWProc)GetProcAddress(gdi, "CreateFontIndirectW");
        if (!realText || !realFont || *textSlot != (void *)realText ||
            *fontSlot != (void *)realFont) {
            textSlot = fontSlot = 0;
            goto incompatible;
        }
        gbModule = gb;
        *textSlot = (void *)fixedText;
        *fontSlot = (void *)fixedFont;
        OutputDebugStringA("RanceTextFix: hooks active\n");
    } else if (reason == DLL_PROCESS_DETACH && !reserved && gbModule &&
               GetModuleHandleA("GBText.dll") == gbModule) {
        if (textSlot && *textSlot == (void *)fixedText) *textSlot = (void *)realText;
        if (fontSlot && *fontSlot == (void *)fixedFont) *fontSlot = (void *)realFont;
    }
    return TRUE;
incompatible:
    OutputDebugStringA("RanceTextFix: incompatible GBText; no hooks installed\n");
    return TRUE;
}
