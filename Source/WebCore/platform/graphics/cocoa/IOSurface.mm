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
#import "IOSurfaceSPI.h"
#import "ImageBuffer.h"
#import "ImageBufferDataCG.h"
#import "MachSendRight.h"
#import <wtf/Assertions.h>

extern "C" {
CGContextRef CGIOSurfaceContextCreate(IOSurfaceRef, size_t, size_t, size_t, size_t, CGColorSpaceRef, CGBitmapInfo);
CGImageRef CGIOSurfaceContextCreateImage(CGContextRef);
CGImageRef CGIOSurfaceContextCreateImageReference(CGContextRef);
}

using namespace WebCore;

inline std::unique_ptr<IOSurface> IOSurface::surfaceFromPool(IntSize size, IntSize contextSize, ColorSpace colorSpace, Format pixelFormat)
{
    auto cachedSurface = IOSurfacePool::sharedPool().takeSurface(size, colorSpace, pixelFormat);
    if (!cachedSurface)
        return nullptr;

    cachedSurface->setContextSize(contextSize);
    return cachedSurface;
}

std::unique_ptr<IOSurface> IOSurface::create(IntSize size, ColorSpace colorSpace, Format pixelFormat)
{
    if (auto cachedSurface = surfaceFromPool(size, size, colorSpace, pixelFormat))
        return cachedSurface;

    return std::unique_ptr<IOSurface>(new IOSurface(size, colorSpace, pixelFormat));
}

std::unique_ptr<IOSurface> IOSurface::create(IntSize size, IntSize contextSize, ColorSpace colorSpace, Format pixelFormat)
{
    if (auto cachedSurface = surfaceFromPool(size, contextSize, colorSpace, pixelFormat))
        return cachedSurface;
    return std::unique_ptr<IOSurface>(new IOSurface(size, contextSize, colorSpace, pixelFormat));
}

std::unique_ptr<IOSurface> IOSurface::createFromSendRight(const MachSendRight& sendRight, ColorSpace colorSpace)
{
    auto surface = adoptCF(IOSurfaceLookupFromMachPort(sendRight.sendRight()));
    return IOSurface::createFromSurface(surface.get(), colorSpace);
}

std::unique_ptr<IOSurface> IOSurface::createFromSurface(IOSurfaceRef surface, ColorSpace colorSpace)
{
    return std::unique_ptr<IOSurface>(new IOSurface(surface, colorSpace));
}

std::unique_ptr<IOSurface> IOSurface::createFromImage(CGImageRef image)
{
    if (!image)
        return nullptr;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);

    auto surface = IOSurface::create(IntSize(width, height), ColorSpaceSRGB);
    auto surfaceContext = surface->ensurePlatformContext();
    CGContextDrawImage(surfaceContext, CGRectMake(0, 0, width, height), image);
    CGContextFlush(surfaceContext);

    return surface;
}

void IOSurface::moveToPool(std::unique_ptr<IOSurface>&& surface)
{
    IOSurfacePool::sharedPool().addSurface(WTFMove(surface));
}

std::unique_ptr<IOSurface> IOSurface::createFromImageBuffer(std::unique_ptr<ImageBuffer> imageBuffer)
{
    return WTFMove(imageBuffer->m_data.surface);
}

