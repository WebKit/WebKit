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

#include "sglrGLContext.hpp"
#include "sglrShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTexture.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluStrUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace sglr
{

using std::string;
using std::vector;
using tcu::TestLog;
using tcu::TextureFormat;
using tcu::Vec4;

GLContext::GLContext(const glu::RenderContext &context, tcu::TestLog &log, uint32_t logFlags,
                     const tcu::IVec4 &baseViewport)
    : Context(context.getType())
    , m_context(context)
    , m_log(log)
    , m_logFlags(logFlags)
    , m_baseViewport(baseViewport)
    , m_curViewport(0, 0, m_baseViewport.z(), m_baseViewport.w())
    , m_curScissor(0, 0, m_baseViewport.z(), m_baseViewport.w())
    , m_readFramebufferBinding(0)
    , m_drawFramebufferBinding(0)
    , m_wrapper(nullptr)
{
    const glw::Functions &gl = m_context.getFunctions();

    // Logging?
    m_wrapper = new glu::CallLogWrapper(gl, log);
    m_wrapper->enableLogging((logFlags & GLCONTEXT_LOG_CALLS) != 0);

    // Setup base viewport. This offset is active when default framebuffer is active.
    // \note Calls related to setting up base viewport are not included in log.
    gl.viewport(baseViewport.x(), baseViewport.y(), baseViewport.z(), baseViewport.w());
}

GLContext::~GLContext(void)
{
    const glw::Functions &gl = m_context.getFunctions();

    // Clean up all still alive objects
    for (std::set<uint32_t>::const_iterator i = m_allocatedFbos.begin(); i != m_allocatedFbos.end(); i++)
    {
        uint32_t fbo = *i;
        gl.deleteFramebuffers(1, &fbo);
    }

    for (std::set<uint32_t>::const_iterator i = m_allocatedRbos.begin(); i != m_allocatedRbos.end(); i++)
    {
        uint32_t rbo = *i;
        gl.deleteRenderbuffers(1, &rbo);
    }

    for (std::set<uint32_t>::const_iterator i = m_allocatedTextures.begin(); i != m_allocatedTextures.end(); i++)
    {
        uint32_t tex = *i;
        gl.deleteTextures(1, &tex);
    }

    for (std::set<uint32_t>::const_iterator i = m_allocatedBuffers.begin(); i != m_allocatedBuffers.end(); i++)
    {
        uint32_t buf = *i;
        gl.deleteBuffers(1, &buf);
    }

    for (std::set<uint32_t>::const_iterator i = m_allocatedVaos.begin(); i != m_allocatedVaos.end(); i++)
    {
        uint32_t vao = *i;
        gl.deleteVertexArrays(1, &vao);
    }

    for (std::vector<glu::ShaderProgram *>::iterator i = m_programs.begin(); i != m_programs.end(); i++)
    {
        delete *i;
    }

    gl.useProgram(0);

    delete m_wrapper;
}

void GLContext::enableLogging(uint32_t logFlags)
{
    m_logFlags = logFlags;
    m_wrapper->enableLogging((logFlags & GLCONTEXT_LOG_CALLS) != 0);
}

tcu::IVec2 GLContext::getDrawOffset(void) const
{
    if (m_drawFramebufferBinding)
        return tcu::IVec2(0, 0);
    else
        return tcu::IVec2(m_baseViewport.x(), m_baseViewport.y());
}

tcu::IVec2 GLContext::getReadOffset(void) const
{
    if (m_readFramebufferBinding)
        return tcu::IVec2(0, 0);
    else
        return tcu::IVec2(m_baseViewport.x(), m_baseViewport.y());
}

int GLContext::getWidth(void) const
{
    return m_baseViewport.z();
}

int GLContext::getHeight(void) const
{
    return m_baseViewport.w();
}

void GLContext::activeTexture(uint32_t texture)
{
    m_wrapper->glActiveTexture(texture);
}

void GLContext::texParameteri(uint32_t target, uint32_t pname, int value)
{
    m_wrapper->glTexParameteri(target, pname, value);
}

uint32_t GLContext::checkFramebufferStatus(uint32_t target)
{
    return m_wrapper->glCheckFramebufferStatus(target);
}

void GLContext::viewport(int x, int y, int width, int height)
{
    m_curViewport     = tcu::IVec4(x, y, width, height);
    tcu::IVec2 offset = getDrawOffset();

    // \note For clarity don't add the offset to log
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glViewport(" << x << ", " << y << ", " << width << ", " << height << ");"
              << TestLog::EndMessage;
    m_context.getFunctions().viewport(x + offset.x(), y + offset.y(), width, height);
}

void GLContext::bindTexture(uint32_t target, uint32_t texture)
{
    m_allocatedTextures.insert(texture);
    m_wrapper->glBindTexture(target, texture);
}

void GLContext::genTextures(int numTextures, uint32_t *textures)
{
    m_wrapper->glGenTextures(numTextures, textures);
    if (numTextures > 0)
        m_allocatedTextures.insert(textures, textures + numTextures);
}

void GLContext::deleteTextures(int numTextures, const uint32_t *textures)
{
    for (int i = 0; i < numTextures; i++)
        m_allocatedTextures.erase(textures[i]);
    m_wrapper->glDeleteTextures(numTextures, textures);
}

void GLContext::bindFramebuffer(uint32_t target, uint32_t framebuffer)
{
    // \todo [2011-10-13 pyry] This is a bit of a hack since test cases assumes 0 default fbo.
    uint32_t defaultFbo = m_context.getDefaultFramebuffer();
    TCU_CHECK(framebuffer == 0 || framebuffer != defaultFbo);

    bool isValidTarget = target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER;

    if (isValidTarget && framebuffer != 0)
        m_allocatedFbos.insert(framebuffer);

    // Update bindings.
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
        m_readFramebufferBinding = framebuffer;

    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        m_drawFramebufferBinding = framebuffer;

    if (framebuffer == 0) // Redirect 0 to platform-defined default framebuffer.
        m_wrapper->glBindFramebuffer(target, defaultFbo);
    else
        m_wrapper->glBindFramebuffer(target, framebuffer);

    // Update viewport and scissor if we updated draw framebuffer binding \note Not logged for clarity
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
    {
        tcu::IVec2 offset = getDrawOffset();
        m_context.getFunctions().viewport(m_curViewport.x() + offset.x(), m_curViewport.y() + offset.y(),
                                          m_curViewport.z(), m_curViewport.w());
        m_context.getFunctions().scissor(m_curScissor.x() + offset.x(), m_curScissor.y() + offset.y(), m_curScissor.z(),
                                         m_curScissor.w());
    }
}

void GLContext::genFramebuffers(int numFramebuffers, uint32_t *framebuffers)
{
    m_wrapper->glGenFramebuffers(numFramebuffers, framebuffers);
    if (numFramebuffers > 0)
        m_allocatedFbos.insert(framebuffers, framebuffers + numFramebuffers);
}

void GLContext::deleteFramebuffers(int numFramebuffers, const uint32_t *framebuffers)
{
    for (int i = 0; i < numFramebuffers; i++)
        m_allocatedFbos.erase(framebuffers[i]);
    m_wrapper->glDeleteFramebuffers(numFramebuffers, framebuffers);
}

void GLContext::bindRenderbuffer(uint32_t target, uint32_t renderbuffer)
{
    m_allocatedRbos.insert(renderbuffer);
    m_wrapper->glBindRenderbuffer(target, renderbuffer);
}

void GLContext::genRenderbuffers(int numRenderbuffers, uint32_t *renderbuffers)
{
    m_wrapper->glGenRenderbuffers(numRenderbuffers, renderbuffers);
    if (numRenderbuffers > 0)
        m_allocatedRbos.insert(renderbuffers, renderbuffers + numRenderbuffers);
}

void GLContext::deleteRenderbuffers(int numRenderbuffers, const uint32_t *renderbuffers)
{
    for (int i = 0; i < numRenderbuffers; i++)
        m_allocatedRbos.erase(renderbuffers[i]);
    m_wrapper->glDeleteRenderbuffers(numRenderbuffers, renderbuffers);
}

void GLContext::pixelStorei(uint32_t pname, int param)
{
    m_wrapper->glPixelStorei(pname, param);
}

void GLContext::texImage1D(uint32_t target, int level, uint32_t internalFormat, int width, int border, uint32_t format,
                           uint32_t type, const void *data)
{
    m_wrapper->glTexImage1D(target, level, internalFormat, width, border, format, type, data);
}

void GLContext::texImage2D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int border,
                           uint32_t format, uint32_t type, const void *data)
{
    m_wrapper->glTexImage2D(target, level, internalFormat, width, height, border, format, type, data);
}

