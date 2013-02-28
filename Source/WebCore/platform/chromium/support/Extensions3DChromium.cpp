/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Extensions3DChromium.h"

#include "GraphicsContext3D.h"
#include "GraphicsContext3DPrivate.h"
#include "NotImplemented.h"
#include <public/WebGraphicsContext3D.h>
#include <wtf/text/CString.h>

namespace WebCore {

Extensions3DChromium::Extensions3DChromium(GraphicsContext3DPrivate* priv)
    : m_private(priv)
{
}

Extensions3DChromium::~Extensions3DChromium()
{
}

bool Extensions3DChromium::supports(const String& name)
{
    return m_private->supportsExtension(name);
}

void Extensions3DChromium::ensureEnabled(const String& name)
{
    bool result = m_private->ensureExtensionEnabled(name);
    ASSERT_UNUSED(result, result);
}

bool Extensions3DChromium::isEnabled(const String& name)
{
    return m_private->isExtensionEnabled(name);
}

int Extensions3DChromium::getGraphicsResetStatusARB()
{
    return static_cast<int>(m_private->webContext()->getGraphicsResetStatusARB());
}

void Extensions3DChromium::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter)
{
    m_private->webContext()->blitFramebufferCHROMIUM(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void Extensions3DChromium::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    m_private->webContext()->renderbufferStorageMultisampleCHROMIUM(target, samples, internalformat, width, height);
}

void* Extensions3DChromium::mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access)
{
    return m_private->webContext()->mapBufferSubDataCHROMIUM(target, offset, size, access);
}

void Extensions3DChromium::unmapBufferSubDataCHROMIUM(const void* data)
{
    m_private->webContext()->unmapBufferSubDataCHROMIUM(data);
}

void* Extensions3DChromium::mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access)
{
    return m_private->webContext()->mapTexSubImage2DCHROMIUM(target, level, xoffset, yoffset, width, height, format, type, access);
}

void Extensions3DChromium::unmapTexSubImage2DCHROMIUM(const void* data)
{
    m_private->webContext()->unmapTexSubImage2DCHROMIUM(data);
}

Platform3DObject Extensions3DChromium::createVertexArrayOES()
{
    return m_private->webContext()->createVertexArrayOES();
}

void Extensions3DChromium::deleteVertexArrayOES(Platform3DObject array)
{
    m_private->webContext()->deleteVertexArrayOES(array);
}

GC3Dboolean Extensions3DChromium::isVertexArrayOES(Platform3DObject array)
{
    return m_private->webContext()->isVertexArrayOES(array);
}

void Extensions3DChromium::bindVertexArrayOES(Platform3DObject array)
{
    m_private->webContext()->bindVertexArrayOES(array);
}

String Extensions3DChromium::getTranslatedShaderSourceANGLE(Platform3DObject shader)
{
    return m_private->webContext()->getTranslatedShaderSourceANGLE(shader);
}

void Extensions3DChromium::rateLimitOffscreenContextCHROMIUM()
{
    m_private->webContext()->rateLimitOffscreenContextCHROMIUM();
}

void Extensions3DChromium::paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer* imageBuffer)
{
    m_private->paintFramebufferToCanvas(framebuffer, width, height, premultiplyAlpha, imageBuffer);
}

void Extensions3DChromium::texImageIOSurface2DCHROMIUM(unsigned target, int width, int height, uint32_t ioSurfaceId, unsigned plane)
{
    m_private->webContext()->texImageIOSurface2DCHROMIUM(target, width, height, ioSurfaceId, plane);
}

void Extensions3DChromium::texStorage2DEXT(unsigned int target, int levels, unsigned int internalFormat, int width, int height)
{
    m_private->webContext()->texStorage2DEXT(target, levels, internalFormat, width, height);
}

Platform3DObject Extensions3DChromium::createQueryEXT()
{
    return m_private->webContext()->createQueryEXT();
}

void Extensions3DChromium::deleteQueryEXT(Platform3DObject query)
{
    m_private->webContext()->deleteQueryEXT(query);
}

GC3Dboolean Extensions3DChromium::isQueryEXT(Platform3DObject query)
{
    return m_private->webContext()->isQueryEXT(query);
}

void Extensions3DChromium::beginQueryEXT(GC3Denum target, Platform3DObject query)
{
    m_private->webContext()->beginQueryEXT(target, query);
}

void Extensions3DChromium::endQueryEXT(GC3Denum target)
{
    m_private->webContext()->endQueryEXT(target);
}

void Extensions3DChromium::getQueryivEXT(GC3Denum target, GC3Denum pname, GC3Dint* params)
{
    m_private->webContext()->getQueryivEXT(target, pname, params);
}

void Extensions3DChromium::getQueryObjectuivEXT(Platform3DObject query, GC3Denum pname, GC3Duint* params)
{
    m_private->webContext()->getQueryObjectuivEXT(query, pname, params);
}

void Extensions3DChromium::copyTextureCHROMIUM(GC3Denum target, Platform3DObject sourceId, Platform3DObject destId, GC3Dint level, GC3Denum internalFormat)
{
    m_private->webContext()->copyTextureCHROMIUM(target, sourceId, destId, level, internalFormat);
}

void Extensions3DChromium::shallowFlushCHROMIUM()
{
    return m_private->webContext()->shallowFlushCHROMIUM();
}

void Extensions3DChromium::readnPixelsEXT(int x, int y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Dsizei bufSize, void *data)
{
    notImplemented();
}

void Extensions3DChromium::getnUniformfvEXT(GC3Duint program, int location, GC3Dsizei bufSize, float *params)
{
    notImplemented();
}

void Extensions3DChromium::getnUniformivEXT(GC3Duint program, int location, GC3Dsizei bufSize, int *params)
{
    notImplemented();
}

void Extensions3DChromium::insertEventMarkerEXT(const String& marker)
{
    m_private->webContext()->insertEventMarkerEXT(marker.utf8().data());
}

void Extensions3DChromium::pushGroupMarkerEXT(const String& marker)
{
    m_private->webContext()->pushGroupMarkerEXT(marker.utf8().data());
}

void Extensions3DChromium::popGroupMarkerEXT(void)
{
    m_private->webContext()->popGroupMarkerEXT();
}

void Extensions3DChromium::drawBuffersEXT(GC3Dsizei n, const GC3Denum* bufs)
{
    m_private->webContext()->drawBuffersEXT(n, bufs);
}

} // namespace WebCore
