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

#import "CoreGraphicsSPI.h"
#import "GraphicsContextCG.h"
#import "IOSurfacePool.h"
#import "IOSurfaceSPI.h"
#import "ImageBuffer.h"
#import "ImageBufferDataCG.h"
#import "MachSendRight.h"
#import <wtf/Assertions.h>
#import <wtf/MathExtras.h>

#if PLATFORM(IOS)
// Move this into the SPI header once it's possible to put inside the APPLE_INTERNAL_SDK block.
NSString * const WebIOSurfaceAcceleratorUnwireSurfaceKey = @"UnwireSurface";
#endif

using namespace WebCore;

inline std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::surfaceFromPool(IntSize size, IntSize contextSize, CGColorSpaceRef colorSpace, Format pixelFormat)
{
    auto cachedSurface = IOSurfacePool::sharedPool().takeSurface(size, colorSpace, pixelFormat);
    if (!cachedSurface)
        return nullptr;

    cachedSurface->setContextSize(contextSize);
    return cachedSurface;
}

std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::create(IntSize size, CGColorSpaceRef colorSpace, Format pixelFormat)
{
    return IOSurface::create(size, size, colorSpace, pixelFormat);
}

std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::create(IntSize size, IntSize contextSize, CGColorSpaceRef colorSpace, Format pixelFormat)
{
    if (auto cachedSurface = surfaceFromPool(size, contextSize, colorSpace, pixelFormat))
        return cachedSurface;
    bool success = false;
    auto surface = std::unique_ptr<IOSurface>(new IOSurface(size, contextSize, colorSpace, pixelFormat, success));
    if (!success)
        return nullptr;
    return surface;
}

std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::createFromSendRight(const MachSendRight& sendRight, CGColorSpaceRef colorSpace)
{
    auto surface = adoptCF(IOSurfaceLookupFromMachPort(sendRight.sendRight()));
    return IOSurface::createFromSurface(surface.get(), colorSpace);
}

std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::createFromSurface(IOSurfaceRef surface, CGColorSpaceRef colorSpace)
{
    return std::unique_ptr<IOSurface>(new IOSurface(surface, colorSpace));
}

std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::createFromImage(CGImageRef image)
{
    if (!image)
        return nullptr;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);

    auto surface = IOSurface::create(IntSize(width, height), sRGBColorSpaceRef());
    if (!surface)
        return nullptr;
    auto surfaceContext = surface->ensurePlatformContext();
    CGContextDrawImage(surfaceContext, CGRectMake(0, 0, width, height), image);
    CGContextFlush(surfaceContext);
    return surface;
}

void WebCore::IOSurface::moveToPool(std::unique_ptr<IOSurface>&& surface)
{
    IOSurfacePool::sharedPool().addSurface(WTFMove(surface));
}

std::unique_ptr<WebCore::IOSurface> WebCore::IOSurface::createFromImageBuffer(std::unique_ptr<ImageBuffer> imageBuffer)
{
    return WTFMove(imageBuffer->m_data.surface);
}

static NSDictionary *optionsForBiplanarSurface(IntSize size, unsigned pixelFormat, size_t firstPlaneBytesPerPixel, size_t secondPlaneBytesPerPixel)
{
    int width = size.width();
    int height = size.height();

    size_t firstPlaneBytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * firstPlaneBytesPerPixel);
    size_t firstPlaneTotalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * firstPlaneBytesPerRow);

    size_t secondPlaneBytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * secondPlaneBytesPerPixel);
    size_t secondPlaneTotalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * secondPlaneBytesPerRow);
    
    size_t totalBytes = firstPlaneTotalBytes + secondPlaneTotalBytes;
    ASSERT(totalBytes);

    NSArray *planeInfo = @[
        @{
            (id)kIOSurfacePlaneWidth: @(width),
            (id)kIOSurfacePlaneHeight: @(height),
            (id)kIOSurfacePlaneBytesPerRow: @(firstPlaneBytesPerRow),
            (id)kIOSurfacePlaneOffset: @(0),
            (id)kIOSurfacePlaneSize: @(firstPlaneTotalBytes)
        },
        @{
            (id)kIOSurfacePlaneWidth: @(width),
            (id)kIOSurfacePlaneHeight: @(height),
            (id)kIOSurfacePlaneBytesPerRow: @(secondPlaneBytesPerRow),
            (id)kIOSurfacePlaneOffset: @(firstPlaneTotalBytes),
            (id)kIOSurfacePlaneSize: @(secondPlaneTotalBytes)
        }
    ];

    return @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceAllocSize: @(totalBytes),
