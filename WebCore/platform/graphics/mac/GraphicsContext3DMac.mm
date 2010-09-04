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

#import "BlockExceptions.h"

#include "ArrayBuffer.h"
#include "ArrayBufferView.h"
#include "WebGLObject.h"
#include "CanvasRenderingContext.h"
#include "Float32Array.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "ImageBuffer.h"
#include "Int32Array.h"
#include "NotImplemented.h"
#include "Uint8Array.h"
#include "WebGLLayer.h"
#include <CoreGraphics/CGBitmapContext.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/gl.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

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

PassOwnPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow)
{
    OwnPtr<GraphicsContext3D> context(new GraphicsContext3D(attrs, hostWindow));
    return context->m_contextObj ? context.release() : 0;
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow* hostWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_attrs(attrs)
    , m_contextObj(0)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_boundFBO(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
{
    UNUSED_PARAM(hostWindow);

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
    
    validateAttributes();

    // Create the WebGLLayer
    BEGIN_BLOCK_OBJC_EXCEPTIONS
        m_webGLLayer.adoptNS([[WebGLLayer alloc] initWithGraphicsContext3D:this]);
#ifndef NDEBUG
        [m_webGLLayer.get() setName:@"WebGL Layer"];
#endif    
    END_BLOCK_OBJC_EXCEPTIONS
    
    // create a texture to render into
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ::glBindTexture(GL_TEXTURE_2D, 0);
    
    // create an FBO
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    
    m_boundFBO = m_fbo;
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth))
        ::glGenRenderbuffersEXT(1, &m_depthStencilBuffer);

    // create an multisample FBO
    if (m_attrs.antialias) {
        ::glGenFramebuffersEXT(1, &m_multisampleFBO);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        m_boundFBO = m_multisampleFBO;
        ::glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
    }
    
    // ANGLE initialization.

    TBuiltInResource ANGLEResources;

    ANGLEResources.MaxVertexAttribs = 0;
    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    ANGLEResources.MaxVertexUniformVectors = 0;
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    ANGLEResources.MaxVaryingVectors = 0;
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    ANGLEResources.MaxVertexTextureImageUnits = 0;
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    ANGLEResources.MaxCombinedTextureImageUnits = 0;
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    ANGLEResources.MaxTextureImageUnits = 0;
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    ANGLEResources.MaxFragmentUniformVectors = 0;
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    
    m_compiler.setResources(ANGLEResources);
    
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    if (m_contextObj) {
        CGLSetCurrentContext(m_contextObj);
        ::glDeleteTextures(1, &m_texture);
        if (m_attrs.antialias) {
            ::glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attrs.stencil || m_attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
            ::glDeleteFramebuffersEXT(1, &m_multisampleFBO);
        } else {
            if (m_attrs.stencil || m_attrs.depth)
                ::glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
        }
        ::glDeleteFramebuffersEXT(1, &m_fbo);
        CGLSetCurrentContext(0);
        CGLDestroyContext(m_contextObj);
    }
}

void GraphicsContext3D::validateAttributes()
{
    const char* extensions = reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS));
    if (m_attrs.stencil) {
        if (std::strstr(extensions, "GL_EXT_packed_depth_stencil")) {
            if (!m_attrs.depth)
                m_attrs.depth = true;
        } else
            m_attrs.stencil = false;
    }
    if (m_attrs.antialias) {
        bool isValidVendor = true;
        // Currently in Mac we only turn on antialias if vendor is NVIDIA.
        const char* vendor = reinterpret_cast<const char*>(::glGetString(GL_VENDOR));
        if (!std::strstr(vendor, "NVIDIA"))
            isValidVendor = false;
        if (!isValidVendor || !std::strstr(extensions, "GL_EXT_framebuffer_multisample"))
            m_attrs.antialias = false;
    }
    // FIXME: instead of enforcing premultipliedAlpha = true, implement the
    // correct behavior when premultipliedAlpha = false is requested.
    m_attrs.premultipliedAlpha = true;
}

void GraphicsContext3D::makeContextCurrent()
{
    CGLSetCurrentContext(m_contextObj);
}

