//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// libGLESv1_CM.cpp: Implements the exported OpenGL ES 1.0 functions.

#include "angle_gl.h"

#include "libGLESv2/entry_points_gles_1_0_autogen.h"
#include "libGLESv2/entry_points_gles_2_0_autogen.h"
#include "libGLESv2/entry_points_gles_3_2_autogen.h"
#include "libGLESv2/entry_points_gles_ext_autogen.h"

extern "C" {

void GL_APIENTRY glAlphaFunc(GLenum func, GLfloat ref)
{
    return gl::AlphaFunc(func, ref);
}

void GL_APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    return gl::ClearColor(red, green, blue, alpha);
}

void GL_APIENTRY glClearDepthf(GLfloat d)
{
    return gl::ClearDepthf(d);
}

void GL_APIENTRY glClipPlanef(GLenum p, const GLfloat *eqn)
{
    return gl::ClipPlanef(p, eqn);
}

void GL_APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    return gl::Color4f(red, green, blue, alpha);
}

void GL_APIENTRY glDepthRangef(GLfloat n, GLfloat f)
{
    return gl::DepthRangef(n, f);
}

void GL_APIENTRY glFogf(GLenum pname, GLfloat param)
{
    return gl::Fogf(pname, param);
}

void GL_APIENTRY glFogfv(GLenum pname, const GLfloat *params)
{
    return gl::Fogfv(pname, params);
}

void GL_APIENTRY glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    return gl::Frustumf(l, r, b, t, n, f);
}

void GL_APIENTRY glGetClipPlanef(GLenum plane, GLfloat *equation)
{
    return gl::GetClipPlanef(plane, equation);
}

void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat *data)
{
    return gl::GetFloatv(pname, data);
}

void GL_APIENTRY glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
    return gl::GetLightfv(light, pname, params);
}

void GL_APIENTRY glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
    return gl::GetMaterialfv(face, pname, params);
}

void GL_APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
    return gl::GetTexEnvfv(target, pname, params);
}

void GL_APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    return gl::GetTexParameterfv(target, pname, params);
}

void GL_APIENTRY glLightModelf(GLenum pname, GLfloat param)
{
    return gl::LightModelf(pname, param);
}

void GL_APIENTRY glLightModelfv(GLenum pname, const GLfloat *params)
{
    return gl::LightModelfv(pname, params);
}

void GL_APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param)
{
    return gl::Lightf(light, pname, param);
}

void GL_APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    return gl::Lightfv(light, pname, params);
}

void GL_APIENTRY glLineWidth(GLfloat width)
{
    return gl::LineWidth(width);
}

void GL_APIENTRY glLoadMatrixf(const GLfloat *m)
{
    return gl::LoadMatrixf(m);
}

void GL_APIENTRY glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    return gl::Materialf(face, pname, param);
}

void GL_APIENTRY glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    return gl::Materialfv(face, pname, params);
}

void GL_APIENTRY glMultMatrixf(const GLfloat *m)
{
    return gl::MultMatrixf(m);
}

void GL_APIENTRY glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    return gl::MultiTexCoord4f(target, s, t, r, q);
}

void GL_APIENTRY glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    return gl::Normal3f(nx, ny, nz);
}

void GL_APIENTRY glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    return gl::Orthof(l, r, b, t, n, f);
}

void GL_APIENTRY glPointParameterf(GLenum pname, GLfloat param)
{
    return gl::PointParameterf(pname, param);
}

void GL_APIENTRY glPointParameterfv(GLenum pname, const GLfloat *params)
{
    return gl::PointParameterfv(pname, params);
}

void GL_APIENTRY glPointSize(GLfloat size)
{
    return gl::PointSize(size);
}

void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
    return gl::PolygonOffset(factor, units);
}

void GL_APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    return gl::Rotatef(angle, x, y, z);
}

void GL_APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    return gl::Scalef(x, y, z);
}

void GL_APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    return gl::TexEnvf(target, pname, param);
}

void GL_APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
    return gl::TexEnvfv(target, pname, params);
}

void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    return gl::TexParameterf(target, pname, param);
}

void GL_APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    return gl::TexParameterfv(target, pname, params);
}

void GL_APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    return gl::Translatef(x, y, z);
}

void GL_APIENTRY glActiveTexture(GLenum texture)
{
    return gl::ActiveTexture(texture);
}

void GL_APIENTRY glAlphaFuncx(GLenum func, GLfixed ref)
{
    return gl::AlphaFuncx(func, ref);
}

