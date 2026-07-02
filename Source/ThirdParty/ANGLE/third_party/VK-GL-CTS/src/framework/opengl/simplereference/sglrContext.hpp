#ifndef _SGLRCONTEXT_HPP
#define _SGLRCONTEXT_HPP
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
 * \brief Simplified GLES reference context.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuSurface.hpp"
#include "gluRenderContext.hpp"
#include "sglrShaderProgram.hpp"

/*--------------------------------------------------------------------*//*!
 * \brief Reference OpenGL API implementation
 *//*--------------------------------------------------------------------*/
namespace sglr
{

// Abstract drawing context with GL-style API

class Context
{
public:
    Context(glu::ContextType type) : m_type(type)
    {
    }
    virtual ~Context(void)
    {
    }

    virtual int getWidth(void) const  = 0;
    virtual int getHeight(void) const = 0;

    virtual void activeTexture(uint32_t texture)               = 0;
    virtual void viewport(int x, int y, int width, int height) = 0;

    virtual void bindTexture(uint32_t target, uint32_t texture)            = 0;
    virtual void genTextures(int numTextures, uint32_t *textures)          = 0;
    virtual void deleteTextures(int numTextures, const uint32_t *textures) = 0;

    virtual void bindFramebuffer(uint32_t target, uint32_t framebuffer)                = 0;
    virtual void genFramebuffers(int numFramebuffers, uint32_t *framebuffers)          = 0;
    virtual void deleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers) = 0;

