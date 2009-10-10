/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "CachedImage.h"
#include "CanvasActiveInfo.h"
#include "CanvasArray.h"
#include "CanvasBuffer.h"
#include "CanvasFramebuffer.h"
#include "CanvasFloatArray.h"
#include "CanvasIntArray.h"
#include "CanvasObject.h"
#include "CanvasProgram.h"
#include "CanvasRenderbuffer.h"
#include "CanvasShader.h"
#include "CanvasTexture.h"
#include "CanvasUnsignedByteArray.h"
#include "CString.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "NotImplemented.h"
#include "WebKitCSSMatrix.h"

#include <CoreGraphics/CGBitmapContext.h>

namespace WebCore {

GraphicsContext3D::GraphicsContext3D()
{
    CGLPixelFormatAttribute attribs[] =
    {
        (CGLPixelFormatAttribute) kCGLPFAAccelerated,
        (CGLPixelFormatAttribute) kCGLPFAColorSize, (CGLPixelFormatAttribute) 32,
        (CGLPixelFormatAttribute) kCGLPFADepthSize, (CGLPixelFormatAttribute) 32,
        (CGLPixelFormatAttribute) kCGLPFASupersample,
        (CGLPixelFormatAttribute) 0
    };
    
    CGLPixelFormatObj pixelFormatObj;
    GLint numPixelFormats;
    
    CGLChoosePixelFormat(attribs, &pixelFormatObj, &numPixelFormats);
    
    CGLCreateContext(pixelFormatObj, 0, &m_contextObj);
    
    CGLDestroyPixelFormat(pixelFormatObj);
    
    // Set the current context to the one given to us.
    CGLSetCurrentContext(m_contextObj);
    
    // create a texture to render into
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    ::glBindTexture(GL_TEXTURE_2D, 0);
    
    // create an FBO
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    
    ::glGenRenderbuffersEXT(1, &m_depthBuffer);
    ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthBuffer);
    ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, 1, 1);
    ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthBuffer);
    
    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    CGLSetCurrentContext(m_contextObj);
    ::glDeleteRenderbuffersEXT(1, & m_depthBuffer);
    ::glDeleteTextures(1, &m_texture);
    ::glDeleteFramebuffersEXT(1, &m_fbo);
    CGLSetCurrentContext(0);
    CGLDestroyContext(m_contextObj);
}

void GraphicsContext3D::checkError() const
{
    // FIXME: This needs to only be done in the debug context. It will probably throw an exception
    // on error and print the error message to the debug console
    GLenum error = ::glGetError();
    if (error != GL_NO_ERROR)
        notImplemented();
}

void GraphicsContext3D::makeContextCurrent()
{
    CGLSetCurrentContext(m_contextObj);
}

void GraphicsContext3D::beginPaint(CanvasRenderingContext3D* context)
{
    UNUSED_PARAM(context);
}

void GraphicsContext3D::endPaint()
{
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight)
        return;
    
    m_currentWidth = width;
    m_currentHeight = height;
    
    CGLSetCurrentContext(m_contextObj);
    
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    ::glBindTexture(GL_TEXTURE_2D, 0);
    
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthBuffer);
    ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width, height);
    ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthBuffer);
    GLenum status = ::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        // FIXME: cleanup
        notImplemented();
    }

    ::glViewport(0, 0, m_currentWidth, m_currentHeight);
    ::glClear(GL_COLOR_BUFFER_BIT);
    ::glFlush();
}

static inline void ensureContext(CGLContextObj context)
{
    CGLContextObj currentContext = CGLGetCurrentContext();
    if (currentContext != context)
        CGLSetCurrentContext(context);
}

void GraphicsContext3D::activeTexture(unsigned long texture)
{
    ensureContext(m_contextObj);
    ::glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;
    ensureContext(m_contextObj);
    ::glAttachShader((GLuint) program->object(), (GLuint) shader->object());
}

