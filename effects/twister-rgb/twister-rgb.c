#include "startup.h"
#include "blitter.h"
#include "coplist.h"
#include "ilbm.h"
#include "png.h"
#include "fx.h"
#include "memory.h"
#include "sprite.h"
#include "tasks.h"

STRPTR __cwdpath = "data";

#define WIDTH   144
#define HEIGHT  255
#define DEPTH   5
#define STARTX  96

static BitmapT *twister;
static PixmapT *texture;
static PaletteT *gradient;
static CopListT *cp[2];
static CopInsT *bplptr[2][DEPTH];
static CopInsT *bplmod[2][HEIGHT];
static CopInsT *colors[2][HEIGHT];
static WORD active = 0;

static SpriteT *left[2];
static SpriteT *right[2];
static CopInsT *sprptr[2][8];

static void Load() {
  twister = LoadILBMCustom("twister.ilbm", BM_DISPLAYABLE);
  texture = LoadPNG("twister-texture.png", PM_RGB12, MEMF_PUBLIC);
  gradient = LoadPalette("twister-gradient.ilbm");

  {
    BitmapT *_left = LoadILBMCustom("twister-left.ilbm", 0);
    BitmapT *_right = LoadILBMCustom("twister-right.ilbm", 0);

    left[0] = NewSpriteFromBitmap(256, _left, 0, 0);
    left[1] = NewSpriteFromBitmap(256, _left, 16, 0);
    right[0] = NewSpriteFromBitmap(256, _right, 0, 0);
    right[1] = NewSpriteFromBitmap(256, _right, 16, 0);

    DeleteBitmap(_right);
    DeleteBitmap(_left);
  }
}

static void UnLoad() {
  DeleteSprite(left[0]);
  DeleteSprite(left[1]);
  DeleteSprite(right[0]);
  DeleteSprite(right[1]);
  DeleteBitmap(twister);
  DeletePixmap(texture);
  DeletePalette(gradient);
}

static void MakeCopperList(CopListT **ptr, WORD n) {
  CopListT *cp = NewCopList(100 + HEIGHT * 5 + (31 * HEIGHT / 3));
  WORD *pixels = texture->pixels;
  WORD i, j, k;

  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(STARTX), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, bplptr[n], twister, DEPTH);
  CopSetupSprites(cp, sprptr[n]);
  CopMove16(cp, dmacon, DMAF_SETCLR|DMAF_RASTER);
  CopMove16(cp, diwstrt, 0x2c81);
  CopMove16(cp, diwstop, 0x2bc1);
  CopSetColor(cp, 0, &gradient->colors[0]);

  for (i = 0, k = 0; i < HEIGHT; i++) {
    CopWaitSafe(cp, Y(i), 0);
    bplmod[n][i] = CopMove16(cp, bpl1mod, -32);
    CopMove16(cp, bpl2mod, -32);
    CopMove16(cp, bpldat[0], 0);

    CopSetColor(cp, 0, &gradient->colors[i]);

    if ((i % 3) == 0) {
      colors[n][k++] = CopSetRGB(cp, 1, *pixels++);
      for (j = 2; j < 32; j++)
        CopSetRGB(cp, j, *pixels++);
    }
  }

  CopEnd(cp);

  CopInsSet32(sprptr[n][4], left[0]->data);
  CopInsSet32(sprptr[n][5], left[1]->data);
  CopInsSet32(sprptr[n][6], right[0]->data);
  CopInsSet32(sprptr[n][7], right[1]->data);

  *ptr = cp;
}

static void Init() {
  EnableDMA(DMAF_BLITTER);

  MakeCopperList(&cp[0], 0);
  MakeCopperList(&cp[1], 1);

  UpdateSprite(left[0], X(0), Y(0));
  UpdateSprite(left[1], X(16), Y(0));
  UpdateSprite(right[0], X(320 - 32), Y(0));
  UpdateSprite(right[1], X(320 - 16), Y(0));

  CopListActivate(cp[1]);
  EnableDMA(DMAF_RASTER | DMAF_SPRITE);
}

static void Kill() {
  DeleteCopList(cp[0]);
  DeleteCopList(cp[1]);
}

static inline WORD rotate(WORD f) {
  return (COS(f) >> 3) & 255;
}

static void SetupLines(WORD f) {
  WORD y0 = rotate(f);

  /* first line */
  {
    LONG y = (WORD)twister->bytesPerRow * y0;
    APTR *planes = twister->planes;
    CopInsT **bpl = bplptr[active];
    WORD n = DEPTH;

    while (--n >= 0)
      CopInsSet32(*bpl++, (*planes++) + y);
  }

  /* consecutive lines */
  {
    WORD **modptr = (WORD **)bplmod[active];
    WORD y1, i, m;

    for (i = 1; i < HEIGHT; i++, y0 = y1, f += 2) {
      WORD *mod = *modptr++;

      y1 = rotate(f);
      m = ((y1 - y0) - 1) * (WIDTH / 8);

      mod[1] = m;
      mod[3] = m;
    }
  }
}

static __regargs void SetupTexture(CopInsT **colors, WORD y) {
  WORD *pixels = texture->pixels;
  WORD height = texture->height;
  WORD width = texture->width;
  WORD n = height;

  y %= height;

  pixels += y * width;

  while (--n >= 0) {
    WORD *ins = (WORD *)(*colors++) + 1;

    ins[0] = *pixels++;
    ins[2] = *pixels++;
    ins[4] = *pixels++;
    ins[6] = *pixels++;
    ins[8] = *pixels++;
    ins[10] = *pixels++;
    ins[12] = *pixels++;
    ins[14] = *pixels++;
    ins[16] = *pixels++;
    ins[18] = *pixels++;
    ins[20] = *pixels++;
    ins[22] = *pixels++;
    ins[24] = *pixels++;
    ins[26] = *pixels++;
    ins[28] = *pixels++;
    ins[30] = *pixels++;
    ins[32] = *pixels++;
    ins[34] = *pixels++;
    ins[36] = *pixels++;
    ins[38] = *pixels++;
    ins[40] = *pixels++;
    ins[42] = *pixels++;
    ins[44] = *pixels++;
    ins[46] = *pixels++;
    ins[48] = *pixels++;
    ins[50] = *pixels++;
    ins[52] = *pixels++;
    ins[54] = *pixels++;
    ins[56] = *pixels++;
    ins[58] = *pixels++;
    ins[60] = *pixels++;

    y++;

    if (y >= height) {
      pixels = texture->pixels;
      y = 0;
    }
  }
}

static void Render() {
  // LONG lines = ReadLineCounter();
  SetupLines(frameCount * 16);
  SetupTexture(colors[active], frameCount);
  // Log("twister: %ld\n", ReadLineCounter() - lines);

  CopListRun(cp[active]);
  TaskWait(VBlankEvent);
  active ^= 1;
}

EffectT Effect = { Load, UnLoad, Init, Kill, Render };
