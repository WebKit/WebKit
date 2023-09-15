//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL1.cpp: Validation functions for OpenGL 1.0 entry point parameters

#include "libANGLE/validationGL1_autogen.h"

namespace gl
{

bool ValidateAccum(const Context *, angle::EntryPoint entryPoint, GLenum op, GLfloat value)
{
    return true;
}

bool ValidateBegin(const Context *, angle::EntryPoint entryPoint, GLenum mode)
{
    return true;
}

bool ValidateBitmap(const Context *,
                    angle::EntryPoint entryPoint,
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

bool ValidateCallList(const Context *, angle::EntryPoint entryPoint, GLuint list)
{
    return true;
}

bool ValidateCallLists(const Context *,
                       angle::EntryPoint entryPoint,
                       GLsizei n,
                       GLenum type,
                       const void *lists)
{
    return true;
}

bool ValidateClearAccum(const Context *,
                        angle::EntryPoint entryPoint,
                        GLfloat red,
                        GLfloat green,
                        GLfloat blue,
                        GLfloat alpha)
{
    return true;
}

bool ValidateClearDepth(const Context *, angle::EntryPoint entryPoint, GLdouble depth)
{
    return true;
}

bool ValidateClearIndex(const Context *, angle::EntryPoint entryPoint, GLfloat c)
{
    return true;
}

bool ValidateClipPlane(const Context *,
                       angle::EntryPoint entryPoint,
                       GLenum plane,
                       const GLdouble *equation)
{
    return true;
}

bool ValidateColor3b(const Context *,
                     angle::EntryPoint entryPoint,
                     GLbyte red,
                     GLbyte green,
                     GLbyte blue)
{
    return true;
}

bool ValidateColor3bv(const Context *, angle::EntryPoint entryPoint, const GLbyte *v)
{
    return true;
}

bool ValidateColor3d(const Context *,
                     angle::EntryPoint entryPoint,
                     GLdouble red,
                     GLdouble green,
                     GLdouble blue)
{
    return true;
}

bool ValidateColor3dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateColor3f(const Context *,
                     angle::EntryPoint entryPoint,
                     GLfloat red,
                     GLfloat green,
                     GLfloat blue)
{
    return true;
}

bool ValidateColor3fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateColor3i(const Context *,
                     angle::EntryPoint entryPoint,
                     GLint red,
                     GLint green,
                     GLint blue)
{
    return true;
}

bool ValidateColor3iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateColor3s(const Context *,
                     angle::EntryPoint entryPoint,
                     GLshort red,
                     GLshort green,
                     GLshort blue)
{
    return true;
}

bool ValidateColor3sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateColor3ub(const Context *,
                      angle::EntryPoint entryPoint,
                      GLubyte red,
                      GLubyte green,
                      GLubyte blue)
{
    return true;
}

bool ValidateColor3ubv(const Context *, angle::EntryPoint entryPoint, const GLubyte *v)
{
    return true;
}

bool ValidateColor3ui(const Context *,
                      angle::EntryPoint entryPoint,
                      GLuint red,
                      GLuint green,
                      GLuint blue)
{
    return true;
}

bool ValidateColor3uiv(const Context *, angle::EntryPoint entryPoint, const GLuint *v)
{
    return true;
}

bool ValidateColor3us(const Context *,
                      angle::EntryPoint entryPoint,
                      GLushort red,
                      GLushort green,
                      GLushort blue)
{
    return true;
}

bool ValidateColor3usv(const Context *, angle::EntryPoint entryPoint, const GLushort *v)
{
    return true;
}

bool ValidateColor4b(const Context *,
                     angle::EntryPoint entryPoint,
                     GLbyte red,
                     GLbyte green,
                     GLbyte blue,
                     GLbyte alpha)
{
    return true;
}

bool ValidateColor4bv(const Context *, angle::EntryPoint entryPoint, const GLbyte *v)
{
    return true;
}

bool ValidateColor4d(const Context *,
                     angle::EntryPoint entryPoint,
                     GLdouble red,
                     GLdouble green,
                     GLdouble blue,
                     GLdouble alpha)
{
    return true;
}

bool ValidateColor4dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateColor4fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateColor4i(const Context *,
                     angle::EntryPoint entryPoint,
                     GLint red,
                     GLint green,
                     GLint blue,
                     GLint alpha)
{
    return true;
}

bool ValidateColor4iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateColor4s(const Context *,
                     angle::EntryPoint entryPoint,
                     GLshort red,
                     GLshort green,
                     GLshort blue,
                     GLshort alpha)
{
    return true;
}

bool ValidateColor4sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateColor4ubv(const Context *, angle::EntryPoint entryPoint, const GLubyte *v)
{
    return true;
}

bool ValidateColor4ui(const Context *,
                      angle::EntryPoint entryPoint,
                      GLuint red,
                      GLuint green,
                      GLuint blue,
                      GLuint alpha)
{
    return true;
}

bool ValidateColor4uiv(const Context *, angle::EntryPoint entryPoint, const GLuint *v)
{
    return true;
}

bool ValidateColor4us(const Context *,
                      angle::EntryPoint entryPoint,
                      GLushort red,
                      GLushort green,
                      GLushort blue,
                      GLushort alpha)
{
    return true;
}

bool ValidateColor4usv(const Context *, angle::EntryPoint entryPoint, const GLushort *v)
{
    return true;
}

bool ValidateColorMaterial(const Context *, angle::EntryPoint entryPoint, GLenum face, GLenum mode)
{
    return true;
}

bool ValidateCopyPixels(const Context *,
                        angle::EntryPoint entryPoint,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum type)
{
    return true;
}

bool ValidateDeleteLists(const Context *, angle::EntryPoint entryPoint, GLuint list, GLsizei range)
{
    return true;
}

bool ValidateDepthRange(const Context *, angle::EntryPoint entryPoint, GLdouble n, GLdouble f)
{
    return true;
}

bool ValidateDrawBuffer(const Context *, angle::EntryPoint entryPoint, GLenum buf)
{
    return true;
}

bool ValidateDrawPixels(const Context *,
                        angle::EntryPoint entryPoint,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        const void *pixels)
{
    return true;
}

bool ValidateEdgeFlag(const Context *, angle::EntryPoint entryPoint, GLboolean flag)
{
    return true;
}

bool ValidateEdgeFlagv(const Context *, angle::EntryPoint entryPoint, const GLboolean *flag)
{
    return true;
}

bool ValidateEnd(const Context *, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidateEndList(const Context *, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidateEvalCoord1d(const Context *, angle::EntryPoint entryPoint, GLdouble u)
{
    return true;
}

bool ValidateEvalCoord1dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *u)
{
    return true;
}

bool ValidateEvalCoord1f(const Context *, angle::EntryPoint entryPoint, GLfloat u)
{
    return true;
}

bool ValidateEvalCoord1fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *u)
{
    return true;
}

bool ValidateEvalCoord2d(const Context *, angle::EntryPoint entryPoint, GLdouble u, GLdouble v)
{
    return true;
}

bool ValidateEvalCoord2dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *u)
{
    return true;
}

bool ValidateEvalCoord2f(const Context *, angle::EntryPoint entryPoint, GLfloat u, GLfloat v)
{
    return true;
}

bool ValidateEvalCoord2fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *u)
{
    return true;
}