void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
    return gl::BindBuffer(target, buffer);
}

void GL_APIENTRY glBindTexture(GLenum target, GLuint texture)
{
    return gl::BindTexture(target, texture);
}

void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    return gl::BlendFunc(sfactor, dfactor);
}

void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    return gl::BufferData(target, size, data, usage);
}

void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    return gl::BufferSubData(target, offset, size, data);
}

void GL_APIENTRY glClear(GLbitfield mask)
{
    return gl::Clear(mask);
}

void GL_APIENTRY glClearColorx(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    return gl::ClearColorx(red, green, blue, alpha);
}

void GL_APIENTRY glClearDepthx(GLfixed depth)
{
    return gl::ClearDepthx(depth);
}

void GL_APIENTRY glClearStencil(GLint s)
{
    return gl::ClearStencil(s);
}

void GL_APIENTRY glClientActiveTexture(GLenum texture)
{
    return gl::ClientActiveTexture(texture);
}

void GL_APIENTRY glClipPlanex(GLenum plane, const GLfixed *equation)
{
    return gl::ClipPlanex(plane, equation);
}

void GL_APIENTRY glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    return gl::Color4ub(red, green, blue, alpha);
}

void GL_APIENTRY glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    return gl::Color4x(red, green, blue, alpha);
}

void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    return gl::ColorMask(red, green, blue, alpha);
}

void GL_APIENTRY glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    return gl::ColorPointer(size, type, stride, pointer);
}

void GL_APIENTRY glCompressedTexImage2D(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void *data)
{
    return gl::CompressedTexImage2D(target, level, internalformat, width, height, border, imageSize,
                                    data);
}

void GL_APIENTRY glCompressedTexSubImage2D(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void *data)
{
    return gl::CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                                       imageSize, data);
}

void GL_APIENTRY glCopyTexImage2D(GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border)
{
    return gl::CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GL_APIENTRY glCopyTexSubImage2D(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height)
{
    return gl::CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GL_APIENTRY glCullFace(GLenum mode)
{
    return gl::CullFace(mode);
}

void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    return gl::DeleteBuffers(n, buffers);
}

void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
    return gl::DeleteTextures(n, textures);
}

void GL_APIENTRY glDepthFunc(GLenum func)
{
    return gl::DepthFunc(func);
}

void GL_APIENTRY glDepthMask(GLboolean flag)
{
    return gl::DepthMask(flag);
}

void GL_APIENTRY glDepthRangex(GLfixed n, GLfixed f)
{
    return gl::DepthRangex(n, f);
}

void GL_APIENTRY glDisable(GLenum cap)
{
    return gl::Disable(cap);
}

void GL_APIENTRY glDisableClientState(GLenum array)
{
    return gl::DisableClientState(array);
}

void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    return gl::DrawArrays(mode, first, count);
}

void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    return gl::DrawElements(mode, count, type, indices);
}

void GL_APIENTRY glEnable(GLenum cap)
{
    return gl::Enable(cap);
}

void GL_APIENTRY glEnableClientState(GLenum array)
{
    return gl::EnableClientState(array);
}

void GL_APIENTRY glFinish(void)
{
    return gl::Finish();
}

void GL_APIENTRY glFlush(void)
{
    return gl::Flush();
}

void GL_APIENTRY glFogx(GLenum pname, GLfixed param)
{
    return gl::Fogx(pname, param);
}

void GL_APIENTRY glFogxv(GLenum pname, const GLfixed *param)
{
    return gl::Fogxv(pname, param);
}

void GL_APIENTRY glFrontFace(GLenum mode)
{
    return gl::FrontFace(mode);
}

void GL_APIENTRY glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    return gl::Frustumx(l, r, b, t, n, f);
}

void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean *data)
{
    return gl::GetBooleanv(pname, data);
}

void GL_APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetBufferParameteriv(target, pname, params);
}

void GL_APIENTRY glGetClipPlanex(GLenum plane, GLfixed *equation)
{
    return gl::GetClipPlanex(plane, equation);
}

void GL_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
    return gl::GenBuffers(n, buffers);
}

void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
    return gl::GenTextures(n, textures);
}

GLenum GL_APIENTRY glGetError(void)
{
    return gl::GetError();
}

void GL_APIENTRY glGetFixedv(GLenum pname, GLfixed *params)
{
    return gl::GetFixedv(pname, params);
}

void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *data)
{
    return gl::GetIntegerv(pname, data);
}

void GL_APIENTRY glGetLightxv(GLenum light, GLenum pname, GLfixed *params)
{
    return gl::GetLightxv(light, pname, params);
}