IOSurface::IOSurface(IntSize size, ColorSpace colorSpace, Format format)
    : m_colorSpace(colorSpace)
    , m_size(size)
    , m_contextSize(size)
{
    unsigned pixelFormat;
    unsigned bytesPerPixel;
    unsigned bytesPerElement;

    int width = size.width();
    int height = size.height();

    NSDictionary *options;
    
    if (format == Format::RGB10A8) {
        pixelFormat = 'b3a8';
        
        // RGB plane (10-10-10)
        bytesPerPixel = 4;
        bytesPerElement = 4;

        size_t rgbPlaneBytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
        size_t rgbPlaneTotalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * rgbPlaneBytesPerRow);

        // Alpha plane (8)
        bytesPerElement = 1;
        size_t alphaPlaneBytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
        size_t alphaPlaneTotalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * alphaPlaneBytesPerRow);
        
        m_totalBytes = rgbPlaneTotalBytes + alphaPlaneTotalBytes;

        NSArray *planeInfo = @[
            @{
                (id)kIOSurfacePlaneWidth: @(width),
                (id)kIOSurfacePlaneHeight: @(height),
                (id)kIOSurfacePlaneBytesPerRow: @(rgbPlaneBytesPerRow),
                (id)kIOSurfacePlaneOffset: @(0),
                (id)kIOSurfacePlaneSize: @(rgbPlaneTotalBytes)
            },
            @{
                (id)kIOSurfacePlaneWidth: @(width),
                (id)kIOSurfacePlaneHeight: @(height),
                (id)kIOSurfacePlaneBytesPerRow: @(alphaPlaneBytesPerRow),
                (id)kIOSurfacePlaneOffset: @(rgbPlaneTotalBytes),
                (id)kIOSurfacePlaneSize: @(alphaPlaneTotalBytes)
            }
        ];

        options = @{
            (id)kIOSurfaceWidth: @(width),
            (id)kIOSurfaceHeight: @(height),
            (id)kIOSurfacePixelFormat: @(pixelFormat),
            (id)kIOSurfaceAllocSize: @(m_totalBytes),
#if PLATFORM(IOS)
            (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache),
#endif
            (id)kIOSurfacePlaneInfo: planeInfo,
        };
    } else {
        unsigned elementWidth;

        switch (format) {
        case Format::RGBA:
            pixelFormat = 'BGRA';
            bytesPerPixel = 4;
            bytesPerElement = 4;
            elementWidth = 1;
            break;
        case Format::YUV422:
            pixelFormat = 'yuvf';
            bytesPerPixel = 2;
            bytesPerElement = 4;
            elementWidth = 2;
            break;
        case Format::RGB10:
            pixelFormat = 'w30r';
            bytesPerPixel = 4;
            bytesPerElement = 4;
            elementWidth = 1;
            break;
        case Format::RGB10A8:
            ASSERT_NOT_REACHED();
            pixelFormat = 'b3a8';
            bytesPerPixel = 1;
            bytesPerElement = 1;
            elementWidth = 1;
            break;
        }

        size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerPixel);
        ASSERT(bytesPerRow);

        m_totalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
        ASSERT(m_totalBytes);

        options = @{
            (id)kIOSurfaceWidth: @(width),
            (id)kIOSurfaceHeight: @(height),
            (id)kIOSurfacePixelFormat: @(pixelFormat),
            (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
            (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
            (id)kIOSurfaceAllocSize: @(m_totalBytes),
#if PLATFORM(IOS)
            (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache),
#endif
            (id)kIOSurfaceElementWidth: @(elementWidth),
            (id)kIOSurfaceElementHeight: @(1)
        };
    }
    
    m_surface = adoptCF(IOSurfaceCreate((CFDictionaryRef)options));
    if (!m_surface)
        NSLog(@"Surface creation failed for options %@", options);
}

IOSurface::IOSurface(IntSize size, IntSize contextSize, ColorSpace colorSpace, Format pixelFormat)
    : IOSurface(size, colorSpace, pixelFormat)
{
    ASSERT(contextSize.width() <= size.width());
    ASSERT(contextSize.height() <= size.height());
    m_contextSize = contextSize;
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

MachSendRight IOSurface::createSendRight() const
{
    return MachSendRight::adopt(IOSurfaceCreateMachPort(m_surface.get()));
}

RetainPtr<CGImageRef> IOSurface::createImage()
{
    return adoptCF(CGIOSurfaceContextCreateImage(ensurePlatformContext()));
}

RetainPtr<CGImageRef> IOSurface::sinkIntoImage(std::unique_ptr<IOSurface> surface)
{
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    return adoptCF(CGIOSurfaceContextCreateImageReference(surface->ensurePlatformContext()));
#else
    return surface->createImage();
#endif
}

void IOSurface::setContextSize(IntSize contextSize)
{
    if (contextSize == m_contextSize)
        return;

    // Release the graphics context and update the context size. Next time the graphics context is
    // accessed, we will construct it again with the right size.
    releaseGraphicsContext();
    m_contextSize = contextSize;
}

CGContextRef IOSurface::ensurePlatformContext()
{
    if (m_cgContext)
        return m_cgContext.get();

    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;

    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 32;
    
    switch (format()) {
    case Format::RGBA:
        break;
    case Format::RGB10:
        bitsPerComponent = 10;
        bitsPerPixel = 32;
        break;
    case Format::RGB10A8:
        // FIXME: This doesn't take the two-plane format into account.
        bitsPerComponent = 10;
        bitsPerPixel = 32;
        break;
    case Format::YUV422:
        ASSERT_NOT_REACHED();
        break;
    }
    
    m_cgContext = adoptCF(CGIOSurfaceContextCreate(m_surface.get(), m_contextSize.width(), m_contextSize.height(), bitsPerComponent, bitsPerPixel, cachedCGColorSpace(m_colorSpace), bitmapInfo));

    return m_cgContext.get();
}

GraphicsContext& IOSurface::ensureGraphicsContext()
{
    if (m_graphicsContext)
        return *m_graphicsContext;

    m_graphicsContext = std::make_unique<GraphicsContext>(ensurePlatformContext());
    m_graphicsContext->setIsAcceleratedContext(true);

    return *m_graphicsContext;
}

IOSurface::SurfaceState IOSurface::state() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState == kIOSurfacePurgeableEmpty ? IOSurface::SurfaceState::Empty : IOSurface::SurfaceState::Valid;
}

