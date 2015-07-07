/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "IOSurface.h"

#if USE(IOSURFACE)

#import "GraphicsContextCG.h"
#import "IOSurfacePool.h"
#import <IOSurface/IOSurface.h>
#import <wtf/Assertions.h>

#if __has_include(<IOSurface/IOSurfacePrivate.h>)
#import <IOSurface/IOSurfacePrivate.h>
#else
enum {
    kIOSurfacePurgeableNonVolatile = 0,
    kIOSurfacePurgeableVolatile = 1,
    kIOSurfacePurgeableEmpty = 2,
    kIOSurfacePurgeableKeepCurrent = 3,
};
#endif

extern "C" {
CGContextRef CGIOSurfaceContextCreate(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo);
CGImageRef CGIOSurfaceContextCreateImage(CGContextRef);
IOReturn IOSurfaceSetPurgeable(IOSurfaceRef, uint32_t, uint32_t *);
}

using namespace WebCore;

PassRefPtr<IOSurface> IOSurface::create(IntSize size, ColorSpace colorSpace)
{
    if (RefPtr<IOSurface> cachedSurface = IOSurfacePool::sharedPool().takeSurface(size, colorSpace))
        return cachedSurface.release();
    return adoptRef(new IOSurface(size, colorSpace));
}

PassRefPtr<IOSurface> IOSurface::createFromMachPort(mach_port_t machPort, ColorSpace colorSpace)
{
    RetainPtr<IOSurfaceRef> surface = adoptCF(IOSurfaceLookupFromMachPort(machPort));
    return IOSurface::createFromSurface(surface.get(), colorSpace);
}

PassRefPtr<IOSurface> IOSurface::createFromSurface(IOSurfaceRef surface, ColorSpace colorSpace)
{
    return adoptRef(new IOSurface(surface, colorSpace));
}

PassRefPtr<IOSurface> IOSurface::createFromImage(CGImageRef image)
{
    if (!image)
        return nullptr;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);

    RefPtr<IOSurface> surface = IOSurface::create(IntSize(width, height), ColorSpaceDeviceRGB);
    auto surfaceContext = surface->ensurePlatformContext();
    CGContextDrawImage(surfaceContext, CGRectMake(0, 0, width, height), image);
    CGContextFlush(surfaceContext);

    return surface.release();
}

IOSurface::IOSurface(IntSize size, ColorSpace colorSpace)
    : m_colorSpace(colorSpace)
    , m_size(size)
{
    unsigned pixelFormat = 'BGRA';
    unsigned bytesPerElement = 4;
    int width = size.width();
    int height = size.height();

    size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
    ASSERT(bytesPerRow);

    m_totalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    ASSERT(m_totalBytes);

    NSDictionary *options = @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
        (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
        (id)kIOSurfaceAllocSize: @(m_totalBytes),
#if PLATFORM(IOS)
        (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache)
#endif
    };

    m_surface = adoptCF(IOSurfaceCreate((CFDictionaryRef)options));
}

IOSurface::IOSurface(IOSurfaceRef surface, ColorSpace colorSpace)
    : m_colorSpace(colorSpace)
    , m_surface(surface)
{
    m_size = IntSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));
    m_totalBytes = IOSurfaceGetAllocSize(surface);
}

IntSize IOSurface::maximumSize()
{
    return IntSize(IOSurfaceGetPropertyMaximum(kIOSurfaceWidth), IOSurfaceGetPropertyMaximum(kIOSurfaceHeight));
}

mach_port_t IOSurface::createMachPort() const
{
    return IOSurfaceCreateMachPort(m_surface.get());
}

RetainPtr<CGImageRef> IOSurface::createImage()
{
    return adoptCF(CGIOSurfaceContextCreateImage(ensurePlatformContext()));
}

CGContextRef IOSurface::ensurePlatformContext()
{
    if (m_cgContext)
        return m_cgContext.get();

    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 32;
    m_cgContext = adoptCF(CGIOSurfaceContextCreate(m_surface.get(), m_size.width(), m_size.height(), bitsPerComponent, bitsPerPixel, cachedCGColorSpace(m_colorSpace), bitmapInfo));

    return m_cgContext.get();
}

GraphicsContext& IOSurface::ensureGraphicsContext()
{
    if (m_graphicsContext)
        return *m_graphicsContext;

    m_graphicsContext = adoptPtr(new GraphicsContext(ensurePlatformContext()));

    return *m_graphicsContext;
}

IOSurface::SurfaceState IOSurface::state() const
{
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState == kIOSurfacePurgeableEmpty ? IOSurface::SurfaceState::Empty : IOSurface::SurfaceState::Valid;
#else
    return SurfaceState::Valid;
#endif
}

bool IOSurface::isVolatile() const
{
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState != kIOSurfacePurgeableNonVolatile;
#else
    return false;
#endif
}

IOSurface::SurfaceState IOSurface::setIsVolatile(bool isVolatile)
{
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), isVolatile ? kIOSurfacePurgeableVolatile : kIOSurfacePurgeableNonVolatile, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);

    if (previousState == kIOSurfacePurgeableEmpty)
        return IOSurface::SurfaceState::Empty;
#else
    UNUSED_PARAM(isVolatile);
#endif

    return IOSurface::SurfaceState::Valid;
}

bool IOSurface::isInUse() const
{
    return IOSurfaceIsInUse(m_surface.get());
}

void IOSurface::releaseGraphicsContext()
{
    m_graphicsContext = nullptr;
    m_cgContext = nullptr;
}

#endif // USE(IOSURFACE)
