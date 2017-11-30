//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// libGLESv2.cpp: Implements the exported OpenGL ES 2.0 functions.

#include "libGLESv2/entry_points_gles_2_0_autogen.h"
#include "libGLESv2/entry_points_gles_2_0_ext.h"
#include "libGLESv2/entry_points_gles_3_0_autogen.h"
#include "libGLESv2/entry_points_gles_3_1_autogen.h"

#include "common/event_tracer.h"

extern "C" {

void GL_APIENTRY glActiveTexture(GLenum texture)
{
    return gl::ActiveTexture(texture);
}

void GL_APIENTRY glAttachShader(GLuint program, GLuint shader)
{
    return gl::AttachShader(program, shader);
}

void GL_APIENTRY glBindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
    return gl::BindAttribLocation(program, index, name);
}

void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
    return gl::BindBuffer(target, buffer);
}

void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    return gl::BindFramebuffer(target, framebuffer);
}

void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    return gl::BindRenderbuffer(target, renderbuffer);
}

void GL_APIENTRY glBindTexture(GLenum target, GLuint texture)
{
    return gl::BindTexture(target, texture);
}

void GL_APIENTRY glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    return gl::BlendColor(red, green, blue, alpha);
}

void GL_APIENTRY glBlendEquation(GLenum mode)
{
    return gl::BlendEquation(mode);
}

void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    return gl::BlendEquationSeparate(modeRGB, modeAlpha);
}

void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    return gl::BlendFunc(sfactor, dfactor);
}

void GL_APIENTRY glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    return gl::BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    return gl::BufferData(target, size, data, usage);
}

void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    return gl::BufferSubData(target, offset, size, data);
}

GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target)
{
    return gl::CheckFramebufferStatus(target);
}

void GL_APIENTRY glClear(GLbitfield mask)
{
    return gl::Clear(mask);
}

void GL_APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    return gl::ClearColor(red, green, blue, alpha);
}

void GL_APIENTRY glClearDepthf(GLfloat depth)
{
    return gl::ClearDepthf(depth);
}

void GL_APIENTRY glClearStencil(GLint s)
{
    return gl::ClearStencil(s);
}

void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    return gl::ColorMask(red, green, blue, alpha);
}

void GL_APIENTRY glCompileShader(GLuint shader)
{
    return gl::CompileShader(shader);
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

GLuint GL_APIENTRY glCreateProgram(void)
{
    return gl::CreateProgram();
}

GLuint GL_APIENTRY glCreateShader(GLenum type)
{
    return gl::CreateShader(type);
}

void GL_APIENTRY glCullFace(GLenum mode)
{
    return gl::CullFace(mode);
}

void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
    return gl::DeleteBuffers(n, buffers);
}

void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
    return gl::DeleteFramebuffers(n, framebuffers);
}

void GL_APIENTRY glDeleteProgram(GLuint program)
{
    return gl::DeleteProgram(program);
}

void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
    return gl::DeleteRenderbuffers(n, renderbuffers);
}

void GL_APIENTRY glDeleteShader(GLuint shader)
{
    return gl::DeleteShader(shader);
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

void GL_APIENTRY glDepthRangef(GLfloat n, GLfloat f)
{
    return gl::DepthRangef(n, f);
}

void GL_APIENTRY glDetachShader(GLuint program, GLuint shader)
{
    return gl::DetachShader(program, shader);
}

void GL_APIENTRY glDisable(GLenum cap)
{
    return gl::Disable(cap);
}

void GL_APIENTRY glDisableVertexAttribArray(GLuint index)
{
    return gl::DisableVertexAttribArray(index);
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

void GL_APIENTRY glEnableVertexAttribArray(GLuint index)
{
    return gl::EnableVertexAttribArray(index);
}

void GL_APIENTRY glFinish(void)
{
    return gl::Finish();
}

void GL_APIENTRY glFlush(void)
{
    return gl::Flush();
}

void GL_APIENTRY glFramebufferRenderbuffer(GLenum target,
                                           GLenum attachment,
                                           GLenum renderbuffertarget,
                                           GLuint renderbuffer)
{
    return gl::FramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void GL_APIENTRY glFramebufferTexture2D(GLenum target,
                                        GLenum attachment,
                                        GLenum textarget,
                                        GLuint texture,
                                        GLint level)
{
    return gl::FramebufferTexture2D(target, attachment, textarget, texture, level);
}

void GL_APIENTRY glFrontFace(GLenum mode)
{
    return gl::FrontFace(mode);
}

void GL_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
    return gl::GenBuffers(n, buffers);
}

void GL_APIENTRY glGenerateMipmap(GLenum target)
{
    return gl::GenerateMipmap(target);
}

void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    return gl::GenFramebuffers(n, framebuffers);
}

void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    return gl::GenRenderbuffers(n, renderbuffers);
}

void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
    return gl::GenTextures(n, textures);
}

void GL_APIENTRY glGetActiveAttrib(GLuint program,
                                   GLuint index,
                                   GLsizei bufsize,
                                   GLsizei *length,
                                   GLint *size,
                                   GLenum *type,
                                   GLchar *name)
{
    return gl::GetActiveAttrib(program, index, bufsize, length, size, type, name);
}

void GL_APIENTRY glGetActiveUniform(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei *length,
                                    GLint *size,
                                    GLenum *type,
                                    GLchar *name)
{
    return gl::GetActiveUniform(program, index, bufsize, length, size, type, name);
}

void GL_APIENTRY glGetAttachedShaders(GLuint program,
                                      GLsizei maxcount,
                                      GLsizei *count,
                                      GLuint *shaders)
{
    return gl::GetAttachedShaders(program, maxcount, count, shaders);
}

GLint GL_APIENTRY glGetAttribLocation(GLuint program, const GLchar *name)
{
    return gl::GetAttribLocation(program, name);
}

void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean *params)
{
    return gl::GetBooleanv(pname, params);
}

void GL_APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetBufferParameteriv(target, pname, params);
}

GLenum GL_APIENTRY glGetError(void)
{
    return gl::GetError();
}

void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat *params)
{
    return gl::GetFloatv(pname, params);
}

void GL_APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target,
                                                       GLenum attachment,
                                                       GLenum pname,
                                                       GLint *params)
{
    return gl::GetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *params)
{
    return gl::GetIntegerv(pname, params);
}

void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    return gl::GetProgramiv(program, pname, params);
}

void GL_APIENTRY glGetProgramInfoLog(GLuint program,
                                     GLsizei bufsize,
                                     GLsizei *length,
                                     GLchar *infolog)
{
    return gl::GetProgramInfoLog(program, bufsize, length, infolog);
}

void GL_APIENTRY glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetRenderbufferParameteriv(target, pname, params);
}

void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    return gl::GetShaderiv(shader, pname, params);
}