#if PLATFORM(IOS)
        (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache),
#endif
        (id)kIOSurfacePlaneInfo: planeInfo,
    };
}

static NSDictionary *optionsFor32BitSurface(IntSize size, unsigned pixelFormat)
{
    int width = size.width();
    int height = size.height();

    unsigned bytesPerElement = 4;
    unsigned bytesPerPixel = 4;

    size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerPixel);
    ASSERT(bytesPerRow);

    size_t totalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    ASSERT(totalBytes);

    return @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
        (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
        (id)kIOSurfaceAllocSize: @(totalBytes),
#if PLATFORM(IOS)
        (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache),
#endif
        (id)kIOSurfaceElementHeight: @(1)
    };

}

WebCore::IOSurface::IOSurface(IntSize size, IntSize contextSize, CGColorSpaceRef colorSpace, Format format, bool& success)
    : m_colorSpace(colorSpace)
    , m_size(size)
    , m_contextSize(contextSize)
{
    ASSERT(!success);
    ASSERT(contextSize.width() <= size.width());
    ASSERT(contextSize.height() <= size.height());

    NSDictionary *options;

    switch (format) {
    case Format::RGBA:
        options = optionsFor32BitSurface(size, 'BGRA');
        break;
    case Format::RGB10:
        options = optionsFor32BitSurface(size, 'w30r');
        break;
    case Format::RGB10A8:
        options = optionsForBiplanarSurface(size, 'b3a8', 4, 1);
        break;
    case Format::YUV422:
        options = optionsForBiplanarSurface(size, '422f', 1, 1);
        break;
    }
    m_surface = adoptCF(IOSurfaceCreate((CFDictionaryRef)options));
    success = !!m_surface;
    if (success)
        m_totalBytes = IOSurfaceGetAllocSize(m_surface.get());
    else
        RELEASE_LOG_ERROR("Surface creation failed for size: (%d %d) and format: (%d)", size.width(), size.height(), format);
}

WebCore::IOSurface::IOSurface(IOSurfaceRef surface, CGColorSpaceRef colorSpace)
    : m_colorSpace(colorSpace)
    , m_surface(surface)
{
    m_size = IntSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));
    m_totalBytes = IOSurfaceGetAllocSize(surface);
}

IntSize WebCore::IOSurface::maximumSize()
{
    IntSize maxSize(clampToInteger(IOSurfaceGetPropertyMaximum(kIOSurfaceWidth)), clampToInteger(IOSurfaceGetPropertyMaximum(kIOSurfaceHeight)));

    // Protect against maxSize being { 0, 0 }.
    const int iOSMaxSurfaceDimensionLowerBound = 1024;

#if PLATFORM(IOS)
    // Match limits imposed by Core Animation. FIXME: should have API for this <rdar://problem/25454148>
    const int iOSMaxSurfaceDimension = 8 * 1024;
#else
    // IOSurface::maximumSize() can return { INT_MAX, INT_MAX } when hardware acceleration is unavailable.
    const int iOSMaxSurfaceDimension = 32 * 1024;
#endif

    return maxSize.constrainedBetween({ iOSMaxSurfaceDimensionLowerBound, iOSMaxSurfaceDimensionLowerBound }, { iOSMaxSurfaceDimension, iOSMaxSurfaceDimension });
}

MachSendRight WebCore::IOSurface::createSendRight() const
{
    return MachSendRight::adopt(IOSurfaceCreateMachPort(m_surface.get()));
}

RetainPtr<CGImageRef> WebCore::IOSurface::createImage()
{
    return adoptCF(CGIOSurfaceContextCreateImage(ensurePlatformContext()));
}

RetainPtr<CGImageRef> WebCore::IOSurface::sinkIntoImage(std::unique_ptr<IOSurface> surface)
{
#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100)
    return adoptCF(CGIOSurfaceContextCreateImageReference(surface->ensurePlatformContext()));
#else
    return surface->createImage();
#endif
}

void WebCore::IOSurface::setContextSize(IntSize contextSize)
{
    if (contextSize == m_contextSize)
        return;

    // Release the graphics context and update the context size. Next time the graphics context is
    // accessed, we will construct it again with the right size.
    releaseGraphicsContext();
    m_contextSize = contextSize;
}