void GL_APIENTRY glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params)
{
    return gl::GetMaterialxv(face, pname, params);
}

void GL_APIENTRY glGetPointerv(GLenum pname, void **params)
{
    return gl::GetPointerv(pname, params);
}

const GLubyte *GL_APIENTRY glGetString(GLenum name)
{
    return gl::GetString(name);
}

void GL_APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetTexEnviv(target, pname, params);
}

void GL_APIENTRY glGetTexEnvxv(GLenum target, GLenum pname, GLfixed *params)
{
    return gl::GetTexEnvxv(target, pname, params);
}

void GL_APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetTexParameteriv(target, pname, params);
}

void GL_APIENTRY glGetTexParameterxv(GLenum target, GLenum pname, GLfixed *params)
{
    return gl::GetTexParameterxv(target, pname, params);
}

void GL_APIENTRY glHint(GLenum target, GLenum mode)
{
    return gl::Hint(target, mode);
}

GLboolean GL_APIENTRY glIsBuffer(GLuint buffer)
{
    return gl::IsBuffer(buffer);
}

GLboolean GL_APIENTRY glIsEnabled(GLenum cap)
{
    return gl::IsEnabled(cap);
}

GLboolean GL_APIENTRY glIsTexture(GLuint texture)
{
    return gl::IsTexture(texture);
}

void GL_APIENTRY glLightModelx(GLenum pname, GLfixed param)
{
    return gl::LightModelx(pname, param);
}

void GL_APIENTRY glLightModelxv(GLenum pname, const GLfixed *param)
{
    return gl::LightModelxv(pname, param);
}

void GL_APIENTRY glLightx(GLenum light, GLenum pname, GLfixed param)
{
    return gl::Lightx(light, pname, param);
}

void GL_APIENTRY glLightxv(GLenum light, GLenum pname, const GLfixed *params)
{
    return gl::Lightxv(light, pname, params);
}

void GL_APIENTRY glLineWidthx(GLfixed width)
{
    return gl::LineWidthx(width);
}

void GL_APIENTRY glLoadIdentity(void)
{
    return gl::LoadIdentity();
}

void GL_APIENTRY glLoadMatrixx(const GLfixed *m)
{
    return gl::LoadMatrixx(m);
}

void GL_APIENTRY glLogicOp(GLenum opcode)
{
    return gl::LogicOp(opcode);
}

void GL_APIENTRY glMaterialx(GLenum face, GLenum pname, GLfixed param)
{
    return gl::Materialx(face, pname, param);
}

void GL_APIENTRY glMaterialxv(GLenum face, GLenum pname, const GLfixed *param)
{
    return gl::Materialxv(face, pname, param);
}

void GL_APIENTRY glMatrixMode(GLenum mode)
{
    return gl::MatrixMode(mode);
}

void GL_APIENTRY glMultMatrixx(const GLfixed *m)
{
    return gl::MultMatrixx(m);
}

void GL_APIENTRY glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
    return gl::MultiTexCoord4x(texture, s, t, r, q);
}

void GL_APIENTRY glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
    return gl::Normal3x(nx, ny, nz);
}

void GL_APIENTRY glNormalPointer(GLenum type, GLsizei stride, const void *pointer)
{
    return gl::NormalPointer(type, stride, pointer);
}

void GL_APIENTRY glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    return gl::Orthox(l, r, b, t, n, f);
}

void GL_APIENTRY glPixelStorei(GLenum pname, GLint param)
{
    return gl::PixelStorei(pname, param);
}

void GL_APIENTRY glPointParameterx(GLenum pname, GLfixed param)
{
    return gl::PointParameterx(pname, param);
}

void GL_APIENTRY glPointParameterxv(GLenum pname, const GLfixed *params)
{
    return gl::PointParameterxv(pname, params);
}

void GL_APIENTRY glPointSizex(GLfixed size)
{
    return gl::PointSizex(size);
}

void GL_APIENTRY glPolygonOffsetx(GLfixed factor, GLfixed units)
{
    return gl::PolygonOffsetx(factor, units);
}

void GL_APIENTRY glPopMatrix(void)
{
    return gl::PopMatrix();
}

void GL_APIENTRY glPushMatrix(void)
{
    return gl::PushMatrix();
}

void GL_APIENTRY glReadPixels(GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLenum format,
                              GLenum type,
                              void *pixels)
{
    return gl::ReadPixels(x, y, width, height, format, type, pixels);
}

void GL_APIENTRY glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    return gl::Rotatex(angle, x, y, z);
}

