/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShareableSurface.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "MachPort.h"
#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>
#include <mach/mach_port.h>

// The CGLMacro.h header adds an implicit CGLContextObj parameter to all OpenGL calls,
// which is good because it allows us to make OpenGL calls without saving and restoring the
// current context. The context argument is named "cgl_ctx" by default, so we the macro
// below to declare this variable.
#define DECLARE_GL_CONTEXT_VARIABLE(name) \
    CGLContextObj cgl_ctx = (name)

// It expects a context named "
using namespace WebCore;

namespace WebKit {

ShareableSurface::Handle::Handle()
    : m_port(MACH_PORT_NULL)
{
}

ShareableSurface::Handle::~Handle()
{
    if (m_port != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), m_port);
}

void ShareableSurface::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(CoreIPC::MachPort(m_port, MACH_MSG_TYPE_MOVE_SEND));
    m_port = MACH_PORT_NULL;
}

bool ShareableSurface::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    ASSERT_ARG(handle, handle.m_port == MACH_PORT_NULL);

    CoreIPC::MachPort machPort;
    if (!decoder->decode(machPort))
        return false;

    handle.m_port = machPort.port();
    return false;
}

static RetainPtr<IOSurfaceRef> createIOSurface(const IntSize& size)
{
    int width = size.width();
    int height = size.height();
    
    unsigned bytesPerElement = 4;
    unsigned long bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
    if (!bytesPerRow)
        return 0;
    
    unsigned long allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    if (!allocSize)
        return 0;
    
    unsigned pixelFormat = 'BGRA';

    static const size_t numKeys = 6;
    const void *keys[numKeys];
    const void *values[numKeys];
    keys[0] = kIOSurfaceWidth;
    values[0] = CFNumberCreate(0, kCFNumberIntType, &width);
    keys[1] = kIOSurfaceHeight;
    values[1] = CFNumberCreate(0, kCFNumberIntType, &height);
    keys[2] = kIOSurfacePixelFormat;
    values[2] = CFNumberCreate(0, kCFNumberIntType, &pixelFormat);
    keys[3] = kIOSurfaceBytesPerElement;
    values[3] = CFNumberCreate(0, kCFNumberIntType, &bytesPerElement);
    keys[4] = kIOSurfaceBytesPerRow;
    values[4] = CFNumberCreate(0, kCFNumberLongType, &bytesPerRow);
    keys[5] = kIOSurfaceAllocSize;
    values[5] = CFNumberCreate(0, kCFNumberLongType, &allocSize);
    
    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, CFDictionaryCreate(0, keys, values, numKeys, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (unsigned i = 0; i < numKeys; i++)
        CFRelease(values[i]);
    
    return RetainPtr<IOSurfaceRef>(AdoptCF, IOSurfaceCreate(dictionary.get()));
}
        
PassRefPtr<ShareableSurface> ShareableSurface::create(CGLContextObj cglContextObj, const IntSize& size)
{
    RetainPtr<IOSurfaceRef> ioSurface = createIOSurface(size);
    if (!ioSurface)
        return 0;

    return adoptRef(new ShareableSurface(cglContextObj, size, ioSurface.get()));
}

PassRefPtr<ShareableSurface> ShareableSurface::create(CGLContextObj cglContextObj, const Handle& handle)
{
    ASSERT_ARG(handle, handle.m_port != MACH_PORT_NULL);

    RetainPtr<IOSurfaceRef> ioSurface(AdoptCF, IOSurfaceLookupFromMachPort(handle.m_port));
    if (!ioSurface)
        return 0;

    IntSize size = IntSize(IOSurfaceGetWidth(ioSurface.get()), IOSurfaceGetHeight(ioSurface.get()));
    
    return adoptRef(new ShareableSurface(cglContextObj, size, ioSurface.get()));
}

ShareableSurface::ShareableSurface(CGLContextObj cglContextObj, const IntSize& size, IOSurfaceRef ioSurface)
    : m_cglContextObj(CGLRetainContext(cglContextObj))
    , m_size(size)
    , m_textureID(0)
    , m_frameBufferObjectID(0)
    , m_ioSurface(ioSurface)
{
}

ShareableSurface::~ShareableSurface()
{
    DECLARE_GL_CONTEXT_VARIABLE(m_cglContextObj);

    if (m_textureID)
        glDeleteTextures(1, &m_textureID);

    if (m_frameBufferObjectID)
        glDeleteFramebuffersEXT(1, &m_frameBufferObjectID);

    CGLReleaseContext(m_cglContextObj);
}

bool ShareableSurface::createHandle(Handle& handle)
{
    ASSERT_ARG(handle, handle.m_port == MACH_PORT_NULL);

    mach_port_t port = IOSurfaceCreateMachPort(m_ioSurface.get());
    if (port == MACH_PORT_NULL)
        return false;

    handle.m_port = port;
    return true;
}

void ShareableSurface::attach()
{
    DECLARE_GL_CONTEXT_VARIABLE(m_cglContextObj);

    if (!m_frameBufferObjectID) {
        // Generate a frame buffer object.
        glGenFramebuffersEXT(1, &m_frameBufferObjectID);

        // Associate it with the texture.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_frameBufferObjectID);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_EXT, textureID(), 0);
    } else
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_frameBufferObjectID);
}

void ShareableSurface::detach()
{
    DECLARE_GL_CONTEXT_VARIABLE(m_cglContextObj);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

unsigned ShareableSurface::textureID()
{
    if (m_textureID)
        return m_textureID;

    DECLARE_GL_CONTEXT_VARIABLE(m_cglContextObj);
    
    // Generate a texture.
    glGenTextures(1, &m_textureID);
    
    // Associate it with our IOSurface.
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, m_textureID);
    CGLTexImageIOSurface2D(cgl_ctx, GL_TEXTURE_RECTANGLE_EXT, GL_RGBA8, m_size.width(), m_size.height(), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_ioSurface.get(), 0);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);

    return m_textureID;
}

} // namespace WebKit

