#include "startup.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "blitter.h"
#include "circle.h"
#include "fx.h"
#include "2d.h"
#include "random.h"
#include "sprite.h"
#include "tasks.h"

STRPTR __cwdpath = "data";

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 4
#define HSIZE 32
#define VSIZE 10

#define LANE_W (WIDTH + HSIZE * 2)
#define LANE_H 40
#define CARS 50

#define LANEL_Y (HEIGHT / 2 - LANE_H - 16)
#define LANER_Y (HEIGHT / 2 + 16)

typedef struct {
  WORD speed;
  WORD x, y;
  BOOL active;
  BOOL side;
} Car;

static Car cars[CARS];

static CopListT *cp;
static UWORD active = 0;
static CopInsT *sprptr[8];
static CopInsT *bplptr[2][DEPTH];
static BitmapT *carry;

static BitmapT *lanes[2];
static BitmapT *carLeft;
static BitmapT *carRight;
static BitmapT *laneBg;
static BitmapT *cityTop;
static BitmapT *cityBottom;
static SpriteT *sprite[8];
static PaletteT *spritePal;

static void Load() {
  laneBg = LoadILBM("highway-lane.ilbm");
  cityTop = LoadILBM("highway-city-top-2.ilbm");
  cityBottom = LoadILBM("highway-city-bottom-2.ilbm");
  carLeft = LoadILBM("highway-car-left-2.ilbm");
  carRight = LoadILBM("highway-car-right-2.ilbm");

  {
    BitmapT *title = LoadILBM("highway-sprite.ilbm");
    ITER(i, 0, 7, sprite[i] = NewSpriteFromBitmap(24, title, 16 * i, 0));
    spritePal = title->palette;
    DeleteBitmap(title);
  }
}

static void UnLoad() {
  ITER(i, 0, 7, DeleteSprite(sprite[i]));

  DeletePalette(carLeft->palette);
  DeleteBitmap(carLeft);
  DeletePalette(carRight->palette);
  DeleteBitmap(carRight);
  DeletePalette(cityTop->palette);
  DeleteBitmap(cityTop);
  DeletePalette(cityBottom->palette);
  DeleteBitmap(cityBottom);
  DeletePalette(laneBg->palette);
  DeleteBitmap(laneBg);
  DeletePalette(spritePal);
}

static void MakeCopperList(CopListT *cp) {
  CopInit(cp);
  CopSetupSprites(cp, sprptr);
  CopLoadPal(cp, spritePal, 16);
  CopLoadPal(cp, spritePal, 20);
  CopLoadPal(cp, spritePal, 24);
  CopLoadPal(cp, spritePal, 28);

  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, NULL, cityTop, DEPTH);
  CopWait(cp, Y(-18), 0);
  CopLoadPal(cp, cityTop->palette, 0);

  CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);

  {
    CopWait(cp, Y(LANEL_Y - 2), 8);
    CopMove16(cp, dmacon, DMAF_RASTER);
    CopLoadPal(cp, carLeft->palette, 0);
    CopSetupBitplanes(cp, bplptr[0], lanes[active], DEPTH);
    CopMove16(cp, bpl1mod, 8);
    CopMove16(cp, bpl2mod, 8);

    CopWait(cp, Y(LANEL_Y), 8);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);

    CopWait(cp, Y(LANEL_Y + LANE_H), 8);
    CopMove16(cp, dmacon, DMAF_RASTER);
  }

  // use an undocumented trick to make sprites visible while bitplanes are off
  {
    WORD y0 = LANEL_Y + LANE_H + 1;
    WORD y1 = LANER_Y - 2;

    while (y0 < y1) {
      CopWait(cp, Y(y0), X(-12));
      CopMove16(cp, bpldat[0], 0);
      y0++;
    }
  }

  {
    CopWait(cp, Y(LANER_Y - 1), 8);
    CopLoadPal(cp, carRight->palette, 0);
    CopSetupBitplanes(cp, bplptr[1], lanes[active], DEPTH);
    CopMove16(cp, bpl1mod, 8);
    CopMove16(cp, bpl2mod, 8);

    CopWait(cp, Y(LANER_Y), 8);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
    CopWait(cp, Y(LANER_Y + LANE_H), 8);
    CopMove16(cp, dmacon, DMAF_RASTER);
  }

  {
    CopWait(cp, Y(LANER_Y + LANE_H + 1), 8);
    CopLoadPal(cp, cityBottom->palette, 0);
    CopSetupBitplanes(cp, NULL, cityBottom, DEPTH);
    CopWait(cp, Y(LANER_Y + LANE_H + 2), 8);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
  }

  CopEnd(cp);

  ITER(i, 0, 7, CopInsSet32(sprptr[i], sprite[i]->data));
}

static void Init() {
  lanes[0] = NewBitmap(LANE_W, LANE_H * 2, DEPTH);
  lanes[1] = NewBitmap(LANE_W, LANE_H * 2, DEPTH);
  carry = NewBitmap(HSIZE + 16, VSIZE, 2);

  cp = NewCopList(300);
  MakeCopperList(cp);
  CopListActivate(cp);
  EnableDMA(DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE);

  ITER(i, 0, 7, UpdateSprite(sprite[i], X(96 + 16 * i), Y(LANEL_Y + LANE_H + 4)));
}

static void Kill() {
  DisableDMA(DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE);

  DeleteBitmap(lanes[0]);
  DeleteBitmap(lanes[1]);
  DeleteBitmap(carry);
  DeleteCopList(cp);
}

static inline void CarInit(Car *car) {
  car->speed = random() & 15;
  car->x = 0;
  car->y = (random() & 3) * 10;
  car->active = TRUE;
  car->side = random() & 1;
}

static inline void CarMove(Car *car, WORD step) {
  car->x += (car->speed + 32) * step;
  if (car->x >= fx4i(LANE_W))
    car->active = FALSE;
}

/* Add new car if there's a free slot. */
static inline void AddCar() {
  Car *car = cars;
  Car *last = cars + CARS;

  do {
    if (!car->active) {
      CarInit(car);
      break;
    }
  } while (++car < last);
}

/* Draw each active cars. */
static void DrawCars(WORD step) {
  Car *car = cars;
  Car *last = cars + CARS;

  do {
    if (car->active) {
      WORD x = (car->x + 7) / 16;

      if (car->side)
        BitmapAddSaturated(lanes[active], x, car->y + LANE_H, carRight, carry);
      else
        BitmapAddSaturated(lanes[active], LANE_W - x, car->y, carLeft, carry);

      CarMove(car, step);
    }
  } while (++car < last);
}

static void AddCars() {
  static WORD iterCount = 0;

  iterCount += frameCount - lastFrameCount;
  while (iterCount > 10) {
    AddCar();
    iterCount -= 10;
  }
}

static void Render() {
  BitmapCopy(lanes[active], HSIZE, 0, laneBg);
  BitmapCopy(lanes[active], HSIZE, LANE_H, laneBg);

  AddCars();
  DrawCars(frameCount - lastFrameCount);

  {
    WORD i;

    for (i = 0; i < DEPTH; i++) {
      APTR bplpt = lanes[active]->planes[i] + 4;
      UWORD stride = lanes[active]->bytesPerRow;
      CopInsSet32(bplptr[0][i], bplpt);
      CopInsSet32(bplptr[1][i], bplpt + stride * LANE_H);
    }
  }

  TaskWait(VBlankEvent);
  active ^= 1;
}

EffectT Effect = { Load, UnLoad, Init, Kill, Render };