void GL_APIENTRY glGetShaderInfoLog(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei *length,
                                    GLchar *infolog)
{
    return gl::GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GL_APIENTRY glGetShaderPrecisionFormat(GLenum shadertype,
                                            GLenum precisiontype,
                                            GLint *range,
                                            GLint *precision)
{
    return gl::GetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void GL_APIENTRY glGetShaderSource(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source)
{
    return gl::GetShaderSource(shader, bufsize, length, source);
}

const GLubyte *GL_APIENTRY glGetString(GLenum name)
{
    return gl::GetString(name);
}

void GL_APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
    return gl::GetTexParameterfv(target, pname, params);
}

void GL_APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetTexParameteriv(target, pname, params);
}

void GL_APIENTRY glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
    return gl::GetUniformfv(program, location, params);
}

void GL_APIENTRY glGetUniformiv(GLuint program, GLint location, GLint *params)
{
    return gl::GetUniformiv(program, location, params);
}

GLint GL_APIENTRY glGetUniformLocation(GLuint program, const GLchar *name)
{
    return gl::GetUniformLocation(program, name);
}

void GL_APIENTRY glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
    return gl::GetVertexAttribfv(index, pname, params);
}

void GL_APIENTRY glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    return gl::GetVertexAttribiv(index, pname, params);
}

void GL_APIENTRY glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    return gl::GetVertexAttribPointerv(index, pname, pointer);
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

GLboolean GL_APIENTRY glIsFramebuffer(GLuint framebuffer)
{
    return gl::IsFramebuffer(framebuffer);
}

GLboolean GL_APIENTRY glIsProgram(GLuint program)
{
    return gl::IsProgram(program);
}

GLboolean GL_APIENTRY glIsRenderbuffer(GLuint renderbuffer)
{
    return gl::IsRenderbuffer(renderbuffer);
}

GLboolean GL_APIENTRY glIsShader(GLuint shader)
{
    return gl::IsShader(shader);
}

GLboolean GL_APIENTRY glIsTexture(GLuint texture)
{
    return gl::IsTexture(texture);
}

void GL_APIENTRY glLineWidth(GLfloat width)
{
    return gl::LineWidth(width);
}

void GL_APIENTRY glLinkProgram(GLuint program)
{
    return gl::LinkProgram(program);
}

void GL_APIENTRY glPixelStorei(GLenum pname, GLint param)
{
    return gl::PixelStorei(pname, param);
}

void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
    return gl::PolygonOffset(factor, units);
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

void GL_APIENTRY glReleaseShaderCompiler(void)
{
    return gl::ReleaseShaderCompiler();
}

void GL_APIENTRY glRenderbufferStorage(GLenum target,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height)
{
    return gl::RenderbufferStorage(target, internalformat, width, height);
}

void GL_APIENTRY glSampleCoverage(GLfloat value, GLboolean invert)
{
    return gl::SampleCoverage(value, invert);
}

void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    return gl::Scissor(x, y, width, height);
}

void GL_APIENTRY glShaderBinary(GLsizei n,
                                const GLuint *shaders,
                                GLenum binaryformat,
                                const void *binary,
                                GLsizei length)
{
    return gl::ShaderBinary(n, shaders, binaryformat, binary, length);
}

void GL_APIENTRY glShaderSource(GLuint shader,
                                GLsizei count,
                                const GLchar *const *string,
                                const GLint *length)
{
    return gl::ShaderSource(shader, count, string, length);
}

void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    return gl::StencilFunc(func, ref, mask);
}

void GL_APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    return gl::StencilFuncSeparate(face, func, ref, mask);
}

void GL_APIENTRY glStencilMask(GLuint mask)
{
    return gl::StencilMask(mask);
}

void GL_APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask)
{
    return gl::StencilMaskSeparate(face, mask);
}

void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    return gl::StencilOp(fail, zfail, zpass);
}

void GL_APIENTRY glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    return gl::StencilOpSeparate(face, fail, zfail, zpass);
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

void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    return gl::TexParameterf(target, pname, param);
}

void GL_APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
    return gl::TexParameterfv(target, pname, params);
}

void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    return gl::TexParameteri(target, pname, param);
}

void GL_APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
    return gl::TexParameteriv(target, pname, params);
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

void GL_APIENTRY glUniform1f(GLint location, GLfloat x)
{
    return gl::Uniform1f(location, x);
}

void GL_APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    return gl::Uniform1fv(location, count, v);
}

void GL_APIENTRY glUniform1i(GLint location, GLint x)
{
    return gl::Uniform1i(location, x);
}

void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    return gl::Uniform1iv(location, count, v);
}

void GL_APIENTRY glUniform2f(GLint location, GLfloat x, GLfloat y)
{
    return gl::Uniform2f(location, x, y);
}

void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    return gl::Uniform2fv(location, count, v);
}

void GL_APIENTRY glUniform2i(GLint location, GLint x, GLint y)
{
    return gl::Uniform2i(location, x, y);
}

void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    return gl::Uniform2iv(location, count, v);
}

void GL_APIENTRY glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    return gl::Uniform3f(location, x, y, z);
}

void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    return gl::Uniform3fv(location, count, v);
}

void GL_APIENTRY glUniform3i(GLint location, GLint x, GLint y, GLint z)
{
    return gl::Uniform3i(location, x, y, z);
}

void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    return gl::Uniform3iv(location, count, v);
}

void GL_APIENTRY glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    return gl::Uniform4f(location, x, y, z, w);
}

void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    return gl::Uniform4fv(location, count, v);
}

void GL_APIENTRY glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    return gl::Uniform4i(location, x, y, z, w);
}

void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    return gl::Uniform4iv(location, count, v);
}

void GL_APIENTRY glUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    return gl::UniformMatrix2fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    return gl::UniformMatrix3fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value)
{
    return gl::UniformMatrix4fv(location, count, transpose, value);
}

void GL_APIENTRY glUseProgram(GLuint program)
{
    return gl::UseProgram(program);
}

void GL_APIENTRY glValidateProgram(GLuint program)
{
    return gl::ValidateProgram(program);
}

void GL_APIENTRY glVertexAttrib1f(GLuint indx, GLfloat x)
{
    return gl::VertexAttrib1f(indx, x);
}

void GL_APIENTRY glVertexAttrib1fv(GLuint indx, const GLfloat *values)
{
    return gl::VertexAttrib1fv(indx, values);
}

void GL_APIENTRY glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y)
{
    return gl::VertexAttrib2f(indx, x, y);
}

void GL_APIENTRY glVertexAttrib2fv(GLuint indx, const GLfloat *values)
{
    return gl::VertexAttrib2fv(indx, values);
}

void GL_APIENTRY glVertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
    return gl::VertexAttrib3f(indx, x, y, z);
}

void GL_APIENTRY glVertexAttrib3fv(GLuint indx, const GLfloat *values)
{
    return gl::VertexAttrib3fv(indx, values);
}

void GL_APIENTRY glVertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    return gl::VertexAttrib4f(indx, x, y, z, w);
}

void GL_APIENTRY glVertexAttrib4fv(GLuint indx, const GLfloat *values)
{
    return gl::VertexAttrib4fv(indx, values);
}

