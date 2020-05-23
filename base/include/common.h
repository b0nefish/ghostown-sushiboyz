#ifndef __COMMON_H__
#define __COMMON_H__

#include <exec/types.h>

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define abs(a) (((a) < 0) ? (-(a)) : (a))

#define offsetof(st, m) \
  ((ULONG)((char *)&((st *)0)->m - (char *)0))

#define align(x, n) \
  (((x) + (n) - 1) & (-(n)))

#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define STRUCT(ctype, ...) \
  (ctype[1]){(ctype){##__VA_ARGS__}}

#define MAKE_ID(a,b,c,d) \
        ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))

#define ITER(_VAR, _BEGIN, _END, _EXPR) { \
  WORD _VAR; \
  for (_VAR = _BEGIN; _VAR <= _END; _VAR++) { \
    _EXPR; \
  } \
}

/* assumes that abs(idx) < 32768 */
static inline WORD getword(APTR tab, WORD idx) {
  WORD res;
  asm("addw  %1,%1\n"
      "movew (%2,%1:w),%0\n"
      : "=r" (res)
      : "d" (idx), "a" (tab)
      : "1");
  return res;
}

/* assumes that abs(idx) < 16384 */
static inline LONG getlong(APTR tab, WORD idx) {
  LONG res;
  asm("addw  %1,%1\n"
      "addw  %1,%1\n"
      "movel (%2,%1:w),%0\n"
      : "=r" (res)
      : "d" (idx), "a" (tab)
      : "1");
  return res;
}

static inline WORD absw(WORD a) {
  if (a < 0)
    return -a;
  return a;
}

static inline ULONG swap16(ULONG a) {
  asm("swap %0": "+d" (a));
  return a;
}

static inline UWORD swap8(UWORD a) {
  return (a << 8) | (a >> 8);
}

static inline WORD div16(LONG a, WORD b) {
  asm("divs %1,%0"
      : "+d" (a)
      : "dm" (b));
  return a;
}

static inline WORD mod16(LONG a, WORD b) {
  asm("divs %1,%0\n"
      "swap %0"
      : "+d" (a)
      : "dm" (b));
  return a;
}

static inline void bclr(UBYTE *ptr, BYTE bit) {
  asm("bclr %1,%0" :: "m" (*ptr), "dI" (bit));
}

static inline void bset(UBYTE *ptr, BYTE bit) {
  asm("bset %1,%0" :: "m" (*ptr), "dI" (bit));
}

static inline void bchg(UBYTE *ptr, BYTE bit) {
  asm("bchg %1,%0" :: "m" (*ptr), "dI" (bit));
}

#define rorw(a, b) \
  (((a) << (16 - (b))) | ((a) >> (b)))

#define swapr(a, b) \
  asm ("exg %0,%1" : "+r" (a), "+r" (b))

#define pushl(a) \
  asm ("movel %0,%-" :: "r" (a))

#define popl(a) \
  asm ("movel %+,%0" : "=r" (a))

static inline APTR GetSP() {
  APTR sp;
  asm("movel sp,%0" : "=r" (sp));
  return sp;
}

#define Breakpoint(n) { asm ("bkpt %0" :: "n" (n)); }

void Log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void FmtStr(char *str, ULONG size, const char *format, ...)
  __attribute__ ((format (printf, 3, 4)));
__regargs void MemDump(APTR ptr, LONG n);

#define Panic(args...) do { Log(args); exit(10); } while (0)

/*
 * Macros for handling symbol table information (aka linker set elements).
 *
 * https://sourceware.org/gdb/onlinedocs/stabs/Non_002dStab-Symbol-Types.html
 */

/* Add symbol 's' to list 'l' (type 't': 22=text, 24=data, 26=bss). */
#define ADD2LIST(s, l, t) \
  asm(".stabs \"_" #l "\"," #t ",0,0,_" #s )

/*
 * Install private constructors and destructors pri MUST be in [-127, 127]
 *
 * Note that library auto-opening happens at -60.
 */
#define ADD2INIT(ctor, pri) \
  ADD2LIST(ctor, __INIT_LIST__, 22); \
  asm(".stabs \"___INIT_LIST__\",20,0,0," #pri "+128")
#define ADD2EXIT(dtor, pri) \
  ADD2LIST(dtor, __EXIT_LIST__, 22); \
  asm(".stabs \"___EXIT_LIST__\",20,0,0," #pri "+128")

/* Make symbol alias from a to b. */
#define ALIAS(a,b) \
  asm(".stabs \"_" #a "\",11,0,0,0;.stabs \"_" #b "\",1,0,0,0")

#define PROFILE_BEGIN(NAME)                                             \
{                                                                       \
  static LONG average_ ## NAME = 0;                                     \
  static WORD count_ ## NAME = 0;                                       \
  LONG lines_ ## NAME = ReadLineCounter();

#define PROFILE_END(NAME)                                               \
  average_ ## NAME += ReadLineCounter() - lines_ ## NAME;               \
  count_ ## NAME ++;                                                    \
  Log(#NAME ": %ld\n", (LONG)div16(average_ ## NAME, count_ ## NAME));  \
}                                                                       \

#endif