bool ValidateEvalMesh1(const Context *,
                       angle::EntryPoint entryPoint,
                       GLenum mode,
                       GLint i1,
                       GLint i2)
{
    return true;
}

bool ValidateEvalMesh2(const Context *,
                       angle::EntryPoint entryPoint,
                       GLenum mode,
                       GLint i1,
                       GLint i2,
                       GLint j1,
                       GLint j2)
{
    return true;
}

bool ValidateEvalPoint1(const Context *, angle::EntryPoint entryPoint, GLint i)
{
    return true;
}

bool ValidateEvalPoint2(const Context *, angle::EntryPoint entryPoint, GLint i, GLint j)
{
    return true;
}

bool ValidateFeedbackBuffer(const Context *,
                            angle::EntryPoint entryPoint,
                            GLsizei size,
                            GLenum type,
                            const GLfloat *buffer)
{
    return true;
}

bool ValidateFogi(const Context *, angle::EntryPoint entryPoint, GLenum pname, GLint param)
{
    return true;
}

bool ValidateFogiv(const Context *, angle::EntryPoint entryPoint, GLenum pname, const GLint *params)
{
    return true;
}

bool ValidateFrustum(const Context *,
                     angle::EntryPoint entryPoint,
                     GLdouble left,
                     GLdouble right,
                     GLdouble bottom,
                     GLdouble top,
                     GLdouble zNear,
                     GLdouble zFar)
{
    return true;
}

bool ValidateGenLists(const Context *, angle::EntryPoint entryPoint, GLsizei range)
{
    return true;
}

bool ValidateGetClipPlane(const Context *,
                          angle::EntryPoint entryPoint,
                          GLenum plane,
                          const GLdouble *equation)
{
    return true;
}

bool ValidateGetDoublev(const Context *,
                        angle::EntryPoint entryPoint,
                        GLenum pname,
                        const GLdouble *data)
{
    return true;
}

bool ValidateGetLightiv(const Context *,
                        angle::EntryPoint entryPoint,
                        GLenum light,
                        GLenum pname,
                        const GLint *params)
{
    return true;
}

bool ValidateGetMapdv(const Context *,
                      angle::EntryPoint entryPoint,
                      GLenum target,
                      GLenum query,
                      const GLdouble *v)
{
    return true;
}

bool ValidateGetMapfv(const Context *,
                      angle::EntryPoint entryPoint,
                      GLenum target,
                      GLenum query,
                      const GLfloat *v)
{
    return true;
}

bool ValidateGetMapiv(const Context *,
                      angle::EntryPoint entryPoint,
                      GLenum target,
                      GLenum query,
                      const GLint *v)
{
    return true;
}

bool ValidateGetMaterialiv(const Context *,
                           angle::EntryPoint entryPoint,
                           GLenum face,
                           GLenum pname,
                           const GLint *params)
{
    return true;
}

bool ValidateGetPixelMapfv(const Context *,
                           angle::EntryPoint entryPoint,
                           GLenum map,
                           const GLfloat *values)
{
    return true;
}

bool ValidateGetPixelMapuiv(const Context *,
                            angle::EntryPoint entryPoint,
                            GLenum map,
                            const GLuint *values)
{
    return true;
}