void GraphicsContext3D::bindAttribLocation(CanvasProgram* program, unsigned long index, const String& name)
{
    if (!program)
        return;
    ensureContext(m_contextObj);
    ::glBindAttribLocation((GLuint) program->object(), index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(unsigned long target, CanvasBuffer* buffer)
{
    ensureContext(m_contextObj);
    ::glBindBuffer(target, buffer ? (GLuint) buffer->object() : 0);
}


void GraphicsContext3D::bindFramebuffer(unsigned long target, CanvasFramebuffer* buffer)
{
    ensureContext(m_contextObj);
    ::glBindFramebufferEXT(target, buffer ? (GLuint) buffer->object() : m_fbo);
}

void GraphicsContext3D::bindRenderbuffer(unsigned long target, CanvasRenderbuffer* renderbuffer)
{
    ensureContext(m_contextObj);
    ::glBindBuffer(target, renderbuffer ? (GLuint) renderbuffer->object() : 0);
}


void GraphicsContext3D::bindTexture(unsigned long target, CanvasTexture* texture)
{
    ensureContext(m_contextObj);
    ::glBindTexture(target, texture ? (GLuint) texture->object() : 0);
}

void GraphicsContext3D::blendColor(double red, double green, double blue, double alpha)
{
    ensureContext(m_contextObj);
    ::glBlendColor(static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue), static_cast<float>(alpha));
}

void GraphicsContext3D::blendEquation( unsigned long mode )
{
    ensureContext(m_contextObj);
    ::glBlendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    ensureContext(m_contextObj);
    ::glBlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContext3D::blendFunc(unsigned long sfactor, unsigned long dfactor)
{
    ensureContext(m_contextObj);
    ::glBlendFunc(sfactor, dfactor);
}       

void GraphicsContext3D::blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    ensureContext(m_contextObj);
    ::glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(unsigned long target, int size, unsigned long usage)
{
    ensureContext(m_contextObj);
    ::glBufferData(target, size, 0, usage);
}
void GraphicsContext3D::bufferData(unsigned long target, CanvasArray* array, unsigned long usage)
{
    if (!array || !array->length())
        return;
    
    ensureContext(m_contextObj);
    ::glBufferData(target, array->sizeInBytes(), array->baseAddress(), usage);
}

void GraphicsContext3D::bufferSubData(unsigned long target, long offset, CanvasArray* array)
{
    if (!array || !array->length())
        return;
    
    ensureContext(m_contextObj);
    ::glBufferSubData(target, offset, array->sizeInBytes(), array->baseAddress());
}

unsigned long GraphicsContext3D::checkFramebufferStatus(unsigned long target)
{
    ensureContext(m_contextObj);
    return ::glCheckFramebufferStatusEXT(target);
}

void GraphicsContext3D::clearColor(double r, double g, double b, double a)
{
    ensureContext(m_contextObj);
    ::glClearColor(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), static_cast<float>(a));
}

void GraphicsContext3D::clear(unsigned long mask)
{
    ensureContext(m_contextObj);
    ::glClear(mask);
}

void GraphicsContext3D::clearDepth(double depth)
{
    ensureContext(m_contextObj);
    ::glClearDepth(depth);
}

void GraphicsContext3D::clearStencil(long s)
{
    ensureContext(m_contextObj);
    ::glClearStencil(s);
}

void GraphicsContext3D::colorMask(bool red, bool green, bool blue, bool alpha)
{
    ensureContext(m_contextObj);
    ::glColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(CanvasShader* shader)
{
    if (!shader)
        return;
    
    ensureContext(m_contextObj);
    ::glCompileShader((GLuint) shader->object());
}

void GraphicsContext3D::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    ensureContext(m_contextObj);
    ::glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void GraphicsContext3D::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    ensureContext(m_contextObj);
    ::glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GraphicsContext3D::cullFace(unsigned long mode)
{
    ensureContext(m_contextObj);
    ::glCullFace(mode);
}

void GraphicsContext3D::depthFunc(unsigned long func)
{
    ensureContext(m_contextObj);
    ::glDepthFunc(func);
}

void GraphicsContext3D::depthMask(bool flag)
{
    ensureContext(m_contextObj);
    ::glDepthMask(flag);
}

void GraphicsContext3D::depthRange(double zNear, double zFar)
{
    ensureContext(m_contextObj);
    ::glDepthRange(zNear, zFar);
}

void GraphicsContext3D::detachShader(CanvasProgram* program, CanvasShader* shader)
{
    if (!program || !shader)
        return;
    
    ensureContext(m_contextObj);
    ::glDetachShader((GLuint) program->object(), (GLuint) shader->object());
}

void GraphicsContext3D::disable(unsigned long cap)
{
    ensureContext(m_contextObj);
    ::glDisable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(unsigned long index)
{
    ensureContext(m_contextObj);
    ::glDisableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(unsigned long mode, long first, long count)
{
    ensureContext(m_contextObj);
    ::glDrawArrays(mode, first, count);
}

void GraphicsContext3D::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    ensureContext(m_contextObj);
    ::glDrawElements(mode, count, type, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::enable(unsigned long cap)
{
    ensureContext(m_contextObj);
    ::glEnable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(unsigned long index)
{
    ensureContext(m_contextObj);
    ::glEnableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    ensureContext(m_contextObj);
    ::glFinish();
}

void GraphicsContext3D::flush()
{
    ensureContext(m_contextObj);
    ::glFlush();
}

void GraphicsContext3D::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, CanvasRenderbuffer* buffer)
{
    if (!buffer)
        return;
    
    ensureContext(m_contextObj);
    ::glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, (GLuint) buffer->object());
}

void GraphicsContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, CanvasTexture* texture, long level)
{
    if (!texture)
        return;
    
    ensureContext(m_contextObj);
    ::glFramebufferTexture2DEXT(target, attachment, textarget, (GLuint) texture->object(), level);
}

void GraphicsContext3D::frontFace(unsigned long mode)
{
    ensureContext(m_contextObj);
    ::glFrontFace(mode);
}

void GraphicsContext3D::generateMipmap(unsigned long target)
{
    ensureContext(m_contextObj);
    ::glGenerateMipmapEXT(target);
}

bool GraphicsContext3D::getActiveAttrib(CanvasProgram* program, unsigned long index, ActiveInfo& info)
{
    if (!program->object())
        return false;
    ensureContext(m_contextObj);
    GLint maxAttributeSize = 0;
    ::glGetProgramiv(static_cast<GLuint>(program->object()), GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    GLchar name[maxAttributeSize]; // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveAttrib(static_cast<GLuint>(program->object()), index, maxAttributeSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}
    
bool GraphicsContext3D::getActiveUniform(CanvasProgram* program, unsigned long index, ActiveInfo& info)
{
    if (!program->object())
        return false;
    ensureContext(m_contextObj);
    GLint maxUniformSize = 0;
    ::glGetProgramiv(static_cast<GLuint>(program->object()), GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    GLchar name[maxUniformSize]; // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveUniform(static_cast<GLuint>(program->object()), index, maxUniformSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}

int GraphicsContext3D::getAttribLocation(CanvasProgram* program, const String& name)
{
    if (!program)
        return -1;
    
    ensureContext(m_contextObj);
    return ::glGetAttribLocation((GLuint) program->object(), name.utf8().data());
}

unsigned long GraphicsContext3D::getError()
{
    ensureContext(m_contextObj);
    return ::glGetError();
}

String GraphicsContext3D::getString(unsigned long name)
{
    ensureContext(m_contextObj);
    return String((const char*) ::glGetString(name));
}

void GraphicsContext3D::hint(unsigned long target, unsigned long mode)
{
    ensureContext(m_contextObj);
    ::glHint(target, mode);
}

bool GraphicsContext3D::isBuffer(CanvasBuffer* buffer)
{
    if (!buffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsBuffer((GLuint) buffer->object());
}

bool GraphicsContext3D::isEnabled(unsigned long cap)
{
    ensureContext(m_contextObj);
    return ::glIsEnabled(cap);
}

bool GraphicsContext3D::isFramebuffer(CanvasFramebuffer* framebuffer)
{
    if (!framebuffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsFramebufferEXT((GLuint) framebuffer->object());
}

bool GraphicsContext3D::isProgram(CanvasProgram* program)
{
    if (!program)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsProgram((GLuint) program->object());
}

bool GraphicsContext3D::isRenderbuffer(CanvasRenderbuffer* renderbuffer)
{
    if (!renderbuffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsRenderbufferEXT((GLuint) renderbuffer->object());
}

bool GraphicsContext3D::isShader(CanvasShader* shader)
{
    if (!shader)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsShader((GLuint) shader->object());
}

bool GraphicsContext3D::isTexture(CanvasTexture* texture)
{
    if (!texture)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsTexture((GLuint) texture->object());
}

void GraphicsContext3D::lineWidth(double width)
{
    ensureContext(m_contextObj);
    ::glLineWidth(static_cast<float>(width));
}

void GraphicsContext3D::linkProgram(CanvasProgram* program)
{
    if (!program)
        return;
    
    ensureContext(m_contextObj);
    ::glLinkProgram((GLuint) program->object());
}

void GraphicsContext3D::pixelStorei(unsigned long pname, long param)
{
    ensureContext(m_contextObj);
    ::glPixelStorei(pname, param);
}

void GraphicsContext3D::polygonOffset(double factor, double units)
{
    ensureContext(m_contextObj);
    ::glPolygonOffset(static_cast<float>(factor), static_cast<float>(units));
}

void GraphicsContext3D::releaseShaderCompiler()
{
    // FIXME: This is not implemented on desktop OpenGL. We need to have ifdefs for the different GL variants
    ensureContext(m_contextObj);
    //::glReleaseShaderCompiler();
}

void GraphicsContext3D::renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    ensureContext(m_contextObj);
    ::glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GraphicsContext3D::sampleCoverage(double value, bool invert)
{
    ensureContext(m_contextObj);
    ::glSampleCoverage(static_cast<float>(value), invert);
}

void GraphicsContext3D::scissor(long x, long y, unsigned long width, unsigned long height)
{
    ensureContext(m_contextObj);
    ::glScissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(CanvasShader* shader, const String& string)
{
    if (!shader)
        return;
    
    ensureContext(m_contextObj);
    const CString& cs = string.utf8();
    const char* s = cs.data();
    
    int length = string.length();
    ::glShaderSource((GLuint) shader->object(), 1, &s, &length);
}

void GraphicsContext3D::stencilFunc(unsigned long func, long ref, unsigned long mask)
{
    ensureContext(m_contextObj);
    ::glStencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    ensureContext(m_contextObj);
    ::glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(unsigned long mask)
{
    ensureContext(m_contextObj);
    ::glStencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(unsigned long face, unsigned long mask)
{
    ensureContext(m_contextObj);
    ::glStencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    ensureContext(m_contextObj);
    ::glStencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    ensureContext(m_contextObj);
    ::glStencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContext3D::texParameterf(unsigned target, unsigned pname, float value)
{
    ensureContext(m_contextObj);
    ::glTexParameterf(target, pname, static_cast<float>(value));
}

void GraphicsContext3D::texParameteri(unsigned target, unsigned pname, int value)
{
    ensureContext(m_contextObj);
    ::glTexParameteri(target, pname, static_cast<float>(value));
}

void GraphicsContext3D::uniform1f(long location, float v0)
{
    ensureContext(m_contextObj);
    ::glUniform1f(location, v0);
}

void GraphicsContext3D::uniform1fv(long location, float* array, int size)
{
    ensureContext(m_contextObj);
    ::glUniform1fv(location, size, array);
}

void GraphicsContext3D::uniform2f(long location, float v0, float v1)
{
    ensureContext(m_contextObj);
    ::glUniform2f(location, v0, v1);
}

void GraphicsContext3D::uniform2fv(long location, float* array, int size)
{
    // FIXME: length needs to be a multiple of 2
    ensureContext(m_contextObj);
    ::glUniform2fv(location, size, array);
}

void GraphicsContext3D::uniform3f(long location, float v0, float v1, float v2)
{
    ensureContext(m_contextObj);
    ::glUniform3f(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3fv(long location, float* array, int size)
{
    // FIXME: length needs to be a multiple of 3
    ensureContext(m_contextObj);
    ::glUniform3fv(location, size, array);
}

void GraphicsContext3D::uniform4f(long location, float v0, float v1, float v2, float v3)
{
    ensureContext(m_contextObj);
    ::glUniform4f(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4fv(long location, float* array, int size)
{
    // FIXME: length needs to be a multiple of 4
    ensureContext(m_contextObj);
    ::glUniform4fv(location, size, array);
}

void GraphicsContext3D::uniform1i(long location, int v0)
{
    ensureContext(m_contextObj);
    ::glUniform1i(location, v0);
}

void GraphicsContext3D::uniform1iv(long location, int* array, int size)
{
    ensureContext(m_contextObj);
    ::glUniform1iv(location, size, array);
}

void GraphicsContext3D::uniform2i(long location, int v0, int v1)
{
    ensureContext(m_contextObj);
    ::glUniform2i(location, v0, v1);
}

void GraphicsContext3D::uniform2iv(long location, int* array, int size)
{
    // FIXME: length needs to be a multiple of 2
    ensureContext(m_contextObj);
    ::glUniform2iv(location, size, array);
}

void GraphicsContext3D::uniform3i(long location, int v0, int v1, int v2)
{
    ensureContext(m_contextObj);
    ::glUniform3i(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3iv(long location, int* array, int size)
{
    // FIXME: length needs to be a multiple of 3
    ensureContext(m_contextObj);
    ::glUniform3iv(location, size, array);
}

void GraphicsContext3D::uniform4i(long location, int v0, int v1, int v2, int v3)
{
    ensureContext(m_contextObj);
    ::glUniform4i(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4iv(long location, int* array, int size)
{
    // FIXME: length needs to be a multiple of 4
    ensureContext(m_contextObj);
    ::glUniform4iv(location, size, array);
}

void GraphicsContext3D::uniformMatrix2fv(long location, bool transpose, float* array, int size)
{
    // FIXME: length needs to be a multiple of 4
    ensureContext(m_contextObj);
    ::glUniformMatrix2fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix3fv(long location, bool transpose, float* array, int size)
{
    // FIXME: length needs to be a multiple of 9
    ensureContext(m_contextObj);
    ::glUniformMatrix3fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix4fv(long location, bool transpose, float* array, int size)
{
    // FIXME: length needs to be a multiple of 16
    ensureContext(m_contextObj);
    ::glUniformMatrix4fv(location, size, transpose, array);
}

void GraphicsContext3D::useProgram(CanvasProgram* program)
{
    if (!program)
        return;
    
    ensureContext(m_contextObj);
    ::glUseProgram((GLuint) program->object());
}

void GraphicsContext3D::validateProgram(CanvasProgram* program)
{
    if (!program)
        return;
    
    ensureContext(m_contextObj);
    ::glValidateProgram((GLuint) program->object());
}

void GraphicsContext3D::vertexAttrib1f(unsigned long indx, float v0)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib1f(indx, v0);
}

void GraphicsContext3D::vertexAttrib1fv(unsigned long indx, float* array)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib1fv(indx, array);
}

void GraphicsContext3D::vertexAttrib2f(unsigned long indx, float v0, float v1)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib2f(indx, v0, v1);
}

void GraphicsContext3D::vertexAttrib2fv(unsigned long indx, float* array)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib2fv(indx, array);
}

void GraphicsContext3D::vertexAttrib3f(unsigned long indx, float v0, float v1, float v2)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib3f(indx, v0, v1, v2);
}

void GraphicsContext3D::vertexAttrib3fv(unsigned long indx, float* array)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib3fv(indx, array);
}

void GraphicsContext3D::vertexAttrib4f(unsigned long indx, float v0, float v1, float v2, float v3)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib4f(indx, v0, v1, v2, v3);
}

void GraphicsContext3D::vertexAttrib4fv(unsigned long indx, float* array)
{
    ensureContext(m_contextObj);
    ::glVertexAttrib4fv(indx, array);
}

void GraphicsContext3D::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized, unsigned long stride, unsigned long offset)
{
    ensureContext(m_contextObj);
    ::glVertexAttribPointer(indx, size, type, normalized, stride, reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(long x, long y, unsigned long width, unsigned long height)
{
    ensureContext(m_contextObj);
    ::glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

static int sizeForGetParam(unsigned long pname)
{
    switch(pname) {
        case GL_ACTIVE_TEXTURE:                  return 1;
        case GL_ALIASED_LINE_WIDTH_RANGE:        return 2;
        case GL_ALIASED_POINT_SIZE_RANGE:        return 2;
        case GL_ALPHA_BITS:                      return 1;
        case GL_ARRAY_BUFFER_BINDING:            return 1; // (* actually a CanvasBuffer*)
        case GL_BLEND:                           return 1;
        case GL_BLEND_COLOR:                     return 4;
        case GL_BLEND_DST_ALPHA:                 return 1;
        case GL_BLEND_DST_RGB:                   return 1;
        case GL_BLEND_EQUATION_ALPHA:            return 1;
        case GL_BLEND_EQUATION_RGB:              return 1;
        case GL_BLEND_SRC_ALPHA:                 return 1;
        case GL_BLEND_SRC_RGB:                   return 1;
        case GL_BLUE_BITS:                       return 1;
        case GL_COLOR_CLEAR_VALUE:               return 4;
        case GL_COLOR_WRITEMASK:                 return 4;
        case GL_COMPRESSED_TEXTURE_FORMATS:      return GL_NUM_COMPRESSED_TEXTURE_FORMATS;
        case GL_CULL_FACE:                       return 1;
        case GL_CULL_FACE_MODE:                  return 1;
        case GL_CURRENT_PROGRAM:                 return 1; // (* actually a CanvasProgram*)
        case GL_DEPTH_BITS:                      return 1;
        case GL_DEPTH_CLEAR_VALUE:               return 1;
        case GL_DEPTH_FUNC:                      return 1;
        case GL_DEPTH_RANGE:                     return 2;
        case GL_DEPTH_TEST:                      return 1;
        case GL_DEPTH_WRITEMASK:                 return 1;
        case GL_DITHER:                          return 1;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:    return 1; // (* actually a CanvasBuffer*)
        case GL_FRAMEBUFFER_BINDING_EXT:         return 1; // (* actually a CanvasFramebuffer*)
        case GL_FRONT_FACE:                      return 1;
        case GL_GENERATE_MIPMAP_HINT:            return 1;
        case GL_GREEN_BITS:                      return 1;
            //case GL_IMPLEMENTATION_COLOR_READ_FORMAT:return 1;
            //case GL_IMPLEMENTATION_COLOR_READ_TYPE:  return 1;
        case GL_LINE_WIDTH:                      return 1;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:return 1;
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:       return 1;
            //case GL_MAX_FRAGMENT_UNIFORM_VECTORS:    return 1;
        case GL_MAX_RENDERBUFFER_SIZE_EXT:       return 1;
        case GL_MAX_TEXTURE_IMAGE_UNITS:         return 1;
        case GL_MAX_TEXTURE_SIZE:                return 1;
            //case GL_MAX_VARYING_VECTORS:             return 1;
        case GL_MAX_VERTEX_ATTRIBS:              return 1;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:  return 1;
            //case GL_MAX_VERTEX_UNIFORM_VECTORS:      return 1;
        case GL_MAX_VIEWPORT_DIMS:               return 2;
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS:  return 1;
            //case GL_NUM_SHADER_BINARY_FORMATS:       return 1;
        case GL_PACK_ALIGNMENT:                  return 1;
        case GL_POLYGON_OFFSET_FACTOR:           return 1;
        case GL_POLYGON_OFFSET_FILL:             return 1;
        case GL_POLYGON_OFFSET_UNITS:            return 1;
        case GL_RED_BITS:                        return 1;
        case GL_RENDERBUFFER_BINDING_EXT:        return 1; // (* actually a CanvasRenderbuffer*)
        case GL_SAMPLE_BUFFERS:                  return 1;
        case GL_SAMPLE_COVERAGE_INVERT:          return 1;
        case GL_SAMPLE_COVERAGE_VALUE:           return 1;
        case GL_SAMPLES:                         return 1;
        case GL_SCISSOR_BOX:                     return 4;
        case GL_SCISSOR_TEST:                    return 1;
            //case GL_SHADER_BINARY_FORMATS:           return GL_NUM_SHADER_BINARY_FORMATS;
            //case GL_SHADER_COMPILER:                 return 1;
        case GL_STENCIL_BACK_FAIL:               return 1;
        case GL_STENCIL_BACK_FUNC:               return 1;
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:    return 1;
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:    return 1;
        case GL_STENCIL_BACK_REF:                return 1;
        case GL_STENCIL_BACK_VALUE_MASK:         return 1;
        case GL_STENCIL_BACK_WRITEMASK:          return 1;
        case GL_STENCIL_BITS:                    return 1;
        case GL_STENCIL_CLEAR_VALUE:             return 1;
        case GL_STENCIL_FAIL:                    return 1;
        case GL_STENCIL_FUNC:                    return 1;
        case GL_STENCIL_PASS_DEPTH_FAIL:         return 1;
        case GL_STENCIL_PASS_DEPTH_PASS:         return 1;
        case GL_STENCIL_REF:                     return 1;
        case GL_STENCIL_TEST:                    return 1;
        case GL_STENCIL_VALUE_MASK:              return 1;
        case GL_STENCIL_WRITEMASK:               return 1;
        case GL_SUBPIXEL_BITS:                   return 1;
        case GL_TEXTURE_BINDING_2D:              return 1; // (* actually a CanvasTexture*)
        case GL_TEXTURE_BINDING_CUBE_MAP:        return 1; // (* actually a CanvasTexture*)
        case GL_UNPACK_ALIGNMENT:                return 1;
        case GL_VIEWPORT:                        return 4;
    }
    
    return -1;
}

bool GraphicsContext3D::getBoolean(unsigned long pname)
{
    int size = sizeForGetParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    bool isAlloced = false;
    GLboolean buf[4];
    GLboolean* pbuf = buf;
            
    if (size > 4) {
        pbuf = (GLboolean*) malloc(size * sizeof(GLboolean));
        isAlloced = true;
    }
            
    ::glGetBooleanv(pname, pbuf);
    
    bool value = pbuf[0];

    if (isAlloced)
        free(pbuf);
    
    return value;
}

PassRefPtr<CanvasUnsignedByteArray> GraphicsContext3D::getBooleanv(unsigned long pname)
{
    int size = sizeForGetParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    RefPtr<CanvasUnsignedByteArray> array = CanvasUnsignedByteArray::create(size);
    bool isAlloced = false;
    GLboolean buf[4];
    GLboolean* pbuf = buf;
            
    if (size > 4) {
        pbuf = (GLboolean*) malloc(size * sizeof(GLboolean));
        isAlloced = true;
    }
            
    ::glGetBooleanv(pname, pbuf);
            
    for (int i = 0; i < size; ++i)
        array->set(i, static_cast<unsigned char>(pbuf[i]));
            
    if (isAlloced)
        free(pbuf);
    
    return array;
}

float GraphicsContext3D::getFloat(unsigned long pname)
{
    int size = sizeForGetParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    bool isAlloced = false;
    GLfloat buf[4];
    GLfloat* pbuf = buf;
            
    if (size > 4) {
        pbuf = (GLfloat*) malloc(size * sizeof(GLfloat));
        isAlloced = true;
    }
            
    ::glGetFloatv(pname, pbuf);
    
    float value = pbuf[0];

    if (isAlloced)
        free(pbuf);
    
    return value;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getFloatv(unsigned long pname)
{
    int size = sizeForGetParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    RefPtr<CanvasFloatArray> array = CanvasFloatArray::create(size);
    bool isAlloced = false;
    GLfloat buf[4];
    GLfloat* pbuf = buf;
            
    if (size > 4) {
        pbuf = (GLfloat*) malloc(size * sizeof(GLfloat));
        isAlloced = true;
    }
            
    ::glGetFloatv(pname, pbuf);
            
    for (int i = 0; i < size; ++i)
        array->set(i, static_cast<float>(pbuf[i]));
            
    if (isAlloced)
        free(pbuf);
    
    return array;
}

int GraphicsContext3D::getInteger(unsigned long pname)
{
    int size = sizeForGetParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    bool isAlloced = false;
    GLint buf[4];
    GLint* pbuf = buf;
            
    if (size > 4) {
        pbuf = (GLint*) malloc(size * sizeof(GLint));
        isAlloced = true;
    }
            
    ::glGetIntegerv(pname, pbuf);
    
    int value = pbuf[0];

    if (isAlloced)
        free(pbuf);
    
    return value;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getIntegerv(unsigned long pname)
{
    int size = sizeForGetParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(size);
    bool isAlloced = false;
    GLint buf[4];
    GLint* pbuf = buf;
            
    if (size > 4) {
        pbuf = (GLint*) malloc(size * sizeof(GLint));
        isAlloced = true;
    }
            
    ::glGetIntegerv(pname, pbuf);
            
    for (int i = 0; i < size; ++i)
        array->set(i, static_cast<int>(pbuf[i]));
            
    if (isAlloced)
        free(pbuf);
    
    return array;
}

int GraphicsContext3D::getBufferParameteri(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLint data;
    ::glGetBufferParameteriv(target, pname, &data);
    return data;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getBufferParameteriv(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(1);
    GLint data;
    ::glGetBufferParameteriv(target, pname, &data);
    array->set(0, static_cast<int>(data));
    
    return array;
}

int GraphicsContext3D::getFramebufferAttachmentParameteri(unsigned long target, unsigned long attachment, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLint data;
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, &data);
    return data;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname)
{
    ensureContext(m_contextObj);
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(1);
    GLint data;
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, &data);
    array->set(0, static_cast<int>(data));
    
    return array;
}

int GraphicsContext3D::getProgrami(CanvasProgram* program, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLint data;
    ::glGetProgramiv((GLuint) program->object(), pname, &data);
    return data;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getProgramiv(CanvasProgram* program, unsigned long pname)
{
    ensureContext(m_contextObj);
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(1);
    GLint data;
    ::glGetProgramiv((GLuint) program->object(), pname, &data);
    array->set(0, static_cast<int>(data));
    
    return array;
}

String GraphicsContext3D::getProgramInfoLog(CanvasProgram* program)
{
    if (!program)
        return String();
    
    ensureContext(m_contextObj);
    GLint length;
    ::glGetProgramiv((GLuint) program->object(), GL_INFO_LOG_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) malloc(length);
    ::glGetProgramInfoLog((GLuint) program->object(), length, &size, info);
    String s(info);
    free(info);
    return s;
}

int GraphicsContext3D::getRenderbufferParameteri(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLint data;
    ::glGetBufferParameteriv(target, pname, &data);
    return data;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getRenderbufferParameteriv(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(1);
    GLint data;
    ::glGetBufferParameteriv(target, pname, &data);
    array->set(0, static_cast<int>(data));
    
    return array;
}

int GraphicsContext3D::getShaderi(CanvasShader* shader, unsigned long pname)
{
    if (!shader)
        return 0;
    
    ensureContext(m_contextObj);
    GLint data;
    ::glGetShaderiv((GLuint) shader->object(), pname, &data);
    return data;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getShaderiv(CanvasShader* shader, unsigned long pname)
{
    if (!shader)
        return 0;
    
    ensureContext(m_contextObj);
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(1);
    GLint data;
    ::glGetShaderiv((GLuint) shader->object(), pname, &data);
    array->set(0, static_cast<int>(data));
    
    return array;
}

String GraphicsContext3D::getShaderInfoLog(CanvasShader* shader)
{
    if (!shader)
        return String();
    
    ensureContext(m_contextObj);
    GLint length;
    ::glGetShaderiv((GLuint) shader->object(), GL_INFO_LOG_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) malloc(length);
    ::glGetShaderInfoLog((GLuint) shader->object(), length, &size, info);
    String s(info);
    free(info);
    return s;
}

String GraphicsContext3D::getShaderSource(CanvasShader* shader)
{
    if (!shader)
        return String();
    
    ensureContext(m_contextObj);
    GLint length;
    ::glGetShaderiv((GLuint) shader->object(), GL_SHADER_SOURCE_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) malloc(length);
    ::glGetShaderSource((GLuint) shader->object(), length, &size, info);
    String s(info);
    free(info);
    return s;
}


float GraphicsContext3D::getTexParameterf(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLfloat data;
    ::glGetTexParameterfv(target, pname, &data);
    return data;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getTexParameterfv(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    RefPtr<CanvasFloatArray> array = CanvasFloatArray::create(1);
    GLfloat data;
    ::glGetTexParameterfv(target, pname, &data);
    array->set(0, static_cast<float>(data));
    
    return array;
}

int GraphicsContext3D::getTexParameteri(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLint data;
    ::glGetTexParameteriv(target, pname, &data);
    return data;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getTexParameteriv(unsigned long target, unsigned long pname)
{
    ensureContext(m_contextObj);
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(1);
    GLint data;
    ::glGetTexParameteriv(target, pname, &data);
    array->set(0, static_cast<int>(data));
    
    return array;
}

float GraphicsContext3D::getUniformf(CanvasProgram* program, long location)
{
    // FIXME: We need to query glGetUniformLocation to determine the size needed
    UNUSED_PARAM(program);
    UNUSED_PARAM(location);
    notImplemented();
    return 0;
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getUniformfv(CanvasProgram* program, long location)
{
    // FIXME: We need to query glGetUniformLocation to determine the size needed
    UNUSED_PARAM(program);
    UNUSED_PARAM(location);
    notImplemented();
    return 0;
}

int GraphicsContext3D::getUniformi(CanvasProgram* program, long location)
{
    // FIXME: We need to query glGetUniformLocation to determine the size needed
    UNUSED_PARAM(program);
    UNUSED_PARAM(location);
    notImplemented();
    return 0;
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getUniformiv(CanvasProgram* program, long location)
{
    // FIXME: We need to query glGetUniformLocation to determine the size needed
    UNUSED_PARAM(program);
    UNUSED_PARAM(location);
    notImplemented();
    return 0;
}

long GraphicsContext3D::getUniformLocation(CanvasProgram* program, const String& name)
{
    if (!program)
        return -1;
    
    ensureContext(m_contextObj);
    return ::glGetUniformLocation((GLuint) program->object(), name.utf8().data());
}

static int sizeForGetVertexAttribParam(unsigned long pname)
{
    switch(pname) {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:     return 1; // (* actually a CanvasBuffer*)
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:            return 1;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:               return 1;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:             return 1;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:               return 1;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:         return 1;
        case GL_CURRENT_VERTEX_ATTRIB:                  return 4;
    }
    
    return -1;
}

float GraphicsContext3D::getVertexAttribf(unsigned long index, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLfloat buf[4];
    ::glGetVertexAttribfv(index, pname, buf);
    return buf[0];
}

PassRefPtr<CanvasFloatArray> GraphicsContext3D::getVertexAttribfv(unsigned long index, unsigned long pname)
{
    int size = sizeForGetVertexAttribParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    RefPtr<CanvasFloatArray> array = CanvasFloatArray::create(size);
    GLfloat buf[4];
    ::glGetVertexAttribfv(index, pname, buf);
            
    for (int i = 0; i < size; ++i)
        array->set(i, static_cast<float>(buf[i]));
    
    return array;
}

int GraphicsContext3D::getVertexAttribi(unsigned long index, unsigned long pname)
{
    ensureContext(m_contextObj);
    GLint buf[4];
    ::glGetVertexAttribiv(index, pname, buf);
    return buf[0];
}

PassRefPtr<CanvasIntArray> GraphicsContext3D::getVertexAttribiv(unsigned long index, unsigned long pname)
{
    int size = sizeForGetVertexAttribParam(pname);
    if (size < 1) 
        return 0;
    
    ensureContext(m_contextObj);
    
    RefPtr<CanvasIntArray> array = CanvasIntArray::create(size);
    GLint buf[4];
    ::glGetVertexAttribiv(index, pname, buf);
            
    for (int i = 0; i < size; ++i)
        array->set(i, static_cast<int>(buf[i]));
    
    return array;
}

long GraphicsContext3D::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    ensureContext(m_contextObj);
    
    void* pointer;
    ::glGetVertexAttribPointerv(index, pname, &pointer);
    return reinterpret_cast<long>(pointer);
}

// Assumes the texture you want to go into is bound
static void imageToTexture(Image* image, unsigned target, unsigned level)
{
    if (!image)
        return;
    
    CGImageRef textureImage = image->getCGImageRef();
    if (!textureImage)
        return;
    
    size_t textureWidth = CGImageGetWidth(textureImage);
    size_t textureHeight = CGImageGetHeight(textureImage);
    
    GLubyte* textureData = (GLubyte*) malloc(textureWidth * textureHeight * 4);
    CGContextRef textureContext = CGBitmapContextCreate(textureData, textureWidth, textureHeight, 8, textureWidth * 4, 
                                                        CGImageGetColorSpace(textureImage), kCGImageAlphaPremultipliedLast);
    
    CGContextDrawImage(textureContext, CGRectMake(0, 0, (CGFloat)textureWidth, (CGFloat)textureHeight), textureImage);
    CGContextRelease(textureContext);
    
    ::glTexImage2D(target, level, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
    free(textureData);
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, CanvasArray* pixels)
{
    // FIXME: Need to do bounds checking on the buffer here.
    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels->baseAddress());
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, ImageData* pixels)
{
    // FIXME: need to implement this form
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(border);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pixels);
    return -1;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLImageElement* image, bool flipY, bool premultiplyAlpha)
{
    // FIXME: need to support flipY and premultiplyAlpha
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    
    if (!image)
        return -1;
    
    ensureContext(m_contextObj);
    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return -1;
    
    imageToTexture(cachedImage->image(), target, level);
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas, bool flipY, bool premultiplyAlpha)
{
    // FIXME: need to support flipY and premultiplyAlpha
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    
    if (!canvas)
        return -1;
    
    ensureContext(m_contextObj);
    ImageBuffer* buffer = canvas->buffer();
    if (!buffer)
        return -1;
    
    imageToTexture(buffer->image(), target, level);
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video, bool flipY, bool premultiplyAlpha)
{
    // FIXME: need to implement this form
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(video);

    // FIXME: need to support flipY and premultiplyAlpha
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, unsigned format, unsigned type, CanvasArray* pixels)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoff);
    UNUSED_PARAM(yoff);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pixels);
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, unsigned format, unsigned type, ImageData* pixels)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoff);
    UNUSED_PARAM(yoff);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pixels);
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLImageElement* image, bool flipY, bool premultiplyAlpha)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoff);
    UNUSED_PARAM(yoff);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(image);

    // FIXME: need to support flipY and premultiplyAlpha    
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLCanvasElement* canvas, bool flipY, bool premultiplyAlpha)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoff);
    UNUSED_PARAM(yoff);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(canvas);

    // FIXME: need to support flipY and premultiplyAlpha    
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    return -1;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, HTMLVideoElement* video, bool flipY, bool premultiplyAlpha)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoff);
    UNUSED_PARAM(yoff);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(video);

    // FIXME: need to support flipY and premultiplyAlpha    
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    return -1;
}

unsigned GraphicsContext3D::createBuffer()
{
    ensureContext(m_contextObj);
    GLuint o;
    glGenBuffers(1, &o);
    return o;
}

unsigned GraphicsContext3D::createFramebuffer()
{
    ensureContext(m_contextObj);
    GLuint o;
    glGenFramebuffersEXT(1, &o);
    return o;
}

unsigned GraphicsContext3D::createProgram()
{
    ensureContext(m_contextObj);
    return glCreateProgram();
}

unsigned GraphicsContext3D::createRenderbuffer()
{
    ensureContext(m_contextObj);
    GLuint o;
    glGenRenderbuffersEXT(1, &o);
    return o;
}

unsigned GraphicsContext3D::createShader(ShaderType type)
{
    ensureContext(m_contextObj);
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

unsigned GraphicsContext3D::createTexture()
{
    ensureContext(m_contextObj);
    GLuint o;
    glGenTextures(1, &o);
    return o;
}

void GraphicsContext3D::deleteBuffer(unsigned buffer)
{
    ensureContext(m_contextObj);
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(unsigned framebuffer)
{
    ensureContext(m_contextObj);
    glDeleteFramebuffersEXT(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(unsigned program)
{
    ensureContext(m_contextObj);
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(unsigned renderbuffer)
{
    ensureContext(m_contextObj);
    glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(unsigned shader)
{
    ensureContext(m_contextObj);
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(unsigned texture)
{
    ensureContext(m_contextObj);
    glDeleteTextures(1, &texture);
}

int GraphicsContext3D::sizeInBytes(int type)
{
    switch (type) {
        case GL_BYTE:
            return sizeof(GLbyte);
        case GL_UNSIGNED_BYTE:
            return sizeof(GLubyte);
        case GL_SHORT:
            return sizeof(GLshort);
        case GL_UNSIGNED_SHORT:
            return sizeof(GLushort);
        case GL_INT:
            return sizeof(GLint);
        case GL_UNSIGNED_INT:
            return sizeof(GLuint);
        case GL_FLOAT:
            return sizeof(GLfloat);
        default:
            return 0;
    }
}

}

#endif // ENABLE(3D_CANVAS)