void GL_APIENTRY glVertexAttribPointer(GLuint indx,
                                       GLint size,
                                       GLenum type,
                                       GLboolean normalized,
                                       GLsizei stride,
                                       const void *ptr)
{
    return gl::VertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    return gl::Viewport(x, y, width, height);
}

void GL_APIENTRY glReadBuffer(GLenum mode)
{
    return gl::ReadBuffer(mode);
}

void GL_APIENTRY glDrawRangeElements(GLenum mode,
                                     GLuint start,
                                     GLuint end,
                                     GLsizei count,
                                     GLenum type,
                                     const void *indices)
{
    return gl::DrawRangeElements(mode, start, end, count, type, indices);
}

void GL_APIENTRY glTexImage3D(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void *pixels)
{
    return gl::TexImage3D(target, level, internalformat, width, height, depth, border, format, type,
                          pixels);
}

void GL_APIENTRY glTexSubImage3D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 const void *pixels)
{
    return gl::TexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                             type, pixels);
}

void GL_APIENTRY glCopyTexSubImage3D(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height)
{
    return gl::CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void GL_APIENTRY glCompressedTexImage3D(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void *data)
{
    return gl::CompressedTexImage3D(target, level, internalformat, width, height, depth, border,
                                    imageSize, data);
}

void GL_APIENTRY glCompressedTexSubImage3D(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void *data)
{
    return gl::CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                                       depth, format, imageSize, data);
}

void GL_APIENTRY glGenQueries(GLsizei n, GLuint *ids)
{
    return gl::GenQueries(n, ids);
}

void GL_APIENTRY glDeleteQueries(GLsizei n, const GLuint *ids)
{
    return gl::DeleteQueries(n, ids);
}

GLboolean GL_APIENTRY glIsQuery(GLuint id)
{
    return gl::IsQuery(id);
}

void GL_APIENTRY glBeginQuery(GLenum target, GLuint id)
{
    return gl::BeginQuery(target, id);
}

void GL_APIENTRY glEndQuery(GLenum target)
{
    return gl::EndQuery(target);
}

void GL_APIENTRY glGetQueryiv(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetQueryiv(target, pname, params);
}

void GL_APIENTRY glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
    return gl::GetQueryObjectuiv(id, pname, params);
}

GLboolean GL_APIENTRY glUnmapBuffer(GLenum target)
{
    return gl::UnmapBuffer(target);
}

void GL_APIENTRY glGetBufferPointerv(GLenum target, GLenum pname, void **params)
{
    return gl::GetBufferPointerv(target, pname, params);
}

void GL_APIENTRY glDrawBuffers(GLsizei n, const GLenum *bufs)
{
    return gl::DrawBuffers(n, bufs);
}

void GL_APIENTRY glUniformMatrix2x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    return gl::UniformMatrix2x3fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix3x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    return gl::UniformMatrix3x2fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix2x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    return gl::UniformMatrix2x4fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix4x2fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    return gl::UniformMatrix4x2fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix3x4fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    return gl::UniformMatrix3x4fv(location, count, transpose, value);
}

void GL_APIENTRY glUniformMatrix4x3fv(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    return gl::UniformMatrix4x3fv(location, count, transpose, value);
}

void GL_APIENTRY glBlitFramebuffer(GLint srcX0,
                                   GLint srcY0,
                                   GLint srcX1,
                                   GLint srcY1,
                                   GLint dstX0,
                                   GLint dstY0,
                                   GLint dstX1,
                                   GLint dstY1,
                                   GLbitfield mask,
                                   GLenum filter)
{
    return gl::BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
                               filter);
}

void GL_APIENTRY glRenderbufferStorageMultisample(GLenum target,
                                                  GLsizei samples,
                                                  GLenum internalformat,
                                                  GLsizei width,
                                                  GLsizei height)
{
    return gl::RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GL_APIENTRY glFramebufferTextureLayer(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLint layer)
{
    return gl::FramebufferTextureLayer(target, attachment, texture, level, layer);
}

void *GL_APIENTRY glMapBufferRange(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr length,
                                   GLbitfield access)
{
    return gl::MapBufferRange(target, offset, length, access);
}

void GL_APIENTRY glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)
{
    return gl::FlushMappedBufferRange(target, offset, length);
}

void GL_APIENTRY glBindVertexArray(GLuint array)
{
    return gl::BindVertexArray(array);
}

void GL_APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    return gl::DeleteVertexArrays(n, arrays);
}

void GL_APIENTRY glGenVertexArrays(GLsizei n, GLuint *arrays)
{
    return gl::GenVertexArrays(n, arrays);
}

GLboolean GL_APIENTRY glIsVertexArray(GLuint array)
{
    return gl::IsVertexArray(array);
}

void GL_APIENTRY glGetIntegeri_v(GLenum target, GLuint index, GLint *data)
{
    return gl::GetIntegeri_v(target, index, data);
}

void GL_APIENTRY glBeginTransformFeedback(GLenum primitiveMode)
{
    return gl::BeginTransformFeedback(primitiveMode);
}

void GL_APIENTRY glEndTransformFeedback(void)
{
    return gl::EndTransformFeedback();
}

void GL_APIENTRY
glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    return gl::BindBufferRange(target, index, buffer, offset, size);
}

void GL_APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    return gl::BindBufferBase(target, index, buffer);
}

void GL_APIENTRY glTransformFeedbackVaryings(GLuint program,
                                             GLsizei count,
                                             const GLchar *const *varyings,
                                             GLenum bufferMode)
{
    return gl::TransformFeedbackVaryings(program, count, varyings, bufferMode);
}

void GL_APIENTRY glGetTransformFeedbackVarying(GLuint program,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLsizei *size,
                                               GLenum *type,
                                               GLchar *name)
{
    return gl::GetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}

void GL_APIENTRY
glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    return gl::VertexAttribIPointer(index, size, type, stride, pointer);
}

void GL_APIENTRY glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
    return gl::GetVertexAttribIiv(index, pname, params);
}

void GL_APIENTRY glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
    return gl::GetVertexAttribIuiv(index, pname, params);
}

void GL_APIENTRY glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    return gl::VertexAttribI4i(index, x, y, z, w);
}

void GL_APIENTRY glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    return gl::VertexAttribI4ui(index, x, y, z, w);
}

void GL_APIENTRY glVertexAttribI4iv(GLuint index, const GLint *v)
{
    return gl::VertexAttribI4iv(index, v);
}

void GL_APIENTRY glVertexAttribI4uiv(GLuint index, const GLuint *v)
{
    return gl::VertexAttribI4uiv(index, v);
}

void GL_APIENTRY glGetUniformuiv(GLuint program, GLint location, GLuint *params)
{
    return gl::GetUniformuiv(program, location, params);
}

GLint GL_APIENTRY glGetFragDataLocation(GLuint program, const GLchar *name)
{
    return gl::GetFragDataLocation(program, name);
}

void GL_APIENTRY glUniform1ui(GLint location, GLuint v0)
{
    return gl::Uniform1ui(location, v0);
}

