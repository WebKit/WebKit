/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"
#include "Extensions3DOpenGLES.h"
#include "IntRect.h"
#include "IntSize.h"
#include "LayerWebKitThread.h"
#include "NotImplemented.h"
#include "OpenGLESShims.h"

namespace WebCore {

void GraphicsContext3D::releaseShaderCompiler()
{
    makeContextCurrent();
    ::glReleaseShaderCompiler();
}

void GraphicsContext3D::readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels)
{
    ::glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    int totalBytes = width * height * 4;
    if (isGLES2Compliant()) {
        for (int i = 0; i < totalBytes; i += 4)
            std::swap(pixels[i], pixels[i + 2]); // Convert to BGRA.
    }
}

void GraphicsContext3D::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    // FIXME: We don't support antialiasing yet.
    notImplemented();
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    ::glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetIntegerv(pname, value);
}

void GraphicsContext3D::getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision)
{
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();
    ::glGetShaderPrecisionFormat(shaderType, precisionType, range, precision);
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::validateAttributes()
{
    validateDepthStencil("GL_OES_packed_depth_stencil");

    if (m_attrs.antialias) {
        Extensions3D* extensions = getExtensions();
        if (!extensions->supports("GL_IMG_multisampled_render_to_texture"))
            m_attrs.antialias = false;
    }
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
    ::glDepthRangef(zNear, zFar);
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
    ::glClearDepthf(depth);
}


Extensions3D* GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DOpenGLES(this));
    return m_extensions.get();
}

bool GraphicsContext3D::systemAllowsMultisamplingOnATICards() const
{
    return false; // not applicable
}

}

#endif // ENABLE(WEBGL)