bool ValidateGetPixelMapusv(const Context *,
                            angle::EntryPoint entryPoint,
                            GLenum map,
                            const GLushort *values)
{
    return true;
}

bool ValidateGetPolygonStipple(const Context *, angle::EntryPoint entryPoint, const GLubyte *mask)
{
    return true;
}

bool ValidateGetTexGendv(const Context *,
                         angle::EntryPoint entryPoint,
                         GLenum coord,
                         GLenum pname,
                         const GLdouble *params)
{
    return true;
}

bool ValidateGetTexGenfv(const Context *,
                         angle::EntryPoint entryPoint,
                         GLenum coord,
                         GLenum pname,
                         const GLfloat *params)
{
    return true;
}

bool ValidateGetTexGeniv(const Context *,
                         angle::EntryPoint entryPoint,
                         GLenum coord,
                         GLenum pname,
                         const GLint *params)
{
    return true;
}

bool ValidateGetTexImage(const Context *,
                         angle::EntryPoint entryPoint,
                         TextureTarget target,
                         GLint level,
                         GLenum format,
                         GLenum type,
                         const void *pixels)
{
    return true;
}

bool ValidateIndexMask(const Context *, angle::EntryPoint entryPoint, GLuint mask)
{
    return true;
}

bool ValidateIndexd(const Context *, angle::EntryPoint entryPoint, GLdouble c)
{
    return true;
}

bool ValidateIndexdv(const Context *, angle::EntryPoint entryPoint, const GLdouble *c)
{
    return true;
}

bool ValidateIndexf(const Context *, angle::EntryPoint entryPoint, GLfloat c)
{
    return true;
}

bool ValidateIndexfv(const Context *, angle::EntryPoint entryPoint, const GLfloat *c)
{
    return true;
}

bool ValidateIndexi(const Context *, angle::EntryPoint entryPoint, GLint c)
{
    return true;
}

bool ValidateIndexiv(const Context *, angle::EntryPoint entryPoint, const GLint *c)
{
    return true;
}

bool ValidateIndexs(const Context *, angle::EntryPoint entryPoint, GLshort c)
{
    return true;
}

bool ValidateIndexsv(const Context *, angle::EntryPoint entryPoint, const GLshort *c)
{
    return true;
}

