#include <math.h>

#include "system/memory.h"
#include "gfx/matrix3d.h"

#define M_FOREACH(I, J)           \
  for ((I) = 0; (I) < 4; (I)++)   \
    for ((J) = 0; (J) < 4; (J)++)

#define M(A,I,J) (*A)[I][J]

Matrix3D *NewMatrix3D() {
  return NEW_SZ(Matrix3D);
}

void DeleteMatrix3D(Matrix3D *matrix) {
  DELETE(matrix);
}

void M3D_Multiply(Matrix3D *d, Matrix3D *a, Matrix3D *b) {
  int i, j, k;

  M_FOREACH(i, j) {
    M(d,i,j) = 0.0f;

    for (k = 0; k < 4; k++)
      M(d,i,j) += M(a,i,k) * M(b,k,j);
  }
}

void M3D_Transpose(Matrix3D *d, Matrix3D *a) {
  int i, j; 

  M_FOREACH(i, j)
    M(d,i,j) = M(a,j,i);
}


void M3D_LoadIdentity(Matrix3D *d) {
  int i, j; 

  M_FOREACH(i, j)
    M(d,i,j) = (i == j) ? 1.0f : 0.0f;
}

void M3D_LoadRotation(Matrix3D *d, float angleX, float angleY, float angleZ) {
  M3D_LoadIdentity(d);

  angleX *= 3.14159265f / 180.0f;
  angleY *= 3.14159265f / 180.0f;
  angleZ *= 3.14159265f / 180.0f;

  float sinX = sin(angleX);
  float cosX = cos(angleX);
  float sinY = sin(angleY);
  float cosY = cos(angleY);
  float sinZ = sin(angleZ);
  float cosZ = cos(angleZ);

  M(d,0,0) = cosY * cosZ;
  M(d,0,1) = cosY * sinZ;
  M(d,0,2) = -sinY;
  M(d,1,0) = sinX * sinY * cosZ - cosX * sinZ;
  M(d,1,1) = sinX * sinY * sinZ + cosX * cosZ;
  M(d,1,2) = sinX * cosY;
  M(d,2,0) = cosX * sinY * cosZ + sinX * sinZ;
  M(d,2,1) = cosX * sinY * sinZ - sinX * cosZ;
  M(d,2,2) = cosX * cosY;
}

void M3D_LoadScaling(Matrix3D *d, float scaleX, float scaleY, float scaleZ) {
  M3D_LoadIdentity(d);

  M(d,0,0) = scaleX;
  M(d,1,1) = scaleY;
  M(d,2,2) = scaleZ;
}

void M3D_LoadTranslation(Matrix3D *d, float moveX, float moveY, float moveZ) {
  M3D_LoadIdentity(d);

  M(d,3,0) = moveX;
  M(d,3,1) = moveY;
  M(d,3,2) = moveZ;
}

void M3D_LoadPerspective(Matrix3D *d, float viewerX, float viewerY, float viewerZ) {
  M3D_LoadIdentity(d);

  M(d,3,0) = -viewerX;
  M(d,3,1) = -viewerY;
  M(d,2,3) = 1.0f / viewerZ;
  M(d,3,3) = 0;
}

void M3D_Transform(VertexT *dst, VertexT *src, int n, Matrix3D *m) {
  int i;

  for (i = 0; i < n; i++) {
    float x = src[i].x;
    float y = src[i].y;
    float z = src[i].z;

    dst[i].x = (int16_t)(M(m,0,0) * x + M(m,1,0) * y + M(m,2,0) * z + M(m,3,0));
    dst[i].y = (int16_t)(M(m,0,1) * x + M(m,1,1) * y + M(m,2,1) * z + M(m,3,1));
    dst[i].z = (int16_t)(M(m,0,2) * x + M(m,1,2) * y + M(m,2,2) * z + M(m,3,2));
  }
}

void M3D_Project2D(PointT *dst, VertexT *src, int n, Matrix3D *m) {
  int i;

  for (i = 0; i < n; i++) {
    float x = src[i].x;
    float y = src[i].y;
    float z = src[i].z;

    float pX = M(m,0,0) * x + M(m,1,0) * y + M(m,2,0) * z + M(m,3,0);
    float pY = M(m,0,1) * x + M(m,1,1) * y + M(m,2,1) * z + M(m,3,1);
    float pZ = M(m,0,3) * x + M(m,1,3) * y + M(m,2,3) * z + M(m,3,3);

    dst[i].x = (int16_t)(pX / pZ);
    dst[i].y = (int16_t)(pY / pZ);
  }
}