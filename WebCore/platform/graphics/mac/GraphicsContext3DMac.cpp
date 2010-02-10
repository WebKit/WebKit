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

#include "CanvasObject.h"
#include "CString.h"
#include "ImageBuffer.h"
#include "NotImplemented.h"
#include "WebGLActiveInfo.h"
#include "WebGLArray.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLFloatArray.h"
#include "WebGLIntArray.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLUnsignedByteArray.h"
#include <CoreGraphics/CGBitmapContext.h>
#include <OpenGL/CGLRenderers.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

static void setPixelFormat(Vector<CGLPixelFormatAttribute>& attribs, int colorBits, int depthBits, bool accelerated, bool supersample, bool closest)
{
    attribs.clear();
    
    attribs.append(kCGLPFAColorSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(colorBits));
    attribs.append(kCGLPFADepthSize);
    attribs.append(static_cast<CGLPixelFormatAttribute>(depthBits));
    
    if (accelerated)
        attribs.append(kCGLPFAAccelerated);
    else {
        attribs.append(kCGLPFARendererID);
        attribs.append(static_cast<CGLPixelFormatAttribute>(kCGLRendererGenericFloatID));
    }
        
    if (supersample)
        attribs.append(kCGLPFASupersample);
        
    if (closest)
        attribs.append(kCGLPFAClosestPolicy);
        
    attribs.append(static_cast<CGLPixelFormatAttribute>(0));
}

PassOwnPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs)
{
    OwnPtr<GraphicsContext3D> context(new GraphicsContext3D(attrs));
    return context->m_contextObj ? context.release() : 0;
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs)
    : m_attrs(attrs)
    , m_contextObj(0)
    , m_texture(0)
    , m_fbo(0)
    , m_depthBuffer(0)
{
    // FIXME: we need to take into account the user's requested
    // context creation attributes, in particular stencil and
    // antialias, and determine which could and could not be honored
    // based on the capabilities of the OpenGL implementation.
    m_attrs.alpha = true;
    m_attrs.depth = true;
    m_attrs.stencil = false;
    m_attrs.antialias = false;
    m_attrs.premultipliedAlpha = true;

    Vector<CGLPixelFormatAttribute> attribs;
    CGLPixelFormatObj pixelFormatObj = 0;
    GLint numPixelFormats = 0;
    
    // We will try:
    //
    //  1) 32 bit RGBA/32 bit depth/accelerated/supersampled
    //  2) 32 bit RGBA/32 bit depth/accelerated
    //  3) 32 bit RGBA/16 bit depth/accelerated
    //  4) closest to 32 bit RGBA/16 bit depth/software renderer
    //
    //  If none of that works, we simply fail and set m_contextObj to 0.
    
    setPixelFormat(attribs, 32, 32, true, true, false);
    CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
    if (numPixelFormats == 0) {
        setPixelFormat(attribs, 32, 32, true, false, false);
        CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
        
        if (numPixelFormats == 0) {
            setPixelFormat(attribs, 32, 16, true, false, false);
            CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
        
            if (numPixelFormats == 0) {
                setPixelFormat(attribs, 32, 16, false, false, true);
                CGLChoosePixelFormat(attribs.data(), &pixelFormatObj, &numPixelFormats);
        
                if (numPixelFormats == 0) {
                    // Could not find an acceptable renderer - fail
                    return;
                }
            }
        }
    }
    
    CGLError err = CGLCreateContext(pixelFormatObj, 0, &m_contextObj);
    CGLDestroyPixelFormat(pixelFormatObj);
    
    if (err != kCGLNoError || !m_contextObj) {
        // Could not create the context - fail
        m_contextObj = 0;
        return;
    }

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
    if (m_contextObj) {
        CGLSetCurrentContext(m_contextObj);
        ::glDeleteRenderbuffersEXT(1, & m_depthBuffer);
        ::glDeleteTextures(1, &m_texture);
        ::glDeleteFramebuffersEXT(1, &m_fbo);
        CGLSetCurrentContext(0);
        CGLDestroyContext(m_contextObj);
    }
}