bool ValidateInitNames(const Context *, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidateIsList(const Context *, angle::EntryPoint entryPoint, GLuint list)
{
    return true;
}

bool ValidateLightModeli(const Context *, angle::EntryPoint entryPoint, GLenum pname, GLint param)
{
    return true;
}

bool ValidateLightModeliv(const Context *,
                          angle::EntryPoint entryPoint,
                          GLenum pname,
                          const GLint *params)
{
    return true;
}

bool ValidateLighti(const Context *,
                    angle::EntryPoint entryPoint,
                    GLenum light,
                    GLenum pname,
                    GLint param)
{
    return true;
}

bool ValidateLightiv(const Context *,
                     angle::EntryPoint entryPoint,
                     GLenum light,
                     GLenum pname,
                     const GLint *params)
{
    return true;
}

bool ValidateLineStipple(const Context *,
                         angle::EntryPoint entryPoint,
                         GLint factor,
                         GLushort pattern)
{
    return true;
}

bool ValidateListBase(const Context *, angle::EntryPoint entryPoint, GLuint base)
{
    return true;
}

bool ValidateLoadMatrixd(const Context *, angle::EntryPoint entryPoint, const GLdouble *m)
{
    return true;
}

bool ValidateLoadName(const Context *, angle::EntryPoint entryPoint, GLuint name)
{
    return true;
}

bool ValidateMap1d(const Context *,
                   angle::EntryPoint entryPoint,
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
                   angle::EntryPoint entryPoint,
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
                   angle::EntryPoint entryPoint,
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
                   angle::EntryPoint entryPoint,
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

bool ValidateMapGrid1d(const Context *,
                       angle::EntryPoint entryPoint,
                       GLint un,
                       GLdouble u1,
                       GLdouble u2)
{
    return true;
}

bool ValidateMapGrid1f(const Context *,
                       angle::EntryPoint entryPoint,
                       GLint un,
                       GLfloat u1,
                       GLfloat u2)
{
    return true;
}

bool ValidateMapGrid2d(const Context *,
                       angle::EntryPoint entryPoint,
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
                       angle::EntryPoint entryPoint,
                       GLint un,
                       GLfloat u1,
                       GLfloat u2,
                       GLint vn,
                       GLfloat v1,
                       GLfloat v2)
{
    return true;
}

bool ValidateMateriali(const Context *,
                       angle::EntryPoint entryPoint,
                       GLenum face,
                       GLenum pname,
                       GLint param)
{
    return true;
}

bool ValidateMaterialiv(const Context *,
                        angle::EntryPoint entryPoint,
                        GLenum face,
                        GLenum pname,
                        const GLint *params)
{
    return true;
}

bool ValidateMultMatrixd(const Context *, angle::EntryPoint entryPoint, const GLdouble *m)
{
    return true;
}

bool ValidateNewList(const Context *, angle::EntryPoint entryPoint, GLuint list, GLenum mode)
{
    return true;
}

bool ValidateNormal3b(const Context *,
                      angle::EntryPoint entryPoint,
                      GLbyte nx,
                      GLbyte ny,
                      GLbyte nz)
{
    return true;
}

bool ValidateNormal3bv(const Context *, angle::EntryPoint entryPoint, const GLbyte *v)
{
    return true;
}

bool ValidateNormal3d(const Context *,
                      angle::EntryPoint entryPoint,
                      GLdouble nx,
                      GLdouble ny,
                      GLdouble nz)
{
    return true;
}

bool ValidateNormal3dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateNormal3fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateNormal3i(const Context *, angle::EntryPoint entryPoint, GLint nx, GLint ny, GLint nz)
{
    return true;
}

bool ValidateNormal3iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateNormal3s(const Context *,
                      angle::EntryPoint entryPoint,
                      GLshort nx,
                      GLshort ny,
                      GLshort nz)
{
    return true;
}

bool ValidateNormal3sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateOrtho(const Context *,
                   angle::EntryPoint entryPoint,
                   GLdouble left,
                   GLdouble right,
                   GLdouble bottom,
                   GLdouble top,
                   GLdouble zNear,
                   GLdouble zFar)
{
    return true;
}

bool ValidatePassThrough(const Context *, angle::EntryPoint entryPoint, GLfloat token)
{
    return true;
}

bool ValidatePixelMapfv(const Context *,
                        angle::EntryPoint entryPoint,
                        GLenum map,
                        GLsizei mapsize,
                        const GLfloat *values)
{
    return true;
}

bool ValidatePixelMapuiv(const Context *,
                         angle::EntryPoint entryPoint,
                         GLenum map,
                         GLsizei mapsize,
                         const GLuint *values)
{
    return true;
}

bool ValidatePixelMapusv(const Context *,
                         angle::EntryPoint entryPoint,
                         GLenum map,
                         GLsizei mapsize,
                         const GLushort *values)
{
    return true;
}

bool ValidatePixelStoref(const Context *, angle::EntryPoint entryPoint, GLenum pname, GLfloat param)
{
    return true;
}

bool ValidatePixelTransferf(const Context *,
                            angle::EntryPoint entryPoint,
                            GLenum pname,
                            GLfloat param)
{
    return true;
}

bool ValidatePixelTransferi(const Context *,
                            angle::EntryPoint entryPoint,
                            GLenum pname,
                            GLint param)
{
    return true;
}

bool ValidatePixelZoom(const Context *,
                       angle::EntryPoint entryPoint,
                       GLfloat xfactor,
                       GLfloat yfactor)
{
    return true;
}

bool ValidatePolygonMode(const PrivateState &state,
                         ErrorSet *errors,
                         angle::EntryPoint entryPoint,
                         GLenum face,
                         PolygonMode modePacked)
{
    return true;
}

bool ValidatePolygonStipple(const Context *, angle::EntryPoint entryPoint, const GLubyte *mask)
{
    return true;
}

bool ValidatePopAttrib(const Context *, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidatePopName(const Context *, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidatePushAttrib(const Context *, angle::EntryPoint entryPoint, GLbitfield mask)
{
    return true;
}

bool ValidatePushName(const Context *, angle::EntryPoint entryPoint, GLuint name)
{
    return true;
}

bool ValidateRasterPos2d(const Context *, angle::EntryPoint entryPoint, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateRasterPos2dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateRasterPos2f(const Context *, angle::EntryPoint entryPoint, GLfloat x, GLfloat y)
{
    return true;
}

bool ValidateRasterPos2fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateRasterPos2i(const Context *, angle::EntryPoint entryPoint, GLint x, GLint y)
{
    return true;
}

bool ValidateRasterPos2iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateRasterPos2s(const Context *, angle::EntryPoint entryPoint, GLshort x, GLshort y)
{
    return true;
}

bool ValidateRasterPos2sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateRasterPos3d(const Context *,
                         angle::EntryPoint entryPoint,
                         GLdouble x,
                         GLdouble y,
                         GLdouble z)
{
    return true;
}

bool ValidateRasterPos3dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateRasterPos3f(const Context *,
                         angle::EntryPoint entryPoint,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z)
{
    return true;
}

bool ValidateRasterPos3fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateRasterPos3i(const Context *, angle::EntryPoint entryPoint, GLint x, GLint y, GLint z)
{
    return true;
}

bool ValidateRasterPos3iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateRasterPos3s(const Context *,
                         angle::EntryPoint entryPoint,
                         GLshort x,
                         GLshort y,
                         GLshort z)
{
    return true;
}

bool ValidateRasterPos3sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateRasterPos4d(const Context *,
                         angle::EntryPoint entryPoint,
                         GLdouble x,
                         GLdouble y,
                         GLdouble z,
                         GLdouble w)
{
    return true;
}

bool ValidateRasterPos4dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateRasterPos4f(const Context *,
                         angle::EntryPoint entryPoint,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z,
                         GLfloat w)
{
    return true;
}

bool ValidateRasterPos4fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateRasterPos4i(const Context *,
                         angle::EntryPoint entryPoint,
                         GLint x,
                         GLint y,
                         GLint z,
                         GLint w)
{
    return true;
}

bool ValidateRasterPos4iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateRasterPos4s(const Context *,
                         angle::EntryPoint entryPoint,
                         GLshort x,
                         GLshort y,
                         GLshort z,
                         GLshort w)
{
    return true;
}

bool ValidateRasterPos4sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateRectd(const Context *,
                   angle::EntryPoint entryPoint,
                   GLdouble x1,
                   GLdouble y1,
                   GLdouble x2,
                   GLdouble y2)
{
    return true;
}

bool ValidateRectdv(const Context *,
                    angle::EntryPoint entryPoint,
                    const GLdouble *v1,
                    const GLdouble *v2)
{
    return true;
}

bool ValidateRectf(const Context *,
                   angle::EntryPoint entryPoint,
                   GLfloat x1,
                   GLfloat y1,
                   GLfloat x2,
                   GLfloat y2)
{
    return true;
}

bool ValidateRectfv(const Context *,
                    angle::EntryPoint entryPoint,
                    const GLfloat *v1,
                    const GLfloat *v2)
{
    return true;
}

bool ValidateRecti(const Context *,
                   angle::EntryPoint entryPoint,
                   GLint x1,
                   GLint y1,
                   GLint x2,
                   GLint y2)
{
    return true;
}

bool ValidateRectiv(const Context *, angle::EntryPoint entryPoint, const GLint *v1, const GLint *v2)
{
    return true;
}

bool ValidateRects(const Context *,
                   angle::EntryPoint entryPoint,
                   GLshort x1,
                   GLshort y1,
                   GLshort x2,
                   GLshort y2)
{
    return true;
}

bool ValidateRectsv(const Context *,
                    angle::EntryPoint entryPoint,
                    const GLshort *v1,
                    const GLshort *v2)
{
    return true;
}

bool ValidateRenderMode(const Context *, angle::EntryPoint entryPoint, GLenum mode)
{
    return true;
}

bool ValidateRotated(const Context *,
                     angle::EntryPoint entryPoint,
                     GLdouble angle,
                     GLdouble x,
                     GLdouble y,
                     GLdouble z)
{
    return true;
}

bool ValidateScaled(const Context *,
                    angle::EntryPoint entryPoint,
                    GLdouble x,
                    GLdouble y,
                    GLdouble z)
{
    return true;
}

bool ValidateSelectBuffer(const Context *,
                          angle::EntryPoint entryPoint,
                          GLsizei size,
                          const GLuint *buffer)
{
    return true;
}

bool ValidateTexCoord1d(const Context *, angle::EntryPoint entryPoint, GLdouble s)
{
    return true;
}

bool ValidateTexCoord1dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord1f(const Context *, angle::EntryPoint entryPoint, GLfloat s)
{
    return true;
}

bool ValidateTexCoord1fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord1i(const Context *, angle::EntryPoint entryPoint, GLint s)
{
    return true;
}

bool ValidateTexCoord1iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateTexCoord1s(const Context *, angle::EntryPoint entryPoint, GLshort s)
{
    return true;
}

bool ValidateTexCoord1sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateTexCoord2d(const Context *, angle::EntryPoint entryPoint, GLdouble s, GLdouble t)
{
    return true;
}

bool ValidateTexCoord2dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord2f(const Context *, angle::EntryPoint entryPoint, GLfloat s, GLfloat t)
{
    return true;
}

bool ValidateTexCoord2fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord2i(const Context *, angle::EntryPoint entryPoint, GLint s, GLint t)
{
    return true;
}

bool ValidateTexCoord2iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateTexCoord2s(const Context *, angle::EntryPoint entryPoint, GLshort s, GLshort t)
{
    return true;
}

bool ValidateTexCoord2sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateTexCoord3d(const Context *,
                        angle::EntryPoint entryPoint,
                        GLdouble s,
                        GLdouble t,
                        GLdouble r)
{
    return true;
}

bool ValidateTexCoord3dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord3f(const Context *,
                        angle::EntryPoint entryPoint,
                        GLfloat s,
                        GLfloat t,
                        GLfloat r)
{
    return true;
}

bool ValidateTexCoord3fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord3i(const Context *, angle::EntryPoint entryPoint, GLint s, GLint t, GLint r)
{
    return true;
}

bool ValidateTexCoord3iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateTexCoord3s(const Context *,
                        angle::EntryPoint entryPoint,
                        GLshort s,
                        GLshort t,
                        GLshort r)
{
    return true;
}

bool ValidateTexCoord3sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateTexCoord4d(const Context *,
                        angle::EntryPoint entryPoint,
                        GLdouble s,
                        GLdouble t,
                        GLdouble r,
                        GLdouble q)
{
    return true;
}

bool ValidateTexCoord4dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateTexCoord4f(const Context *,
                        angle::EntryPoint entryPoint,
                        GLfloat s,
                        GLfloat t,
                        GLfloat r,
                        GLfloat q)
{
    return true;
}

bool ValidateTexCoord4fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateTexCoord4i(const Context *,
                        angle::EntryPoint entryPoint,
                        GLint s,
                        GLint t,
                        GLint r,
                        GLint q)
{
    return true;
}

bool ValidateTexCoord4iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateTexCoord4s(const Context *,
                        angle::EntryPoint entryPoint,
                        GLshort s,
                        GLshort t,
                        GLshort r,
                        GLshort q)
{
    return true;
}

bool ValidateTexCoord4sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateTexGend(const Context *,
                     angle::EntryPoint entryPoint,
                     GLenum coord,
                     GLenum pname,
                     GLdouble param)
{
    return true;
}

bool ValidateTexGendv(const Context *,
                      angle::EntryPoint entryPoint,
                      GLenum coord,
                      GLenum pname,
                      const GLdouble *params)
{
    return true;
}

bool ValidateTexGenf(const Context *,
                     angle::EntryPoint entryPoint,
                     GLenum coord,
                     GLenum pname,
                     GLfloat param)
{
    return true;
}
bool ValidateTexGenfv(const Context *,
                      angle::EntryPoint entryPoint,
                      GLenum coord,
                      GLenum pname,
                      const GLfloat *params)
{
    return true;
}

bool ValidateTexGeni(const Context *,
                     angle::EntryPoint entryPoint,
                     GLenum coord,
                     GLenum pname,
                     GLint param)
{
    return true;
}

bool ValidateTexGeniv(const Context *,
                      angle::EntryPoint entryPoint,
                      GLenum coord,
                      GLenum pname,
                      const GLint *params)
{
    return true;
}

bool ValidateTexImage1D(const Context *,
                        angle::EntryPoint entryPoint,
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

bool ValidateTranslated(const Context *,
                        angle::EntryPoint entryPoint,
                        GLdouble x,
                        GLdouble y,
                        GLdouble z)
{
    return true;
}

bool ValidateVertex2d(const Context *, angle::EntryPoint entryPoint, GLdouble x, GLdouble y)
{
    return true;
}

bool ValidateVertex2dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateVertex2f(const Context *, angle::EntryPoint entryPoint, GLfloat x, GLfloat y)
{
    return true;
}

bool ValidateVertex2fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateVertex2i(const Context *, angle::EntryPoint entryPoint, GLint x, GLint y)
{
    return true;
}

bool ValidateVertex2iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateVertex2s(const Context *, angle::EntryPoint entryPoint, GLshort x, GLshort y)
{
    return true;
}

bool ValidateVertex2sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateVertex3d(const Context *,
                      angle::EntryPoint entryPoint,
                      GLdouble x,
                      GLdouble y,
                      GLdouble z)
{
    return true;
}

bool ValidateVertex3dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateVertex3f(const Context *,
                      angle::EntryPoint entryPoint,
                      GLfloat x,
                      GLfloat y,
                      GLfloat z)
{
    return true;
}

bool ValidateVertex3fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateVertex3i(const Context *, angle::EntryPoint entryPoint, GLint x, GLint y, GLint z)
{
    return true;
}

bool ValidateVertex3iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateVertex3s(const Context *,
                      angle::EntryPoint entryPoint,
                      GLshort x,
                      GLshort y,
                      GLshort z)
{
    return true;
}

bool ValidateVertex3sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateVertex4d(const Context *,
                      angle::EntryPoint entryPoint,
                      GLdouble x,
                      GLdouble y,
                      GLdouble z,
                      GLdouble w)
{
    return true;
}

bool ValidateVertex4dv(const Context *, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateVertex4f(const Context *,
                      angle::EntryPoint entryPoint,
                      GLfloat x,
                      GLfloat y,
                      GLfloat z,
                      GLfloat w)
{
    return true;
}

bool ValidateVertex4fv(const Context *, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateVertex4i(const Context *,
                      angle::EntryPoint entryPoint,
                      GLint x,
                      GLint y,
                      GLint z,
                      GLint w)
{
    return true;
}

bool ValidateVertex4iv(const Context *, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateVertex4s(const Context *,
                      angle::EntryPoint entryPoint,
                      GLshort x,
                      GLshort y,
                      GLshort z,
                      GLshort w)
{
    return true;
}

bool ValidateVertex4sv(const Context *, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateAreTexturesResident(const Context *context,
                                 angle::EntryPoint entryPoint,
                                 GLsizei n,
                                 const GLuint *textures,
                                 const GLboolean *residences)
{
    return true;
}

bool ValidateArrayElement(const Context *context, angle::EntryPoint entryPoint, GLint i)
{
    return true;
}

bool ValidateCopyTexImage1D(const Context *context,
                            angle::EntryPoint entryPoint,
                            GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLint border)
{
    return true;
}

bool ValidateCopyTexSubImage1D(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint x,
                               GLint y,
                               GLsizei width)
{
    return true;
}

bool ValidateEdgeFlagPointer(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLsizei stride,
                             const void *pointer)
{
    return true;
}

bool ValidateIndexPointer(const Context *context,
                          angle::EntryPoint entryPoint,
                          GLenum type,
                          GLsizei stride,
                          const void *pointer)
{
    return true;
}

bool ValidateIndexub(const Context *context, angle::EntryPoint entryPoint, GLubyte c)
{
    return true;
}

bool ValidateIndexubv(const Context *context, angle::EntryPoint entryPoint, const GLubyte *c)
{
    return true;
}

bool ValidateInterleavedArrays(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLenum format,
                               GLsizei stride,
                               const void *pointer)
{
    return true;
}

bool ValidatePopClientAttrib(const Context *context, angle::EntryPoint entryPoint)
{
    return true;
}

bool ValidatePrioritizeTextures(const Context *context,
                                angle::EntryPoint entryPoint,
                                GLsizei n,
                                const GLuint *textures,
                                const GLfloat *priorities)
{
    return true;
}

bool ValidatePushClientAttrib(const Context *context, angle::EntryPoint entryPoint, GLbitfield mask)
{
    return true;
}

bool ValidateTexSubImage1D(const Context *context,
                           angle::EntryPoint entryPoint,
                           GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLsizei width,
                           GLenum format,
                           GLenum type,
                           const void *pixels)
{
    return true;
}

bool ValidateCompressedTexImage1D(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLint border,
                                  GLsizei imageSize,
                                  const void *data)
{
    return true;
}

bool ValidateCompressedTexSubImage1D(const Context *context,
                                     angle::EntryPoint entryPoint,
                                     GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLsizei width,
                                     GLenum format,
                                     GLsizei imageSize,
                                     const void *data)
{
    return true;
}

bool ValidateGetCompressedTexImage(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   TextureTarget targetPacked,
                                   GLint level,
                                   const void *img)
{
    return true;
}

bool ValidateLoadTransposeMatrixd(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  const GLdouble *m)
{
    return true;
}

bool ValidateLoadTransposeMatrixf(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  const GLfloat *m)
{
    return true;
}

bool ValidateMultTransposeMatrixd(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  const GLdouble *m)
{
    return true;
}

bool ValidateMultTransposeMatrixf(const Context *context,
                                  angle::EntryPoint entryPoint,
                                  const GLfloat *m)
{
    return true;
}

bool ValidateMultiTexCoord1d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLdouble s)
{
    return true;
}

bool ValidateMultiTexCoord1dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord1f(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLfloat s)
{
    return true;
}

bool ValidateMultiTexCoord1fv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord1i(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLint s)
{
    return true;
}

bool ValidateMultiTexCoord1iv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord1s(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLshort s)
{
    return true;
}

bool ValidateMultiTexCoord1sv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLshort *v)
{
    return true;
}

bool ValidateMultiTexCoord2d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLdouble s,
                             GLdouble t)
{
    return true;
}

bool ValidateMultiTexCoord2dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord2f(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLfloat s,
                             GLfloat t)
{
    return true;
}

bool ValidateMultiTexCoord2fv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord2i(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLint s,
                             GLint t)
{
    return true;
}

bool ValidateMultiTexCoord2iv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord2s(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLshort s,
                             GLshort t)
{
    return true;
}

bool ValidateMultiTexCoord2sv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLshort *v)
{
    return true;
}

bool ValidateMultiTexCoord3d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLdouble s,
                             GLdouble t,
                             GLdouble r)
{
    return true;
}

bool ValidateMultiTexCoord3dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord3f(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLfloat s,
                             GLfloat t,
                             GLfloat r)
{
    return true;
}

bool ValidateMultiTexCoord3fv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord3i(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLint s,
                             GLint t,
                             GLint r)
{
    return true;
}

bool ValidateMultiTexCoord3iv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord3s(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLshort s,
                             GLshort t,
                             GLshort r)
{
    return true;
}

bool ValidateMultiTexCoord3sv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLshort *v)
{
    return true;
}

bool ValidateMultiTexCoord4d(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLdouble s,
                             GLdouble t,
                             GLdouble r,
                             GLdouble q)
{
    return true;
}

bool ValidateMultiTexCoord4dv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLdouble *v)
{
    return true;
}

bool ValidateMultiTexCoord4fv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLfloat *v)
{
    return true;
}

bool ValidateMultiTexCoord4i(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLint s,
                             GLint t,
                             GLint r,
                             GLint q)
{
    return true;
}

bool ValidateMultiTexCoord4iv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLint *v)
{
    return true;
}

bool ValidateMultiTexCoord4s(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum target,
                             GLshort s,
                             GLshort t,
                             GLshort r,
                             GLshort q)
{
    return true;
}

bool ValidateMultiTexCoord4sv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              const GLshort *v)
{
    return true;
}