void GLContext::texImage3D(uint32_t target, int level, uint32_t internalFormat, int width, int height, int depth,
                           int border, uint32_t format, uint32_t type, const void *data)
{
    m_wrapper->glTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, data);
}

void GLContext::texSubImage1D(uint32_t target, int level, int xoffset, int width, uint32_t format, uint32_t type,
                              const void *data)
{
    m_wrapper->glTexSubImage1D(target, level, xoffset, width, format, type, data);
}

void GLContext::texSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int width, int height,
                              uint32_t format, uint32_t type, const void *data)
{
    m_wrapper->glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data);
}

void GLContext::texSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int width, int height,
                              int depth, uint32_t format, uint32_t type, const void *data)
{
    m_wrapper->glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
}

void GLContext::copyTexImage1D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width, int border)
{
    // Don't log offset.
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glCopyTexImage1D(" << glu::getTextureTargetStr(target) << ", " << level << ", "
              << glu::getTextureFormatStr(internalFormat) << ", " << x << ", " << y << ", " << width << ", " << border
              << ")" << TestLog::EndMessage;

    tcu::IVec2 offset = getReadOffset();
    m_context.getFunctions().copyTexImage1D(target, level, internalFormat, offset.x() + x, offset.y() + y, width,
                                            border);
}

void GLContext::copyTexImage2D(uint32_t target, int level, uint32_t internalFormat, int x, int y, int width, int height,
                               int border)
{
    // Don't log offset.
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glCopyTexImage2D(" << glu::getTextureTargetStr(target) << ", " << level << ", "
              << glu::getTextureFormatStr(internalFormat) << ", " << x << ", " << y << ", " << width << ", " << height
              << ", " << border << ")" << TestLog::EndMessage;

    tcu::IVec2 offset = getReadOffset();
    m_context.getFunctions().copyTexImage2D(target, level, internalFormat, offset.x() + x, offset.y() + y, width,
                                            height, border);
}

void GLContext::copyTexSubImage1D(uint32_t target, int level, int xoffset, int x, int y, int width)
{
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glCopyTexSubImage1D(" << glu::getTextureTargetStr(target) << ", " << level << ", "
              << xoffset << ", " << x << ", " << y << ", " << width << ")" << TestLog::EndMessage;

    tcu::IVec2 offset = getReadOffset();
    m_context.getFunctions().copyTexSubImage1D(target, level, xoffset, offset.x() + x, offset.y() + y, width);
}

void GLContext::copyTexSubImage2D(uint32_t target, int level, int xoffset, int yoffset, int x, int y, int width,
                                  int height)
{
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glCopyTexSubImage2D(" << glu::getTextureTargetStr(target) << ", " << level << ", "
              << xoffset << ", " << yoffset << ", " << x << ", " << y << ", " << width << ", " << height << ")"
              << TestLog::EndMessage;

    tcu::IVec2 offset = getReadOffset();
    m_context.getFunctions().copyTexSubImage2D(target, level, xoffset, yoffset, offset.x() + x, offset.y() + y, width,
                                               height);
}

void GLContext::copyTexSubImage3D(uint32_t target, int level, int xoffset, int yoffset, int zoffset, int x, int y,
                                  int width, int height)
{
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glCopyTexSubImage3D(" << glu::getTextureTargetStr(target) << ", " << level << ", "
              << xoffset << ", " << yoffset << ", " << zoffset << ", " << x << ", " << y << ", " << width << ", "
              << height << ")" << TestLog::EndMessage;

    tcu::IVec2 offset = getReadOffset();
    m_context.getFunctions().copyTexSubImage3D(target, level, xoffset, yoffset, zoffset, offset.x() + x, offset.y() + y,
                                               width, height);
}

void GLContext::texStorage2D(uint32_t target, int levels, uint32_t internalFormat, int width, int height)
{
    m_wrapper->glTexStorage2D(target, levels, internalFormat, width, height);
}

void GLContext::texStorage3D(uint32_t target, int levels, uint32_t internalFormat, int width, int height, int depth)
{
    m_wrapper->glTexStorage3D(target, levels, internalFormat, width, height, depth);
}

void GLContext::framebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture,
                                     int level)
{
    m_wrapper->glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void GLContext::framebufferTextureLayer(uint32_t target, uint32_t attachment, uint32_t texture, int level, int layer)
{
    m_wrapper->glFramebufferTextureLayer(target, attachment, texture, level, layer);
}

void GLContext::framebufferRenderbuffer(uint32_t target, uint32_t attachment, uint32_t renderbuffertarget,
                                        uint32_t renderbuffer)
{
    m_wrapper->glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void GLContext::getFramebufferAttachmentParameteriv(uint32_t target, uint32_t attachment, uint32_t pname, int *params)
{
    m_wrapper->glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void GLContext::renderbufferStorage(uint32_t target, uint32_t internalformat, int width, int height)
{
    m_wrapper->glRenderbufferStorage(target, internalformat, width, height);
}

void GLContext::renderbufferStorageMultisample(uint32_t target, int samples, uint32_t internalFormat, int width,
                                               int height)
{
    m_wrapper->glRenderbufferStorageMultisample(target, samples, internalFormat, width, height);
}

void GLContext::bindBuffer(uint32_t target, uint32_t buffer)
{
    m_allocatedBuffers.insert(buffer);
    m_wrapper->glBindBuffer(target, buffer);
}

void GLContext::genBuffers(int numBuffers, uint32_t *buffers)
{
    m_wrapper->glGenBuffers(numBuffers, buffers);
    if (numBuffers > 0)
        m_allocatedBuffers.insert(buffers, buffers + numBuffers);
}

void GLContext::deleteBuffers(int numBuffers, const uint32_t *buffers)
{
    m_wrapper->glDeleteBuffers(numBuffers, buffers);
    for (int i = 0; i < numBuffers; i++)
        m_allocatedBuffers.erase(buffers[i]);
}

void GLContext::bufferData(uint32_t target, intptr_t size, const void *data, uint32_t usage)
{
    m_wrapper->glBufferData(target, (glw::GLsizeiptr)size, data, usage);
}

void GLContext::bufferSubData(uint32_t target, intptr_t offset, intptr_t size, const void *data)
{
    m_wrapper->glBufferSubData(target, (glw::GLintptr)offset, (glw::GLsizeiptr)size, data);
}

void GLContext::clearColor(float red, float green, float blue, float alpha)
{
    m_wrapper->glClearColor(red, green, blue, alpha);
}

void GLContext::clearDepthf(float depth)
{
    m_wrapper->glClearDepthf(depth);
}

void GLContext::clearStencil(int stencil)
{
    m_wrapper->glClearStencil(stencil);
}

void GLContext::clear(uint32_t buffers)
{
    m_wrapper->glClear(buffers);
}

void GLContext::clearBufferiv(uint32_t buffer, int drawbuffer, const int *value)
{
    m_wrapper->glClearBufferiv(buffer, drawbuffer, value);
}

void GLContext::clearBufferfv(uint32_t buffer, int drawbuffer, const float *value)
{
    m_wrapper->glClearBufferfv(buffer, drawbuffer, value);
}

void GLContext::clearBufferuiv(uint32_t buffer, int drawbuffer, const uint32_t *value)
{
    m_wrapper->glClearBufferuiv(buffer, drawbuffer, value);
}

void GLContext::clearBufferfi(uint32_t buffer, int drawbuffer, float depth, int stencil)
{
    m_wrapper->glClearBufferfi(buffer, drawbuffer, depth, stencil);
}

void GLContext::scissor(int x, int y, int width, int height)
{
    m_curScissor = tcu::IVec4(x, y, width, height);

    // \note For clarity don't add the offset to log
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glScissor(" << x << ", " << y << ", " << width << ", " << height << ");"
              << TestLog::EndMessage;

    tcu::IVec2 offset = getDrawOffset();
    m_context.getFunctions().scissor(offset.x() + x, offset.y() + y, width, height);
}

void GLContext::enable(uint32_t cap)
{
    m_wrapper->glEnable(cap);
}

void GLContext::disable(uint32_t cap)
{
    m_wrapper->glDisable(cap);
}

void GLContext::stencilFunc(uint32_t func, int ref, uint32_t mask)
{
    m_wrapper->glStencilFunc(func, ref, mask);
}

void GLContext::stencilOp(uint32_t sfail, uint32_t dpfail, uint32_t dppass)
{
    m_wrapper->glStencilOp(sfail, dpfail, dppass);
}

void GLContext::depthFunc(uint32_t func)
{
    m_wrapper->glDepthFunc(func);
}

void GLContext::depthRangef(float n, float f)
{
    m_wrapper->glDepthRangef(n, f);
}

void GLContext::depthRange(double n, double f)
{
    m_wrapper->glDepthRange(n, f);
}

void GLContext::polygonOffset(float factor, float units)
{
    m_wrapper->glPolygonOffset(factor, units);
}

void GLContext::provokingVertex(uint32_t convention)
{
    m_wrapper->glProvokingVertex(convention);
}

void GLContext::primitiveRestartIndex(uint32_t index)
{
    m_wrapper->glPrimitiveRestartIndex(index);
}

void GLContext::stencilFuncSeparate(uint32_t face, uint32_t func, int ref, uint32_t mask)
{
    m_wrapper->glStencilFuncSeparate(face, func, ref, mask);
}

void GLContext::stencilOpSeparate(uint32_t face, uint32_t sfail, uint32_t dpfail, uint32_t dppass)
{
    m_wrapper->glStencilOpSeparate(face, sfail, dpfail, dppass);
}

void GLContext::blendEquation(uint32_t mode)
{
    m_wrapper->glBlendEquation(mode);
}

void GLContext::blendEquationSeparate(uint32_t modeRGB, uint32_t modeAlpha)
{
    m_wrapper->glBlendEquationSeparate(modeRGB, modeAlpha);
}

void GLContext::blendFunc(uint32_t src, uint32_t dst)
{
    m_wrapper->glBlendFunc(src, dst);
}

void GLContext::blendFuncSeparate(uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha)
{
    m_wrapper->glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GLContext::blendColor(float red, float green, float blue, float alpha)
{
    m_wrapper->glBlendColor(red, green, blue, alpha);
}

void GLContext::colorMask(bool r, bool g, bool b, bool a)
{
    m_wrapper->glColorMask((glw::GLboolean)r, (glw::GLboolean)g, (glw::GLboolean)b, (glw::GLboolean)a);
}

void GLContext::depthMask(bool mask)
{
    m_wrapper->glDepthMask((glw::GLboolean)mask);
}

void GLContext::stencilMask(uint32_t mask)
{
    m_wrapper->glStencilMask(mask);
}

void GLContext::stencilMaskSeparate(uint32_t face, uint32_t mask)
{
    m_wrapper->glStencilMaskSeparate(face, mask);
}

void GLContext::blitFramebuffer(int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0, int dstX1, int dstY1,
                                uint32_t mask, uint32_t filter)
{
    tcu::IVec2 drawOffset = getDrawOffset();
    tcu::IVec2 readOffset = getReadOffset();

    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glBlitFramebuffer(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", " << srcY1
              << ", " << dstX0 << ", " << dstY0 << ", " << dstX1 << ", " << dstY1 << ", " << glu::getBufferMaskStr(mask)
              << ", " << glu::getTextureFilterStr(filter) << ")" << TestLog::EndMessage;

    m_context.getFunctions().blitFramebuffer(readOffset.x() + srcX0, readOffset.y() + srcY0, readOffset.x() + srcX1,
                                             readOffset.y() + srcY1, drawOffset.x() + dstX0, drawOffset.y() + dstY0,
                                             drawOffset.x() + dstX1, drawOffset.y() + dstY1, mask, filter);
}

void GLContext::invalidateSubFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments, int x, int y,
                                         int width, int height)
{
    tcu::IVec2 drawOffset = getDrawOffset();

    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glInvalidateSubFramebuffer(" << glu::getFramebufferTargetStr(target) << ", "
              << numAttachments << ", " << glu::getInvalidateAttachmentStr(attachments, numAttachments) << ", " << x
              << ", " << y << ", " << width << ", " << height << ")" << TestLog::EndMessage;

    m_context.getFunctions().invalidateSubFramebuffer(target, numAttachments, attachments, x + drawOffset.x(),
                                                      y + drawOffset.y(), width, height);
}

void GLContext::invalidateFramebuffer(uint32_t target, int numAttachments, const uint32_t *attachments)
{
    m_wrapper->glInvalidateFramebuffer(target, numAttachments, attachments);
}

void GLContext::bindVertexArray(uint32_t array)
{
    m_wrapper->glBindVertexArray(array);
}

void GLContext::genVertexArrays(int numArrays, uint32_t *vertexArrays)
{
    m_wrapper->glGenVertexArrays(numArrays, vertexArrays);
    if (numArrays > 0)
        m_allocatedVaos.insert(vertexArrays, vertexArrays + numArrays);
}

void GLContext::deleteVertexArrays(int numArrays, const uint32_t *vertexArrays)
{
    for (int i = 0; i < numArrays; i++)
        m_allocatedVaos.erase(vertexArrays[i]);
    m_wrapper->glDeleteVertexArrays(numArrays, vertexArrays);
}

void GLContext::vertexAttribPointer(uint32_t index, int size, uint32_t type, bool normalized, int stride,
                                    const void *pointer)
{
    m_wrapper->glVertexAttribPointer(index, size, type, (glw::GLboolean)normalized, stride, pointer);
}

void GLContext::vertexAttribIPointer(uint32_t index, int size, uint32_t type, int stride, const void *pointer)
{
    m_wrapper->glVertexAttribIPointer(index, size, type, stride, pointer);
}

void GLContext::enableVertexAttribArray(uint32_t index)
{
    m_wrapper->glEnableVertexAttribArray(index);
}

void GLContext::disableVertexAttribArray(uint32_t index)
{
    m_wrapper->glDisableVertexAttribArray(index);
}

void GLContext::vertexAttribDivisor(uint32_t index, uint32_t divisor)
{
    m_wrapper->glVertexAttribDivisor(index, divisor);
}

void GLContext::vertexAttrib1f(uint32_t index, float x)
{
    m_wrapper->glVertexAttrib1f(index, x);
}

void GLContext::vertexAttrib2f(uint32_t index, float x, float y)
{
    m_wrapper->glVertexAttrib2f(index, x, y);
}

void GLContext::vertexAttrib3f(uint32_t index, float x, float y, float z)
{
    m_wrapper->glVertexAttrib3f(index, x, y, z);
}

void GLContext::vertexAttrib4f(uint32_t index, float x, float y, float z, float w)
{
    m_wrapper->glVertexAttrib4f(index, x, y, z, w);
}

void GLContext::vertexAttribI4i(uint32_t index, int32_t x, int32_t y, int32_t z, int32_t w)
{
    m_wrapper->glVertexAttribI4i(index, x, y, z, w);
}

void GLContext::vertexAttribI4ui(uint32_t index, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    m_wrapper->glVertexAttribI4ui(index, x, y, z, w);
}

int32_t GLContext::getAttribLocation(uint32_t program, const char *name)
{
    return m_wrapper->glGetAttribLocation(program, name);
}

void GLContext::uniform1f(int32_t location, float v0)
{
    m_wrapper->glUniform1f(location, v0);
}

void GLContext::uniform1i(int32_t location, int32_t v0)
{
    m_wrapper->glUniform1i(location, v0);
}

void GLContext::uniform1fv(int32_t location, int32_t count, const float *value)
{
    m_wrapper->glUniform1fv(location, count, value);
}

void GLContext::uniform2fv(int32_t location, int32_t count, const float *value)
{
    m_wrapper->glUniform2fv(location, count, value);
}

void GLContext::uniform3fv(int32_t location, int32_t count, const float *value)
{
    m_wrapper->glUniform3fv(location, count, value);
}

void GLContext::uniform4fv(int32_t location, int32_t count, const float *value)
{
    m_wrapper->glUniform4fv(location, count, value);
}

void GLContext::uniform1iv(int32_t location, int32_t count, const int32_t *value)
{
    m_wrapper->glUniform1iv(location, count, value);
}

void GLContext::uniform2iv(int32_t location, int32_t count, const int32_t *value)
{
    m_wrapper->glUniform2iv(location, count, value);
}

void GLContext::uniform3iv(int32_t location, int32_t count, const int32_t *value)
{
    m_wrapper->glUniform3iv(location, count, value);
}

void GLContext::uniform4iv(int32_t location, int32_t count, const int32_t *value)
{
    m_wrapper->glUniform4iv(location, count, value);
}

void GLContext::uniformMatrix3fv(int32_t location, int32_t count, bool transpose, const float *value)
{
    m_wrapper->glUniformMatrix3fv(location, count, (glw::GLboolean)transpose, value);
}

void GLContext::uniformMatrix4fv(int32_t location, int32_t count, bool transpose, const float *value)
{
    m_wrapper->glUniformMatrix4fv(location, count, (glw::GLboolean)transpose, value);
}
int32_t GLContext::getUniformLocation(uint32_t program, const char *name)
{
    return m_wrapper->glGetUniformLocation(program, name);
}

void GLContext::lineWidth(float w)
{
    m_wrapper->glLineWidth(w);
}

void GLContext::drawArrays(uint32_t mode, int first, int count)
{
    m_wrapper->glDrawArrays(mode, first, count);
}

void GLContext::drawArraysInstanced(uint32_t mode, int first, int count, int instanceCount)
{
    m_wrapper->glDrawArraysInstanced(mode, first, count, instanceCount);
}

void GLContext::drawElements(uint32_t mode, int count, uint32_t type, const void *indices)
{
    m_wrapper->glDrawElements(mode, count, type, indices);
}

void GLContext::drawElementsInstanced(uint32_t mode, int count, uint32_t type, const void *indices, int instanceCount)
{
    m_wrapper->glDrawElementsInstanced(mode, count, type, indices, instanceCount);
}

void GLContext::drawElementsBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices, int baseVertex)
{
    m_wrapper->glDrawElementsBaseVertex(mode, count, type, indices, baseVertex);
}

void GLContext::drawElementsInstancedBaseVertex(uint32_t mode, int count, uint32_t type, const void *indices,
                                                int instanceCount, int baseVertex)
{
    m_wrapper->glDrawElementsInstancedBaseVertex(mode, count, type, indices, instanceCount, baseVertex);
}

void GLContext::drawRangeElements(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                  const void *indices)
{
    m_wrapper->glDrawRangeElements(mode, start, end, count, type, indices);
}

void GLContext::drawRangeElementsBaseVertex(uint32_t mode, uint32_t start, uint32_t end, int count, uint32_t type,
                                            const void *indices, int baseVertex)
{
    m_wrapper->glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, baseVertex);
}

void GLContext::drawArraysIndirect(uint32_t mode, const void *indirect)
{
    m_wrapper->glDrawArraysIndirect(mode, indirect);
}

void GLContext::drawElementsIndirect(uint32_t mode, uint32_t type, const void *indirect)
{
    m_wrapper->glDrawElementsIndirect(mode, type, indirect);
}

void GLContext::multiDrawArrays(uint32_t mode, const int *first, const int *count, int primCount)
{
    m_wrapper->glMultiDrawArrays(mode, first, count, primCount);
}

void GLContext::multiDrawElements(uint32_t mode, const int *count, uint32_t type, const void **indices, int primCount)
{
    m_wrapper->glMultiDrawElements(mode, count, type, indices, primCount);
}

void GLContext::multiDrawElementsBaseVertex(uint32_t mode, const int *count, uint32_t type, const void **indices,
                                            int primCount, const int *baseVertex)
{
    m_wrapper->glMultiDrawElementsBaseVertex(mode, count, type, indices, primCount, baseVertex);
}

uint32_t GLContext::createProgram(ShaderProgram *shader)
{
    m_programs.reserve(m_programs.size() + 1);

    glu::ShaderProgram *program = nullptr;

    if (!shader->m_hasGeometryShader)
        program = new glu::ShaderProgram(m_context, glu::makeVtxFragSources(shader->m_vertSrc, shader->m_fragSrc));
    else
        program = new glu::ShaderProgram(m_context, glu::ProgramSources() << glu::VertexSource(shader->m_vertSrc)
                                                                          << glu::FragmentSource(shader->m_fragSrc)
                                                                          << glu::GeometrySource(shader->m_geomSrc));

    if (!program->isOk())
    {
        m_log << *program;
        delete program;
        TCU_FAIL("Compile failed");
    }

    if ((m_logFlags & GLCONTEXT_LOG_PROGRAMS) != 0)
        m_log << *program;

    m_programs.push_back(program);
    return program->getProgram();
}

void GLContext::deleteProgram(uint32_t program)
{
    for (std::vector<glu::ShaderProgram *>::iterator i = m_programs.begin(); i != m_programs.end(); i++)
    {
        if ((*i)->getProgram() == program)
        {
            delete *i;
            m_programs.erase(i);
            return;
        }
    }

    DE_FATAL("invalid delete");
}

void GLContext::useProgram(uint32_t program)
{
    m_wrapper->glUseProgram(program);
}

void GLContext::readPixels(int x, int y, int width, int height, uint32_t format, uint32_t type, void *data)
{
    // Don't log offset.
    if ((m_logFlags & GLCONTEXT_LOG_CALLS) != 0)
        m_log << TestLog::Message << "glReadPixels(" << x << ", " << y << ", " << width << ", " << height << ", "
              << glu::getTextureFormatStr(format) << ", " << glu::getTypeStr(type) << ", " << data << ")"
              << TestLog::EndMessage;

    tcu::IVec2 offset = getReadOffset();
    m_context.getFunctions().readPixels(x + offset.x(), y + offset.y(), width, height, format, type, data);
}

uint32_t GLContext::getError(void)
{
    return m_wrapper->glGetError();
}

void GLContext::finish(void)
{
    m_wrapper->glFinish();
}

void GLContext::getIntegerv(uint32_t pname, int *params)
{
    m_wrapper->glGetIntegerv(pname, params);
}

const char *GLContext::getString(uint32_t pname)
{
    return (const char *)m_wrapper->glGetString(pname);
}

} // namespace sglr
