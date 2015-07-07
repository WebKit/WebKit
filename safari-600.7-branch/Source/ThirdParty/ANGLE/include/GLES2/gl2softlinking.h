/* 
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "SoftLinking.h"

SOFT_LINK_LIBRARY(libGLESv2)

SOFT_LINK(libGLESv2, glActiveTexture, void, GL_APIENTRY, (GLenum texture), (texture));
SOFT_LINK(libGLESv2, glAttachShader, void, GL_APIENTRY, (GLuint program, GLuint shader), (program, shader));
SOFT_LINK(libGLESv2, glBindAttribLocation, void, GL_APIENTRY, (GLuint program, GLuint index, const GLchar* name), (program, index, name));
SOFT_LINK(libGLESv2, glBindBuffer, void, GL_APIENTRY, (GLenum target, GLuint buffer), (target, buffer));
SOFT_LINK(libGLESv2, glBindFramebuffer, void, GL_APIENTRY, (GLenum target, GLuint framebuffer), (target, framebuffer));
SOFT_LINK(libGLESv2, glBindRenderbuffer, void, GL_APIENTRY, (GLenum target, GLuint renderbuffer), (target, renderbuffer));
SOFT_LINK(libGLESv2, glBindTexture, void, GL_APIENTRY, (GLenum target, GLuint texture), (target, texture));
SOFT_LINK(libGLESv2, glBlendColor, void, GL_APIENTRY, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha));
SOFT_LINK(libGLESv2, glBlendEquation, void, GL_APIENTRY, (GLenum mode), (mode));
SOFT_LINK(libGLESv2, glBlendEquationSeparate, void, GL_APIENTRY, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha));
SOFT_LINK(libGLESv2, glBlendFunc, void, GL_APIENTRY, (GLenum sfactor, GLenum dfactor), (sfactor, dfactor));
SOFT_LINK(libGLESv2, glBlendFuncSeparate, void, GL_APIENTRY, (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha), (srcRGB, dstRGB, srcAlpha, dstAlpha));
SOFT_LINK(libGLESv2, glBufferData, void, GL_APIENTRY, (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage), (target, size, data, usage));
SOFT_LINK(libGLESv2, glBufferSubData, void, GL_APIENTRY, (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data), (target, offset, size, data));
SOFT_LINK(libGLESv2, glCheckFramebufferStatus, GLenum, GL_APIENTRY, (GLenum target), (target));
SOFT_LINK(libGLESv2, glClear, void, GL_APIENTRY, (GLbitfield mask), (mask));
SOFT_LINK(libGLESv2, glClearColor, void, GL_APIENTRY, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha));
SOFT_LINK(libGLESv2, glClearDepthf, void, GL_APIENTRY, (GLclampf depth), (depth));
SOFT_LINK(libGLESv2, glClearStencil, void, GL_APIENTRY, (GLint s), (s));
SOFT_LINK(libGLESv2, glColorMask, void, GL_APIENTRY, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha), (red, green, blue, alpha));
SOFT_LINK(libGLESv2, glCompileShader, void, GL_APIENTRY, (GLuint shader), (shader));
SOFT_LINK(libGLESv2, glCompressedTexImage2D, void, GL_APIENTRY, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data), (target, level, internalformat, width, height, border, imageSize, data));
SOFT_LINK(libGLESv2, glCompressedTexSubImage2D, void, GL_APIENTRY, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid* data), (target, level, xoffset, yoffset, width, height, format, imageSize, data));
SOFT_LINK(libGLESv2, glCopyTexImage2D, void, GL_APIENTRY, (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border), (target, level, internalformat, x, y, width, height, border));
SOFT_LINK(libGLESv2, glCopyTexSubImage2D, void, GL_APIENTRY, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height), (target, level, xoffset, yoffset, x, y, width, height));
SOFT_LINK(libGLESv2, glCreateProgram, GLuint, GL_APIENTRY, (void), ());
SOFT_LINK(libGLESv2, glCreateShader, GLuint, GL_APIENTRY, (GLenum type), (type));
SOFT_LINK(libGLESv2, glCullFace, void, GL_APIENTRY, (GLenum mode), (mode));
SOFT_LINK(libGLESv2, glDeleteBuffers, void, GL_APIENTRY, (GLsizei n, const GLuint* buffers), (n, buffers));
SOFT_LINK(libGLESv2, glDeleteFramebuffers, void, GL_APIENTRY, (GLsizei n, const GLuint* framebuffers), (n, framebuffers));
SOFT_LINK(libGLESv2, glDeleteProgram, void, GL_APIENTRY, (GLuint program), (program));
SOFT_LINK(libGLESv2, glDeleteRenderbuffers, void, GL_APIENTRY, (GLsizei n, const GLuint* renderbuffers), (n, renderbuffers));
SOFT_LINK(libGLESv2, glDeleteShader, void, GL_APIENTRY, (GLuint shader), (shader));
SOFT_LINK(libGLESv2, glDeleteTextures, void, GL_APIENTRY, (GLsizei n, const GLuint* textures), (n, textures));
SOFT_LINK(libGLESv2, glDepthFunc, void, GL_APIENTRY, (GLenum func), (func));
SOFT_LINK(libGLESv2, glDepthMask, void, GL_APIENTRY, (GLboolean flag), (flag));
SOFT_LINK(libGLESv2, glDepthRangef, void, GL_APIENTRY, (GLclampf zNear, GLclampf zFar), (zNear, zFar));
SOFT_LINK(libGLESv2, glDetachShader, void, GL_APIENTRY, (GLuint program, GLuint shader), (program, shader));
SOFT_LINK(libGLESv2, glDisable, void, GL_APIENTRY, (GLenum cap), (cap));
SOFT_LINK(libGLESv2, glDisableVertexAttribArray, void, GL_APIENTRY, (GLuint index), (index));
SOFT_LINK(libGLESv2, glDrawArrays, void, GL_APIENTRY, (GLenum mode, GLint first, GLsizei count), (mode, first, count));
SOFT_LINK(libGLESv2, glDrawElements, void, GL_APIENTRY, (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices), (mode, count, type, indices));
SOFT_LINK(libGLESv2, glEnable, void, GL_APIENTRY, (GLenum cap), (cap));
SOFT_LINK(libGLESv2, glEnableVertexAttribArray, void, GL_APIENTRY, (GLuint index), (index));
SOFT_LINK(libGLESv2, glFinish, void, GL_APIENTRY, (void), ());
SOFT_LINK(libGLESv2, glFlush, void, GL_APIENTRY, (void), ());
SOFT_LINK(libGLESv2, glFramebufferRenderbuffer, void, GL_APIENTRY, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer), (target, attachment, renderbuffertarget, renderbuffer));
SOFT_LINK(libGLESv2, glFramebufferTexture2D, void, GL_APIENTRY, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level));
SOFT_LINK(libGLESv2, glFrontFace, void, GL_APIENTRY, (GLenum mode), (mode));
SOFT_LINK(libGLESv2, glGenBuffers, void, GL_APIENTRY, (GLsizei n, GLuint* buffers), (n, buffers));
SOFT_LINK(libGLESv2, glGenerateMipmap, void, GL_APIENTRY, (GLenum target), (target));
SOFT_LINK(libGLESv2, glGenFramebuffers, void, GL_APIENTRY, (GLsizei n, GLuint* framebuffers), (n, framebuffers));
SOFT_LINK(libGLESv2, glGenRenderbuffers, int, GL_APIENTRY, (GLsizei n, GLuint* renderbuffers), (n, renderbuffers));
SOFT_LINK(libGLESv2, glGenTextures, void, GL_APIENTRY, (GLsizei n, GLuint* textures), (n, textures));
SOFT_LINK(libGLESv2, glGetActiveAttrib, void, GL_APIENTRY, (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufsize, length, size, type, name));
SOFT_LINK(libGLESv2, glGetActiveUniform, GLenum, GL_APIENTRY, (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name), (program, index, bufsize, length, size, type, name));
SOFT_LINK(libGLESv2, glGetAttachedShaders, void, GL_APIENTRY, (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders), (program, maxcount, count, shaders));
SOFT_LINK(libGLESv2, glGetAttribLocation, int, GL_APIENTRY, (GLuint program, const GLchar* name), (program, name));
SOFT_LINK(libGLESv2, glGetBooleanv, void, GL_APIENTRY, (GLenum pname, GLboolean* params), (pname, params));
SOFT_LINK(libGLESv2, glGetBufferParameteriv, void, GL_APIENTRY, (GLenum target, GLenum pname, GLint* params), (target, pname, params));
SOFT_LINK(libGLESv2, glGetError, GLenum, GL_APIENTRY, (void), ());
SOFT_LINK(libGLESv2, glGetFloatv, void, GL_APIENTRY, (GLenum pname, GLfloat* params), (pname, params));
SOFT_LINK(libGLESv2, glGetFramebufferAttachmentParameteriv, void, GL_APIENTRY, (GLenum target, GLenum attachment, GLenum pname, GLint* params), (target, attachment, pname, params));
SOFT_LINK(libGLESv2, glGetIntegerv, void, GL_APIENTRY, (GLenum pname, GLint* params), (pname, params));
SOFT_LINK(libGLESv2, glGetProgramiv, void, GL_APIENTRY, (GLuint program, GLenum pname, GLint* params), (program, pname, params));
SOFT_LINK(libGLESv2, glGetProgramInfoLog, void, GL_APIENTRY, (GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog), (program, bufsize, length, infolog));
SOFT_LINK(libGLESv2, glGetRenderbufferParameteriv, void, GL_APIENTRY, (GLenum target, GLenum pname, GLint* params), (target, pname, params));
SOFT_LINK(libGLESv2, glGetShaderiv, void, GL_APIENTRY, (GLuint shader, GLenum pname, GLint* params), (shader, pname, params));
SOFT_LINK(libGLESv2, glGetShaderInfoLog, void, GL_APIENTRY, (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog), (shader, bufsize, length, infolog));
SOFT_LINK(libGLESv2, glGetShaderPrecisionFormat, void, GL_APIENTRY, (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision), (shadertype, precisiontype, range, precision));
SOFT_LINK(libGLESv2, glGetShaderSource, void, GL_APIENTRY, (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source), (shader, bufsize, length, source));
SOFT_LINK(libGLESv2, glGetString, const GLubyte*, GL_APIENTRY, (GLenum name), (name));
SOFT_LINK(libGLESv2, glGetTexParameterfv, void, GL_APIENTRY, (GLenum target, GLenum pname, GLfloat* params), (target, pname, params));
SOFT_LINK(libGLESv2, glGetTexParameteriv, void, GL_APIENTRY, (GLenum target, GLenum pname, GLint* params), (target, pname, params));
SOFT_LINK(libGLESv2, glGetUniformfv, void, GL_APIENTRY, (GLuint program, GLint location, GLfloat* params), (program, location, params));
SOFT_LINK(libGLESv2, glGetUniformiv, void, GL_APIENTRY, (GLuint program, GLint location, GLint* params), (program, location, params));
SOFT_LINK(libGLESv2, glGetUniformLocation, int, GL_APIENTRY, (GLuint program, const GLchar* name), (program, name));
SOFT_LINK(libGLESv2, glGetVertexAttribfv, void, GL_APIENTRY, (GLuint index, GLenum pname, GLfloat* params), (index, pname, params));
SOFT_LINK(libGLESv2, glGetVertexAttribiv, void, GL_APIENTRY, (GLenum target, GLenum pname, GLint* params), (target, pname, params));
SOFT_LINK(libGLESv2, glGetVertexAttribPointerv, void, GL_APIENTRY, (GLuint index, GLenum pname, GLvoid** pointer), (index, pname, pointer));
SOFT_LINK(libGLESv2, glHint, void, GL_APIENTRY, (GLenum target, GLenum mode), (target, mode));
SOFT_LINK(libGLESv2, glIsBuffer, GLboolean, GL_APIENTRY, (GLuint buffer), (buffer));
SOFT_LINK(libGLESv2, glIsEnabled, GLboolean, GL_APIENTRY, (GLenum cap), (cap));
SOFT_LINK(libGLESv2, glIsFramebuffer, GLboolean, GL_APIENTRY, (GLuint framebuffer), (framebuffer));
SOFT_LINK(libGLESv2, glIsProgram, GLboolean, GL_APIENTRY, (GLuint program), (program));
SOFT_LINK(libGLESv2, glIsRenderbuffer, GLboolean, GL_APIENTRY, (GLuint renderbuffer), (renderbuffer));
SOFT_LINK(libGLESv2, glIsShader, GLboolean, GL_APIENTRY, (GLuint shader), (shader));
SOFT_LINK(libGLESv2, glIsTexture, GLboolean, GL_APIENTRY, (GLuint texture), (texture));
SOFT_LINK(libGLESv2, glLineWidth, void, GL_APIENTRY, (GLfloat width), (width));
SOFT_LINK(libGLESv2, glLinkProgram, void, GL_APIENTRY, (GLuint program), (program));
SOFT_LINK(libGLESv2, glPixelStorei, void, GL_APIENTRY, (GLenum pname, GLint param), (pname, param));
SOFT_LINK(libGLESv2, glPolygonOffset, void, GL_APIENTRY, (GLfloat factor, GLfloat units), (factor, units));
SOFT_LINK(libGLESv2, glReadPixels, void, GL_APIENTRY, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels), (x, y, width, height, format, type, pixels));
SOFT_LINK(libGLESv2, glReleaseShaderCompiler, void, GL_APIENTRY, (void), ());
SOFT_LINK(libGLESv2, glRenderbufferStorage, void, GL_APIENTRY, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height), (target, internalformat, width, height));
SOFT_LINK(libGLESv2, glSampleCoverage, void, GL_APIENTRY, (GLclampf value, GLboolean invert), (value, invert));
SOFT_LINK(libGLESv2, glScissor, void, GL_APIENTRY, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height));
SOFT_LINK(libGLESv2, glShaderBinary, void, GL_APIENTRY, (GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length), (n, shaders, binaryformat, binary, length));
SOFT_LINK(libGLESv2, glShaderSource, void, GL_APIENTRY, (GLuint shader, GLsizei count, const GLchar** string, const GLint* length), (shader, count, string, length));
SOFT_LINK(libGLESv2, glStencilFunc, void, GL_APIENTRY, (GLenum func, GLint ref, GLuint mask), (func, ref, mask));
SOFT_LINK(libGLESv2, glStencilFuncSeparate, void, GL_APIENTRY, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask));
SOFT_LINK(libGLESv2, glStencilMask, void, GL_APIENTRY, (GLuint mask), (mask));
SOFT_LINK(libGLESv2, glStencilMaskSeparate, void, GL_APIENTRY, (GLenum face, GLuint mask), (face, mask));
SOFT_LINK(libGLESv2, glStencilOp, void, GL_APIENTRY, (GLenum fail, GLenum zfail, GLenum zpass), (fail, zfail, zpass));
SOFT_LINK(libGLESv2, glStencilOpSeparate, void, GL_APIENTRY, (GLenum face, GLenum fail, GLenum zfail, GLenum zpass), (face, fail, zfail, zpass));
SOFT_LINK(libGLESv2, glTexImage2D, void, GL_APIENTRY, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels), (target, level, internalformat, width, height, border, format, type, pixels));
SOFT_LINK(libGLESv2, glTexParameterf, void, GL_APIENTRY, (GLenum target, GLenum pname, GLfloat param), (target, pname, param));
SOFT_LINK(libGLESv2, glTexParameterfv, void, GL_APIENTRY, (GLenum target, GLenum pname, const GLfloat* params), (target, pname, params));
SOFT_LINK(libGLESv2, glTexParameteri, void, GL_APIENTRY, (GLenum target, GLenum pname, GLint param), (target, pname, param));
SOFT_LINK(libGLESv2, glTexParameteriv, void, GL_APIENTRY, (GLenum target, GLenum pname, const GLint* params), (target, pname, params));
SOFT_LINK(libGLESv2, glTexSubImage2D, void, GL_APIENTRY, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels), (target, level, xoffset, yoffset, width, height, format, type, pixels));
SOFT_LINK(libGLESv2, glUniform1f, void, GL_APIENTRY, (GLint location, GLfloat x), (location, x));
SOFT_LINK(libGLESv2, glUniform1fv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLfloat* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform1i, void, GL_APIENTRY, (GLint location, GLint x), (location, x));
SOFT_LINK(libGLESv2, glUniform1iv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLint* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform2f, void, GL_APIENTRY, (GLint location, GLfloat x, GLfloat y), (location, x, y));
SOFT_LINK(libGLESv2, glUniform2fv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLfloat* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform2i, void, GL_APIENTRY, (GLint location, GLint x, GLint y), (location, x, y));
SOFT_LINK(libGLESv2, glUniform2iv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLint* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform3f, void, GL_APIENTRY, (GLint location, GLfloat x, GLfloat y, GLfloat z), (location, x, y, z));
SOFT_LINK(libGLESv2, glUniform3fv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLfloat* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform3i, void, GL_APIENTRY, (GLint location, GLint x, GLint y, GLint z), (location, x, y, z));
SOFT_LINK(libGLESv2, glUniform3iv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLint* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform4f, void, GL_APIENTRY, (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (location, x, y, z, w));
SOFT_LINK(libGLESv2, glUniform4fv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLfloat* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniform4i, void, GL_APIENTRY, (GLint location, GLint x, GLint y, GLint z, GLint w), (location, x, y, z, w));
SOFT_LINK(libGLESv2, glUniform4iv, void, GL_APIENTRY, (GLint location, GLsizei count, const GLint* v), (location, count, v));
SOFT_LINK(libGLESv2, glUniformMatrix2fv, void, GL_APIENTRY, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value));
SOFT_LINK(libGLESv2, glUniformMatrix3fv, void, GL_APIENTRY, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value));
SOFT_LINK(libGLESv2, glUniformMatrix4fv, void, GL_APIENTRY, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value), (location, count, transpose, value));
SOFT_LINK(libGLESv2, glUseProgram, void, GL_APIENTRY, (GLuint program), (program));
SOFT_LINK(libGLESv2, glValidateProgram, void, GL_APIENTRY, (GLuint program), (program));
SOFT_LINK(libGLESv2, glVertexAttrib1f, void, GL_APIENTRY, (GLuint indx, GLfloat x), (indx, x));
SOFT_LINK(libGLESv2, glVertexAttrib1fv, void, GL_APIENTRY, (GLuint indx, const GLfloat* values), (indx, values));
SOFT_LINK(libGLESv2, glVertexAttrib2f, void, GL_APIENTRY, (GLuint indx, GLfloat x, GLfloat y), (indx, x, y));
SOFT_LINK(libGLESv2, glVertexAttrib2fv, void, GL_APIENTRY, (GLuint indx, const GLfloat* values), (indx, values));
SOFT_LINK(libGLESv2, glVertexAttrib3f, void, GL_APIENTRY, (GLuint indx, GLfloat x, GLfloat y, GLfloat z), (indx, x, y, z));
SOFT_LINK(libGLESv2, glVertexAttrib3fv, void, GL_APIENTRY, (GLuint indx, const GLfloat* values), (indx, values));
SOFT_LINK(libGLESv2, glVertexAttrib4f, void, GL_APIENTRY, (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (indx, x, y, z, w));
SOFT_LINK(libGLESv2, glVertexAttrib4fv, void, GL_APIENTRY, (GLuint indx, const GLfloat* values), (indx, values));
SOFT_LINK(libGLESv2, glVertexAttribPointer, void, GL_APIENTRY, (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr), (indx, size, type, normalized, stride, ptr));
SOFT_LINK(libGLESv2, glViewport, void, GL_APIENTRY, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height));
