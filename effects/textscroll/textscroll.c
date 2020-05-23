#include "startup.h"
#include "hardware.h"
#include "blitter.h"
#include "coplist.h"
#include "gfx.h"
#include "memory.h"
#include "io.h"
#include "ilbm.h"
#include "reader.h"
#include "tasks.h"

STRPTR __cwdpath = "data";

#define WIDTH 640
#define HEIGHT 256
#define DEPTH 1

#define SIZE 8

#define COLUMNS (WIDTH / SIZE)
#define LINES   (HEIGHT / SIZE)

static char *text;

static WORD active = 0;

static CopListT *cp[2];
static CopInsT *linebpl[2][HEIGHT];
static BitmapT *scroll;
static BitmapT *font;

static WORD last_line = -1;
static char *line_start;

static void Load() {
  text = LoadFile("text-scroll.txt", MEMF_PUBLIC);
  font = LoadILBMCustom("text-scroll-font.ilbm", BM_LOAD_PALETTE);
}

static void UnLoad() {
  MemFree(text);
  DeletePalette(font->palette);
  DeleteBitmap(font);
}

static CopListT *MakeCopperList(WORD n) {
  CopListT *cp = NewCopList(100 + 3 * HEIGHT);
  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_HIRES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, NULL, scroll, DEPTH);

  CopLoadPal(cp, font->palette, 0);
  {
    UWORD i;
    APTR ptr = scroll->planes[0];

    for (i = 0; i < HEIGHT; i++, ptr += scroll->bytesPerRow) {
      CopWaitSafe(cp, Y(i), 0);
      linebpl[n][i] = CopMove32(cp, bplpt[0], ptr);
    }
  }
  CopEnd(cp);
  return cp;
}

static void Init() {
  scroll = NewBitmap(WIDTH, HEIGHT + 16, 1);

  EnableDMA(DMAF_BLITTER);
  BitmapClear(scroll);

  line_start = text;

  cp[0] = MakeCopperList(0);
  cp[1] = MakeCopperList(1);

  CopListActivate(cp[active]);

  EnableDMA(DMAF_RASTER);
}

static void Kill() {
  DeleteCopList(cp[0]);
  DeleteCopList(cp[1]);
  DeleteBitmap(scroll);
}

static void RenderLine(UBYTE *dst, UBYTE *line, WORD size) {
  WORD dwidth = scroll->bytesPerRow;
  WORD swidth = font->bytesPerRow;
  UBYTE *src = font->planes[0];
  WORD x = 0;

  while (--size >= 0) {
    WORD i = (*line++) - 32;
    WORD j = x++;
    WORD h = 8;

    if (i < 0)
      continue;

    while (--h >= 0) {
      dst[j] = src[i];
      i += swidth;
      j += dwidth;
    }
  }
}

static void SetupLinePointers() {
  CopInsT **ins = linebpl[active];
  APTR plane = scroll->planes[0];
  LONG stride = scroll->bytesPerRow;
  LONG bplsize = scroll->bplSize;
  WORD y = (LONG)(frameCount / 2 + 8) % (WORD)scroll->height;
  APTR start = plane + (WORD)stride * y;
  APTR end = plane + bplsize;
  WORD n = HEIGHT;

  while (--n >= 0) {
    if (start >= end)
      start -= bplsize;
    CopInsSet32(*ins++, start);
    start += stride;
  }
}

static void RenderNextLineIfNeeded() {
  Area2D rect = {0, 0, WIDTH, SIZE};
  WORD s = frameCount / 16;

  if (s > last_line) {
    APTR ptr = scroll->planes[0];
    WORD line_num = (s % (LINES + 2)) * SIZE;
    char *line_end;
    WORD size;

    line_end = line_start;
    SkipLine(&line_end);
    size = (line_end - line_start) - 1;

    ptr += line_num * scroll->bytesPerRow;

    rect.y = line_num;
    BitmapClearArea(scroll, &rect);
    WaitBlitter();
    RenderLine(ptr, line_start, min(size, COLUMNS));

    last_line = s;
    SkipLine(&line_start);
  }
}

static void Render() {
  SetupLinePointers();
  RenderNextLineIfNeeded();

  CopListRun(cp[active]);
  TaskWait(VBlankEvent);
  active ^= 1;
}

EffectT Effect = { Load, UnLoad, Init, Kill, Render };