void GraphicsContext3D::makeContextCurrent()
{
    CGLSetCurrentContext(m_contextObj);
}

void GraphicsContext3D::beginPaint(WebGLRenderingContext* context)
{
    UNUSED_PARAM(context);
}

void GraphicsContext3D::endPaint()
{
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight || !m_contextObj)
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

    ::glClear(GL_COLOR_BUFFER_BIT);
    ::glFlush();
}

static inline void ensureContext(CGLContextObj context)
{
    if (!context)
        return;
        
    CGLContextObj currentContext = CGLGetCurrentContext();
    if (currentContext != context)
        CGLSetCurrentContext(context);
}

void GraphicsContext3D::activeTexture(unsigned long texture)
{
    ensureContext(m_contextObj);
    ::glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(WebGLProgram* program, WebGLShader* shader)
{
    ASSERT(program);
    ASSERT(shader);
    ensureContext(m_contextObj);
    ::glAttachShader((GLuint) program->object(), (GLuint) shader->object());
}

void GraphicsContext3D::bindAttribLocation(WebGLProgram* program, unsigned long index, const String& name)
{
    ASSERT(program);
    ensureContext(m_contextObj);
    ::glBindAttribLocation((GLuint) program->object(), index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(unsigned long target, WebGLBuffer* buffer)
{
    ensureContext(m_contextObj);
    ::glBindBuffer(target, buffer ? (GLuint) buffer->object() : 0);
}


void GraphicsContext3D::bindFramebuffer(unsigned long target, WebGLFramebuffer* buffer)
{
    ensureContext(m_contextObj);
    ::glBindFramebufferEXT(target, (buffer && buffer->object()) ? (GLuint) buffer->object() : m_fbo);
}

void GraphicsContext3D::bindRenderbuffer(unsigned long target, WebGLRenderbuffer* renderbuffer)
{
    ensureContext(m_contextObj);
    ::glBindRenderbufferEXT(target, renderbuffer ? (GLuint) renderbuffer->object() : 0);
}


void GraphicsContext3D::bindTexture(unsigned long target, WebGLTexture* texture)
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
void GraphicsContext3D::bufferData(unsigned long target, WebGLArray* array, unsigned long usage)
{
    if (!array || !array->length())
        return;
    
    ensureContext(m_contextObj);
    ::glBufferData(target, array->byteLength(), array->baseAddress(), usage);
}

void GraphicsContext3D::bufferSubData(unsigned long target, long offset, WebGLArray* array)
{
    if (!array || !array->length())
        return;
    
    ensureContext(m_contextObj);
    ::glBufferSubData(target, offset, array->byteLength(), array->baseAddress());
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

void GraphicsContext3D::compileShader(WebGLShader* shader)
{
    ASSERT(shader);
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

void GraphicsContext3D::detachShader(WebGLProgram* program, WebGLShader* shader)
{
    ASSERT(program);
    ASSERT(shader);
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

void GraphicsContext3D::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLRenderbuffer* buffer)
{
    ensureContext(m_contextObj);
    ::glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, buffer ? (GLuint) buffer->object() : 0);
}

void GraphicsContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLTexture* texture, long level)
{
    ensureContext(m_contextObj);
    ::glFramebufferTexture2DEXT(target, attachment, textarget, texture ? (GLuint) texture->object() : 0, level);
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

bool GraphicsContext3D::getActiveAttrib(WebGLProgram* program, unsigned long index, ActiveInfo& info)
{
    if (!program->object()) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
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
    
bool GraphicsContext3D::getActiveUniform(WebGLProgram* program, unsigned long index, ActiveInfo& info)
{
    if (!program->object()) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
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

int GraphicsContext3D::getAttribLocation(WebGLProgram* program, const String& name)
{
    if (!program)
        return -1;
    
    ensureContext(m_contextObj);
    return ::glGetAttribLocation((GLuint) program->object(), name.utf8().data());
}

GraphicsContext3D::Attributes GraphicsContext3D::getContextAttributes()
{
    return m_attrs;
}

unsigned long GraphicsContext3D::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_syntheticErrors.begin();
        unsigned long err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }

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

bool GraphicsContext3D::isBuffer(WebGLBuffer* buffer)
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

bool GraphicsContext3D::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsFramebufferEXT((GLuint) framebuffer->object());
}

bool GraphicsContext3D::isProgram(WebGLProgram* program)
{
    if (!program)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsProgram((GLuint) program->object());
}

bool GraphicsContext3D::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsRenderbufferEXT((GLuint) renderbuffer->object());
}

bool GraphicsContext3D::isShader(WebGLShader* shader)
{
    if (!shader)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsShader((GLuint) shader->object());
}

bool GraphicsContext3D::isTexture(WebGLTexture* texture)
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

void GraphicsContext3D::linkProgram(WebGLProgram* program)
{
    ASSERT(program);
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

PassRefPtr<WebGLArray> GraphicsContext3D::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type)
{
    ensureContext(m_contextObj);
    
    // FIXME: For now we only accept GL_UNSIGNED_BYTE/GL_RGBA. In reality OpenGL ES 2.0 accepts that pair and one other
    // as specified by GL_IMPLEMENTATION_COLOR_READ_FORMAT and GL_IMPLEMENTATION_COLOR_READ_TYPE. But for now we will
    // not accept those.
    // FIXME: Also, we should throw when an unacceptable value is passed
    if (type != GL_UNSIGNED_BYTE || format != GL_RGBA)
        return 0;
        
    RefPtr<WebGLUnsignedByteArray> array = WebGLUnsignedByteArray::create(width * height * 4);
    ::glReadPixels(x, y, width, height, format, type, (GLvoid*) array->data());
    return array;    
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

void GraphicsContext3D::shaderSource(WebGLShader* shader, const String& string)
{
    ASSERT(shader);
    
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

void GraphicsContext3D::useProgram(WebGLProgram* program)
{
    ASSERT(program);
    
    ensureContext(m_contextObj);
    ::glUseProgram((GLuint) program->object());
}

void GraphicsContext3D::validateProgram(WebGLProgram* program)
{
    ASSERT(program);
    
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

void GraphicsContext3D::getBooleanv(unsigned long pname, unsigned char* value)
{
    ensureContext(m_contextObj);
    ::glGetBooleanv(pname, value);
}

void GraphicsContext3D::getBufferParameteriv(unsigned long target, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetBufferParameteriv(target, pname, value);
}

void GraphicsContext3D::getFloatv(unsigned long pname, float* value)
{
    ensureContext(m_contextObj);
    ::glGetFloatv(pname, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void GraphicsContext3D::getIntegerv(unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetIntegerv(pname, value);
}

void GraphicsContext3D::getProgramiv(WebGLProgram* program, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetProgramiv((GLuint) program->object(), pname, value);
}

String GraphicsContext3D::getProgramInfoLog(WebGLProgram* program)
{
    ASSERT(program);
    
    ensureContext(m_contextObj);
    GLint length;
    ::glGetProgramiv((GLuint) program->object(), GL_INFO_LOG_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    ::glGetProgramInfoLog((GLuint) program->object(), length, &size, info);
    String s(info);
    fastFree(info);
    return s;
}

void GraphicsContext3D::getRenderbufferParameteriv(unsigned long target, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetRenderbufferParameterivEXT(target, pname, value);
}

void GraphicsContext3D::getShaderiv(WebGLShader* shader, unsigned long pname, int* value)
{
    ASSERT(shader);
    
    ensureContext(m_contextObj);
    ::glGetShaderiv((GLuint) shader->object(), pname, value);
}

String GraphicsContext3D::getShaderInfoLog(WebGLShader* shader)
{
    ASSERT(shader);
    
    ensureContext(m_contextObj);
    GLint length;
    ::glGetShaderiv((GLuint) shader->object(), GL_INFO_LOG_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";
        
    ::glGetShaderInfoLog((GLuint) shader->object(), length, &size, info);
    String s(info);
    fastFree(info);
    return s;
}

String GraphicsContext3D::getShaderSource(WebGLShader* shader)
{
    ASSERT(shader);

    ensureContext(m_contextObj);
    GLint length;
    ::glGetShaderiv((GLuint) shader->object(), GL_SHADER_SOURCE_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";
        
    ::glGetShaderSource((GLuint) shader->object(), length, &size, info);
    String s(info);
    fastFree(info);
    return s;
}


void GraphicsContext3D::getTexParameterfv(unsigned long target, unsigned long pname, float* value)
{
    ensureContext(m_contextObj);
    ::glGetTexParameterfv(target, pname, value);
}

void GraphicsContext3D::getTexParameteriv(unsigned long target, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetTexParameteriv(target, pname, value);
}

void GraphicsContext3D::getUniformfv(WebGLProgram* program, long location, float* value)
{
    ensureContext(m_contextObj);
    ::glGetUniformfv((GLuint) program->object(), location, value);
}

void GraphicsContext3D::getUniformiv(WebGLProgram* program, long location, int* value)
{
    ensureContext(m_contextObj);
    ::glGetUniformiv((GLuint) program->object(), location, value);
}

long GraphicsContext3D::getUniformLocation(WebGLProgram* program, const String& name)
{
    ASSERT(program);
    
    ensureContext(m_contextObj);
    return ::glGetUniformLocation((GLuint) program->object(), name.utf8().data());
}

void GraphicsContext3D::getVertexAttribfv(unsigned long index, unsigned long pname, float* value)
{
    ensureContext(m_contextObj);
    ::glGetVertexAttribfv(index, pname, value);
}

void GraphicsContext3D::getVertexAttribiv(unsigned long index, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetVertexAttribiv(index, pname, value);
}

long GraphicsContext3D::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    ensureContext(m_contextObj);
    
    void* pointer;
    ::glGetVertexAttribPointerv(index, pname, &pointer);
    return reinterpret_cast<long>(pointer);
}

// Returned pointer must be freed by fastFree()
static bool imageToTexture(Image* image, GLubyte*& buffer, size_t& width, size_t& height)
{
    if (!image)
        return false;
    
    CGImageRef textureImage = image->getCGImageRef();
    if (!textureImage)
        return false;
    
    width = CGImageGetWidth(textureImage);
    height = CGImageGetHeight(textureImage);
    
    buffer = (GLubyte*) fastMalloc(width * height * 4);
    if (!buffer)
        return false;
        
    CGContextRef textureContext = CGBitmapContextCreate(buffer, width, height, 8, width * 4, 
                                                        CGImageGetColorSpace(textureImage), kCGImageAlphaPremultipliedLast);
    CGContextSetBlendMode(textureContext, kCGBlendModeCopy);
    CGContextDrawImage(textureContext, CGRectMake(0, 0, (CGFloat)width, (CGFloat)height), textureImage);
    CGContextRelease(textureContext);
    return true;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels)
{
    // FIXME: Need to do bounds checking on the buffer here.
    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return 0;
}

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, Image* image, bool flipY, bool premultiplyAlpha)
{
    // FIXME: need to support flipY and premultiplyAlpha
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    ASSERT(image);
    
    ensureContext(m_contextObj);
    GLubyte* buffer;
    size_t width;
    size_t height;
    if (!imageToTexture(image, buffer, width, height))
        return -1;
    
    ::glTexImage2D(target, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    fastFree(buffer);
    return 0;
}
    
int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    // FIXME: Need to do bounds checking on the buffer here.
    ::glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
    return 0;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, Image* image, bool flipY, bool premultiplyAlpha)
{
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    // FIXME: need to support flipY and premultiplyAlpha
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    ASSERT(image);
    
    ensureContext(m_contextObj);
    GLubyte* buffer;
    size_t width;
    size_t height;
    if (!imageToTexture(image, buffer, width, height))
        return -1;
    
    ::glTexSubImage2D(target, level, xoff, yoff, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    fastFree(buffer);
    return 0;
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

unsigned GraphicsContext3D::createShader(unsigned long type)
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

void GraphicsContext3D::synthesizeGLError(unsigned long error)
{
    m_syntheticErrors.add(error);
}

}

#endif // ENABLE(3D_CANVAS)