void GL_APIENTRY glUniform2ui(GLint location, GLuint v0, GLuint v1)
{
    return gl::Uniform2ui(location, v0, v1);
}

void GL_APIENTRY glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    return gl::Uniform3ui(location, v0, v1, v2);
}

void GL_APIENTRY glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    return gl::Uniform4ui(location, v0, v1, v2, v3);
}

void GL_APIENTRY glUniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
    return gl::Uniform1uiv(location, count, value);
}

void GL_APIENTRY glUniform2uiv(GLint location, GLsizei count, const GLuint *value)
{
    return gl::Uniform2uiv(location, count, value);
}

void GL_APIENTRY glUniform3uiv(GLint location, GLsizei count, const GLuint *value)
{
    return gl::Uniform3uiv(location, count, value);
}

void GL_APIENTRY glUniform4uiv(GLint location, GLsizei count, const GLuint *value)
{
    return gl::Uniform4uiv(location, count, value);
}

void GL_APIENTRY glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
    return gl::ClearBufferiv(buffer, drawbuffer, value);
}

void GL_APIENTRY glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
    return gl::ClearBufferuiv(buffer, drawbuffer, value);
}

void GL_APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    return gl::ClearBufferfv(buffer, drawbuffer, value);
}

void GL_APIENTRY glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    return gl::ClearBufferfi(buffer, drawbuffer, depth, stencil);
}

const GLubyte *GL_APIENTRY glGetStringi(GLenum name, GLuint index)
{
    return gl::GetStringi(name, index);
}

void GL_APIENTRY glCopyBufferSubData(GLenum readTarget,
                                     GLenum writeTarget,
                                     GLintptr readOffset,
                                     GLintptr writeOffset,
                                     GLsizeiptr size)
{
    return gl::CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void GL_APIENTRY glGetUniformIndices(GLuint program,
                                     GLsizei uniformCount,
                                     const GLchar *const *uniformNames,
                                     GLuint *uniformIndices)
{
    return gl::GetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}

void GL_APIENTRY glGetActiveUniformsiv(GLuint program,
                                       GLsizei uniformCount,
                                       const GLuint *uniformIndices,
                                       GLenum pname,
                                       GLint *params)
{
    return gl::GetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}

GLuint GL_APIENTRY glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
    return gl::GetUniformBlockIndex(program, uniformBlockName);
}

void GL_APIENTRY glGetActiveUniformBlockiv(GLuint program,
                                           GLuint uniformBlockIndex,
                                           GLenum pname,
                                           GLint *params)
{
    return gl::GetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void GL_APIENTRY glGetActiveUniformBlockName(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLchar *uniformBlockName)
{
    return gl::GetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length,
                                         uniformBlockName);
}

void GL_APIENTRY glUniformBlockBinding(GLuint program,
                                       GLuint uniformBlockIndex,
                                       GLuint uniformBlockBinding)
{
    return gl::UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

void GL_APIENTRY glDrawArraysInstanced(GLenum mode,
                                       GLint first,
                                       GLsizei count,
                                       GLsizei instanceCount)
{
    return gl::DrawArraysInstanced(mode, first, count, instanceCount);
}

void GL_APIENTRY glDrawElementsInstanced(GLenum mode,
                                         GLsizei count,
                                         GLenum type,
                                         const void *indices,
                                         GLsizei instanceCount)
{
    return gl::DrawElementsInstanced(mode, count, type, indices, instanceCount);
}

GLsync GL_APIENTRY glFenceSync(GLenum condition, GLbitfield flags)
{
    return gl::FenceSync(condition, flags);
}

GLboolean GL_APIENTRY glIsSync(GLsync sync)
{
    return gl::IsSync(sync);
}

void GL_APIENTRY glDeleteSync(GLsync sync)
{
    return gl::DeleteSync(sync);
}

GLenum GL_APIENTRY glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    return gl::ClientWaitSync(sync, flags, timeout);
}

void GL_APIENTRY glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    return gl::WaitSync(sync, flags, timeout);
}

void GL_APIENTRY glGetInteger64v(GLenum pname, GLint64 *params)
{
    return gl::GetInteger64v(pname, params);
}

void GL_APIENTRY
glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
    return gl::GetSynciv(sync, pname, bufSize, length, values);
}

void GL_APIENTRY glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data)
{
    return gl::GetInteger64i_v(target, index, data);
}

void GL_APIENTRY glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params)
{
    return gl::GetBufferParameteri64v(target, pname, params);
}

void GL_APIENTRY glGenSamplers(GLsizei count, GLuint *samplers)
{
    return gl::GenSamplers(count, samplers);
}

void GL_APIENTRY glDeleteSamplers(GLsizei count, const GLuint *samplers)
{
    return gl::DeleteSamplers(count, samplers);
}

GLboolean GL_APIENTRY glIsSampler(GLuint sampler)
{
    return gl::IsSampler(sampler);
}

void GL_APIENTRY glBindSampler(GLuint unit, GLuint sampler)
{
    return gl::BindSampler(unit, sampler);
}

void GL_APIENTRY glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    return gl::SamplerParameteri(sampler, pname, param);
}

void GL_APIENTRY glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param)
{
    return gl::SamplerParameteriv(sampler, pname, param);
}

void GL_APIENTRY glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    return gl::SamplerParameterf(sampler, pname, param);
}

void GL_APIENTRY glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)
{
    return gl::SamplerParameterfv(sampler, pname, param);
}

void GL_APIENTRY glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
    return gl::GetSamplerParameteriv(sampler, pname, params);
}

void GL_APIENTRY glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
    return gl::GetSamplerParameterfv(sampler, pname, params);
}

void GL_APIENTRY glVertexAttribDivisor(GLuint index, GLuint divisor)
{
    return gl::VertexAttribDivisor(index, divisor);
}

void GL_APIENTRY glBindTransformFeedback(GLenum target, GLuint id)
{
    return gl::BindTransformFeedback(target, id);
}

void GL_APIENTRY glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
    return gl::DeleteTransformFeedbacks(n, ids);
}

void GL_APIENTRY glGenTransformFeedbacks(GLsizei n, GLuint *ids)
{
    return gl::GenTransformFeedbacks(n, ids);
}

GLboolean GL_APIENTRY glIsTransformFeedback(GLuint id)
{
    return gl::IsTransformFeedback(id);
}

void GL_APIENTRY glPauseTransformFeedback(void)
{
    return gl::PauseTransformFeedback();
}

void GL_APIENTRY glResumeTransformFeedback(void)
{
    return gl::ResumeTransformFeedback();
}

void GL_APIENTRY glGetProgramBinary(GLuint program,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLenum *binaryFormat,
                                    void *binary)
{
    return gl::GetProgramBinary(program, bufSize, length, binaryFormat, binary);
}

void GL_APIENTRY glProgramBinary(GLuint program,
                                 GLenum binaryFormat,
                                 const void *binary,
                                 GLsizei length)
{
    return gl::ProgramBinary(program, binaryFormat, binary, length);
}

