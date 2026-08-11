#ifndef _SGLRCONTEXTWRAPPER_HPP
#define _SGLRCONTEXTWRAPPER_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Context wrapper that exposes sglr API as GL-compatible API.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVector.hpp"

namespace sglr
{

class Shader;
class Context;

class ContextWrapper
{
public:
    ContextWrapper(void);
    ~ContextWrapper(void);

    void setContext(Context *context);
    Context *getCurrentContext(void) const;

    int getWidth(void) const;
    int getHeight(void) const;

    // GL-compatible API.
    void glActiveTexture(uint32_t texture);
    void glAttachShader(uint32_t program, uint32_t shader);
    void glBindAttribLocation(uint32_t program, uint32_t index, const char *name);
    void glBindBuffer(uint32_t target, uint32_t buffer);
    void glBindFramebuffer(uint32_t target, uint32_t framebuffer);
    void glBindRenderbuffer(uint32_t target, uint32_t renderbuffer);
    void glBindTexture(uint32_t target, uint32_t texture);
    void glBlendColor(float red, float green, float blue, float alpha);
    void glBlendEquation(uint32_t mode);
    void glBlendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha);
    void glBlendFunc(uint32_t sfactor, uint32_t dfactor);
    void glBlendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha);
    void glBufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage);
    void glBufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data);
    uint32_t glCheckFramebufferStatus(uint32_t target);
    void glClear(uint32_t mask);
    void glClearColor(float red, float green, float blue, float alpha);
    void glClearDepthf(float depth);
    void glClearStencil(int s);
    void glColorMask(bool red, bool green, bool blue, bool alpha);
    void glCompileShader(uint32_t shader);
    void glCompressedTexImage2D(uint32_t target, int level, uint32_t internalformat, int width, int height, int border,
                                int imageSize, const void *data);
    void glCompressedTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                                   uint32_t format, int imageSize, const void *data);
    void glCopyTexImage1D(uint32_t target, int level, uint32_t internalformat, int x, int y, int width, int border);
    void glCopyTexImage2D(uint32_t target, int level, uint32_t internalformat, int x, int y, int width, int height,
                          int border);
    void glCopyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width);
    void glCopyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width, int height);
    uint32_t glCreateProgram();
    uint32_t glCreateShader(uint32_t type);
    void glCullFace(uint32_t mode);
    void glDeleteBuffers(int n, const uint32_t *buffers);
    void glDeleteFramebuffers(int n, const uint32_t *framebuffers);
    void glDeleteProgram(uint32_t program);
    void glDeleteRenderbuffers(int n, const uint32_t *renderbuffers);
    void glDeleteShader(uint32_t shader);
    void glDeleteTextures(int n, const uint32_t *textures);
    void glDepthFunc(uint32_t func);
    void glDepthMask(bool flag);
    void glDepthRangef(float n, float f);
    void glDetachShader(uint32_t program, uint32_t shader);
    void glDisable(uint32_t cap);
    void glDisableVertexAttribArray(uint32_t index);
    void glDrawArrays(uint32_t mode, int first, int count);
    void glDrawElements(uint32_t mode, int count, uint32_t type, const void *indices);
    void glEnable(uint32_t cap);
    void glEnableVertexAttribArray(uint32_t index);
    void glFinish();
    void glFlush();
    void glFramebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                   uint32_t renderbuffer);
    void glFramebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int level);
    void glFrontFace(uint32_t mode);
    void glGenBuffers(int n, uint32_t *buffers);
    void glGenerateMipmap(uint32_t target);
    void glGenFramebuffers(int n, uint32_t *framebuffers);
    void glGenRenderbuffers(int n, uint32_t *renderbuffers);
    void glGenTextures(int n, uint32_t *textures);
    void glGetActiveAttrib(uint32_t program, uint32_t index, int bufsize, int *length, int *size, uint32_t *type,
                           char *name);
    void glGetActiveUniform(uint32_t program, uint32_t index, int bufsize, int *length, int *size, uint32_t *type,
                            char *name);
    void glGetAttachedShaders(uint32_t program, int maxcount, int *count, uint32_t *shaders);
    int glGetAttribLocation(uint32_t program, const char *name);
    void glGetBooleanv(uint32_t pname, bool *params);
    void glGetBufferParameteriv(uint32_t target, uint32_t pname, int *params);
    uint32_t glGetError();
    void glGetFloatv(uint32_t pname, float *params);
    void glGetFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname, int *params);
    void glGetIntegerv(uint32_t pname, int *params);
    void glGetProgramiv(uint32_t program, uint32_t pname, int *params);
    void glGetProgramInfoLog(uint32_t program, int bufsize, int *length, char *infolog);
    void glGetRenderbufferParameteriv(uint32_t target, uint32_t pname, int *params);
    void glGetShaderiv(uint32_t shader, uint32_t pname, int *params);
    void glGetShaderInfoLog(uint32_t shader, int bufsize, int *length, char *infolog);
    void glGetShaderPrecisionFormat(uint32_t shadertype, uint32_t precisiontype, int *range, int *precision);
    void glGetShaderSource(uint32_t shader, int bufsize, int *length, char *source);
    const uint8_t *glGetString(uint32_t name);
    void glGetTexParameterfv(uint32_t target, uint32_t pname, float *params);
    void glGetTexParameteriv(uint32_t target, uint32_t pname, int *params);
    void glGetUniformfv(uint32_t program, int location, float *params);
    void glGetUniformiv(uint32_t program, int location, int *params);
    int glGetUniformLocation(uint32_t program, const char *name);
    void glGetVertexAttribfv(uint32_t index, uint32_t pname, float *params);
    void glGetVertexAttribiv(uint32_t index, uint32_t pname, int *params);
    void glGetVertexAttribPointerv(uint32_t index, uint32_t pname, void **pointer);
    void glHint(uint32_t target, uint32_t mode);
    bool glIsBuffer(uint32_t buffer);
    bool glIsEnabled(uint32_t cap);
    bool glIsFramebuffer(uint32_t framebuffer);
    bool glIsProgram(uint32_t program);
    bool glIsRenderbuffer(uint32_t renderbuffer);
    bool glIsShader(uint32_t shader);
    bool glIsTexture(uint32_t texture);
    void glLineWidth(float width);
    void glLinkProgram(uint32_t program);
    void glPixelStorei(uint32_t pname, int param);
    void glPolygonOffset(float factor, float units);
    void glReadPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *pixels);
    void glReleaseShaderCompiler();
    void glRenderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height);
    void glSampleCoverage(float value, bool invert);
    void glScissor(int x, int y, int width, int height);
    void glShaderBinary(int n, const uint32_t *shaders, uint32_t binaryformat, const void *binary, int length);
    void glShaderSource(uint32_t shader, int count, const char *const *string, const int *length);
    void glStencilFunc(uint32_t func, int ref, uint32_t mask);
    void glStencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask);
    void glStencilMask(uint32_t mask);
    void glStencilMaskSeparate(uint32_t face, uint32_t mask);
    void glStencilOp(uint32_t fail, uint32_t zfail, uint32_t zpass);
    void glStencilOpSeparate(uint32_t face, uint32_t fail, uint32_t zfail, uint32_t zpass);
    void glTexImage1D(uint32_t target, int level, int internalformat, int width, int border, uint32_t format,
                      uint32_t type, const void *pixels);
    void glTexImage2D(uint32_t target, int level, int internalformat, int width, int height, int border,
                      uint32_t format, uint32_t type, const void *pixels);
    void glTexParameterf(uint32_t target, uint32_t pname, float param);
    void glTexParameterfv(uint32_t target, uint32_t pname, const float *params);
    void glTexParameteri(uint32_t target, uint32_t pname, int param);
    void glTexParameteriv(uint32_t target, uint32_t pname, const int *params);
    void glTexSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                         const void *pixels);
    void glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height, uint32_t format,
                         uint32_t type, const void *pixels);
    void glUniform1f(int location, float x);
    void glUniform1fv(int location, int count, const float *v);
    void glUniform1i(int location, int x);
    void glUniform1iv(int location, int count, const int *v);
    void glUniform2f(int location, float x, float y);
    void glUniform2fv(int location, int count, const float *v);
    void glUniform2i(int location, int x, int y);
    void glUniform2iv(int location, int count, const int *v);
    void glUniform3f(int location, float x, float y, float z);
    void glUniform3fv(int location, int count, const float *v);
    void glUniform3i(int location, int x, int y, int z);
    void glUniform3iv(int location, int count, const int *v);
    void glUniform4f(int location, float x, float y, float z, float w);
    void glUniform4fv(int location, int count, const float *v);
    void glUniform4i(int location, int x, int y, int z, int w);
    void glUniform4iv(int location, int count, const int *v);
    void glUniformMatrix2fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix3fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix4fv(int location, int count, bool transpose, const float *value);
    void glUseProgram(uint32_t program);
    void glValidateProgram(uint32_t program);
    void glVertexAttrib1f(uint32_t indx, float x);
    void glVertexAttrib1fv(uint32_t indx, const float *values);
    void glVertexAttrib2f(uint32_t indx, float x, float y);
    void glVertexAttrib2fv(uint32_t indx, const float *values);
    void glVertexAttrib3f(uint32_t indx, float x, float y, float z);
    void glVertexAttrib3fv(uint32_t indx, const float *values);
    void glVertexAttrib4f(uint32_t indx, float x, float y, float z, float w);
    void glVertexAttrib4fv(uint32_t indx, const float *values);
    void glVertexAttribPointer(uint32_t indx, int size, uint32_t type, bool normalized, int stride, const void *ptr);
    void glViewport(int x, int y, int width, int height);
    void glReadBuffer(uint32_t mode);
    void glDrawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                             const void *indices);
    void glTexImage3D(uint32_t target, int level, int internalformat, int width, int height, int depth, int border,
                      uint32_t format, uint32_t type, const void *pixels);
    void glTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width, int height,
                         int depth, uint32_t format, uint32_t type, const void *pixels);
    void glCopyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x, int y, int width,
                             int height);
    void glCompressedTexImage3D(uint32_t target, int level, uint32_t internalformat, int width, int height, int depth,
                                int border, int imageSize, const void *data);
    void glCompressedTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width,
                                   int height, int depth, uint32_t format, int imageSize, const void *data);
    void glGenQueries(int n, uint32_t *ids);
    void glDeleteQueries(int n, const uint32_t *ids);
    bool glIsQuery(uint32_t id);
    void glBeginQuery(uint32_t target, uint32_t id);
    void glEndQuery(uint32_t target);
    void glGetQueryiv(uint32_t target, uint32_t pname, int *params);
    void glGetQueryObjectuiv(uint32_t id, uint32_t pname, uint32_t *params);
    bool glUnmapBuffer(uint32_t target);
    void glGetBufferPointerv(uint32_t target, uint32_t pname, void **params);
    void glDrawBuffers(int n, const uint32_t *bufs);
    void glUniformMatrix2x3fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix3x2fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix2x4fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix4x2fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix3x4fv(int location, int count, bool transpose, const float *value);
    void glUniformMatrix4x3fv(int location, int count, bool transpose, const float *value);
    void glBlitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1,
                           uint32_t mask, uint32_t filter);
    void glRenderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalformat, int width, int height);
    void glFramebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level, int layer);
    void *glMapBufferRange(uint32_t target, intptr_t offset, intptr_t length, uint32_t access);
    void glFlushMappedBufferRange(uint32_t target, intptr_t offset, intptr_t length);
    void glBindVertexArray(uint32_t array);
    void glDeleteVertexArrays(int n, const uint32_t *arrays);
    void glGenVertexArrays(int n, uint32_t *arrays);
    bool glIsVertexArray(uint32_t array);
    void glGetIntegeri_v(uint32_t target, uint32_t index, int *data);
    void glBeginTransformFeedback(uint32_t primitiveMode);
    void glEndTransformFeedback();
    void glBindBufferRange(uint32_t target, uint32_t index, uint32_t buffer, intptr_t offset, intptr_t size);
    void glBindBufferBase(uint32_t target, uint32_t index, uint32_t buffer);
    void glTransformFeedbackVaryings(uint32_t program, int count, const char *const *varyings, uint32_t bufferMode);
    void glGetTransformFeedbackVarying(uint32_t program, uint32_t index, int bufSize, int *length, int *size,
                                       uint32_t *type, char *name);
    void glVertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride, const void *pointer);
    void glGetVertexAttribIiv(uint32_t index, uint32_t pname, int *params);
    void glGetVertexAttribIuiv(uint32_t index, uint32_t pname, uint32_t *params);
    void glVertexAttribI4i(uint32_t index, int x, int y, int z, int w);
    void glVertexAttribI4ui(uint32_t index, uint32_t x, uint32_t y, uint32_t z, uint32_t w);
    void glVertexAttribI4iv(uint32_t index, const int *v);
    void glVertexAttribI4uiv(uint32_t index, const uint32_t *v);
    void glGetUniformuiv(uint32_t program, int location, uint32_t *params);
    int glGetFragDataLocation(uint32_t program, const char *name);
    void glUniform1ui(int location, uint32_t v0);
    void glUniform2ui(int location, uint32_t v0, uint32_t v1);
    void glUniform3ui(int location, uint32_t v0, uint32_t v1, uint32_t v2);
    void glUniform4ui(int location, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3);
    void glUniform1uiv(int location, int count, const uint32_t *value);
    void glUniform2uiv(int location, int count, const uint32_t *value);
    void glUniform3uiv(int location, int count, const uint32_t *value);
    void glUniform4uiv(int location, int count, const uint32_t *value);
    void glClearBufferiv(uint32_t buffer, int drawbuffer, const int *value);
    void glClearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value);
    void glClearBufferfv(uint32_t buffer, int drawbuffer, const float *value);
    void glClearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil);
    const uint8_t *glGetStringi(uint32_t name, uint32_t index);
    void glCopyBufferSubData(uint32_t readTarget, uint32_t writeTarget, intptr_t readOffset, intptr_t writeOffset,
                             intptr_t size);
    void glGetUniformIndices(uint32_t program, int uniformCount, const char *const *uniformNames,
                             uint32_t *uniformIndices);
    void glGetActiveUniformsiv(uint32_t program, int uniformCount, const uint32_t *uniformIndices, uint32_t pname,
                               int *params);
    uint32_t glGetUniformBlockIndex(uint32_t program, const char *uniformBlockName);
    void glGetActiveUniformBlockiv(uint32_t program, uint32_t uniformBlockIndex, uint32_t pname, int *params);
    void glGetActiveUniformBlockName(uint32_t program, uint32_t uniformBlockIndex, int bufSize, int *length,
                                     char *uniformBlockName);
    void glUniformBlockBinding(uint32_t program, uint32_t uniformBlockIndex, uint32_t uniformBlockBinding);
    void glDrawArraysInstanced(uint32_t mode, int first, int count, int primcount);
    void glDrawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices, int primcount);
    void *glFenceSync(uint32_t condition, uint32_t flags);
    bool glIsSync(void *sync);
    void glDeleteSync(void *sync);
    uint32_t glClientWaitSync(void *sync, uint32_t flags, uint64_t timeout);
    void glWaitSync(void *sync, uint32_t flags, uint64_t timeout);
    void glGetInteger64v(uint32_t pname, int64_t *params);
    void glGetSynciv(void *sync, uint32_t pname, int bufSize, int *length, int *values);
    void glGetInteger64i_v(uint32_t target, uint32_t index, int64_t *data);
    void glGetBufferParameteri64v(uint32_t target, uint32_t pname, int64_t *params);
    void glGenSamplers(int count, uint32_t *samplers);
    void glDeleteSamplers(int count, const uint32_t *samplers);
    bool glIsSampler(uint32_t sampler);
    void glBindSampler(uint32_t unit, uint32_t sampler);
    void glSamplerParameteri(uint32_t sampler, uint32_t pname, int param);
    void glSamplerParameteriv(uint32_t sampler, uint32_t pname, const int *param);
    void glSamplerParameterf(uint32_t sampler, uint32_t pname, float param);
    void glSamplerParameterfv(uint32_t sampler, uint32_t pname, const float *param);
    void glGetSamplerParameteriv(uint32_t sampler, uint32_t pname, int *params);
    void glGetSamplerParameterfv(uint32_t sampler, uint32_t pname, float *params);
    void glVertexAttribDivisor(uint32_t index, uint32_t divisor);
    void glBindTransformFeedback(uint32_t target, uint32_t id);
    void glDeleteTransformFeedbacks(int n, const uint32_t *ids);
    void glGenTransformFeedbacks(int n, uint32_t *ids);
    bool glIsTransformFeedback(uint32_t id);
    void glPauseTransformFeedback();
    void glResumeTransformFeedback();
    void glGetProgramBinary(uint32_t program, int bufSize, int *length, uint32_t *binaryFormat, void *binary);
    void glProgramBinary(uint32_t program, uint32_t binaryFormat, const void *binary, int length);
    void glProgramParameteri(uint32_t program, uint32_t pname, int value);
    void glInvalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments);
    void glInvalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x, int y,
                                    int width, int height);
    void glTexStorage2D(uint32_t target, int levels, uint32_t internalformat, int width, int height);
    void glTexStorage3D(uint32_t target, int levels, uint32_t internalformat, int width, int height, int depth);
    void glGetInternalformativ(uint32_t target, uint32_t internalformat, uint32_t pname, int bufSize, int *params);

private:
    Context *m_curCtx;
};

} // namespace sglr

#endif // _SGLRCONTEXTWRAPPER_HPP
