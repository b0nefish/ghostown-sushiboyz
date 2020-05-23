#include <graphics/view.h>

#include "config.h"
#include "iff.h"
#include "ilbm.h"
#include "memory.h"
#if USE_LZO
#include "lzo.h"
#endif
#include "inflate.h"

#define ID_ILBM MAKE_ID('I', 'L', 'B', 'M')
#define ID_BMHD MAKE_ID('B', 'M', 'H', 'D')
#define ID_CMAP MAKE_ID('C', 'M', 'A', 'P')
#define ID_CAMG MAKE_ID('C', 'A', 'M', 'G')
#define ID_PCHG MAKE_ID('P', 'C', 'H', 'G')

/* Masking technique. */
#define mskNone                0
#define mskHasMask             1
#define mskHasTransparentColor 2
#define mskLasso               3

typedef struct BitmapHeader {
  UWORD w, h;                  /* raster width & height in pixels      */
  WORD  x, y;                  /* pixel position for this image        */
  UBYTE nPlanes;               /* # source bitplanes                   */
  UBYTE masking;
  UBYTE compression;
  UBYTE pad1;                  /* unused; ignore on read, write as 0   */
  UWORD transparentColor;      /* transparent "color number" (sort of) */
  UBYTE xAspect, yAspect;      /* pixel aspect, a ratio width : height */
  WORD  pageWidth, pageHeight; /* source "page" size in pixels    */
} BitmapHeaderT;

#define PCHG_COMP_NONE  0
#define PCHGF_12BIT     1

typedef struct PCHGHeader {
  UWORD Compression;
  UWORD Flags;
  WORD  StartLine;
  UWORD LineCount;
  UWORD ChangedLines;
  UWORD MinReg;
  UWORD MaxReg;
  UWORD MaxChanges;
  ULONG TotalChanges;
} PCHGHeaderT;

typedef struct LineChanges {
  UBYTE ChangeCount16;
  UBYTE ChangeCount32;
  UWORD PaletteChange[0];
} LineChangesT;

__regargs static void UnRLE(BYTE *data, LONG size, BYTE *uncompressed) {
  BYTE *src = data;
  BYTE *end = data + size;
  BYTE *dst = uncompressed;

  do {
    WORD code = *src++;

    if (code < 0) {
      BYTE b = *src++;
      WORD n = -code;

      do { *dst++ = b; } while (--n != -1);
    } else {
      WORD n = code;

      do { *dst++ = *src++; } while (--n != -1);
    }
  } while (src < end);
}

__regargs static void Deinterleave(BYTE *data, BitmapT *bitmap) { 
  LONG bytesPerRow = bitmap->bytesPerRow;
  LONG modulo = (WORD)bytesPerRow * (WORD)(bitmap->depth - 1);
  WORD bplnum = bitmap->depth;
  WORD count = bytesPerRow / 2;
  WORD i = count & 7;
  WORD k = (count + 7) / 8;
  APTR *plane = bitmap->planes;

  do {
    BYTE *src = data;
    BYTE *dst = *plane++;
    WORD rows = bitmap->height;

    do {
      WORD n = k - 1;
      switch (i) {
        case 0: do { *((WORD *)dst)++ = *((WORD *)src)++;
        case 7:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 6:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 5:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 4:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 3:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 2:      *((WORD *)dst)++ = *((WORD *)src)++;
        case 1:      *((WORD *)dst)++ = *((WORD *)src)++;
        } while (--n != -1);
      }

      src += modulo;
    } while (--rows);

    data += bytesPerRow;
  } while (--bplnum);
}

__regargs void BitmapUnpack(BitmapT *bitmap, UWORD flags) {
  if (bitmap->compression) {
    ULONG inLen = (LONG)bitmap->planes[1];
    APTR inBuf = bitmap->planes[0];
    ULONG outLen = BitmapSize(bitmap);
    APTR outBuf = MemAlloc(outLen, MEMF_PUBLIC);

    if (bitmap->compression == COMP_RLE)
      UnRLE(inBuf, inLen, outBuf);
#if USE_LZO
    else if (bitmap->compression == COMP_LZO)
      lzo1x_decompress(inBuf, inLen, outBuf, &outLen);
#endif
#if USE_DEFLATE
    else if (bitmap->compression == COMP_DEFLATE)
      Inflate(inBuf, outBuf);
#endif

    MemFree(inBuf);

    bitmap->compression = COMP_NONE;
    BitmapSetPointers(bitmap, outBuf);
  }

  if ((bitmap->flags & BM_INTERLEAVED) && !(flags & BM_INTERLEAVED)) {
    ULONG size = BitmapSize(bitmap);
    APTR inBuf = bitmap->planes[0];
    APTR outBuf = MemAlloc(size, MEMF_PUBLIC);

    bitmap->flags &= ~BM_INTERLEAVED;
    BitmapSetPointers(bitmap, outBuf);

    Deinterleave(inBuf, bitmap);
    MemFree(inBuf);
  }

  if (flags & BM_DISPLAYABLE)
    BitmapMakeDisplayable(bitmap);
}

static __regargs void LoadCAMG(IffFileT *iff, BitmapT *bitmap) {
  ULONG mode;

  ReadChunk(iff, &mode);

  if (mode & HAM)
    bitmap->flags |= BM_HAM;
  if (mode & EXTRA_HALFBRITE)
    bitmap->flags |= BM_EHB;
}