void GL_APIENTRY glSampleCoverage(GLfloat value, GLboolean invert)
{
    return gl::SampleCoverage(value, invert);
}

void GL_APIENTRY glSampleCoveragex(GLclampx value, GLboolean invert)
{
    return gl::SampleCoveragex(value, invert);
}

void GL_APIENTRY glScalex(GLfixed x, GLfixed y, GLfixed z)
{
    return gl::Scalex(x, y, z);
}

void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    return gl::Scissor(x, y, width, height);
}

void GL_APIENTRY glShadeModel(GLenum mode)
{
    return gl::ShadeModel(mode);
}

void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    return gl::StencilFunc(func, ref, mask);
}

void GL_APIENTRY glStencilMask(GLuint mask)
{
    return gl::StencilMask(mask);
}

void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    return gl::StencilOp(fail, zfail, zpass);
}

void GL_APIENTRY glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    return gl::TexCoordPointer(size, type, stride, pointer);
}

void GL_APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    return gl::TexEnvi(target, pname, param);
}

void GL_APIENTRY glTexEnvx(GLenum target, GLenum pname, GLfixed param)
{
    return gl::TexEnvx(target, pname, param);
}

void GL_APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
    return gl::TexEnviv(target, pname, params);
}

void GL_APIENTRY glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params)
{
    return gl::TexEnvxv(target, pname, params);
}

void GL_APIENTRY glTexImage2D(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void *pixels)
{
    return gl::TexImage2D(target, level, internalformat, width, height, border, format, type,
                          pixels);
}

void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    return gl::TexParameteri(target, pname, param);
}

void GL_APIENTRY glTexParameterx(GLenum target, GLenum pname, GLfixed param)
{
    return gl::TexParameterx(target, pname, param);
}

void GL_APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    return gl::TexParameteriv(target, pname, params);
}

void GL_APIENTRY glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params)
{
    return gl::TexParameterxv(target, pname, params);
}

void GL_APIENTRY glTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 const void *pixels)
{
    return gl::TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void GL_APIENTRY glTranslatex(GLfixed x, GLfixed y, GLfixed z)
{
    return gl::Translatex(x, y, z);
}

void GL_APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    return gl::VertexPointer(size, type, stride, pointer);
}

void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    return gl::Viewport(x, y, width, height);
}

// GL_OES_draw_texture
void GL_APIENTRY glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height)
{
    return gl::DrawTexfOES(x, y, z, width, height);
}

void GL_APIENTRY glDrawTexfvOES(const GLfloat *coords)
{
    return gl::DrawTexfvOES(coords);
}

void GL_APIENTRY glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height)
{
    return gl::DrawTexiOES(x, y, z, width, height);
}

void GL_APIENTRY glDrawTexivOES(const GLint *coords)
{
    return gl::DrawTexivOES(coords);
}

void GL_APIENTRY glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height)
{
    return gl::DrawTexsOES(x, y, z, width, height);
}

void GL_APIENTRY glDrawTexsvOES(const GLshort *coords)
{
    return gl::DrawTexsvOES(coords);
}

void GL_APIENTRY glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
    return gl::DrawTexxOES(x, y, z, width, height);
}

void GL_APIENTRY glDrawTexxvOES(const GLfixed *coords)
{
    return gl::DrawTexxvOES(coords);
}

// GL_OES_matrix_palette
void GL_APIENTRY glCurrentPaletteMatrixOES(GLuint matrixpaletteindex)
{
    return gl::CurrentPaletteMatrixOES(matrixpaletteindex);
}

void GL_APIENTRY glLoadPaletteFromModelViewMatrixOES()
{
    return gl::LoadPaletteFromModelViewMatrixOES();
}

void GL_APIENTRY glMatrixIndexPointerOES(GLint size,
                                         GLenum type,
                                         GLsizei stride,
                                         const void *pointer)
{
    return gl::MatrixIndexPointerOES(size, type, stride, pointer);
}

void GL_APIENTRY glWeightPointerOES(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    return gl::WeightPointerOES(size, type, stride, pointer);
}

// GL_OES_point_size_array
void GL_APIENTRY glPointSizePointerOES(GLenum type, GLsizei stride, const void *pointer)
{
    return gl::PointSizePointerOES(type, stride, pointer);
}

// GL_OES_query_matrix
GLbitfield GL_APIENTRY glQueryMatrixxOES(GLfixed *mantissa, GLint *exponent)
{
    return gl::QueryMatrixxOES(mantissa, exponent);
}

}  // extern "C"
