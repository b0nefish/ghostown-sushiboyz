#include <string.h>

#include "std/atompool.h"
#include "std/debug.h"
#include "std/memory.h"
#include "std/resource.h"
#include "std/list.h"

typedef struct Resource {
  StrT name;
  PtrT ptr;
  FreeFuncT freeFunc;
} ResourceT;

static ListT *ResList;
static AtomPoolT *ResPool;

static void Relinquish(ResourceT *res) {
  if (res->freeFunc) {
    LOG("Freeing resource '%s' at %p.", res->name, res->ptr);
    res->freeFunc(res->ptr);
  }

  MemUnref(res->name);
}

static CmpT FindByName(const ResourceT *res, const StrT name) {
  return strcmp(res->name, name);
}

void StartResourceManager() {
  ResList = NewList();
  ResPool = NewAtomPool(sizeof(ResourceT), 16);
}

void StopResourceManager() {
  DeleteListFull(ResList, (FreeFuncT)Relinquish);
  DeleteAtomPool(ResPool);
}

static void AddResource(const StrT name, PtrT ptr, FreeFuncT freeFunc) {
  ResourceT *res = AtomNew(ResPool);

  res->name = StrDup(name);
  res->ptr = ptr;
  res->freeFunc = freeFunc;

  ListPushFront(ResList, res);
}

void AddRscSimple(const StrT name, PtrT ptr, FreeFuncT freeFunc) {
  if (!ptr)
    PANIC("Missing content for resource '%s'.", name);

  AddResource(name, ptr, freeFunc);
}

void AddRscStatic(const StrT name, PtrT ptr) {
  AddResource(name, ptr, NULL);
}

PtrT GetResource(const StrT name) {
  ResourceT *res = ListSearch(ResList, (CompareFuncT)FindByName, (PtrT)name);

  if (!res)
    PANIC("Resource '%s' not found.", name);
  
  return res->ptr;
}
