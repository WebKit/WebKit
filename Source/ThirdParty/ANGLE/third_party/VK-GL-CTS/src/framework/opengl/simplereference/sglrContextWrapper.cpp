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

#include "sglrContextWrapper.hpp"
#include "sglrContext.hpp"

namespace sglr
{

ContextWrapper::ContextWrapper(void) : m_curCtx(nullptr)
{
}

ContextWrapper::~ContextWrapper(void)
{
}

void ContextWrapper::setContext(Context *context)
{
    m_curCtx = context;
}

Context *ContextWrapper::getCurrentContext(void) const
{
    return m_curCtx;
}

int ContextWrapper::getWidth(void) const
{
    return m_curCtx->getWidth();
}

int ContextWrapper::getHeight(void) const
{
    return m_curCtx->getHeight();
}

void ContextWrapper::glViewport(int x, int y, int width, int height)
{
    m_curCtx->viewport(x, y, width, height);
}

void ContextWrapper::glActiveTexture(uint32_t texture)
{
    m_curCtx->activeTexture(texture);
}

void ContextWrapper::glBindTexture(uint32_t target, uint32_t texture)
{
    m_curCtx->bindTexture(target, texture);
}

void ContextWrapper::glGenTextures(int numTextures, uint32_t *textures)
{
    m_curCtx->genTextures(numTextures, textures);
}

void ContextWrapper::glDeleteTextures(int numTextures, const uint32_t *textures)
{
    m_curCtx->deleteTextures(numTextures, textures);
}

void ContextWrapper::glBindFramebuffer(uint32_t target, uint32_t framebuffer)
{
    m_curCtx->bindFramebuffer(target, framebuffer);
}

void ContextWrapper::glGenFramebuffers(int numFramebuffers, uint32_t *framebuffers)
{
    m_curCtx->genFramebuffers(numFramebuffers, framebuffers);
}

void ContextWrapper::glDeleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers)
{
    m_curCtx->deleteFramebuffers(numFramebuffers, framebuffers);
}

void ContextWrapper::glBindRenderbuffer(uint32_t target, uint32_t renderbuffer)
{
    m_curCtx->bindRenderbuffer(target, renderbuffer);
}

void ContextWrapper::glGenRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers)
{
    m_curCtx->genRenderbuffers(numRenderbuffers, renderbuffers);
}

void ContextWrapper::glDeleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers)
{
    m_curCtx->deleteRenderbuffers(numRenderbuffers, renderbuffers);
}

void ContextWrapper::glPixelStorei(uint32_t pname, int param)
{
    m_curCtx->pixelStorei(pname, param);
}

void ContextWrapper::glTexImage1D(uint32_t target, int level, int internalFormat, int width, int border,
                                  uint32_t format, uint32_t type, const void *data)
{
    m_curCtx->texImage1D(target, level, (uint32_t)internalFormat, width, border, format, type, data);
}

void ContextWrapper::glTexImage2D(uint32_t target, int level, int internalFormat, int width, int height, int border,
                                  uint32_t format, uint32_t type, const void *data)
{
    m_curCtx->texImage2D(target, level, (uint32_t)internalFormat, width, height, border, format, type, data);
}

void ContextWrapper::glTexImage3D(uint32_t target, int level, int internalFormat, int width, int height, int depth,
                                  int border, uint32_t format, uint32_t type, const void *data)
{
    m_curCtx->texImage3D(target, level, (uint32_t)internalFormat, width, height, depth, border, format, type, data);
}

void ContextWrapper::glTexSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                                     const void *data)
{
    m_curCtx->texSubImage1D(target, level, xoffset, width, format, type, data);
}

void ContextWrapper::glTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                                     uint32_t format, uint32_t type, const void *data)
{
    m_curCtx->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data);
}

void ContextWrapper::glTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width,
                                     int height, int depth, uint32_t format, uint32_t type, const void *data)
{
    m_curCtx->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
}

void ContextWrapper::glCopyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                      int border)
{
    m_curCtx->copyTexImage1D(target, level, internalFormat, x, y, width, border);
}

void ContextWrapper::glCopyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width,
                                      int height, int border)
{
    m_curCtx->copyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

void ContextWrapper::glCopyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width)
{
    m_curCtx->copyTexSubImage1D(target, level, xoffset, x, y, width);
}

void ContextWrapper::glCopyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                         int height)
{
    m_curCtx->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void ContextWrapper::glTexStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height)
{
    m_curCtx->texStorage2D(target, levels, internalFormat, width, height);
}

void ContextWrapper::glTexStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height,
                                    int depth)
{
    m_curCtx->texStorage3D(target, levels, internalFormat, width, height, depth);
}

void ContextWrapper::glTexParameteri(uint32_t target, uint32_t pname, int value)
{
    m_curCtx->texParameteri(target, pname, value);
}

void ContextWrapper::glUseProgram(uint32_t program)
{
    m_curCtx->useProgram(program);
}

void ContextWrapper::glFramebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                            int level)
{
    m_curCtx->framebufferTexture2D(target, attachment, textarget, texture, level);
}

