// SPDX-License-Identifier: MIT
/* Compare real GBText/Windows GDI glyphs with a padded, unclipped reference.
 * Exercise ANSI drawing, Unicode drawing, and the game's Unicode call that
 * includes a trailing NUL for a two-byte Chinese source character.
 */
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned int UINT;
typedef unsigned short WORD;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HDC;
typedef void *HFONT;
typedef void *HGDIOBJ;
#define WIN __attribute__((stdcall))
#define IMP __declspec(dllimport)
typedef struct {
    LONG h, w, e, o, weight;
    BYTE italic, underline, strike, charset, out, clip, quality, pitch;
    char face[32];
} LOGFONTA;
typedef struct {
    DWORD size;
    LONG width, height;
    WORD planes, bpp;
    DWORD compression, image;
    LONG xppm, yppm;
    DWORD used, important, colors[256];
} BITMAPINFO;
typedef struct {
    HDC dc;
    HGDIOBJ bmp, oldbmp, oldfont;
    BYTE *bits;
    int width, height, stride, bpp;
} DIB;
IMP HANDLE WIN GetStdHandle(DWORD);
IMP BOOL WIN WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
IMP void WIN ExitProcess(UINT);
IMP HANDLE WIN LoadLibraryA(const char *);
IMP HDC WIN CreateCompatibleDC(HDC);
IMP HFONT WIN CreateFontIndirectA(const LOGFONTA *);
IMP HGDIOBJ WIN SelectObject(HDC, HGDIOBJ);
IMP BOOL WIN DeleteObject(HGDIOBJ);
IMP BOOL WIN DeleteDC(HDC);
IMP HGDIOBJ WIN CreateDIBSection(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
IMP int WIN SetBkMode(HDC, int);
IMP DWORD WIN SetTextColor(HDC, DWORD);
IMP BOOL WIN TextOutA(HDC, int, int, const char *, int);
IMP BOOL WIN TextOutW(HDC, int, int, const WCHAR *, int);
IMP BOOL WIN GdiFlush(void);
IMP int wsprintfA(char *, const char *, ...);
IMP int WIN WideCharToMultiByte(UINT, DWORD, const WCHAR *, int, char *, int, const char *, BOOL *);
IMP HANDLE WIN CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
IMP BOOL WIN CloseHandle(HANDLE);
void *memset(void *d, int v, unsigned int n) {
    volatile BYTE *b = d;
    for (UINT i = 0; i < n; i++)
        b[i] = (BYTE)v;
    return d;
}
static void print(const char *s) {
    DWORD n = 0, l = 0;
    while (s[l])
        l++;
    WriteFile(GetStdHandle((DWORD)-11), s, l, &n, 0);
}
static void fatal(const char *s) {
    print(s);
    ExitProcess(2);
}
static HFONT makeFont(int size, int weight) {
    LOGFONTA lf = {.h = -size * 3, .weight = weight, .charset = 134,
                  .pitch = 1, .face = "Songti"};
    HFONT font = CreateFontIndirectA(&lf);
    if (!font)
        fatal("font failed\r\n");
    return font;
}
static DIB make(int width, int height, int bpp, HFONT font) {
    DIB d = {0};
    BITMAPINFO bi = {0};
    bi.size = 40;
    bi.width = width;
    bi.height = -height;
    bi.planes = 1;
    bi.bpp = bpp;
    for (int i = 0; i < 256; i++)
        bi.colors[i] = i * 0x010101;
    if (bpp == 1)
        bi.colors[1] = 0xffffff;
    d.dc = CreateCompatibleDC(0);
    d.bmp = CreateDIBSection(d.dc, &bi, 0, (void **)&d.bits, 0, 0);
    if (!d.dc || !d.bmp || !d.bits)
        fatal("DIB failed\r\n");
    d.width = width;
    d.height = height;
    d.bpp = bpp;
    d.stride = ((width * bpp + 31) / 32) * 4;
    d.oldbmp = SelectObject(d.dc, d.bmp);
    d.oldfont = SelectObject(d.dc, font);
    SetBkMode(d.dc, 1);
    SetTextColor(d.dc, 0xffffff);
    return d;
}
static void destroy(DIB *d) {
    SelectObject(d->dc, d->oldfont);
    SelectObject(d->dc, d->oldbmp);
    DeleteObject(d->bmp);
    DeleteDC(d->dc);
}
static int pixel(DIB *d, int x, int y) {
    BYTE *row = d->bits + y * d->stride;
    return d->bpp == 32  ? (((DWORD *)row)[x] & 255)
           : d->bpp == 8 ? row[x]
                         : ((row[x / 8] >> (7 - x % 8)) & 1) * 255;
}
static void clear(DIB *d) { memset(d->bits, 0, d->stride * d->height); }
static void draw(DIB *d, WCHAR ch, int api, int x, int y) {
    WCHAR w[2] = {ch, 0};
    char a[8] = {0};
    int n = WideCharToMultiByte(936, 0, w, 1, a, 8, 0, 0);
    BOOL ok = api == 0 ? TextOutA(d->dc, x, y, a, n) : TextOutW(d->dc, x, y, w, api);
    if (!ok)
        fatal("draw failed\r\n");
    GdiFlush();
}
static const WCHAR chars[] = L"他身旁材娇小的女孩则截然相反，手忙脚乱显得很慌张。读取前往专用HPCG鉴"
                             L"赏模式音乐环境设置返回标题画面结束游戏保存";
/* A reference at (cell, cell) exposes clipping at all four edges. Exact pixel
 * equality also detects baseline shifts that an ink-count-only test misses. */
static void run(const char *phase) {
    int cases = 0, shift = 0, clip = 0, empty = 0, shown = 0;
    char out[256];
    for (int size = 12; size <= 24; size += 6)
        for (int weight = 400; weight <= 700; weight += 300) {
            int cell = size * 3;
            HFONT f = makeFont(size, weight);
            int prevclip = clip;
            for (int shape = 0; shape < 2; shape++)
                for (int b = 0; b < 3; b++)
                    for (int api = 0; api < 3; api++) {
                        int bpp = b == 0 ? 1 : b == 1 ? 8 : 32;
                        int width = shape ? ((cell + 15) & ~15) : cell;
                        DIB small = make(width, cell + shape, bpp, f),
                            ref = make(cell * 3, cell * 3, bpp, f);
                        for (int c = 0; chars[c]; c++) {
                            clear(&small);
                            clear(&ref);
                            /* The trailing NUL is not part of the requested glyph.
                               ASCII uses length one in the actual game. */
                            draw(&small, chars[c], api == 2 && chars[c] < 128 ? 1 : api, 0, 0);
                            draw(&ref, chars[c], api == 2 ? 1 : api, cell, cell);
                            int difference = 0, outside = 0, ink = 0;
                            for (int y = 0; y < ref.height; y++)
                                for (int x = 0; x < ref.width; x++)
                                    if (pixel(&ref, x, y)) {
                                        ink++;
                                        if (x < cell || x >= cell * 2 || y < cell || y >= cell * 2)
                                            outside++;
                                    }
                            for (int y = 0; y < cell; y++)
                                for (int x = 0; x < cell; x++)
                                    if (pixel(&small, x, y) != pixel(&ref, x + cell, y + cell))
                                        difference++;
                            cases++;
                            shift += difference > 0;
                            clip += outside > 0;
                            empty += ink == 0;
                            if ((difference || outside) && shown++ < 8) {
                                wsprintfA(out,
                                          "%s issue U+%04X size=%d weight=%d bpp=%d shape=%d "
                                          "api=%d shift=%d outside=%d\r\n",
                                          phase, chars[c], size, weight, bpp, shape, api,
                                          difference, outside);
                                print(out);
                            }
                        }
                        destroy(&small);
                        destroy(&ref);
                    }
            wsprintfA(out, "%s size=%d weight=%d clipped=%d\r\n", phase, size, weight,
                      clip - prevclip);
            print(out);
            DeleteObject(f);
        }
    wsprintfA(out, "%s cases=%d changed=%d clipped=%d empty=%d\r\n", phase, cases, shift, clip,
              empty);
    print(out);
}

static BYTE canvas[800 * 360];
static const WCHAR line0[] = L"他身旁身材娇小的女孩则截然相反，";
static const WCHAR line1[] = L"手忙脚乱，显得很慌张。";
static const WCHAR line2[] = L"读取";
static const WCHAR line3[] = L"前往专用HP";
static const WCHAR line4[] = L"CG鉴赏模式";
static const WCHAR line5[] = L"音乐模式";
static const WCHAR line6[] = L"环境设置";
static const WCHAR line7[] = L"返回标题画面";
static const WCHAR line8[] = L"结束游戏";
static const WCHAR line9[] = L"保存";

static void line(const WCHAR *text, int y, int size, int weight, int api) {
    HFONT f = makeFont(size, weight);
    DIB d = make((size * 3 + 15) & ~15, size * 3 + 1, 8, f);
    int start = 16;
    for (int c = 0; text[c]; c++) {
        clear(&d);
        draw(&d, text[c], api == 2 && text[c] < 128 ? 1 : api, 0, 0);
        for (int py = 0; py < size; py++)
            for (int px = 0; px < size; px++) {
                int sum = 0;
                for (int dy = 0; dy < 3; dy++)
                    for (int dx = 0; dx < 3; dx++)
                        sum += pixel(&d, px * 3 + dx, py * 3 + dy);
                if (start + px < 800 && y + py < 360)
                    canvas[(y + py) * 800 + start + px] = 255 - sum / 9;
            }
        start += text[c] < 128 ? size * 2 / 3 : size;
    }
    destroy(&d);
    DeleteObject(f);
}
static void sample(const char *path) {
    memset(canvas, 255, sizeof canvas);
    line(line0, 10, 24, 700, 0);
    line(line1, 42, 24, 700, 0);
    line(line2, 90, 18, 400, 2);
    line(line3, 118, 18, 400, 2);
    line(line4, 146, 18, 400, 2);
    line(line5, 174, 18, 400, 2);
    line(line6, 202, 18, 400, 2);
    line(line7, 230, 18, 400, 2);
    line(line8, 258, 18, 400, 2);
    line(line9, 286, 18, 400, 2);
    HANDLE file = CreateFileA(path, 0x40000000, 0, 0, 2, 0x80, 0);
    DWORD written;
    if (file == (HANDLE)-1)
        fatal("preview create failed\r\n");
    if (!WriteFile(file, "P5\n800 360\n255\n", 15, &written, 0) || written != 15 ||
        !WriteFile(file, canvas, sizeof canvas, &written, 0) || written != sizeof canvas)
        fatal("preview write failed\r\n");
    CloseHandle(file);
}
void mainCRTStartup(void) {
    if (!LoadLibraryA("DLL\\GBText.dll"))
        fatal("GBText load failed\r\n");
    run("before");
    if (!LoadLibraryA("DLL\\RanceTextFix.dll"))
        fatal("patch load failed\r\n");
    run("after");
    sample("after.pgm");
    ExitProcess(0);
}