void GL_APIENTRY glProgramParameteri(GLuint program, GLenum pname, GLint value)
{
    return gl::ProgramParameteri(program, pname, value);
}

void GL_APIENTRY glInvalidateFramebuffer(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum *attachments)
{
    return gl::InvalidateFramebuffer(target, numAttachments, attachments);
}

void GL_APIENTRY glInvalidateSubFramebuffer(GLenum target,
                                            GLsizei numAttachments,
                                            const GLenum *attachments,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height)
{
    return gl::InvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}

void GL_APIENTRY
glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    return gl::TexStorage2D(target, levels, internalformat, width, height);
}

void GL_APIENTRY glTexStorage3D(GLenum target,
                                GLsizei levels,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth)
{
    return gl::TexStorage3D(target, levels, internalformat, width, height, depth);
}

void GL_APIENTRY glGetInternalformativ(GLenum target,
                                       GLenum internalformat,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLint *params)
{
    return gl::GetInternalformativ(target, internalformat, pname, bufSize, params);
}

void GL_APIENTRY glBlitFramebufferANGLE(GLint srcX0,
                                        GLint srcY0,
                                        GLint srcX1,
                                        GLint srcY1,
                                        GLint dstX0,
                                        GLint dstY0,
                                        GLint dstX1,
                                        GLint dstY1,
                                        GLbitfield mask,
                                        GLenum filter)
{
    return gl::BlitFramebufferANGLE(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
                                    filter);
}

void GL_APIENTRY glRenderbufferStorageMultisampleANGLE(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height)
{
    return gl::RenderbufferStorageMultisampleANGLE(target, samples, internalformat, width, height);
}

void GL_APIENTRY glDiscardFramebufferEXT(GLenum target,
                                         GLsizei numAttachments,
                                         const GLenum *attachments)
{
    return gl::DiscardFramebufferEXT(target, numAttachments, attachments);
}

void GL_APIENTRY glDeleteFencesNV(GLsizei n, const GLuint *fences)
{
    return gl::DeleteFencesNV(n, fences);
}

void GL_APIENTRY glGenFencesNV(GLsizei n, GLuint *fences)
{
    return gl::GenFencesNV(n, fences);
}

GLboolean GL_APIENTRY glIsFenceNV(GLuint fence)
{
    return gl::IsFenceNV(fence);
}

GLboolean GL_APIENTRY glTestFenceNV(GLuint fence)
{
    return gl::TestFenceNV(fence);
}

void GL_APIENTRY glGetFenceivNV(GLuint fence, GLenum pname, GLint *params)
{
    return gl::GetFenceivNV(fence, pname, params);
}

void GL_APIENTRY glFinishFenceNV(GLuint fence)
{
    return gl::FinishFenceNV(fence);
}

void GL_APIENTRY glSetFenceNV(GLuint fence, GLenum condition)
{
    return gl::SetFenceNV(fence, condition);
}

void GL_APIENTRY glGetTranslatedShaderSourceANGLE(GLuint shader,
                                                  GLsizei bufsize,
                                                  GLsizei *length,
                                                  GLchar *source)
{
    return gl::GetTranslatedShaderSourceANGLE(shader, bufsize, length, source);
}

void GL_APIENTRY glTexStorage2DEXT(GLenum target,
                                   GLsizei levels,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height)
{
    return gl::TexStorage2DEXT(target, levels, internalformat, width, height);
}

GLenum GL_APIENTRY glGetGraphicsResetStatusEXT(void)
{
    return gl::GetGraphicsResetStatusEXT();
}

void GL_APIENTRY glReadnPixelsEXT(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  void *data)
{
    return gl::ReadnPixelsEXT(x, y, width, height, format, type, bufSize, data);
}

void GL_APIENTRY glGetnUniformfvEXT(GLuint program, GLint location, GLsizei bufSize, float *params)
{
    return gl::GetnUniformfvEXT(program, location, bufSize, params);
}

void GL_APIENTRY glGetnUniformivEXT(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
    return gl::GetnUniformivEXT(program, location, bufSize, params);
}

void GL_APIENTRY glGenQueriesEXT(GLsizei n, GLuint *ids)
{
    return gl::GenQueriesEXT(n, ids);
}

void GL_APIENTRY glDeleteQueriesEXT(GLsizei n, const GLuint *ids)
{
    return gl::DeleteQueriesEXT(n, ids);
}

GLboolean GL_APIENTRY glIsQueryEXT(GLuint id)
{
    return gl::IsQueryEXT(id);
}

void GL_APIENTRY glBeginQueryEXT(GLenum target, GLuint id)
{
    return gl::BeginQueryEXT(target, id);
}

void GL_APIENTRY glEndQueryEXT(GLenum target)
{
    return gl::EndQueryEXT(target);
}

void GL_APIENTRY glQueryCounterEXT(GLuint id, GLenum target)
{
    return gl::QueryCounterEXT(id, target);
}

void GL_APIENTRY glGetQueryivEXT(GLenum target, GLenum pname, GLint *params)
{
    return gl::GetQueryivEXT(target, pname, params);
}

void GL_APIENTRY glGetQueryObjectivEXT(GLuint id, GLenum pname, GLint *params)
{
    return gl::GetQueryObjectivEXT(id, pname, params);
}

void GL_APIENTRY glGetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint *params)
{
    return gl::GetQueryObjectuivEXT(id, pname, params);
}

void GL_APIENTRY glGetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 *params)
{
    return gl::GetQueryObjecti64vEXT(id, pname, params);
}

void GL_APIENTRY glGetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 *params)
{
    return gl::GetQueryObjectui64vEXT(id, pname, params);
}

void GL_APIENTRY glDrawBuffersEXT(GLsizei n, const GLenum *bufs)
{
    return gl::DrawBuffersEXT(n, bufs);
}

void GL_APIENTRY glDrawArraysInstancedANGLE(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount)
{
    return gl::DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void GL_APIENTRY glDrawElementsInstancedANGLE(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void *indices,
                                              GLsizei primcount)
{
    return gl::DrawElementsInstancedANGLE(mode, count, type, indices, primcount);
}

void GL_APIENTRY glVertexAttribDivisorANGLE(GLuint index, GLuint divisor)
{
    return gl::VertexAttribDivisorANGLE(index, divisor);
}

void GL_APIENTRY glGetProgramBinaryOES(GLuint program,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLenum *binaryFormat,
                                       void *binary)
{
    return gl::GetProgramBinaryOES(program, bufSize, length, binaryFormat, binary);
}

void GL_APIENTRY glProgramBinaryOES(GLuint program,
                                    GLenum binaryFormat,
                                    const void *binary,
                                    GLint length)
{
    return gl::ProgramBinaryOES(program, binaryFormat, binary, length);
}

void *GL_APIENTRY glMapBufferOES(GLenum target, GLenum access)
{
    return gl::MapBufferOES(target, access);
}

GLboolean GL_APIENTRY glUnmapBufferOES(GLenum target)
{
    return gl::UnmapBufferOES(target);
}

void GL_APIENTRY glGetBufferPointervOES(GLenum target, GLenum pname, void **params)
{
    return gl::GetBufferPointervOES(target, pname, params);
}

void *GL_APIENTRY glMapBufferRangeEXT(GLenum target,
                                      GLintptr offset,
                                      GLsizeiptr length,
                                      GLbitfield access)
{
    return gl::MapBufferRangeEXT(target, offset, length, access);
}

void GL_APIENTRY glFlushMappedBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length)
{
    return gl::FlushMappedBufferRangeEXT(target, offset, length);
}