void GraphicsContext3D::paintRenderingResultsToCanvas(CanvasRenderingContext* context)
{
    HTMLCanvasElement* canvas = context->canvas();
    ImageBuffer* imageBuffer = canvas->buffer();

    int rowBytes = m_currentWidth * 4;
    int totalBytes = rowBytes * m_currentHeight;

    OwnArrayPtr<unsigned char> pixels(new unsigned char[totalBytes]);
    if (!pixels)
        return;

    CGLSetCurrentContext(m_contextObj);

    bool mustRestoreFBO = false;
    if (m_attrs.antialias) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        mustRestoreFBO = true;
    } else {
        if (m_boundFBO != m_fbo) {
            mustRestoreFBO = true;
            ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        }
    }

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    ::glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        ::glPixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

    ::glReadPixels(0, 0, m_currentWidth, m_currentHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.get());

    if (mustRestorePackAlignment)
        ::glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);

    paintToCanvas(pixels.get(), m_currentWidth, m_currentHeight,
                  canvas->width(), canvas->height(), imageBuffer->context()->platformContext());
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return false;
}

bool GraphicsContext3D::isGLES2NPOTStrict() const
{
    return false;
}

bool GraphicsContext3D::isErrorGeneratedOnOutOfBoundsAccesses() const
{
    return false;
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight || !m_contextObj)
        return;
    
    m_currentWidth = width;
    m_currentHeight = height;
    
    CGLSetCurrentContext(m_contextObj);
    
    GLuint internalColorFormat, colorFormat, internalDepthStencilFormat = 0;
    if (m_attrs.alpha) {
        internalColorFormat = GL_RGBA8;
        colorFormat = GL_RGBA;
    } else {
        internalColorFormat = GL_RGB8;
        colorFormat = GL_RGB;
    }
    if (m_attrs.stencil || m_attrs.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3D constructor.
        if (m_attrs.stencil && m_attrs.depth)
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
    }

    bool mustRestoreFBO = false;

    // resize multisample FBO
    if (m_attrs.antialias) {
        GLint maxSampleCount;
        ::glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSampleCount);
        GLint sampleCount = std::min(8, maxSampleCount);
        if (sampleCount > maxSampleCount)
            sampleCount = maxSampleCount;
        if (m_boundFBO != m_multisampleFBO) {
            ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
            mustRestoreFBO = true;
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalColorFormat, width, height);
        ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth) {
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, width, height);
            if (m_attrs.stencil)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            if (m_attrs.depth)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    if (m_boundFBO != m_fbo) {
        mustRestoreFBO = true;
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    ::glBindTexture(GL_TEXTURE_2D, 0);
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth)) {
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, width, height);
        if (m_attrs.stencil)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        if (m_attrs.depth)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    }
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        // FIXME: cleanup
        notImplemented();
    }

    if (m_attrs.antialias) {
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        if (m_boundFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    }

    // Initialize renderbuffers to 0.
    GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, depthMask = GL_TRUE, stencilMask = GL_TRUE;
    GLboolean isScissorEnabled = GL_FALSE;
    GLboolean isDitherEnabled = GL_FALSE;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    ::glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    ::glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_attrs.depth) {
        ::glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        ::glDepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_attrs.stencil) {
        ::glGetBooleanv(GL_STENCIL_WRITEMASK, &stencilMask);
        ::glStencilMask(GL_TRUE);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }
    isScissorEnabled = ::glIsEnabled(GL_SCISSOR_TEST);
    ::glDisable(GL_SCISSOR_TEST);
    isDitherEnabled = ::glIsEnabled(GL_DITHER);
    ::glDisable(GL_DITHER);

    ::glClear(clearMask);

    ::glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (m_attrs.depth)
        ::glDepthMask(depthMask);
    if (m_attrs.stencil)
        ::glStencilMask(stencilMask);
    if (isScissorEnabled)
        ::glEnable(GL_SCISSOR_TEST);
    else
        ::glDisable(GL_SCISSOR_TEST);
    if (isDitherEnabled)
        ::glEnable(GL_DITHER);
    else
        ::glDisable(GL_DITHER);

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);

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

