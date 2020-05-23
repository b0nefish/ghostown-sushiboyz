#ifndef __3D_H__
#define __3D_H__

#include "sort.h"
#include "2d.h"
#include "pixmap.h"

/* 3D transformations */

typedef struct {
  WORD u, v;
} UVCoord;

typedef struct {
  WORD x, y, z;
  WORD pad;
} Point3D;

typedef struct {
  WORD m00, m01, m02, x;
  WORD m10, m11, m12, y;
  WORD m20, m21, m22, z;
} Matrix3D;

__regargs void LoadIdentity3D(Matrix3D *M);
__regargs void Translate3D(Matrix3D *M, WORD x, WORD y, WORD z);
__regargs void Scale3D(Matrix3D *M, WORD sx, WORD sy, WORD sz);
__regargs void LoadRotate3D(Matrix3D *M, WORD ax, WORD ay, WORD az);
__regargs void LoadReverseRotate3D(Matrix3D *M, WORD ax, WORD ay, WORD az);
__regargs void Compose3D(Matrix3D *md, Matrix3D *ma, Matrix3D *mb);
__regargs void Transform3D(Matrix3D *M, Point3D *out, Point3D *in, WORD n);

/* 3D polygon and line clipping */

#define PF_NEAR 16
#define PF_FAR  32

typedef struct {
  WORD near;
  WORD far;
} Frustum3D;

extern Frustum3D ClipFrustum;

__regargs void PointsInsideFrustum(Point3D *in, UBYTE *flags, UWORD n);
__regargs UWORD ClipPolygon3D(Point3D *in, Point3D **outp, UWORD n,
                              UWORD clipFlags);

/* 3D mesh representation */

typedef struct {
  PixmapT *pixmap;
  char filename[0];
} MeshImageT;

typedef struct {
  UBYTE r, g, b;
  UBYTE sideness;
  WORD  texture;
} MeshSurfaceT;

typedef struct {
  WORD vertices;
  WORD faces;
  WORD edges;
  WORD surfaces;
  WORD images;

  FLOAT scale;

  Point3D *origVertex;
  Point3D *vertex;
  UVCoord *uv;
  Point3D *faceNormal;
  UBYTE *faceSurface;
  Point3D *vertexNormal;
  EdgeT *edge;
  IndexListT **face;       /* { #face => [#vertex] } */
  IndexListT **faceEdge;   /* { #face => [#edge] } */
  IndexListT **faceUV;     /* { #face => [#uv] } */
  IndexListT **vertexFace; /* { #vertex => [#face] } */

  MeshImageT **image;
  MeshSurfaceT *surface;
} Mesh3D;

__regargs Mesh3D *LoadMesh3D(char *filename, FLOAT scale);
__regargs void DeleteMesh3D(Mesh3D *mesh);

__regargs void CalculateEdges(Mesh3D *mesh);
__regargs void CalculateVertexFaceMap(Mesh3D *mesh);
__regargs void CalculateVertexNormals(Mesh3D *mesh);
__regargs void CalculateFaceNormals(Mesh3D *mesh);

/* 3D object representation */

typedef struct {
  Mesh3D *mesh;

  Point3D rotate;
  Point3D scale;
  Point3D translate;

  Matrix3D objectToWorld; /* object -> world transformation */
  Matrix3D worldToObject; /* world -> object transformation */

  Point3D camera;      /* camera position in object space */

  Point3D *vertex;     /* camera coordinates or screen coordinates + depth */
  BYTE *vertexFlags;   /* used by clipping */
  BYTE *faceFlags;     /* e.g. visiblity flags */
  BYTE *edgeFlags;

  SortItemT *visibleFace;
  WORD visibleFaces;
} Object3D;

__regargs Object3D *NewObject3D(Mesh3D *mesh);
__regargs void DeleteObject3D(Object3D *object);
__regargs void UpdateFaceNormals(Object3D *object);
__regargs void UpdateObjectTransformation(Object3D *object);
__regargs void UpdateFaceVisibility(Object3D *object);
__regargs void UpdateVertexVisibility(Object3D *object);
__regargs void SortFaces(Object3D *object);

#endif