void GL_APIENTRY glInsertEventMarkerEXT(GLsizei length, const char *marker)
{
    return gl::InsertEventMarkerEXT(length, marker);
}

void GL_APIENTRY glPushGroupMarkerEXT(GLsizei length, const char *marker)
{
    return gl::PushGroupMarkerEXT(length, marker);
}

void GL_APIENTRY glPopGroupMarkerEXT()
{
    return gl::PopGroupMarkerEXT();
}

void GL_APIENTRY glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image)
{
    return gl::EGLImageTargetTexture2DOES(target, image);
}

void GL_APIENTRY glEGLImageTargetRenderbufferStorageOES(GLenum target, GLeglImageOES image)
{
    return gl::EGLImageTargetRenderbufferStorageOES(target, image);
}

void GL_APIENTRY glBindVertexArrayOES(GLuint array)
{
    return gl::BindVertexArrayOES(array);
}

void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n, const GLuint *arrays)
{
    return gl::DeleteVertexArraysOES(n, arrays);
}

void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint *arrays)
{
    return gl::GenVertexArraysOES(n, arrays);
}

GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint array)
{
    return gl::IsVertexArrayOES(array);
}

void GL_APIENTRY glDebugMessageControlKHR(GLenum source,
                                          GLenum type,
                                          GLenum severity,
                                          GLsizei count,
                                          const GLuint *ids,
                                          GLboolean enabled)
{
    return gl::DebugMessageControlKHR(source, type, severity, count, ids, enabled);
}

void GL_APIENTRY glDebugMessageInsertKHR(GLenum source,
                                         GLenum type,
                                         GLuint id,
                                         GLenum severity,
                                         GLsizei length,
                                         const GLchar *buf)
{
    return gl::DebugMessageInsertKHR(source, type, id, severity, length, buf);
}

void GL_APIENTRY glDebugMessageCallbackKHR(GLDEBUGPROCKHR callback, const void *userParam)
{
    return gl::DebugMessageCallbackKHR(callback, userParam);
}

GLuint GL_APIENTRY glGetDebugMessageLogKHR(GLuint count,
                                           GLsizei bufSize,
                                           GLenum *sources,
                                           GLenum *types,
                                           GLuint *ids,
                                           GLenum *severities,
                                           GLsizei *lengths,
                                           GLchar *messageLog)
{
    return gl::GetDebugMessageLogKHR(count, bufSize, sources, types, ids, severities, lengths,
                                     messageLog);
}

void GL_APIENTRY glPushDebugGroupKHR(GLenum source,
                                     GLuint id,
                                     GLsizei length,
                                     const GLchar *message)
{
    return gl::PushDebugGroupKHR(source, id, length, message);
}

void GL_APIENTRY glPopDebugGroupKHR(void)
{
    return gl::PopDebugGroupKHR();
}

void GL_APIENTRY glObjectLabelKHR(GLenum identifier,
                                  GLuint name,
                                  GLsizei length,
                                  const GLchar *label)
{
    return gl::ObjectLabelKHR(identifier, name, length, label);
}

void GL_APIENTRY
glGetObjectLabelKHR(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label)
{
    return gl::GetObjectLabelKHR(identifier, name, bufSize, length, label);
}

void GL_APIENTRY glObjectPtrLabelKHR(const void *ptr, GLsizei length, const GLchar *label)
{
    return gl::ObjectPtrLabelKHR(ptr, length, label);
}

void GL_APIENTRY glGetObjectPtrLabelKHR(const void *ptr,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *label)
{
    return gl::GetObjectPtrLabelKHR(ptr, bufSize, length, label);
}

void GL_APIENTRY glGetPointervKHR(GLenum pname, void **params)
{
    return gl::GetPointervKHR(pname, params);
}

void GL_APIENTRY glBindUniformLocationCHROMIUM(GLuint program, GLint location, const GLchar *name)
{
    return gl::BindUniformLocationCHROMIUM(program, location, name);
}

void GL_APIENTRY glCoverageModulationCHROMIUM(GLenum components)
{
    return gl::CoverageModulationCHROMIUM(components);
}

// CHROMIUM_path_rendendering
void GL_APIENTRY glMatrixLoadfCHROMIUM(GLenum matrixMode, const GLfloat *matrix)
{
    gl::MatrixLoadfCHROMIUM(matrixMode, matrix);
}

void GL_APIENTRY glMatrixLoadIdentityCHROMIUM(GLenum matrixMode)
{
    gl::MatrixLoadIdentityCHROMIUM(matrixMode);
}

GLuint GL_APIENTRY glGenPathsCHROMIUM(GLsizei range)
{
    return gl::GenPathsCHROMIUM(range);
}

void GL_APIENTRY glDeletePathsCHROMIUM(GLuint first, GLsizei range)
{
    gl::DeletePathsCHROMIUM(first, range);
}

GLboolean GL_APIENTRY glIsPathCHROMIUM(GLuint path)
{
    return gl::IsPathCHROMIUM(path);
}

void GL_APIENTRY glPathCommandsCHROMIUM(GLuint path,
                                        GLsizei numCommands,
                                        const GLubyte *commands,
                                        GLsizei numCoords,
                                        GLenum coordType,
                                        const void *coords)
{
    gl::PathCommandsCHROMIUM(path, numCommands, commands, numCoords, coordType, coords);
}

void GL_APIENTRY glPathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value)
{
    gl::PathParameterfCHROMIUM(path, pname, value);
}

void GL_APIENTRY glPathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value)
{
    gl::PathParameteriCHROMIUM(path, pname, value);
}

void GL_APIENTRY glGetPathParameterfvCHROMIUM(GLuint path, GLenum pname, GLfloat *value)
{
    gl::GetPathParameterfCHROMIUM(path, pname, value);
}

void GL_APIENTRY glGetPathParameterivCHROMIUM(GLuint path, GLenum pname, GLint *value)
{
    gl::GetPathParameteriCHROMIUM(path, pname, value);
}

void GL_APIENTRY glPathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask)
{
    gl::PathStencilFuncCHROMIUM(func, ref, mask);
}

void GL_APIENTRY glStencilFillPathCHROMIUM(GLuint path, GLenum fillMode, GLuint mask)
{
    gl::StencilFillPathCHROMIUM(path, fillMode, mask);
}

void GL_APIENTRY glStencilStrokePathCHROMIUM(GLuint path, GLint reference, GLuint mask)
{
    gl::StencilStrokePathCHROMIUM(path, reference, mask);
}

