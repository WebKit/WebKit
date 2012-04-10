/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GraphicsSurface.h"

#if USE(GRAPHICS_SURFACE) && OS(DARWIN)
#include <CFNumber.h>
#include <CGLContext.h>
#include <CGLCurrent.h>
#include <CGLIOSurface.h>
#include <IOSurface/IOSurface.h>


namespace WebCore {

uint32_t GraphicsSurface::platformExport()
{
    return IOSurfaceGetID(m_platformSurface);
}

static uint32_t createTexture(IOSurfaceRef handle)
{
    GLuint texture = 0;
    GLuint format = GL_BGRA;
    GLuint type = GL_UNSIGNED_INT_8_8_8_8_REV;
    GLuint internalFormat = GL_RGBA;
    CGLContextObj context = CGLGetCurrentContext();
    if (!context)
        return 0;

    GLint prevTexture;
    GLboolean wasEnabled = glIsEnabled(GL_TEXTURE_RECTANGLE_ARB);
    glGetIntegerv(GL_TEXTURE_RECTANGLE_ARB, &prevTexture);
    if (!wasEnabled)
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
    CGLError error = CGLTexImageIOSurface2D(context, GL_TEXTURE_RECTANGLE_ARB, internalFormat, IOSurfaceGetWidth(handle), IOSurfaceGetHeight(handle), format, type, handle, 0);
    if (error) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, prevTexture);
    if (!wasEnabled)
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
    return texture;
}

void GraphicsSurface::platformCopyToGLTexture(uint32_t target, uint32_t id, const IntRect& targetRect, const IntPoint& offset)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    if (!m_texture)
        m_texture = createTexture(m_platformSurface);
    if (!m_fbo)
        glGenFramebuffers(1, &m_fbo);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glEnable(target);
    glBindTexture(target, id);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, m_texture, 0);
    glCopyTexSubImage2D(target, 0, targetRect.x(), targetRect.y(), offset.x(), offset.y(), targetRect.width(), targetRect.height());
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glPopAttrib();
}

bool GraphicsSurface::platformCreate(const IntSize& size, Flags flags)
{
    unsigned pixelFormat = 'BGRA';
    unsigned bytesPerElement = 4;
    int width = size.width();
    int height = size.height();

    unsigned long bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
    if (!bytesPerRow)
        return false;

    unsigned long allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    if (!allocSize)
        return false;

    const void *keys[7];
    const void *values[7];
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
    keys[6] = kIOSurfaceIsGlobal;
    values[6] = (flags & GraphicsSurface::SupportsSharing) ? kCFBooleanTrue : kCFBooleanFalse;

    CFDictionaryRef dict = CFDictionaryCreate(0, keys, values, 7, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (unsigned i = 0; i < 7; i++)
        CFRelease(values[i]);

    m_platformSurface = IOSurfaceCreate(dict);
    return !!m_platformSurface;
}

bool GraphicsSurface::platformImport(uint32_t token)
{
    m_platformSurface = IOSurfaceLookup(token);
    return !!m_platformSurface;
}

static int ioSurfaceLockOptions(int lockOptions)
{
    int options = 0;
    if (lockOptions & GraphicsSurface::ReadOnly)
        options |= kIOSurfaceLockReadOnly;
    if (!(lockOptions & GraphicsSurface::RetainPixels))
        options |= kIOSurfaceLockAvoidSync;
    return options;
}

static int ioSurfaceUnlockOptions(int lockOptions)
{
    int options = 0;
    if (lockOptions & GraphicsSurface::ReadOnly)
        options |= (kIOSurfaceLockAvoidSync | kIOSurfaceLockReadOnly);
    return options;
}

char* GraphicsSurface::platformLock(const IntRect& rect, int* outputStride, LockOptions lockOptions)
{
    m_lockOptions = lockOptions;
    IOSurfaceLock(m_platformSurface, ioSurfaceLockOptions(m_lockOptions), 0);

    int stride = IOSurfaceGetBytesPerRow(m_platformSurface);
    if (outputStride)
        *outputStride = stride;

    char* base = static_cast<char*>(IOSurfaceGetBaseAddress(m_platformSurface));
    return base + stride * rect.y() + rect.x() * 4;
}

void GraphicsSurface::platformUnlock()
{
    IOSurfaceUnlock(m_platformSurface, ioSurfaceUnlockOptions(m_lockOptions) & (~kIOSurfaceLockAvoidSync), 0);
}

void GraphicsSurface::platformDestroy()
{
    if (m_fbo)
        glDeleteFramebuffers(1, &m_fbo);
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    CFRelease(IOSurfaceRef(m_platformSurface));
}

}
#endif
