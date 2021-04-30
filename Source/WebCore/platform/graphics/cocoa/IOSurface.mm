/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#import "CoreVideoSoftLink.h"
#import "GraphicsContextCG.h"
#import "GraphicsContextGL.h"
#import "HostWindow.h"
#import "IOSurfacePool.h"
#import "ImageBuffer.h"
#import "Logging.h"
#import "PlatformScreen.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/IOSurfaceSPI.h>
#import <wtf/Assertions.h>
#import <wtf/MachSendRight.h>
#import <wtf/MathExtras.h>
#import <wtf/text/TextStream.h>

namespace WebCore {

inline std::unique_ptr<IOSurface> IOSurface::surfaceFromPool(IntSize size, IntSize contextSize, CGColorSpaceRef colorSpace, Format pixelFormat)
{
    auto cachedSurface = IOSurfacePool::sharedPool().takeSurface(size, colorSpace, pixelFormat);
    if (!cachedSurface)
        return nullptr;

    cachedSurface->setContextSize(contextSize);
    return cachedSurface;
}

std::unique_ptr<IOSurface> IOSurface::create(IntSize size, CGColorSpaceRef colorSpace, Format pixelFormat)
{
    return IOSurface::create(size, size, colorSpace, pixelFormat);
}

std::unique_ptr<IOSurface> IOSurface::create(IntSize size, IntSize contextSize, CGColorSpaceRef colorSpace, Format pixelFormat)
{
    if (auto cachedSurface = surfaceFromPool(size, contextSize, colorSpace, pixelFormat)) {
        LOG_WITH_STREAM(IOSurface, stream << "IOSurface::create took from pool: " << *cachedSurface);
        return cachedSurface;
    }
    bool success = false;
    auto surface = std::unique_ptr<IOSurface>(new IOSurface(size, contextSize, colorSpace, pixelFormat, success));
    if (!success) {
        LOG(IOSurface, "IOSurface::create failed to create %dx%d surface", size.width(), size.height());
        return nullptr;
    }

    LOG_WITH_STREAM(IOSurface, stream << "IOSurface::create created " << *surface);
    return surface;
}

std::unique_ptr<IOSurface> IOSurface::createFromSendRight(const MachSendRight&& sendRight, CGColorSpaceRef colorSpace)
{
    auto surface = adoptCF(IOSurfaceLookupFromMachPort(sendRight.sendRight()));
    return IOSurface::createFromSurface(surface.get(), colorSpace);
}

std::unique_ptr<IOSurface> IOSurface::createFromSurface(IOSurfaceRef surface, CGColorSpaceRef colorSpace)
{
    return std::unique_ptr<IOSurface>(new IOSurface(surface, colorSpace));
}

std::unique_ptr<IOSurface> IOSurface::createFromImage(CGImageRef image)
{
    if (!image)
        return nullptr;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);

    auto surface = IOSurface::create(IntSize(width, height), CGImageGetColorSpace(image));
    if (!surface)
        return nullptr;
    auto surfaceContext = surface->ensurePlatformContext();
    CGContextDrawImage(surfaceContext, CGRectMake(0, 0, width, height), image);
    CGContextFlush(surfaceContext);
    return surface;
}

std::unique_ptr<IOSurface> IOSurface::createFromPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return nullptr;

    auto surface = CVPixelBufferGetIOSurface(pixelBuffer);
    if (!surface)
        return nullptr;

    auto colorSpace = CVImageBufferGetColorSpace(pixelBuffer);
    if (!colorSpace)
        return nullptr;

    return createFromSurface(surface, colorSpace);
}

void IOSurface::moveToPool(std::unique_ptr<IOSurface>&& surface)
{
    IOSurfacePool::sharedPool().addSurface(WTFMove(surface));
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
#if PLATFORM(IOS_FAMILY)
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
#if PLATFORM(IOS_FAMILY)
        (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache),
#endif
        (id)kIOSurfaceElementHeight: @(1)
    };

}

IOSurface::IOSurface(IntSize size, IntSize contextSize, CGColorSpaceRef colorSpace, Format format, bool& success)
    : m_colorSpace(colorSpace)
    , m_size(size)
    , m_contextSize(contextSize)
{
    ASSERT(!success);
    ASSERT(contextSize.width() <= size.width());
    ASSERT(contextSize.height() <= size.height());
    ASSERT(!size.isEmpty());

    NSDictionary *options;

    switch (format) {
    case Format::BGRA:
        options = optionsFor32BitSurface(size, 'BGRA');
        break;
#if HAVE(IOSURFACE_RGB10)
    case Format::RGB10:
        options = optionsFor32BitSurface(size, 'w30r');
        break;
    case Format::RGB10A8:
        options = optionsForBiplanarSurface(size, 'b3a8', 4, 1);
        break;
#endif
    case Format::YUV422:
        options = optionsForBiplanarSurface(size, '422f', 1, 1);
        break;
    }
    m_surface = adoptCF(IOSurfaceCreate((CFDictionaryRef)options));
    success = !!m_surface;
    if (success)
        m_totalBytes = IOSurfaceGetAllocSize(m_surface.get());
    else
        RELEASE_LOG_ERROR(Layers, "IOSurface creation failed for size: (%d %d) and format: (%d)", size.width(), size.height(), format);
}

