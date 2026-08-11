#ifndef _SGLRGLCONTEXT_HPP
#define _SGLRGLCONTEXT_HPP
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
 * \brief GL Rendering Context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "sglrContext.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"

#include <set>
#include <vector>

namespace glu
{
class CallLogWrapper;
}

namespace sglr
{

enum GLContextLogFlag
{
    GLCONTEXT_LOG_CALLS    = (1 << 0),
    GLCONTEXT_LOG_PROGRAMS = (1 << 1)
};

class GLContext : public Context
{
public:
    GLContext(const glu::RenderContext &context, tcu::TestLog &log, uint32_t logFlags, const tcu::IVec4 &baseViewport);
    virtual ~GLContext(void);

    void enableLogging(uint32_t logFlags);

    virtual int getWidth(void) const;
    virtual int getHeight(void) const;

    virtual void viewport(int x, int y, int width, int height);
    virtual void activeTexture(uint32_t texture);

    virtual void bindTexture(uint32_t target, uint32_t texture);
    virtual void genTextures(int numTextures, uint32_t *textures);
    virtual void deleteTextures(int numTextures, const uint32_t *textures);

    virtual void bindFramebuffer(uint32_t target, uint32_t framebuffer);
    virtual void genFramebuffers(int numFramebuffers, uint32_t *framebuffers);
    virtual void deleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers);