void GL_APIENTRY glCoverFillPathCHROMIUM(GLuint path, GLenum coverMode)
{
    gl::CoverFillPathCHROMIUM(path, coverMode);
}

void GL_APIENTRY glCoverStrokePathCHROMIUM(GLuint path, GLenum coverMode)
{
    gl::CoverStrokePathCHROMIUM(path, coverMode);
}

void GL_APIENTRY glStencilThenCoverFillPathCHROMIUM(GLuint path,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum coverMode)
{
    gl::StencilThenCoverFillPathCHROMIUM(path, fillMode, mask, coverMode);
}

void GL_APIENTRY glStencilThenCoverStrokePathCHROMIUM(GLuint path,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum coverMode)
{
    gl::StencilThenCoverStrokePathCHROMIUM(path, reference, mask, coverMode);
}

void GL_APIENTRY glCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                  GLenum pathNameType,
                                                  const void *paths,
                                                  GLuint pathBase,
                                                  GLenum coverMode,
                                                  GLenum transformType,
                                                  const GLfloat *transformValues)
{
    gl::CoverFillPathInstancedCHROMIUM(numPaths, pathNameType, paths, pathBase, coverMode,
                                       transformType, transformValues);
}

void GL_APIENTRY glCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                    GLenum pathNameType,
                                                    const void *paths,
                                                    GLuint pathBase,
                                                    GLenum coverMode,
                                                    GLenum transformType,
                                                    const GLfloat *transformValues)
{
    gl::CoverStrokePathInstancedCHROMIUM(numPaths, pathNameType, paths, pathBase, coverMode,
                                         transformType, transformValues);
}

void GL_APIENTRY glStencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                    GLenum pathNameType,
                                                    const void *paths,
                                                    GLuint pathBase,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum transformType,
                                                    const GLfloat *transformValues)
{
    gl::StencilFillPathInstancedCHROMIUM(numPaths, pathNameType, paths, pathBase, fillMode, mask,
                                         transformType, transformValues);
}

void GL_APIENTRY glStencilStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                      GLenum pathNameType,
                                                      const void *paths,
                                                      GLuint pathBase,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum transformType,
                                                      const GLfloat *transformValues)
{
    gl::StencilStrokePathInstancedCHROMIUM(numPaths, pathNameType, paths, pathBase, reference, mask,
                                           transformType, transformValues);
}

void GL_APIENTRY glStencilThenCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                             GLenum pathNameType,
                                                             const void *paths,
                                                             GLuint pathBase,
                                                             GLenum fillMode,
                                                             GLuint mask,
                                                             GLenum coverMode,
                                                             GLenum transformType,
                                                             const GLfloat *transformValues)
{
    gl::StencilThenCoverFillPathInstancedCHROMIUM(numPaths, pathNameType, paths, pathBase, fillMode,
                                                  mask, coverMode, transformType, transformValues);
}

void GL_APIENTRY glStencilThenCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                               GLenum pathNameType,
                                                               const void *paths,
                                                               GLuint pathBase,
                                                               GLint reference,
                                                               GLuint mask,
                                                               GLenum coverMode,
                                                               GLenum transformType,
                                                               const GLfloat *transformValues)
{
    gl::StencilThenCoverStrokePathInstancedCHROMIUM(numPaths, pathNameType, paths, pathBase,
                                                    reference, mask, coverMode, transformType,
                                                    transformValues);
}

void GL_APIENTRY glBindFragmentInputLocationCHROMIUM(GLuint program,
                                                     GLint location,
                                                     const GLchar *name)
{
    gl::BindFragmentInputLocationCHROMIUM(program, location, name);
}

void GL_APIENTRY glProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                                       GLint location,
                                                       GLenum genMode,
                                                       GLint components,
                                                       const GLfloat *coeffs)
{
    gl::ProgramPathFragmentInputGenCHROMIUM(program, location, genMode, components, coeffs);
}

// GLES 3.1
void GL_APIENTRY glDispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    gl::DispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void GL_APIENTRY glDispatchComputeIndirect(GLintptr indirect)
{
    gl::DispatchComputeIndirect(indirect);
}

void GL_APIENTRY glDrawArraysIndirect(GLenum mode, const void *indirect)
{
    gl::DrawArraysIndirect(mode, indirect);
}

void GL_APIENTRY glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    gl::DrawElementsIndirect(mode, type, indirect);
}

void GL_APIENTRY glFramebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    gl::FramebufferParameteri(target, pname, param);
}

void GL_APIENTRY glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    gl::GetFramebufferParameteriv(target, pname, params);
}

void GL_APIENTRY glGetProgramInterfaceiv(GLuint program,
                                         GLenum programInterface,
                                         GLenum pname,
                                         GLint *params)
{
    gl::GetProgramInterfaceiv(program, programInterface, pname, params);
}

GLuint GL_APIENTRY glGetProgramResourceIndex(GLuint program,
                                             GLenum programInterface,
                                             const GLchar *name)
{
    return gl::GetProgramResourceIndex(program, programInterface, name);
}

void GL_APIENTRY glGetProgramResourceName(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLchar *name)
{
    gl::GetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

void GL_APIENTRY glGetProgramResourceiv(GLuint program,
                                        GLenum programInterface,
                                        GLuint index,
                                        GLsizei propCount,
                                        const GLenum *props,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *params)
{
    gl::GetProgramResourceiv(program, programInterface, index, propCount, props, bufSize, length,
                             params);
}

GLint GL_APIENTRY glGetProgramResourceLocation(GLuint program,
                                               GLenum programInterface,
                                               const GLchar *name)
{
    return gl::GetProgramResourceLocation(program, programInterface, name);
}

void GL_APIENTRY glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    gl::UseProgramStages(pipeline, stages, program);
}

void GL_APIENTRY glActiveShaderProgram(GLuint pipeline, GLuint program)
{
    gl::ActiveShaderProgram(pipeline, program);
}

GLuint GL_APIENTRY glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const *strings)
{
    return gl::CreateShaderProgramv(type, count, strings);
}

void GL_APIENTRY glBindProgramPipeline(GLuint pipeline)
{
    gl::BindProgramPipeline(pipeline);
}

void GL_APIENTRY glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines)
{
    gl::DeleteProgramPipelines(n, pipelines);
}

void GL_APIENTRY glGenProgramPipelines(GLsizei n, GLuint *pipelines)
{
    gl::GenProgramPipelines(n, pipelines);
}

GLboolean GL_APIENTRY glIsProgramPipeline(GLuint pipeline)
{
    return gl::IsProgramPipeline(pipeline);
}

void GL_APIENTRY glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    gl::GetProgramPipelineiv(pipeline, pname, params);
}

void GL_APIENTRY glProgramUniform1i(GLuint program, GLint location, GLint v0)
{
    gl::ProgramUniform1i(program, location, v0);
}

void GL_APIENTRY glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    gl::ProgramUniform2i(program, location, v0, v1);
}

void GL_APIENTRY glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    gl::ProgramUniform3i(program, location, v0, v1, v2);
}