IOSurface::IOSurface(IOSurfaceRef surface, CGColorSpaceRef colorSpace)
    : m_colorSpace(colorSpace)
    , m_surface(surface)
{
    m_size = IntSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));
    m_totalBytes = IOSurfaceGetAllocSize(surface);
}

IOSurface::~IOSurface() = default;

static IntSize computeMaximumSurfaceSize()
{
    IntSize maxSize(clampToInteger(IOSurfaceGetPropertyMaximum(kIOSurfaceWidth)), clampToInteger(IOSurfaceGetPropertyMaximum(kIOSurfaceHeight)));

    // Protect against maxSize being { 0, 0 }.
    const int maxSurfaceDimensionLowerBound = 1024;

#if PLATFORM(IOS_FAMILY)
    // Match limits imposed by Core Animation. FIXME: should have API for this <rdar://problem/25454148>
    const int maxSurfaceDimension = 8 * 1024;
#else
    // IOSurface::maximumSize() can return { INT_MAX, INT_MAX } when hardware acceleration is unavailable.
    const int maxSurfaceDimension = 32 * 1024;
#endif

    return maxSize.constrainedBetween({ maxSurfaceDimensionLowerBound, maxSurfaceDimensionLowerBound }, { maxSurfaceDimension, maxSurfaceDimension });
}

static WTF::Atomic<IntSize>& surfaceMaximumSize()
{
    static WTF::Atomic<IntSize> maximumSize;
    return maximumSize;
}

void IOSurface::setMaximumSize(IntSize size)
{
    ASSERT(!size.isEmpty());
    surfaceMaximumSize().store(size);
}

IntSize IOSurface::maximumSize()
{
    auto size = surfaceMaximumSize().load();
    if (size.isEmpty()) {
        auto computedSize = computeMaximumSurfaceSize();
        surfaceMaximumSize().store(computedSize);
        return computedSize;
    }
    return size;
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
    return adoptCF(CGIOSurfaceContextCreateImageReference(surface->ensurePlatformContext()));
}

RetainPtr<CVPixelBufferRef> IOSurface::createPixelBuffer()
{
    CVPixelBufferRef rawBuffer = nullptr;
    auto status = CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, m_surface.get(), nullptr, &rawBuffer);
    if (status == noErr && rawBuffer)
        return adoptCF(rawBuffer);

    return nullptr;
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

CGContextRef IOSurface::ensurePlatformContext(const HostWindow* hostWindow)
{
    if (m_cgContext)
        return m_cgContext.get();

    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;

    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 32;
    
    switch (format()) {
    case Format::BGRA:
        break;
#if HAVE(IOSURFACE_RGB10)
    case Format::RGB10:
    case Format::RGB10A8:
        // A half-float format will be used if CG needs to read back the IOSurface contents,
        // but for an IOSurface-to-IOSurface copy, there should be no conversion.
        bitsPerComponent = 16;
        bitsPerPixel = 64;
        bitmapInfo = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder16Host | kCGBitmapFloatComponents;
        break;
#endif
    case Format::YUV422:
        ASSERT_NOT_REACHED();
        break;
    }
    
    m_cgContext = adoptCF(CGIOSurfaceContextCreate(m_surface.get(), m_contextSize.width(), m_contextSize.height(), bitsPerComponent, bitsPerPixel, m_colorSpace.get(), bitmapInfo));

#if PLATFORM(MAC)
    if (auto displayMask = primaryOpenGLDisplayMask()) {
        if (hostWindow && hostWindow->displayID())
            displayMask = displayMaskForDisplay(hostWindow->displayID());
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CGIOSurfaceContextSetDisplayMask(m_cgContext.get(), displayMask);
ALLOW_DEPRECATED_DECLARATIONS_END
    }
#else
    UNUSED_PARAM(hostWindow);
#endif

    return m_cgContext.get();
}

GraphicsContext& IOSurface::ensureGraphicsContext()
{
    if (m_graphicsContext)
        return *m_graphicsContext;

    m_graphicsContext = makeUnique<GraphicsContext>(ensurePlatformContext());
    m_graphicsContext->setIsAcceleratedContext(true);

    return *m_graphicsContext;
}

VolatilityState IOSurface::state() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState == kIOSurfacePurgeableEmpty ? VolatilityState::Empty : VolatilityState::Valid;
}

bool IOSurface::isVolatile() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState != kIOSurfacePurgeableNonVolatile;
}

VolatilityState IOSurface::setVolatile(bool isVolatile)
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), isVolatile ? kIOSurfacePurgeableVolatile : kIOSurfacePurgeableNonVolatile, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);

    if (previousState == kIOSurfacePurgeableEmpty)
        return VolatilityState::Empty;

    return VolatilityState::Valid;
}