    virtual void bindRenderbuffer(uint32_t target, uint32_t renderbuffer)                 = 0;
    virtual void genRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers)          = 0;
    virtual void deleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers) = 0;

    virtual void pixelStorei(uint32_t pname, int param)                                              = 0;
    virtual void texImage1D(uint32_t target, int level, uint32_t internalFormat, int width, int border, uint32_t format,
                            uint32_t type, const void *data)                                         = 0;
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int border,
                            uint32_t format, uint32_t type, const void *data)                        = 0;
    virtual void texImage3D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int depth,
                            int border, uint32_t format, uint32_t type, const void *data)            = 0;
    virtual void texSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                               const void *data)                                                     = 0;
    virtual void texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                               uint32_t format, uint32_t type, const void *data)                     = 0;
    virtual void texSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width, int height,
                               int depth, uint32_t format, uint32_t type, const void *data)          = 0;
    virtual void copyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int border)                                                          = 0;
    virtual void copyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                int height, int border)                                              = 0;
    virtual void copyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width) = 0;
    virtual void copyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                   int height)                                                       = 0;
    virtual void copyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x, int y,
                                   int width, int height)                                            = 0;

    virtual void texStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height) = 0;
    virtual void texStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height,
                              int depth)                                                                   = 0;

    virtual void texParameteri(uint32_t target, uint32_t pname, int value) = 0;

    virtual void framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                      int level)                = 0;
    virtual void framebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level,
                                         int layer)             = 0;
    virtual void framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                         uint32_t renderbuffer) = 0;
    virtual uint32_t checkFramebufferStatus(uint32_t target)    = 0;

    virtual void getFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname,
                                                     int *params) = 0;

    virtual void renderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height) = 0;
    virtual void renderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalFormat, int width,
                                                int height)                                           = 0;

    virtual void bindBuffer(uint32_t target, uint32_t buffer)           = 0;
    virtual void genBuffers(int numBuffers, uint32_t *buffers)          = 0;
    virtual void deleteBuffers(int numBuffers, const uint32_t *buffers) = 0;

    virtual void bufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage)     = 0;
    virtual void bufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data) = 0;

    virtual void clearColor(float red, float green, float blue, float alpha) = 0;
    virtual void clearDepthf(float depth)                                    = 0;
    virtual void clearStencil(int stencil)                                   = 0;

    virtual void clear(uint32_t buffers)                                                  = 0;
    virtual void clearBufferiv(uint32_t buffer, int drawbuffer, const int *value)         = 0;
    virtual void clearBufferfv(uint32_t buffer, int drawbuffer, const float *value)       = 0;
    virtual void clearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value)   = 0;
    virtual void clearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil) = 0;
    virtual void scissor(int x, int y, int width, int height)                             = 0;

    virtual void enable(uint32_t cap)  = 0;
    virtual void disable(uint32_t cap) = 0;

    virtual void stencilFunc(uint32_t func, int ref, uint32_t mask)                                 = 0;
    virtual void stencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass)                        = 0;
    virtual void stencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask)          = 0;
    virtual void stencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass) = 0;

    virtual void depthFunc(uint32_t func)       = 0;
    virtual void depthRangef(float n, float f)  = 0;
    virtual void depthRange(double n, double f) = 0;

    virtual void polygonOffset(float factor, float units) = 0;
    virtual void provokingVertex(uint32_t convention)     = 0;
    virtual void primitiveRestartIndex(uint32_t index)    = 0;

    virtual void blendEquation(uint32_t mode)                                                              = 0;
    virtual void blendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha)                               = 0;
    virtual void blendFunc(uint32_t src, uint32_t dst)                                                     = 0;
    virtual void blendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha) = 0;
    virtual void blendColor(float red, float green, float blue, float alpha)                               = 0;

    virtual void colorMask(bool r, bool g, bool b, bool a)         = 0;
    virtual void depthMask(bool mask)                              = 0;
    virtual void stencilMask(uint32_t mask)                        = 0;
    virtual void stencilMaskSeparate(uint32_t face, uint32_t mask) = 0;

    virtual void blitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1,
                                 uint32_t mask, uint32_t filter) = 0;

    virtual void invalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x,
                                          int y, int width, int height)                                  = 0;
    virtual void invalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments) = 0;

    virtual void bindVertexArray(uint32_t array)                                 = 0;
    virtual void genVertexArrays(int numArrays, uint32_t *vertexArrays)          = 0;
    virtual void deleteVertexArrays(int numArrays, const uint32_t *vertexArrays) = 0;

    virtual void vertexAttribPointer(uint32_t index, int size, uint32_t type, bool normalized, int stride,
                                     const void *pointer)                                                       = 0;
    virtual void vertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride, const void *pointer) = 0;
    virtual void enableVertexAttribArray(uint32_t index)                                                        = 0;
    virtual void disableVertexAttribArray(uint32_t index)                                                       = 0;
    virtual void vertexAttribDivisor(uint32_t index, uint32_t divisor)                                          = 0;

    virtual void vertexAttrib1f(uint32_t index, float)                                    = 0;
    virtual void vertexAttrib2f(uint32_t index, float, float)                             = 0;
    virtual void vertexAttrib3f(uint32_t index, float, float, float)                      = 0;
    virtual void vertexAttrib4f(uint32_t index, float, float, float, float)               = 0;
    virtual void vertexAttribI4i(uint32_t index, int32_t, int32_t, int32_t, int32_t)      = 0;
    virtual void vertexAttribI4ui(uint32_t index, uint32_t, uint32_t, uint32_t, uint32_t) = 0;

    virtual int32_t getAttribLocation(uint32_t program, const char *name) = 0;

    virtual void uniform1f(int32_t index, float)                                                       = 0;
    virtual void uniform1i(int32_t index, int32_t)                                                     = 0;
    virtual void uniform1fv(int32_t index, int32_t count, const float *)                               = 0;
    virtual void uniform2fv(int32_t index, int32_t count, const float *)                               = 0;
    virtual void uniform3fv(int32_t index, int32_t count, const float *)                               = 0;
    virtual void uniform4fv(int32_t index, int32_t count, const float *)                               = 0;
    virtual void uniform1iv(int32_t index, int32_t count, const int32_t *)                             = 0;
    virtual void uniform2iv(int32_t index, int32_t count, const int32_t *)                             = 0;
    virtual void uniform3iv(int32_t index, int32_t count, const int32_t *)                             = 0;
    virtual void uniform4iv(int32_t index, int32_t count, const int32_t *)                             = 0;
    virtual void uniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value) = 0;
    virtual void uniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value) = 0;
    virtual int32_t getUniformLocation(uint32_t program, const char *name)                             = 0;

    virtual void lineWidth(float) = 0;

    virtual void drawArrays(uint32_t mode, int first, int count)                             = 0;
    virtual void drawArraysInstanced(uint32_t mode, int first, int count, int instanceCount) = 0;
    virtual void drawElements(uint32_t mode, int count, uint32_t type, const void *indices)  = 0;
    virtual void drawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices,
                                       int instanceCount)                                    = 0;
    virtual void drawElementsBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                        int baseVertex)                                      = 0;
    virtual void drawElementsInstancedBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                                 int instanceCount, int baseVertex)          = 0;
    virtual void drawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                   const void *indices)                                      = 0;
    virtual void drawRangeElementsBaseVertex(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                             const void *indices, int baseVertex)            = 0;
    virtual void drawArraysIndirect(uint32_t mode, const void *indirect)                     = 0;
    virtual void drawElementsIndirect(uint32_t mode, uint32_t type, const void *indirect)    = 0;

    virtual void multiDrawArrays(uint32_t mode, const int *first, const int *count, int primCount) = 0;
    virtual void multiDrawElements(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                   int primCount)                                                  = 0;
    virtual void multiDrawElementsBaseVertex(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                             int primCount, const int *baseVertex)                 = 0;

    virtual uint32_t createProgram(ShaderProgram *program) = 0;
    virtual void useProgram(uint32_t program)              = 0;
    virtual void deleteProgram(uint32_t program)           = 0;

    virtual void readPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data) = 0;
    virtual uint32_t getError(void)                                                                          = 0;
    virtual void finish(void)                                                                                = 0;

    virtual void getIntegerv(uint32_t pname, int *params) = 0;
    virtual const char *getString(uint32_t pname)         = 0;

    // Helpers implemented by Context.
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, const tcu::Surface &src);
    virtual void texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height);
    virtual void texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, const tcu::Surface &src);
    virtual void readPixels(tcu::Surface &dst, int x, int y, int width, int height);

    glu::ContextType getType(void)
    {
        return m_type;
    }

private:
    const glu::ContextType m_type;
} DE_WARN_UNUSED_TYPE;

} // namespace sglr

#endif // _SGLRCONTEXT_HPP
