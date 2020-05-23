#include "startup.h"
#include "blitter.h"
#include "coplist.h"
#include "line.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 1

#define CPULINE 0

static BitmapT *screen;
static CopListT *cp;

static void Load() {
  screen = NewBitmap(WIDTH, HEIGHT, DEPTH);
  cp = NewCopList(100);
  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, NULL, screen, DEPTH);
  CopSetRGB(cp, 0, 0x000);
  CopSetRGB(cp, 1, 0xfff);
  CopEnd(cp);
}

static void UnLoad() {
  DeleteCopList(cp);
  DeleteBitmap(screen);
}

static void Init() {
  CopListActivate(cp);
  EnableDMA(DMAF_BLITTER | DMAF_RASTER | DMAF_BLITHOG);

  {
    WORD i;
    LONG lines = ReadLineCounter();

#if CPULINE == 1
    CpuLineSetup(screen, 0);
#else
    BlitterLineSetup(screen, 0, LINE_OR|LINE_SOLID);
#endif

    for (i = 0; i < screen->width; i += 2) {
#if CPULINE == 1
      CpuLine(i, 0, screen->width - 1 - i, screen->height - 1);
#else
      BlitterLine(i, 0, screen->width - 1 - i, screen->height - 1);
#endif
    }

    for (i = 0; i < screen->height; i += 2) {
#if CPULINE == 1
      CpuLine(0, i, screen->width - 1, screen->height - 1 - i);
#else
      BlitterLine(0, i, screen->width - 1, screen->height - 1 - i);
#endif
    }

    Log("lines: %ld\n", ReadLineCounter() - lines);
  }
}

EffectT Effect = { Load, UnLoad, Init, NULL, NULL };