static __regargs void LoadPCHG(IffFileT *iff, BitmapT *bitmap) {
  BYTE *data = MemAlloc(iff->chunk.length, MEMF_PUBLIC);
  PCHGHeaderT *pchg = (APTR)data;

  ReadChunk(iff, data);

  if ((pchg->Compression == PCHG_COMP_NONE) &&
      (pchg->Flags == PCHGF_12BIT)) 
  {
    LONG length = pchg->TotalChanges + bitmap->height;
    UWORD *out = MemAlloc(length * sizeof(UWORD), MEMF_PUBLIC);
    UBYTE *bitvec = (APTR)pchg + sizeof(PCHGHeaderT);
    LineChangesT *line = (APTR)bitvec + ((pchg->LineCount + 31) & ~31) / 8;
    WORD i = pchg->StartLine;

    bitmap->pchgTotal = pchg->TotalChanges;
    bitmap->pchg = out;

    while (i-- > 0)
      *out++ = 0;

    for (i = 0; i < pchg->LineCount; i++) {
      LONG mask = 1 << (7 - (i & 7));
      if (bitvec[i / 8] & mask) {
        UWORD count = line->ChangeCount16;
        UWORD *change = line->PaletteChange;

        *out++ = count;
        while (count-- > 0)
          *out++ = *change++;

        line = (APTR)line + sizeof(LineChangesT) +
          (line->ChangeCount16 + line->ChangeCount32) * sizeof(UWORD);
      } else {
        *out++ = 0;
      }
    }
  }

  MemFree(data);
}

static __regargs void LoadBODY(IffFileT *iff, BitmapT *bitmap, UWORD flags) {
  BYTE *data = MemAlloc(iff->chunk.length, MEMF_PUBLIC);
  LONG size = iff->chunk.length;

  ReadChunk(iff, data);

  if (flags & BM_KEEP_PACKED) {
    bitmap->planes[0] = data;
    bitmap->planes[1] = (APTR)size;
    bitmap->flags &= ~BM_MINIMAL;
  } else {
    if (bitmap->compression) {
      ULONG newSize = bitmap->bplSize * bitmap->depth;
      BYTE *uncompressed = MemAlloc(newSize, MEMF_PUBLIC);

      if (bitmap->compression == COMP_RLE)
        UnRLE(data, size, uncompressed);
#if USE_LZO
      if (bitmap->compression == COMP_LZO)
        lzo1x_decompress(data, size, uncompressed, &newSize);
#endif
#if USE_DEFLATE
      if (bitmap->compression == COMP_DEFLATE)
        Inflate(data, uncompressed);
#endif
      MemFree(data);

      data = uncompressed;
      size = newSize;

      bitmap->compression = 0;
    }

    if (flags & BM_INTERLEAVED)
      memcpy(bitmap->planes[0], data, bitmap->bplSize * bitmap->depth);
    else
      Deinterleave(data, bitmap);

    MemFree(data);
  }
}

static __regargs BitmapT *LoadBMHD(IffFileT *iff, UWORD flags) {
  BitmapHeaderT bmhd;
  BitmapT *bitmap;

  ReadChunk(iff, &bmhd);

  if (flags & BM_KEEP_PACKED)
    flags = BM_MINIMAL|BM_INTERLEAVED;

  bitmap = NewBitmapCustom(bmhd.w, bmhd.h, bmhd.nPlanes, flags);
  bitmap->compression = bmhd.compression;

  return bitmap;
}

static __regargs PaletteT *LoadCMAP(IffFileT *iff) {
  PaletteT *palette = NewPalette(iff->chunk.length / sizeof(ColorT));
  ReadChunk(iff, palette->colors);
  return palette;
}

__regargs BitmapT *LoadILBMCustom(CONST STRPTR filename, UWORD flags) {
  BitmapT *bitmap = NULL;
  PaletteT *palette = NULL;
  IffFileT iff;

  OpenIff(&iff, filename);

  if (iff.header.type != ID_ILBM)
    Panic("[ILBM] File '%s' has wrong type!\n", filename);

  while (ParseChunk(&iff)) {
    if (iff.chunk.type == ID_BMHD)
      bitmap = LoadBMHD(&iff, flags);
    else if (iff.chunk.type == ID_CAMG)
      LoadCAMG(&iff, bitmap);
    else if (iff.chunk.type == ID_PCHG)
      LoadPCHG(&iff, bitmap);
    else if (iff.chunk.type == ID_BODY)
      LoadBODY(&iff, bitmap, flags);
    else if ((iff.chunk.type == ID_CMAP) && (flags & BM_LOAD_PALETTE))
      palette = LoadCMAP(&iff);
    else
      SkipChunk(&iff);
  }

  if (bitmap)
    bitmap->palette = palette;

  CloseIff(&iff);

  return bitmap;
}

__regargs PaletteT *LoadPalette(CONST STRPTR filename) {
  PaletteT *palette = NULL;
  IffFileT iff;

  OpenIff(&iff, filename);

  if (iff.header.type != ID_ILBM)
    Panic("[ILBM] File '%s' has wrong type!\n", filename);

  while (ParseChunk(&iff)) {
    if (iff.chunk.type == ID_CMAP) {
      palette = LoadCMAP(&iff);
      break;
    }
    SkipChunk(&iff);
  }

  CloseIff(&iff);

  return palette;
}