bool IOSurface::isVolatile() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState != kIOSurfacePurgeableNonVolatile;
}

IOSurface::SurfaceState IOSurface::setIsVolatile(bool isVolatile)
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), isVolatile ? kIOSurfacePurgeableVolatile : kIOSurfacePurgeableNonVolatile, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);

    if (previousState == kIOSurfacePurgeableEmpty)
        return IOSurface::SurfaceState::Empty;

    return IOSurface::SurfaceState::Valid;
}

IOSurface::Format IOSurface::format() const
{
    unsigned pixelFormat = IOSurfaceGetPixelFormat(m_surface.get());
    if (pixelFormat == 'BGRA')
        return Format::RGBA;

    if (pixelFormat == 'w30r')
        return Format::RGB10;

    if (pixelFormat == 'b3a8')
        return Format::RGB10A8;

    if (pixelFormat == 'yuvf')
        return Format::YUV422;

    ASSERT_NOT_REACHED();
    return Format::RGBA;
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

#if PLATFORM(IOS)
WEBCORE_EXPORT void IOSurface::copyToSurface(IOSurface& destSurface)
{
    if (destSurface.format() != format()) {
        WTFLogAlways("Trying to copy IOSurface to another surface with a different format");
        return;
    }

    if (destSurface.size() != size()) {
        WTFLogAlways("Trying to copy IOSurface to another surface with a different size");
        return;
    }

    static IOSurfaceAcceleratorRef accelerator;
    if (!accelerator)
        IOSurfaceAcceleratorCreate(nullptr, nullptr, &accelerator);

    IOReturn ret = IOSurfaceAcceleratorTransformSurface(accelerator, m_surface.get(), destSurface.surface(), nullptr, nullptr, nullptr, nullptr, nullptr);
    if (ret)
        WTFLogAlways("IOSurfaceAcceleratorTransformSurface %p to %p failed with error %d", m_surface.get(), destSurface.surface(), ret);
}

bool IOSurface::allowConversionFromFormatToFormat(Format sourceFormat, Format destFormat)
{
    if ((sourceFormat == Format::RGB10 || sourceFormat == Format::RGB10A8) && destFormat == Format::YUV422)
        return false;

    return true;
}

void IOSurface::convertToFormat(std::unique_ptr<WebCore::IOSurface>&& inSurface, Format format, std::function<void(std::unique_ptr<WebCore::IOSurface>)> callback)
{
    static IOSurfaceAcceleratorRef accelerator;
    if (!accelerator) {
        IOSurfaceAcceleratorCreate(nullptr, nullptr, &accelerator);

        auto runLoopSource = IOSurfaceAcceleratorGetRunLoopSource(accelerator);
        CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopDefaultMode);
    }

    if (inSurface->format() == format) {
        callback(WTFMove(inSurface));
        return;
    }

    auto destinationSurface = IOSurface::create(inSurface->size(), inSurface->colorSpace(), format);
    IOSurfaceRef destinationIOSurfaceRef = destinationSurface->surface();

    IOSurfaceAcceleratorCompletion completion;
    completion.completionRefCon = new std::function<void(std::unique_ptr<IOSurface>)> (WTFMove(callback));
    completion.completionRefCon2 = destinationSurface.release();
    completion.completionCallback = [](void *completionRefCon, IOReturn, void * completionRefCon2) {
        auto* callback = static_cast<std::function<void(std::unique_ptr<WebCore::IOSurface>)>*>(completionRefCon);
        auto destinationSurface = std::unique_ptr<IOSurface>(static_cast<IOSurface*>(completionRefCon2));
        
        (*callback)(WTFMove(destinationSurface));
        delete callback;
    };

    IOReturn ret = IOSurfaceAcceleratorTransformSurface(accelerator, inSurface->surface(), destinationIOSurfaceRef, nullptr, nullptr, &completion, nullptr, nullptr);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
}
#endif // PLATFORM(IOS)

#endif // USE(IOSURFACE)