CGContextRef WebCore::IOSurface::ensurePlatformContext()
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
    case Format::RGB10A8:
        // A half-float format will be used if CG needs to read back the IOSurface contents,
        // but for an IOSurface-to-IOSurface copy, there should be no conversion.
        bitsPerComponent = 16;
        bitsPerPixel = 64;
        bitmapInfo = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder16Host | kCGBitmapFloatComponents;
        break;
    case Format::YUV422:
        ASSERT_NOT_REACHED();
        break;
    }
    
    m_cgContext = adoptCF(CGIOSurfaceContextCreate(m_surface.get(), m_contextSize.width(), m_contextSize.height(), bitsPerComponent, bitsPerPixel, m_colorSpace.get(), bitmapInfo));

    return m_cgContext.get();
}

GraphicsContext& WebCore::IOSurface::ensureGraphicsContext()
{
    if (m_graphicsContext)
        return *m_graphicsContext;

    m_graphicsContext = std::make_unique<GraphicsContext>(ensurePlatformContext());
    m_graphicsContext->setIsAcceleratedContext(true);

    return *m_graphicsContext;
}

WebCore::IOSurface::SurfaceState WebCore::IOSurface::state() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState == kIOSurfacePurgeableEmpty ? IOSurface::SurfaceState::Empty : IOSurface::SurfaceState::Valid;
}

bool WebCore::IOSurface::isVolatile() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState != kIOSurfacePurgeableNonVolatile;
}

WebCore::IOSurface::SurfaceState WebCore::IOSurface::setIsVolatile(bool isVolatile)
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), isVolatile ? kIOSurfacePurgeableVolatile : kIOSurfacePurgeableNonVolatile, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);

    if (previousState == kIOSurfacePurgeableEmpty)
        return IOSurface::SurfaceState::Empty;

    return IOSurface::SurfaceState::Valid;
}

WebCore::IOSurface::Format WebCore::IOSurface::format() const
{
    unsigned pixelFormat = IOSurfaceGetPixelFormat(m_surface.get());
    if (pixelFormat == 'BGRA')
        return Format::RGBA;

    if (pixelFormat == 'w30r')
        return Format::RGB10;

    if (pixelFormat == 'b3a8')
        return Format::RGB10A8;

    if (pixelFormat == '422f')
        return Format::YUV422;

    ASSERT_NOT_REACHED();
    return Format::RGBA;
}

bool WebCore::IOSurface::isInUse() const
{
    return IOSurfaceIsInUse(m_surface.get());
}

void WebCore::IOSurface::releaseGraphicsContext()
{
    m_graphicsContext = nullptr;
    m_cgContext = nullptr;
}

#if PLATFORM(IOS)
bool WebCore::IOSurface::allowConversionFromFormatToFormat(Format sourceFormat, Format destFormat)
{
    if ((sourceFormat == Format::RGB10 || sourceFormat == Format::RGB10A8) && destFormat == Format::YUV422)
        return false;

    return true;
}

void WebCore::IOSurface::convertToFormat(std::unique_ptr<WebCore::IOSurface>&& inSurface, Format format, std::function<void(std::unique_ptr<WebCore::IOSurface>)> callback)
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
    if (!destinationSurface) {
        callback(nullptr);
        return;
    }

    IOSurfaceRef destinationIOSurfaceRef = destinationSurface->surface();
    IOSurfaceAcceleratorCompletion completion;
    completion.completionRefCon = new std::function<void(std::unique_ptr<WebCore::IOSurface>)> (WTFMove(callback));
    completion.completionRefCon2 = destinationSurface.release();
    completion.completionCallback = [](void *completionRefCon, IOReturn, void * completionRefCon2) {
        auto* callback = static_cast<std::function<void(std::unique_ptr<WebCore::IOSurface>)>*>(completionRefCon);
        auto destinationSurface = std::unique_ptr<WebCore::IOSurface>(static_cast<WebCore::IOSurface*>(completionRefCon2));
        
        (*callback)(WTFMove(destinationSurface));
        delete callback;
    };

    NSDictionary *options = @{ WebIOSurfaceAcceleratorUnwireSurfaceKey : @YES };

    IOReturn ret = IOSurfaceAcceleratorTransformSurface(accelerator, inSurface->surface(), destinationIOSurfaceRef, (CFDictionaryRef)options, nullptr, &completion, nullptr, nullptr);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
}
#endif // PLATFORM(IOS)

#endif // USE(IOSURFACE)
