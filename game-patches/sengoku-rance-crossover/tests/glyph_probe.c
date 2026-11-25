// SPDX-License-Identifier: MIT
/* Integration probe: genuine GBText, real Wine GDI, isolated memory bitmaps. */
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned int UINT;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HDC;
typedef void *HFONT;
typedef void *HGDIOBJ;
#define WIN __attribute__((stdcall))
#define IMP __declspec(dllimport)
typedef struct {
    LONG h,w,e,o,weight;
    BYTE italic,underline,strike,charset,out,clip,quality,pitch;
    char face[32];
} LOGFONTA;
typedef struct {
    DWORD size;
    LONG width,height;
    WORD planes,bpp;
    DWORD compression,image;
    LONG xppm,yppm;
    DWORD used,important,colors[256];
} BITMAPINFO;
IMP HANDLE WIN GetStdHandle(DWORD);
IMP BOOL WIN WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
IMP void WIN ExitProcess(UINT);
IMP HANDLE WIN LoadLibraryA(const char*);
IMP HDC WIN CreateCompatibleDC(HDC);
IMP HFONT WIN CreateFontIndirectA(const LOGFONTA*);
IMP HGDIOBJ WIN SelectObject(HDC,HGDIOBJ);
IMP BOOL WIN DeleteObject(HGDIOBJ);
IMP BOOL WIN DeleteDC(HDC);
IMP HGDIOBJ WIN CreateDIBSection(HDC,const BITMAPINFO*,UINT,void**,HANDLE,DWORD);
IMP int WIN SetBkMode(HDC,int);
IMP DWORD WIN SetTextColor(HDC,DWORD);
IMP BOOL WIN TextOutA(HDC,int,int,const char*,int);
IMP BOOL WIN GdiFlush(void);
IMP int wsprintfA(char*,const char*,...);

/* Clang may lower large aggregate initialization to memset. No CRT needed. */
void *memset(void *dest, int value, unsigned int count) {
    volatile BYTE *bytes = (volatile BYTE *)dest;
    unsigned int i;
    for (i = 0; i < count; ++i) bytes[i] = (BYTE)value;
    return dest;
}

static void print(const char *s) {
    DWORD n=0,l=0;
    while(s[l]) ++l;
    WriteFile(GetStdHandle((DWORD)-11),s,l,&n,0);
}

static void copy(char *d,const char *s) {
    while((*d++=*s++));
}

static void probe(const char *label,const char *s,int len,int textsize,int rows,int bpp,int padded) {
    int x,y,first=-1,last=-1,inside=0,outside=0,all=0,minx=999,maxx=-1;
    int cell=3*textsize, width=padded ? (cell+15)&~15 : cell;
    int height=cell*rows+(padded ? 1 : 0), stride=((width*bpp+31)/32)*4;
    char out[320];
    void *data=0;
    LOGFONTA lf={0};
    BITMAPINFO bmi={0};
    HDC dc=CreateCompatibleDC(0);
    HGDIOBJ bmp,oldbmp,oldfont;
    HFONT font;
    bmi.size=40;
    bmi.width=width;
    bmi.height=-height;
    bmi.planes=1;
    bmi.bpp=(WORD)bpp;
    for(x=0;x<256;x++) bmi.colors[x]=(DWORD)(x*0x010101);
    if(bpp==1) bmi.colors[1]=0xffffff;
    bmp=CreateDIBSection(dc,&bmi,0,&data,0,0);
    if(!dc || !bmp || !data) { print("DIB creation failed\r\n"); ExitProcess(3); }
    oldbmp=SelectObject(dc,bmp);
    lf.h=-cell;
    lf.weight=400;
    lf.charset=134;
    lf.pitch=1;
    copy(lf.face,"Songti");
    font=CreateFontIndirectA(&lf);
    if(!font) { print("Font creation failed\r\n"); ExitProcess(4); }
    oldfont=SelectObject(dc,font);
    SetBkMode(dc,1);
    SetTextColor(dc,0x00ffffff);
    if(!TextOutA(dc,0,0,s,len)) { print("Text drawing failed\r\n"); ExitProcess(5); }
    GdiFlush();
    for(y=0;y<height;y++) for(x=0;x<width;x++) {
        BYTE *row=(BYTE*)data+y*stride;
        DWORD ink=bpp==32 ? ((DWORD*)row)[x]&0xffffff :
                  bpp==8 ? row[x] : (row[x/8]>>(7-x%8))&1;
        if(ink) {
            if(first<0)first=y;
            last=y; all++;
            if(y>=cell)outside++; else inside++;
            if(x<minx)minx=x;
            if(x>maxx)maxx=x;
        }
    }
    wsprintfA(out,"%s rows=%d size=%d cell=%d ink_y=%d..%d ink_x=%d..%d total=%d visible=%d clipped_bottom=%d\r\n",
              label,rows,textsize,cell,first,last,minx,maxx,all,inside,outside);
    print(out);
    SelectObject(dc,oldfont);
    SelectObject(dc,oldbmp);
    DeleteObject(font); DeleteObject(bmp); DeleteDC(dc);
}

static void run(void) {
    int r;
    for(r=1;r<=2;r++) {
        probe("zhong","\xd6\xd0",2,18,r,32,0);
        probe("guo","\xb9\xfa",2,18,r,32,0);
        probe("wo","\xce\xd2",2,18,r,32,0);
        probe("men","\xc3\xc7",2,18,r,32,0);
        probe("small","\xd6\xd0",2,12,r,32,0);
        probe("large","\xd6\xd0",2,24,r,32,0);
        probe("ascii","A",1,18,r,32,0);
        probe("padded_guo","\xb9\xfa",2,24,r,8,1);
        probe("padded_zhong","\xd6\xd0",2,24,r,8,1);
        probe("padded_ascii","A",1,24,r,8,1);
        probe("mono_guo","\xb9\xfa",2,18,r,1,0);
    }
}

void mainCRTStartup(void) {
    if(!LoadLibraryA("DLL\\GBText.dll")) { print("GBText load failed\r\n"); ExitProcess(2); }
    print("GBText only\r\n"); run();
    if(!LoadLibraryA("DLL\\RanceTextFix.dll")) { print("Patch load failed\r\n"); ExitProcess(2); }
    print("GBText plus patch\r\n"); run();
    ExitProcess(0);
}
