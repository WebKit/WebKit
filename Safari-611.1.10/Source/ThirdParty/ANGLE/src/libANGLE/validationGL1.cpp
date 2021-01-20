//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL1.cpp: Validation functions for OpenGL 1.0 entry point parameters

#include "libANGLE/validationGL1_autogen.h"

namespace gl
{

bool ValidateAccum(const Context *, GLenum op, GLfloat value)
{
    return true;
}

bool ValidateBegin(const Context *, GLenum mode)
{
    return true;
}

bool ValidateBitmap(const Context *,
                    GLsizei width,
                    GLsizei height,
                    GLfloat xorig,
                    GLfloat yorig,
                    GLfloat xmove,
                    GLfloat ymove,
                    const GLubyte *bitmap)
{
    return true;
}

bool ValidateCallList(const Context *, GLuint list)
{
    return true;
}

bool ValidateCallLists(const Context *, GLsizei n, GLenum type, const void *lists)
{
    return true;
}

bool ValidateClearAccum(const Context *, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    return true;
}

bool ValidateClearDepth(const Context *, GLdouble depth)
{
    return true;
}

bool ValidateClearIndex(const Context *, GLfloat c)
{
    return true;
}

bool ValidateClipPlane(const Context *, GLenum plane, const GLdouble *equation)
{
    return true;
}

bool ValidateColor3b(const Context *, GLbyte red, GLbyte green, GLbyte blue)
{
    return true;
}

bool ValidateColor3bv(const Context *, const GLbyte *v)
{
    return true;
}

bool ValidateColor3d(const Context *, GLdouble red, GLdouble green, GLdouble blue)
{
    return true;
}

bool ValidateColor3dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateColor3f(const Context *, GLfloat red, GLfloat green, GLfloat blue)
{
    return true;
}

bool ValidateColor3fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateColor3i(const Context *, GLint red, GLint green, GLint blue)
{
    return true;
}

bool ValidateColor3iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateColor3s(const Context *, GLshort red, GLshort green, GLshort blue)
{
    return true;
}

bool ValidateColor3sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateColor3ub(const Context *, GLubyte red, GLubyte green, GLubyte blue)
{
    return true;
}

bool ValidateColor3ubv(const Context *, const GLubyte *v)
{
    return true;
}

bool ValidateColor3ui(const Context *, GLuint red, GLuint green, GLuint blue)
{
    return true;
}

bool ValidateColor3uiv(const Context *, const GLuint *v)
{
    return true;
}

bool ValidateColor3us(const Context *, GLushort red, GLushort green, GLushort blue)
{
    return true;
}

bool ValidateColor3usv(const Context *, const GLushort *v)
{
    return true;
}

bool ValidateColor4b(const Context *, GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
    return true;
}

bool ValidateColor4bv(const Context *, const GLbyte *v)
{
    return true;
}

bool ValidateColor4d(const Context *, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
    return true;
}

bool ValidateColor4dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateColor4fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateColor4i(const Context *, GLint red, GLint green, GLint blue, GLint alpha)
{
    return true;
}

bool ValidateColor4iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateColor4s(const Context *, GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
    return true;
}

bool ValidateColor4sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateColor4ubv(const Context *, const GLubyte *v)
{
    return true;
}

bool ValidateColor4ui(const Context *, GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
    return true;
}

bool ValidateColor4uiv(const Context *, const GLuint *v)
{
    return true;
}

bool ValidateColor4us(const Context *, GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
    return true;
}

bool ValidateColor4usv(const Context *, const GLushort *v)
{
    return true;
}

bool ValidateColorMaterial(const Context *, GLenum face, GLenum mode)
{
    return true;
}

bool ValidateCopyPixels(const Context *,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum type)
{
    return true;
}

bool ValidateDeleteLists(const Context *, GLuint list, GLsizei range)
{
    return true;
}

bool ValidateDepthRange(const Context *, GLdouble n, GLdouble f)
{
    return true;
}

bool ValidateDrawBuffer(const Context *, GLenum buf)
{
    return true;
}

bool ValidateDrawPixels(const Context *,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        const void *pixels)
{
    return true;
}

bool ValidateEdgeFlag(const Context *, GLboolean flag)
{
    return true;
}

bool ValidateEdgeFlagv(const Context *, const GLboolean *flag)
{
    return true;
}

bool ValidateEnd(const Context *)
{
    return true;
}

bool ValidateEndList(const Context *)
{
    return true;
}

bool ValidateEvalCoord1d(const Context *, GLdouble u)
{
    return true;
}

bool ValidateEvalCoord1dv(const Context *, const GLdouble *u)
{
    return true;
}

bool ValidateEvalCoord1f(const Context *, GLfloat u)
{
    return true;
}

bool ValidateEvalCoord1fv(const Context *, const GLfloat *u)
{
    return true;
}

bool ValidateEvalCoord2d(const Context *, GLdouble u, GLdouble v)
{
    return true;
}

bool ValidateEvalCoord2dv(const Context *, const GLdouble *u)
{
    return true;
}

bool ValidateEvalCoord2f(const Context *, GLfloat u, GLfloat v)
{
    return true;
}

bool ValidateEvalCoord2fv(const Context *, const GLfloat *u)
{
    return true;
}

bool ValidateEvalMesh1(const Context *, GLenum mode, GLint i1, GLint i2)
{
    return true;
}

bool ValidateEvalMesh2(const Context *, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    return true;
}

bool ValidateEvalPoint1(const Context *, GLint i)
{
    return true;
}

bool ValidateEvalPoint2(const Context *, GLint i, GLint j)
{
    return true;
}

bool ValidateFeedbackBuffer(const Context *, GLsizei size, GLenum type, const GLfloat *buffer)
{
    return true;
}

bool ValidateFogi(const Context *, GLenum pname, GLint param)
{
    return true;
}

bool ValidateFogiv(const Context *, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateFrustum(const Context *,
                     GLdouble left,
                     GLdouble right,
                     GLdouble bottom,
                     GLdouble top,
                     GLdouble zNear,
                     GLdouble zFar)
{
    return true;
}

bool ValidateGenLists(const Context *, GLsizei range)
{
    return true;
}

bool ValidateGetClipPlane(const Context *, GLenum plane, const GLdouble *equation)
{
    return true;
}

bool ValidateGetDoublev(const Context *, GLenum pname, const GLdouble *data)
{
    return true;
}

bool ValidateGetLightiv(const Context *, GLenum light, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateGetMapdv(const Context *, GLenum target, GLenum query, const GLdouble *v)
{
    return true;
}

bool ValidateGetMapfv(const Context *, GLenum target, GLenum query, const GLfloat *v)
{
    return true;
}

bool ValidateGetMapiv(const Context *, GLenum target, GLenum query, const GLint *v)
{
    return true;
}

bool ValidateGetMaterialiv(const Context *, GLenum face, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateGetPixelMapfv(const Context *, GLenum map, const GLfloat *values)
{
    return true;
}

bool ValidateGetPixelMapuiv(const Context *, GLenum map, const GLuint *values)
{
    return true;
}

bool ValidateGetPixelMapusv(const Context *, GLenum map, const GLushort *values)
{
    return true;
}

bool ValidateGetPolygonStipple(const Context *, const GLubyte *mask)
{
    return true;
}

bool ValidateGetTexGendv(const Context *, GLenum coord, GLenum pname, const GLdouble *params)
{
    return true;
}

bool ValidateGetTexGenfv(const Context *, GLenum coord, GLenum pname, const GLfloat *params)
{
    return true;
}

bool ValidateGetTexGeniv(const Context *, GLenum coord, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateGetTexImage(const Context *,
                         TextureTarget target,
                         GLint level,
                         GLenum format,
                         GLenum type,
                         const void *pixels)
{
    return true;
}

bool ValidateIndexMask(const Context *, GLuint mask)
{
    return true;
}

bool ValidateIndexd(const Context *, GLdouble c)
{
    return true;
}

bool ValidateIndexdv(const Context *, const GLdouble *c)
{
    return true;
}

bool ValidateIndexf(const Context *, GLfloat c)
{
    return true;
}

bool ValidateIndexfv(const Context *, const GLfloat *c)
{
    return true;
}

bool ValidateIndexi(const Context *, GLint c)
{
    return true;
}

bool ValidateIndexiv(const Context *, const GLint *c)
{
    return true;
}

bool ValidateIndexs(const Context *, GLshort c)
{
    return true;
}

bool ValidateIndexsv(const Context *, const GLshort *c)
{
    return true;
}

bool ValidateInitNames(const Context *)
{
    return true;
}

bool ValidateIsList(const Context *, GLuint list)
{
    return true;
}

bool ValidateLightModeli(const Context *, GLenum pname, GLint param)
{
    return true;
}

bool ValidateLightModeliv(const Context *, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateLighti(const Context *, GLenum light, GLenum pname, GLint param)
{
    return true;
}

bool ValidateLightiv(const Context *, GLenum light, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateLineStipple(const Context *, GLint factor, GLushort pattern)
{
    return true;
}

bool ValidateListBase(const Context *, GLuint base)
{
    return true;
}

bool ValidateLoadMatrixd(const Context *, const GLdouble *m)
{
    return true;
}

bool ValidateLoadName(const Context *, GLuint name)
{
    return true;
}

bool ValidateMap1d(const Context *,
                   GLenum target,
                   GLdouble u1,
                   GLdouble u2,
                   GLint stride,
                   GLint order,
                   const GLdouble *points)
{
    return true;
}

bool ValidateMap1f(const Context *,
                   GLenum target,
                   GLfloat u1,
                   GLfloat u2,
                   GLint stride,
                   GLint order,
                   const GLfloat *points)
{
    return true;
}

bool ValidateMap2d(const Context *,
                   GLenum target,
                   GLdouble u1,
                   GLdouble u2,
                   GLint ustride,
                   GLint uorder,
                   GLdouble v1,
                   GLdouble v2,
                   GLint vstride,
                   GLint vorder,
                   const GLdouble *points)
{
    return true;
}

bool ValidateMap2f(const Context *,
                   GLenum target,
                   GLfloat u1,
                   GLfloat u2,
                   GLint ustride,
                   GLint uorder,
                   GLfloat v1,
                   GLfloat v2,
                   GLint vstride,
                   GLint vorder,
                   const GLfloat *points)
{
    return true;
}

bool ValidateMapGrid1d(const Context *, GLint un, GLdouble u1, GLdouble u2)
{
    return true;
}

bool ValidateMapGrid1f(const Context *, GLint un, GLfloat u1, GLfloat u2)
{
    return true;
}

bool ValidateMapGrid2d(const Context *,
                       GLint un,
                       GLdouble u1,
                       GLdouble u2,
                       GLint vn,
                       GLdouble v1,
                       GLdouble v2)
{
    return true;
}

bool ValidateMapGrid2f(const Context *,
                       GLint un,
                       GLfloat u1,
                       GLfloat u2,
                       GLint vn,
                       GLfloat v1,
                       GLfloat v2)
{
    return true;
}

bool ValidateMateriali(const Context *, GLenum face, GLenum pname, GLint param)
{
    return true;
}

bool ValidateMaterialiv(const Context *, GLenum face, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateMultMatrixd(const Context *, const GLdouble *m)
{
    return true;
}

bool ValidateNewList(const Context *, GLuint list, GLenum mode)
{
    return true;
}

bool ValidateNormal3b(const Context *, GLbyte nx, GLbyte ny, GLbyte nz)
{
    return true;
}

bool ValidateNormal3bv(const Context *, const GLbyte *v)
{
    return true;
}

bool ValidateNormal3d(const Context *, GLdouble nx, GLdouble ny, GLdouble nz)
{
    return true;
}

bool ValidateNormal3dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateNormal3fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateNormal3i(const Context *, GLint nx, GLint ny, GLint nz)
{
    return true;
}

bool ValidateNormal3iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateNormal3s(const Context *, GLshort nx, GLshort ny, GLshort nz)
{
    return true;
}

bool ValidateNormal3sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateOrtho(const Context *,
                   GLdouble left,
                   GLdouble right,
                   GLdouble bottom,
                   GLdouble top,
                   GLdouble zNear,
                   GLdouble zFar)
{
    return true;
}

bool ValidatePassThrough(const Context *, GLfloat token)
{
    return true;
}

bool ValidatePixelMapfv(const Context *, GLenum map, GLsizei mapsize, const GLfloat *values)
{
    return true;
}

bool ValidatePixelMapuiv(const Context *, GLenum map, GLsizei mapsize, const GLuint *values)
{
    return true;
}

bool ValidatePixelMapusv(const Context *, GLenum map, GLsizei mapsize, const GLushort *values)
{
    return true;
}

bool ValidatePixelStoref(const Context *, GLenum pname, GLfloat param)
{
    return true;
}

bool ValidatePixelTransferf(const Context *, GLenum pname, GLfloat param)
{
    return true;
}

bool ValidatePixelTransferi(const Context *, GLenum pname, GLint param)
{
    return true;
}

bool ValidatePixelZoom(const Context *, GLfloat xfactor, GLfloat yfactor)
{
    return true;
}

bool ValidatePolygonMode(const Context *, GLenum face, GLenum mode)
{
    return true;
}

bool ValidatePolygonStipple(const Context *, const GLubyte *mask)
{
    return true;
}

bool ValidatePopAttrib(const Context *)
{
    return true;
}

bool ValidatePopName(const Context *)
{
    return true;
}

bool ValidatePushAttrib(const Context *, GLbitfield mask)
{
    return true;
}

bool ValidatePushName(const Context *, GLuint name)
{
    return true;
}

bool ValidateRasterPos2d(const Context *, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateRasterPos2dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateRasterPos2f(const Context *, GLfloat x, GLfloat y)
{
    return true;
}

bool ValidateRasterPos2fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateRasterPos2i(const Context *, GLint x, GLint y)
{
    return true;
}

bool ValidateRasterPos2iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateRasterPos2s(const Context *, GLshort x, GLshort y)
{
    return true;
}

bool ValidateRasterPos2sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateRasterPos3d(const Context *, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateRasterPos3dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateRasterPos3f(const Context *, GLfloat x, GLfloat y, GLfloat z)
{
    return true;
}

bool ValidateRasterPos3fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateRasterPos3i(const Context *, GLint x, GLint y, GLint z)
{
    return true;
}

bool ValidateRasterPos3iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateRasterPos3s(const Context *, GLshort x, GLshort y, GLshort z)
{
    return true;
}

bool ValidateRasterPos3sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateRasterPos4d(const Context *, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    return true;
}

bool ValidateRasterPos4dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateRasterPos4f(const Context *, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    return true;
}

bool ValidateRasterPos4fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateRasterPos4i(const Context *, GLint x, GLint y, GLint z, GLint w)
{
    return true;
}

bool ValidateRasterPos4iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateRasterPos4s(const Context *, GLshort x, GLshort y, GLshort z, GLshort w)
{
    return true;
}

bool ValidateRasterPos4sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateRectd(const Context *, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
    return true;
}

bool ValidateRectdv(const Context *, const GLdouble *v1, const GLdouble *v2)
{
    return true;
}

bool ValidateRectf(const Context *, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    return true;
}

bool ValidateRectfv(const Context *, const GLfloat *v1, const GLfloat *v2)
{
    return true;
}

bool ValidateRecti(const Context *, GLint x1, GLint y1, GLint x2, GLint y2)
{
    return true;
}

bool ValidateRectiv(const Context *, const GLint *v1, const GLint *v2)
{
    return true;
}

bool ValidateRects(const Context *, GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
    return true;
}

bool ValidateRectsv(const Context *, const GLshort *v1, const GLshort *v2)
{
    return true;
}

bool ValidateRenderMode(const Context *, GLenum mode)
{
    return true;
}

bool ValidateRotated(const Context *, GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateScaled(const Context *, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateSelectBuffer(const Context *, GLsizei size, const GLuint *buffer)
{
    return true;
}

bool ValidateTexCoord1d(const Context *, GLdouble s)
{
    return true;
}

bool ValidateTexCoord1dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord1f(const Context *, GLfloat s)
{
    return true;
}

bool ValidateTexCoord1fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord1i(const Context *, GLint s)
{
    return true;
}

bool ValidateTexCoord1iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateTexCoord1s(const Context *, GLshort s)
{
    return true;
}

bool ValidateTexCoord1sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateTexCoord2d(const Context *, GLdouble s, GLdouble t)
{
    return true;
}

bool ValidateTexCoord2dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord2f(const Context *, GLfloat s, GLfloat t)
{
    return true;
}

bool ValidateTexCoord2fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord2i(const Context *, GLint s, GLint t)
{
    return true;
}

bool ValidateTexCoord2iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateTexCoord2s(const Context *, GLshort s, GLshort t)
{
    return true;
}

bool ValidateTexCoord2sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateTexCoord3d(const Context *, GLdouble s, GLdouble t, GLdouble r)
{
    return true;
}

bool ValidateTexCoord3dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord3f(const Context *, GLfloat s, GLfloat t, GLfloat r)
{
    return true;
}

bool ValidateTexCoord3fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord3i(const Context *, GLint s, GLint t, GLint r)
{
    return true;
}

bool ValidateTexCoord3iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateTexCoord3s(const Context *, GLshort s, GLshort t, GLshort r)
{
    return true;
}

bool ValidateTexCoord3sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateTexCoord4d(const Context *, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
    return true;
}

bool ValidateTexCoord4dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord4f(const Context *, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    return true;
}

bool ValidateTexCoord4fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord4i(const Context *, GLint s, GLint t, GLint r, GLint q)
{
    return true;
}

bool ValidateTexCoord4iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateTexCoord4s(const Context *, GLshort s, GLshort t, GLshort r, GLshort q)
{
    return true;
}

bool ValidateTexCoord4sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateTexGend(const Context *, GLenum coord, GLenum pname, GLdouble param)
{
    return true;
}

bool ValidateTexGendv(const Context *, GLenum coord, GLenum pname, const GLdouble *params)
{
    return true;
}

bool ValidateTexGenf(const Context *, GLenum coord, GLenum pname, GLfloat param)
{
    return true;
}
bool ValidateTexGenfv(const Context *, GLenum coord, GLenum pname, const GLfloat *params)
{
    return true;
}

bool ValidateTexGeni(const Context *, GLenum coord, GLenum pname, GLint param)
{
    return true;
}

bool ValidateTexGeniv(const Context *, GLenum coord, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateTexImage1D(const Context *,
                        GLenum target,
                        GLint level,
                        GLint internalformat,
                        GLsizei width,
                        GLint border,
                        GLenum format,
                        GLenum type,
                        const void *pixels)
{
    return true;
}

bool ValidateTranslated(const Context *, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateVertex2d(const Context *, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateVertex2dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateVertex2f(const Context *, GLfloat x, GLfloat y)
{
    return true;
}

bool ValidateVertex2fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateVertex2i(const Context *, GLint x, GLint y)
{
    return true;
}

bool ValidateVertex2iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateVertex2s(const Context *, GLshort x, GLshort y)
{
    return true;
}

bool ValidateVertex2sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateVertex3d(const Context *, GLdouble x, GLdouble y, GLdouble z)
{
    return true;
}

bool ValidateVertex3dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateVertex3f(const Context *, GLfloat x, GLfloat y, GLfloat z)
{
    return true;
}

bool ValidateVertex3fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateVertex3i(const Context *, GLint x, GLint y, GLint z)
{
    return true;
}

bool ValidateVertex3iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateVertex3s(const Context *, GLshort x, GLshort y, GLshort z)
{
    return true;
}

bool ValidateVertex3sv(const Context *, const GLshort *v)
{
    return true;
}

bool ValidateVertex4d(const Context *, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    return true;
}

bool ValidateVertex4dv(const Context *, const GLdouble *v)
{
    return true;
}

bool ValidateVertex4f(const Context *, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    return true;
}

bool ValidateVertex4fv(const Context *, const GLfloat *v)
{
    return true;
}

bool ValidateVertex4i(const Context *, GLint x, GLint y, GLint z, GLint w)
{
    return true;
}

bool ValidateVertex4iv(const Context *, const GLint *v)
{
    return true;
}

bool ValidateVertex4s(const Context *, GLshort x, GLshort y, GLshort z, GLshort w)
{
    return true;
}

bool ValidateVertex4sv(const Context *, const GLshort *v)
{
    return true;
}

}  // namespace gl