bool ValidateFogCoordPointer(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum type,
                             GLsizei stride,
                             const void *pointer)
{
    return true;
}

bool ValidateFogCoordd(const Context *context, angle::EntryPoint entryPoint, GLdouble coord)
{
    return true;
}

bool ValidateFogCoorddv(const Context *context, angle::EntryPoint entryPoint, const GLdouble *coord)
{
    return true;
}

bool ValidateFogCoordf(const Context *context, angle::EntryPoint entryPoint, GLfloat coord)
{
    return true;
}

bool ValidateFogCoordfv(const Context *context, angle::EntryPoint entryPoint, const GLfloat *coord)
{
    return true;
}

bool ValidateMultiDrawArrays(const Context *context,
                             angle::EntryPoint entryPoint,
                             PrimitiveMode modePacked,
                             const GLint *first,
                             const GLsizei *count,
                             GLsizei drawcount)
{
    return true;
}

bool ValidateMultiDrawElements(const Context *context,
                               angle::EntryPoint entryPoint,
                               PrimitiveMode modePacked,
                               const GLsizei *count,
                               DrawElementsType typePacked,
                               const void *const *indices,
                               GLsizei drawcount)
{
    return true;
}

bool ValidatePointParameteri(const Context *context,
                             angle::EntryPoint entryPoint,
                             GLenum pname,
                             GLint param)
{
    return true;
}