    virtual void bindRenderbuffer(uint32_t target, uint32_t renderbuffer);
    virtual void genRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers);
    virtual void deleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers);

    virtual void pixelStorei(uint32_t pname, int param);
    virtual void texImage1D(uint32_t target, int level, uint32_t internalFormat, int width, int border, uint32_t format,
                            uint32_t type, const void *data);
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int border,
                            uint32_t format, uint32_t type, const void *data);
    virtual void texImage3D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int depth,
                            int border, uint32_t format, uint32_t type, const void *data);
    virtual void texSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                               const void *data);
    virtual void texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                               uint32_t format, uint32_t type, const void *data);
    virtual void texSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width, int height,
                               int depth, uint32_t format, uint32_t type, const void *data);
    virtual void copyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int border);
    virtual void copyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int height, int border);
    virtual void copyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width);
    virtual void copyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                   int height);
    virtual void copyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x, int y,
                                   int width, int height);

    virtual void texStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height);
    virtual void texStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height, int depth);

    virtual void texParameteri(uint32_t target, uint32_t pname, int value);

    virtual void framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                      int level);
    virtual void framebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level, int layer);
    virtual void framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                         uint32_t renderbuffer);
    virtual uint32_t checkFramebufferStatus(uint32_t target);

    virtual void getFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname, int *params);

    virtual void renderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height);
    virtual void renderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalFormat, int width,
                                                int height);

    virtual void bindBuffer(uint32_t target, uint32_t buffer);
    virtual void genBuffers(int numBuffers, uint32_t *buffers);
    virtual void deleteBuffers(int numBuffers, const uint32_t *buffers);

    virtual void bufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage);
    virtual void bufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data);

    virtual void clearColor(float red, float green, float blue, float alpha);
    virtual void clearDepthf(float depth);
    virtual void clearStencil(int stencil);

    virtual void clear(uint32_t buffers);
    virtual void clearBufferiv(uint32_t buffer, int drawbuffer, const int *value);
    virtual void clearBufferfv(uint32_t buffer, int drawbuffer, const float *value);
    virtual void clearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value);
    virtual void clearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil);
    virtual void scissor(int x, int y, int width, int height);

    virtual void enable(uint32_t cap);
    virtual void disable(uint32_t cap);

    virtual void stencilFunc(uint32_t func, int ref, uint32_t mask);
    virtual void stencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass);
    virtual void stencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask);
    virtual void stencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass);

    virtual void depthFunc(uint32_t func);
    virtual void depthRangef(float n, float f);
    virtual void depthRange(double n, double f);

    virtual void polygonOffset(float factor, float units);
    virtual void provokingVertex(uint32_t convention);
    virtual void primitiveRestartIndex(uint32_t index);

    virtual void blendEquation(uint32_t mode);
    virtual void blendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha);
    virtual void blendFunc(uint32_t src, uint32_t dst);
    virtual void blendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha);
    virtual void blendColor(float red, float green, float blue, float alpha);

    virtual void colorMask(bool r, bool g, bool b, bool a);
    virtual void depthMask(bool mask);
    virtual void stencilMask(uint32_t mask);
    virtual void stencilMaskSeparate(uint32_t face, uint32_t mask);

    virtual void blitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1,
                                 uint32_t mask, uint32_t filter);

    virtual void invalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x,
                                          int y, int width, int height);
    virtual void invalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments);

    virtual void bindVertexArray(uint32_t array);
    virtual void genVertexArrays(int numArrays, uint32_t *vertexArrays);
    virtual void deleteVertexArrays(int numArrays, const uint32_t *vertexArrays);

    virtual void vertexAttribPointer(uint32_t index, int size, uint32_t type, bool normalized, int stride,
                                     const void *pointer);
    virtual void vertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride, const void *pointer);
    virtual void enableVertexAttribArray(uint32_t index);
    virtual void disableVertexAttribArray(uint32_t index);
    virtual void vertexAttribDivisor(uint32_t index, uint32_t divisor);

    virtual void vertexAttrib1f(uint32_t index, float);
    virtual void vertexAttrib2f(uint32_t index, float, float);
    virtual void vertexAttrib3f(uint32_t index, float, float, float);
    virtual void vertexAttrib4f(uint32_t index, float, float, float, float);
    virtual void vertexAttribI4i(uint32_t index, int32_t, int32_t, int32_t, int32_t);
    virtual void vertexAttribI4ui(uint32_t index, uint32_t, uint32_t, uint32_t, uint32_t);

    virtual int32_t getAttribLocation(uint32_t program, const char *name);

    virtual void uniform1f(int32_t location, float);
    virtual void uniform1i(int32_t location, int32_t);
    virtual void uniform1fv(int32_t index, int32_t count, const float *);
    virtual void uniform2fv(int32_t index, int32_t count, const float *);
    virtual void uniform3fv(int32_t index, int32_t count, const float *);
    virtual void uniform4fv(int32_t index, int32_t count, const float *);
    virtual void uniform1iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniform2iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniform3iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniform4iv(int32_t index, int32_t count, const int32_t *);
    virtual void uniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value);
    virtual void uniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value);
    virtual int32_t getUniformLocation(uint32_t program, const char *name);

    virtual void lineWidth(float);

    virtual void drawArrays(uint32_t mode, int first, int count);
    virtual void drawArraysInstanced(uint32_t mode, int first, int count, int instanceCount);
    virtual void drawElements(uint32_t mode, int count, uint32_t type, const void *indices);
    virtual void drawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices, int instanceCount);
    virtual void drawElementsBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices, int baseVertex);
    virtual void drawElementsInstancedBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                                 int instanceCount, int baseVertex);
    virtual void drawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                   const void *indices);
    virtual void drawRangeElementsBaseVertex(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                             const void *indices, int baseVertex);
    virtual void drawArraysIndirect(uint32_t mode, const void *indirect);
    virtual void drawElementsIndirect(uint32_t mode, uint32_t type, const void *indirect);

    virtual void multiDrawArrays(uint32_t mode, const int *first, const int *count, int primCount);
    virtual void multiDrawElements(uint32_t mode, const int *count, uint32_t type, const void **indices, int primCount);
    virtual void multiDrawElementsBaseVertex(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                             int primCount, const int *baseVertex);

    virtual uint32_t createProgram(ShaderProgram *);
    virtual void deleteProgram(uint32_t program);
    virtual void useProgram(uint32_t program);

    virtual void readPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data);
    virtual uint32_t getError(void);
    virtual void finish(void);

    virtual void getIntegerv(uint32_t pname, int *params);
    virtual const char *getString(uint32_t pname);

    // Expose helpers from Context.
    using Context::readPixels;
    using Context::texImage2D;
    using Context::texSubImage2D;

private:
    GLContext(const GLContext &other);
    GLContext &operator=(const GLContext &other);

    tcu::IVec2 getReadOffset(void) const;
    tcu::IVec2 getDrawOffset(void) const;

    const glu::RenderContext &m_context;
    tcu::TestLog &m_log;

    uint32_t m_logFlags;
    tcu::IVec4 m_baseViewport;
    tcu::IVec4 m_curViewport;
    tcu::IVec4 m_curScissor;
    uint32_t m_readFramebufferBinding;
    uint32_t m_drawFramebufferBinding;

    glu::CallLogWrapper *m_wrapper;

    // For cleanup
    std::set<uint32_t> m_allocatedTextures;
    std::set<uint32_t> m_allocatedFbos;
    std::set<uint32_t> m_allocatedRbos;
    std::set<uint32_t> m_allocatedBuffers;
    std::set<uint32_t> m_allocatedVaos;
    std::vector<glu::ShaderProgram *> m_programs;
} DE_WARN_UNUSED_TYPE;

} // namespace sglr

#endif // _SGLRGLCONTEXT_HPP