void ContextWrapper::glFramebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level,
                                               int layer)
{
    m_curCtx->framebufferTextureLayer(target, attachment, texture, level, layer);
}

void ContextWrapper::glFramebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                               uint32_t renderbuffer)
{
    m_curCtx->framebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

uint32_t ContextWrapper::glCheckFramebufferStatus(uint32_t target)
{
    return m_curCtx->checkFramebufferStatus(target);
}

void ContextWrapper::glGetFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname,
                                                           int *params)
{
    m_curCtx->getFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void ContextWrapper::glRenderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height)
{
    m_curCtx->renderbufferStorage(target, internalformat, width, height);
}

void ContextWrapper::glRenderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalformat, int width,
                                                      int height)
{
    m_curCtx->renderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void ContextWrapper::glBindBuffer(uint32_t target, uint32_t buffer)
{
    m_curCtx->bindBuffer(target, buffer);
}

void ContextWrapper::glGenBuffers(int n, uint32_t *buffers)
{
    m_curCtx->genBuffers(n, buffers);
}

void ContextWrapper::glDeleteBuffers(int n, const uint32_t *buffers)
{
    m_curCtx->deleteBuffers(n, buffers);
}

void ContextWrapper::glBufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    m_curCtx->bufferData(target, size, data, usage);
}

void ContextWrapper::glBufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    m_curCtx->bufferSubData(target, offset, size, data);
}

void ContextWrapper::glClearColor(float red, float green, float blue, float alpha)
{
    m_curCtx->clearColor(red, green, blue, alpha);
}

void ContextWrapper::glClearDepthf(float depth)
{
    m_curCtx->clearDepthf(depth);
}

void ContextWrapper::glClearStencil(int stencil)
{
    m_curCtx->clearStencil(stencil);
}

void ContextWrapper::glClear(uint32_t buffers)
{
    m_curCtx->clear(buffers);
}

void ContextWrapper::glClearBufferiv(uint32_t buffer, int drawbuffer, const int *value)
{
    m_curCtx->clearBufferiv(buffer, drawbuffer, value);
}

void ContextWrapper::glClearBufferfv(uint32_t buffer, int drawbuffer, const float *value)
{
    m_curCtx->clearBufferfv(buffer, drawbuffer, value);
}

void ContextWrapper::glClearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value)
{
    m_curCtx->clearBufferuiv(buffer, drawbuffer, value);
}

void ContextWrapper::glClearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil)
{
    m_curCtx->clearBufferfi(buffer, drawbuffer, depth, stencil);
}

void ContextWrapper::glScissor(int x, int y, int width, int height)
{
    m_curCtx->scissor(x, y, width, height);
}

void ContextWrapper::glEnable(uint32_t cap)
{
    m_curCtx->enable(cap);
}

void ContextWrapper::glDisable(uint32_t cap)
{
    m_curCtx->disable(cap);
}

void ContextWrapper::glStencilFunc(uint32_t func, int ref, uint32_t mask)
{
    m_curCtx->stencilFunc(func, ref, mask);
}

void ContextWrapper::glStencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass)
{
    m_curCtx->stencilOp(sfail, dpfail, dppass);
}

void ContextWrapper::glDepthFunc(uint32_t func)
{
    m_curCtx->depthFunc(func);
}

void ContextWrapper::glBlendEquation(uint32_t mode)
{
    m_curCtx->blendEquation(mode);
}

void ContextWrapper::glBlendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha)
{
    m_curCtx->blendEquationSeparate(modeRGB, modeAlpha);
}

void ContextWrapper::glBlendFunc(uint32_t src, uint32_t dst)
{
    m_curCtx->blendFunc(src, dst);
}

void ContextWrapper::glBlendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha)
{
    m_curCtx->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void ContextWrapper::glBlendColor(float red, float green, float blue, float alpha)
{
    m_curCtx->blendColor(red, green, blue, alpha);
}

void ContextWrapper::glColorMask(bool r, bool g, bool b, bool a)
{
    m_curCtx->colorMask(r, g, b, a);
}

void ContextWrapper::glDepthMask(bool mask)
{
    m_curCtx->depthMask(mask);
}

void ContextWrapper::glStencilMask(uint32_t mask)
{
    m_curCtx->stencilMask(mask);
}

void ContextWrapper::glBlitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1,
                                       int dstY1, uint32_t mask, uint32_t filter)
{
    m_curCtx->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void ContextWrapper::glInvalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x,
                                                int y, int width, int height)
{
    m_curCtx->invalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}

void ContextWrapper::glInvalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments)
{
    m_curCtx->invalidateFramebuffer(target, numAttachments, attachments);
}

void ContextWrapper::glReadPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data)
{
    m_curCtx->readPixels(x, y, width, height, format, type, data);
}

uint32_t ContextWrapper::glGetError(void)
{
    return m_curCtx->getError();
}

void ContextWrapper::glGetIntegerv(uint32_t pname, int *params)
{
    m_curCtx->getIntegerv(pname, params);
}

} // namespace sglr