void GraphicsContext3D::prepareTexture()
{
    ensureContext(m_contextObj);
    if (m_attrs.antialias) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
    }
    ::glFinish();
}

void GraphicsContext3D::activeTexture(unsigned long texture)
{
    ensureContext(m_contextObj);
    ::glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    ensureContext(m_contextObj);
    ::glAttachShader((GLuint) program, (GLuint) shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, unsigned long index, const String& name)
{
    ASSERT(program);
    ensureContext(m_contextObj);
    ::glBindAttribLocation((GLuint) program, index, name.utf8().data());
}

void GraphicsContext3D::bindBuffer(unsigned long target, Platform3DObject buffer)
{
    ensureContext(m_contextObj);
    ::glBindBuffer(target, (GLuint) buffer);
}


void GraphicsContext3D::bindFramebuffer(unsigned long target, Platform3DObject buffer)
{
    ensureContext(m_contextObj);
    GLuint fbo;
    if (buffer)
        fbo = (GLuint)buffer;
    else
        fbo = (m_attrs.antialias ? m_multisampleFBO : m_fbo);
    if (fbo != m_boundFBO) {
        ::glBindFramebufferEXT(target, fbo);
        m_boundFBO = fbo;
    }
}

void GraphicsContext3D::bindRenderbuffer(unsigned long target, Platform3DObject renderbuffer)
{
    ensureContext(m_contextObj);
    ::glBindRenderbufferEXT(target, (GLuint) renderbuffer);
}


void GraphicsContext3D::bindTexture(unsigned long target, Platform3DObject texture)
{
    ensureContext(m_contextObj);
    ::glBindTexture(target, (GLuint) texture);
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

void GraphicsContext3D::bufferData(unsigned long target, int size, const void* data, unsigned long usage)
{
    ensureContext(m_contextObj);
    ::glBufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(unsigned long target, long offset, int size, const void* data)
{
    ensureContext(m_contextObj);
    ::glBufferSubData(target, offset, size, data);
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

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    ASSERT(shader);
    ensureContext(m_contextObj);

    int GLshaderType;
    ANGLEShaderType shaderType;

    glGetShaderiv(shader, SHADER_TYPE, &GLshaderType);
    
    if (GLshaderType == VERTEX_SHADER)
        shaderType = SHADER_TYPE_VERTEX;
    else if (GLshaderType == FRAGMENT_SHADER)
        shaderType = SHADER_TYPE_FRAGMENT;
    else
        return; // Invalid shader type.

    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end())
        return;

    ShaderSourceEntry& entry = result->second;

    String translatedShaderSource;
    String shaderInfoLog;

    bool isValid = m_compiler.validateShaderSource(entry.source.utf8().data(), shaderType, translatedShaderSource, shaderInfoLog);

    entry.log = shaderInfoLog;
    entry.isValid = isValid;

    if (!isValid)
        return; // Shader didn't validate, don't move forward with compiling translated source    

    int translatedShaderLength = translatedShaderSource.length();

    const CString& translatedShaderCString = translatedShaderSource.utf8();
    const char* translatedShaderPtr = translatedShaderCString.data();
    
    ::glShaderSource((GLuint) shader, 1, &translatedShaderPtr, &translatedShaderLength);
    
    ::glCompileShader((GLuint) shader);
    
    int GLCompileSuccess;
    
    ::glGetShaderiv((GLuint) shader, COMPILE_STATUS, &GLCompileSuccess);
    
    // ASSERT that ANGLE generated GLSL will be accepted by OpenGL
    ASSERT(GLCompileSuccess == GL_TRUE);
}

void GraphicsContext3D::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    ensureContext(m_contextObj);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
    ::glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
}

void GraphicsContext3D::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    ensureContext(m_contextObj);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
    ::glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
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

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    ensureContext(m_contextObj);
    ::glDetachShader((GLuint) program, (GLuint) shader);
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

void GraphicsContext3D::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, Platform3DObject buffer)
{
    ensureContext(m_contextObj);
    GLuint renderbuffer = (GLuint) buffer;
    if (attachment == DEPTH_STENCIL_ATTACHMENT) {
        ::glFramebufferRenderbufferEXT(target, DEPTH_ATTACHMENT, renderbuffertarget, renderbuffer);
        ::glFramebufferRenderbufferEXT(target, STENCIL_ATTACHMENT, renderbuffertarget, renderbuffer);
    } else
        ::glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, renderbuffer);
}

void GraphicsContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, Platform3DObject texture, long level)
{
    ensureContext(m_contextObj);
    ::glFramebufferTexture2DEXT(target, attachment, textarget, (GLuint) texture, level);
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

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, unsigned long index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    ensureContext(m_contextObj);
    GLint maxAttributeSize = 0;
    ::glGetProgramiv(static_cast<GLuint>(program), GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    GLchar name[maxAttributeSize]; // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveAttrib(static_cast<GLuint>(program), index, maxAttributeSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}
    
bool GraphicsContext3D::getActiveUniform(Platform3DObject program, unsigned long index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    ensureContext(m_contextObj);
    GLint maxUniformSize = 0;
    ::glGetProgramiv(static_cast<GLuint>(program), GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    GLchar name[maxUniformSize]; // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveUniform(static_cast<GLuint>(program), index, maxUniformSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, int maxCount, int* count, unsigned int* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    ensureContext(m_contextObj);
    ::glGetAttachedShaders(static_cast<GLuint>(program), maxCount, count, shaders);
}

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;
    
    ensureContext(m_contextObj);
    return ::glGetAttribLocation((GLuint) program, name.utf8().data());
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

bool GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    if (!buffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsBuffer((GLuint) buffer);
}

bool GraphicsContext3D::isEnabled(unsigned long cap)
{
    ensureContext(m_contextObj);
    return ::glIsEnabled(cap);
}

bool GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    if (!framebuffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsFramebufferEXT((GLuint) framebuffer);
}

bool GraphicsContext3D::isProgram(Platform3DObject program)
{
    if (!program)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsProgram((GLuint) program);
}

bool GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    if (!renderbuffer)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsRenderbufferEXT((GLuint) renderbuffer);
}

bool GraphicsContext3D::isShader(Platform3DObject shader)
{
    if (!shader)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsShader((GLuint) shader);
}

bool GraphicsContext3D::isTexture(Platform3DObject texture)
{
    if (!texture)
        return false;
    
    ensureContext(m_contextObj);
    return ::glIsTexture((GLuint) texture);
}

void GraphicsContext3D::lineWidth(double width)
{
    ensureContext(m_contextObj);
    ::glLineWidth(static_cast<float>(width));
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    ASSERT(program);
    ensureContext(m_contextObj);
    ::glLinkProgram((GLuint) program);
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

void GraphicsContext3D::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* data)
{
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    ensureContext(m_contextObj);
    ::glFlush();
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        ::glFlush();
    }
    ::glReadPixels(x, y, width, height, format, type, data);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
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
    switch (internalformat) {
    case DEPTH_STENCIL:
        internalformat = GL_DEPTH24_STENCIL8_EXT;
        break;
    case DEPTH_COMPONENT16:
        internalformat = GL_DEPTH_COMPONENT;
        break;
    case RGBA4:
    case RGB5_A1:
        internalformat = GL_RGBA;
        break;
    case RGB565:
        internalformat = GL_RGB;
        break;
    }
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

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& string)
{
    ASSERT(shader);
    
    ensureContext(m_contextObj);

    ShaderSourceEntry entry;

    entry.source = string;

    m_shaderSourceMap.set(shader, entry);
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

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    ensureContext(m_contextObj);
    ::glUseProgram((GLuint) program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    ASSERT(program);
    
    ensureContext(m_contextObj);
    ::glValidateProgram((GLuint) program);
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
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void GraphicsContext3D::getIntegerv(unsigned long pname, int* value)
{
    // Need to emulate IMPLEMENTATION_COLOR_READ_FORMAT/TYPE for GL.  Any valid
    // combination should work, but GL_RGB/GL_UNSIGNED_BYTE might be the most
    // useful for desktop WebGL users.
    // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and MAX_VARYING_VECTORS
    // because desktop GL's corresponding queries return the number of components
    // whereas GLES2 return the number of vectors (each vector has 4 components).
    // Therefore, the value returned by desktop GL needs to be divided by 4.
    ensureContext(m_contextObj);
    switch (pname) {
    case IMPLEMENTATION_COLOR_READ_FORMAT:
        *value = GL_RGB;
        break;
    case IMPLEMENTATION_COLOR_READ_TYPE:
        *value = GL_UNSIGNED_BYTE;
        break;
    case MAX_FRAGMENT_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VERTEX_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VARYING_VECTORS:
        ::glGetIntegerv(GL_MAX_VARYING_FLOATS, value);
        *value /= 4;
        break;
    default:
        ::glGetIntegerv(pname, value);
    }
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetProgramiv((GLuint) program, pname, value);
}

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
{
    ASSERT(program);
    
    ensureContext(m_contextObj);
    GLint length;
    ::glGetProgramiv((GLuint) program, GL_INFO_LOG_LENGTH, &length);
    
    GLsizei size;
    GLchar* info = (GLchar*) fastMalloc(length);
    if (!info)
        return "";

    ::glGetProgramInfoLog((GLuint) program, length, &size, info);
    String s(info);
    fastFree(info);
    return s;
}

void GraphicsContext3D::getRenderbufferParameteriv(unsigned long target, unsigned long pname, int* value)
{
    ensureContext(m_contextObj);
    ::glGetRenderbufferParameterivEXT(target, pname, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, unsigned long pname, int* value)
{
    ASSERT(shader);
    
    ensureContext(m_contextObj);
    
    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);
    
    switch (pname) {
        case DELETE_STATUS:
        case SHADER_TYPE:
            // Let OpenGL handle these.
        
            ::glGetShaderiv((GLuint) shader, pname, value);
            break;

        case COMPILE_STATUS:
            if (result == m_shaderSourceMap.end()) {
                (*value) = static_cast<int>(false);
                return;
            }
                
            (*value) = static_cast<int>(result->second.isValid);
            break;
            
        case INFO_LOG_LENGTH:
            if (result == m_shaderSourceMap.end()) {
                (*value) = 0;
                return;
            }
            
            (*value) = getShaderInfoLog(shader).length();
            break;
            
        case SHADER_SOURCE_LENGTH:
            (*value) = getShaderSource(shader).length();
            break;
        
        default:
            synthesizeGLError(INVALID_ENUM);
    }
}

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
{
    ASSERT(shader);

    ensureContext(m_contextObj);
    GLint length;
    ::glGetShaderiv((GLuint) shader, GL_INFO_LOG_LENGTH, &length);

    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end())
         return "";

     ShaderSourceEntry entry = result->second;

     if (entry.isValid) {
         GLint length;
         ::glGetShaderiv((GLuint) shader, GL_INFO_LOG_LENGTH, &length);

         GLsizei size;
         GLchar* info = (GLchar*) fastMalloc(length);
         if (!info)
             return "";

         ::glGetShaderInfoLog((GLuint) shader, length, &size, info);

         String s(info);
         fastFree(info);
         return s;
     }
     else {
         return entry.log;
     }
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    ASSERT(shader);

    ensureContext(m_contextObj);

    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end())
        return "";

    return result->second.source;
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

void GraphicsContext3D::getUniformfv(Platform3DObject program, long location, float* value)
{
    ensureContext(m_contextObj);
    ::glGetUniformfv((GLuint) program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, long location, int* value)
{
    ensureContext(m_contextObj);
    ::glGetUniformiv((GLuint) program, location, value);
}

long GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    ASSERT(program);
    
    ensureContext(m_contextObj);
    return ::glGetUniformLocation((GLuint) program, name.utf8().data());
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

int GraphicsContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels)
{
    ensureContext(m_contextObj);

    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return 0;
}

int GraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoff, unsigned yoff, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels)
{
    ensureContext(m_contextObj);

    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size
    ::glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
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