IOSurface::Format IOSurface::format() const
{
    unsigned pixelFormat = IOSurfaceGetPixelFormat(m_surface.get());
    if (pixelFormat == 'BGRA')
        return Format::BGRA;

#if HAVE(IOSURFACE_RGB10)
    if (pixelFormat == 'w30r')
        return Format::RGB10;

    if (pixelFormat == 'b3a8')
        return Format::RGB10A8;
#endif

    if (pixelFormat == '422f')
        return Format::YUV422;

    ASSERT_NOT_REACHED();
    return Format::BGRA;
}

IOSurfaceID IOSurface::surfaceID() const
{
// FIXME: Should be able to do this even without the Apple internal SDK.
// FIXME: Should be able to do this on watchOS and tvOS.
#if PLATFORM(IOS_FAMILY) && (!USE(APPLE_INTERNAL_SDK) || PLATFORM(WATCHOS) || PLATFORM(APPLETV))
    return 0;
#else
    return IOSurfaceGetID(m_surface.get());
#endif
}

size_t IOSurface::bytesPerRow() const
{
    return IOSurfaceGetBytesPerRow(m_surface.get());
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

#if HAVE(IOSURFACE_ACCELERATOR)

bool IOSurface::allowConversionFromFormatToFormat(Format sourceFormat, Format destFormat)
{
#if HAVE(IOSURFACE_RGB10)
    if ((sourceFormat == Format::RGB10 || sourceFormat == Format::RGB10A8) && destFormat == Format::YUV422)
        return false;
#endif

    return true;
}

void IOSurface::convertToFormat(std::unique_ptr<IOSurface>&& inSurface, Format format, WTF::Function<void(std::unique_ptr<IOSurface>)>&& callback)
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
    completion.completionRefCon = new WTF::Function<void(std::unique_ptr<IOSurface>)> (WTFMove(callback));
    completion.completionRefCon2 = destinationSurface.release();
    completion.completionCallback = [](void *completionRefCon, IOReturn, void * completionRefCon2) {
        auto* callback = static_cast<WTF::Function<void(std::unique_ptr<IOSurface>)>*>(completionRefCon);
        auto destinationSurface = std::unique_ptr<IOSurface>(static_cast<IOSurface*>(completionRefCon2));
        
        (*callback)(WTFMove(destinationSurface));
        delete callback;
    };

    NSDictionary *options = @{ (id)kIOSurfaceAcceleratorUnwireSurfaceKey : @YES };

    IOReturn ret = IOSurfaceAcceleratorTransformSurface(accelerator, inSurface->surface(), destinationIOSurfaceRef, (CFDictionaryRef)options, nullptr, &completion, nullptr, nullptr);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
}

#endif // HAVE(IOSURFACE_ACCELERATOR)

#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
void IOSurface::setOwnershipIdentity(task_id_token_t newOwner)
{
    auto result = IOSurfaceSetOwnershipIdentity(m_surface.get(), newOwner, kIOSurfaceMemoryLedgerTagGraphics, 0);
    if (result != kIOReturnSuccess)
        RELEASE_LOG_ERROR(IOSurface, "IOSurface::setOwnershipIdentity: Failed to claim ownership of IOSurface %p, newOwner: %d, error: %d", m_surface.get(), (int)newOwner, result);
}
#endif

void IOSurface::migrateColorSpaceToProperties()
{
    auto colorSpaceProperties = adoptCF(CGColorSpaceCopyPropertyList(m_colorSpace.get()));
    IOSurfaceSetValue(m_surface.get(), kIOSurfaceColorSpace, colorSpaceProperties.get());
}

IOSurface::Format IOSurface::formatForPixelFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::RGBA8:
        RELEASE_ASSERT_NOT_REACHED();
        return IOSurface::Format::BGRA;
    case PixelFormat::BGRA8:
        return IOSurface::Format::BGRA;
#if HAVE(IOSURFACE_RGB10)
    case PixelFormat::RGB10:
        return IOSurface::Format::RGB10;
    case PixelFormat::RGB10A8:
        return IOSurface::Format::RGB10A8;
#else
    case PixelFormat::RGB10:
    case PixelFormat::RGB10A8:
        RELEASE_ASSERT_NOT_REACHED();
        return IOSurface::Format::BGRA;
#endif
    }

    ASSERT_NOT_REACHED();
    return IOSurface::Format::BGRA;
}

static TextStream& operator<<(TextStream& ts, IOSurface::Format format)
{
    switch (format) {
    case IOSurface::Format::BGRA:
        ts << "BGRA";
        break;
    case IOSurface::Format::YUV422:
        ts << "YUV422";
        break;
#if HAVE(IOSURFACE_RGB10)
    case IOSurface::Format::RGB10:
        ts << "RGB10";
        break;
    case IOSurface::Format::RGB10A8:
        ts << "RGB10A8";
        break;
#endif
    }
    return ts;
}

static TextStream& operator<<(TextStream& ts, VolatilityState state)
{
    switch (state) {
    case VolatilityState::Valid:
        ts << "valid";
        break;
    case VolatilityState::Empty:
        ts << "empty";
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const IOSurface& surface)
{
    return ts << "IOSurface " << surface.surfaceID() << " size " << surface.size() << " format " << surface.format() << " state " << surface.state();
}

} // namespace WebCore