bool ValidatePointParameteriv(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum pname,
                              const GLint *params)
{
    return true;
}

bool ValidateSecondaryColor3b(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLbyte red,
                              GLbyte green,
                              GLbyte blue)
{
    return true;
}

bool ValidateSecondaryColor3bv(const Context *context,
                               angle::EntryPoint entryPoint,
                               const GLbyte *v)
{
    return true;
}

bool ValidateSecondaryColor3d(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLdouble red,
                              GLdouble green,
                              GLdouble blue)
{
    return true;
}

bool ValidateSecondaryColor3dv(const Context *context,
                               angle::EntryPoint entryPoint,
                               const GLdouble *v)
{
    return true;
}

bool ValidateSecondaryColor3f(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLfloat red,
                              GLfloat green,
                              GLfloat blue)
{
    return true;
}

bool ValidateSecondaryColor3fv(const Context *context,
                               angle::EntryPoint entryPoint,
                               const GLfloat *v)
{
    return true;
}

bool ValidateSecondaryColor3i(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLint red,
                              GLint green,
                              GLint blue)
{
    return true;
}

bool ValidateSecondaryColor3iv(const Context *context, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateSecondaryColor3s(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLshort red,
                              GLshort green,
                              GLshort blue)
{
    return true;
}

bool ValidateSecondaryColor3sv(const Context *context,
                               angle::EntryPoint entryPoint,
                               const GLshort *v)
{
    return true;
}

bool ValidateSecondaryColor3ub(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLubyte red,
                               GLubyte green,
                               GLubyte blue)
{
    return true;
}

bool ValidateSecondaryColor3ubv(const Context *context,
                                angle::EntryPoint entryPoint,
                                const GLubyte *v)
{
    return true;
}

bool ValidateSecondaryColor3ui(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLuint red,
                               GLuint green,
                               GLuint blue)
{
    return true;
}

bool ValidateSecondaryColor3uiv(const Context *context,
                                angle::EntryPoint entryPoint,
                                const GLuint *v)
{
    return true;
}

bool ValidateSecondaryColor3us(const Context *context,
                               angle::EntryPoint entryPoint,
                               GLushort red,
                               GLushort green,
                               GLushort blue)
{
    return true;
}

bool ValidateSecondaryColor3usv(const Context *context,
                                angle::EntryPoint entryPoint,
                                const GLushort *v)
{
    return true;
}

bool ValidateSecondaryColorPointer(const Context *context,
                                   angle::EntryPoint entryPoint,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer)
{
    return true;
}

bool ValidateWindowPos2d(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLdouble x,
                         GLdouble y)
{
    return true;
}

bool ValidateWindowPos2dv(const Context *context, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateWindowPos2f(const Context *context, angle::EntryPoint entryPoint, GLfloat x, GLfloat y)
{
    return true;
}

bool ValidateWindowPos2fv(const Context *context, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateWindowPos2i(const Context *context, angle::EntryPoint entryPoint, GLint x, GLint y)
{
    return true;
}

bool ValidateWindowPos2iv(const Context *context, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateWindowPos2s(const Context *context, angle::EntryPoint entryPoint, GLshort x, GLshort y)
{
    return true;
}

bool ValidateWindowPos2sv(const Context *context, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateWindowPos3d(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLdouble x,
                         GLdouble y,
                         GLdouble z)
{
    return true;
}

bool ValidateWindowPos3dv(const Context *context, angle::EntryPoint entryPoint, const GLdouble *v)
{
    return true;
}

bool ValidateWindowPos3f(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z)
{
    return true;
}

bool ValidateWindowPos3fv(const Context *context, angle::EntryPoint entryPoint, const GLfloat *v)
{
    return true;
}

bool ValidateWindowPos3i(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLint x,
                         GLint y,
                         GLint z)
{
    return true;
}

bool ValidateWindowPos3iv(const Context *context, angle::EntryPoint entryPoint, const GLint *v)
{
    return true;
}

bool ValidateWindowPos3s(const Context *context,
                         angle::EntryPoint entryPoint,
                         GLshort x,
                         GLshort y,
                         GLshort z)
{
    return true;
}

bool ValidateWindowPos3sv(const Context *context, angle::EntryPoint entryPoint, const GLshort *v)
{
    return true;
}

bool ValidateGetBufferSubData(const Context *context,
                              angle::EntryPoint entryPoint,
                              GLenum target,
                              GLintptr offset,
                              GLsizeiptr size,
                              const void *data)
{
    return true;
}

bool ValidateGetQueryObjectiv(const Context *context,
                              angle::EntryPoint entryPoint,
                              QueryID id,
                              GLenum pname,
                              const GLint *params)
{
    return true;
}

bool ValidateMapBuffer(const Context *context,
                       angle::EntryPoint entryPoint,
                       BufferBinding targetPacked,
                       GLenum access)
{
    return true;
}
}  // namespace gl