void GL_APIENTRY
glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    gl::ProgramUniform4i(program, location, v0, v1, v2, v3);
}

void GL_APIENTRY glProgramUniform1ui(GLuint program, GLint location, GLuint v0)
{
    gl::ProgramUniform1ui(program, location, v0);
}

void GL_APIENTRY glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    gl::ProgramUniform2ui(program, location, v0, v1);
}

void GL_APIENTRY
glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    gl::ProgramUniform3ui(program, location, v0, v1, v2);
}

void GL_APIENTRY
glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    gl::ProgramUniform4ui(program, location, v0, v1, v2, v3);
}

void GL_APIENTRY glProgramUniform1f(GLuint program, GLint location, GLfloat v0)
{
    gl::ProgramUniform1f(program, location, v0);
}

void GL_APIENTRY glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    gl::ProgramUniform2f(program, location, v0, v1);
}

void GL_APIENTRY
glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    gl::ProgramUniform3f(program, location, v0, v1, v2);
}

void GL_APIENTRY
glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    gl::ProgramUniform4f(program, location, v0, v1, v2, v3);
}

void GL_APIENTRY glProgramUniform1iv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLint *value)
{
    gl::ProgramUniform1iv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform2iv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLint *value)
{
    gl::ProgramUniform2iv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform3iv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLint *value)
{
    gl::ProgramUniform3iv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform4iv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLint *value)
{
    gl::ProgramUniform4iv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform1uiv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLuint *value)
{
    gl::ProgramUniform1uiv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform2uiv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLuint *value)
{
    gl::ProgramUniform2uiv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform3uiv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLuint *value)
{
    gl::ProgramUniform3uiv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform4uiv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      const GLuint *value)
{
    gl::ProgramUniform4uiv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform1fv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLfloat *value)
{
    gl::ProgramUniform1fv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform2fv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLfloat *value)
{
    gl::ProgramUniform2fv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform3fv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLfloat *value)
{
    gl::ProgramUniform3fv(program, location, count, value);
}

void GL_APIENTRY glProgramUniform4fv(GLuint program,
                                     GLint location,
                                     GLsizei count,
                                     const GLfloat *value)
{
    gl::ProgramUniform4fv(program, location, count, value);
}

void GL_APIENTRY glProgramUniformMatrix2fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    gl::ProgramUniformMatrix2fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix3fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    gl::ProgramUniformMatrix3fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix4fv(GLuint program,
                                           GLint location,
                                           GLsizei count,
                                           GLboolean transpose,
                                           const GLfloat *value)
{
    gl::ProgramUniformMatrix4fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix2x3fv(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    gl::ProgramUniformMatrix2x3fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix3x2fv(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    gl::ProgramUniformMatrix3x2fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix2x4fv(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    gl::ProgramUniformMatrix2x4fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix4x2fv(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    gl::ProgramUniformMatrix4x2fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix3x4fv(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    gl::ProgramUniformMatrix3x4fv(program, location, count, transpose, value);
}

void GL_APIENTRY glProgramUniformMatrix4x3fv(GLuint program,
                                             GLint location,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value)
{
    gl::ProgramUniformMatrix4x3fv(program, location, count, transpose, value);
}

void GL_APIENTRY glValidateProgramPipeline(GLuint pipeline)
{
    gl::ValidateProgramPipeline(pipeline);
}

void GL_APIENTRY glGetProgramPipelineInfoLog(GLuint pipeline,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLchar *infoLog)
{
    gl::GetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
}

void GL_APIENTRY glBindImageTexture(GLuint unit,
                                    GLuint texture,
                                    GLint level,
                                    GLboolean layered,
                                    GLint layer,
                                    GLenum access,
                                    GLenum format)
{
    gl::BindImageTexture(unit, texture, level, layered, layer, access, format);
}

void GL_APIENTRY glGetBooleani_v(GLenum target, GLuint index, GLboolean *data)
{
    gl::GetBooleani_v(target, index, data);
}

void GL_APIENTRY glMemoryBarrier(GLbitfield barriers)
{
    gl::MemoryBarrier(barriers);
}

void GL_APIENTRY glMemoryBarrierByRegion(GLbitfield barriers)
{
    gl::MemoryBarrierByRegion(barriers);
}

void GL_APIENTRY glTexStorage2DMultisample(GLenum target,
                                           GLsizei samples,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLboolean fixedsamplelocations)
{
    gl::TexStorage2DMultisample(target, samples, internalformat, width, height,
                                fixedsamplelocations);
}

void GL_APIENTRY glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    gl::GetMultisamplefv(pname, index, val);
}

void GL_APIENTRY glSampleMaski(GLuint maskNumber, GLbitfield mask)
{
    gl::SampleMaski(maskNumber, mask);
}

void GL_APIENTRY glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
    gl::GetTexLevelParameteriv(target, level, pname, params);
}

void GL_APIENTRY glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    gl::GetTexLevelParameterfv(target, level, pname, params);
}

void GL_APIENTRY glBindVertexBuffer(GLuint bindingindex,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizei stride)
{
    gl::BindVertexBuffer(bindingindex, buffer, offset, stride);
}

void GL_APIENTRY glVertexAttribFormat(GLuint attribindex,
                                      GLint size,
                                      GLenum type,
                                      GLboolean normalized,
                                      GLuint relativeoffset)
{
    gl::VertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}

void GL_APIENTRY glVertexAttribIFormat(GLuint attribindex,
                                       GLint size,
                                       GLenum type,
                                       GLuint relativeoffset)
{
    gl::VertexAttribIFormat(attribindex, size, type, relativeoffset);
}

void GL_APIENTRY glVertexAttribBinding(GLuint attribindex, GLuint bindingindex)
{
    gl::VertexAttribBinding(attribindex, bindingindex);
}

void GL_APIENTRY glVertexBindingDivisor(GLuint bindingindex, GLuint divisor)
{
    gl::VertexBindingDivisor(bindingindex, divisor);
}

void GL_APIENTRY glFramebufferTextureMultiviewLayeredANGLE(GLenum target,
                                                           GLenum attachment,
                                                           GLuint texture,
                                                           GLint level,
                                                           GLint baseViewIndex,
                                                           GLsizei numViews)
{
    gl::FramebufferTextureMultiviewLayeredANGLE(target, attachment, texture, level, baseViewIndex,
                                                numViews);
}

void GL_APIENTRY glFramebufferTextureMultiviewSideBySideANGLE(GLenum target,
                                                              GLenum attachment,
                                                              GLuint texture,
                                                              GLint level,
                                                              GLsizei numViews,
                                                              const GLint *viewportOffsets)
{
    gl::FramebufferTextureMultiviewSideBySideANGLE(target, attachment, texture, level, numViews,
                                                   viewportOffsets);
}

void GL_APIENTRY glRequestExtensionANGLE(const GLchar *name)
{
    gl::RequestExtensionANGLE(name);
}

}  // extern "C"
