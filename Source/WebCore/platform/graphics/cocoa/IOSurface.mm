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

#import "DestinationColorSpace.h"
#import "GraphicsContextCG.h"
#import "HostWindow.h"
#import "IOSurfacePool.h"
#import "Logging.h"
#import "PlatformScreen.h"
#import "ProcessCapabilities.h"
#import "ProcessIdentity.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/Assertions.h>
#import <wtf/MachSendRight.h>
#import <wtf/MathExtras.h>
#import <wtf/text/TextStream.h>

#import "CoreVideoSoftLink.h"
#import <pal/cg/CoreGraphicsSoftLink.h>

namespace WebCore {

std::unique_ptr<IOSurface> IOSurface::create(IOSurfacePool* pool, IntSize size, const DestinationColorSpace& colorSpace, Format pixelFormat)
{
    ASSERT(ProcessCapabilities::canUseAcceleratedBuffers());

    if (pool) {
        if (auto cachedSurface = pool->takeSurface(size, colorSpace, pixelFormat)) {
            cachedSurface->releaseGraphicsContext();
            LOG_WITH_STREAM(IOSurface, stream << "IOSurface::create took from pool: " << *cachedSurface);
            return cachedSurface;
        }
    }

    bool success = false;
    auto surface = std::unique_ptr<IOSurface>(new IOSurface(size, colorSpace, pixelFormat, success));
    if (!success) {
        LOG(IOSurface, "IOSurface::create failed to create %dx%d surface", size.width(), size.height());
        return nullptr;
    }

    LOG_WITH_STREAM(IOSurface, stream << "IOSurface::create created " << *surface);
    return surface;
}

std::unique_ptr<IOSurface> IOSurface::createFromSendRight(const MachSendRight&& sendRight)
{
    ASSERT(ProcessCapabilities::canUseAcceleratedBuffers());

    auto surface = adoptCF(IOSurfaceLookupFromMachPort(sendRight.sendRight()));
    return IOSurface::createFromSurface(surface.get(), { });
}

std::unique_ptr<IOSurface> IOSurface::createFromSurface(IOSurfaceRef surface, std::optional<DestinationColorSpace>&& colorSpace)
{
    if (!surface)
        return nullptr;

    return std::unique_ptr<IOSurface>(new IOSurface(surface, WTFMove(colorSpace)));
}

std::unique_ptr<IOSurface> IOSurface::createFromImage(IOSurfacePool* pool, CGImageRef image)
{
    if (!image)
        return nullptr;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);

    auto surface = IOSurface::create(pool, IntSize(width, height), DestinationColorSpace { CGImageGetColorSpace(image) });
    if (!surface)
        return nullptr;
    auto surfaceContext = surface->ensurePlatformContext();
    CGContextDrawImage(surfaceContext, CGRectMake(0, 0, width, height), image);
    CGContextFlush(surfaceContext);
    return surface;
}

void IOSurface::moveToPool(std::unique_ptr<IOSurface>&& surface, IOSurfacePool* pool)
{
    if (pool)
        pool->addSurface(WTFMove(surface));
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

IOSurface::IOSurface(IntSize size, const DestinationColorSpace& colorSpace, Format format, bool& success)
    : m_colorSpace(colorSpace)
    , m_size(size)
{
    ASSERT(!success);
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
    if (success) {
        setColorSpaceProperty();
        m_totalBytes = IOSurfaceGetAllocSize(m_surface.get());
    } else
        RELEASE_LOG_ERROR(Layers, "IOSurface creation failed for size: (%d %d) and format: (%d)", size.width(), size.height(), format);
}

IOSurface::IOSurface(IOSurfaceRef surface, std::optional<DestinationColorSpace>&& colorSpace)
    : m_colorSpace(WTFMove(colorSpace))
    , m_surface(surface)
{
    m_size = IntSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface));
    m_totalBytes = IOSurfaceGetAllocSize(surface);

    if (m_colorSpace)
        setColorSpaceProperty();
}

IOSurface::~IOSurface() = default;

static constexpr IntSize fallbackMaxSurfaceDimension()
{
    // Match limits imposed by Core Animation. FIXME: should have API for this <rdar://problem/25454148>
#if PLATFORM(WATCHOS)
    constexpr int maxSurfaceDimension = 4 * 1024;
#elif PLATFORM(MAC)
    constexpr int maxSurfaceDimension = 32 * 1024;
#else
    constexpr int maxSurfaceDimension = 8 * 1024;
#endif
    return { maxSurfaceDimension, maxSurfaceDimension };
}

static IntSize computeMaximumSurfaceSize()
{
    auto maxSize = IntSize { clampToInteger(IOSurfaceGetPropertyMaximum(kIOSurfaceWidth)), clampToInteger(IOSurfaceGetPropertyMaximum(kIOSurfaceHeight)) };

#if PLATFORM(IOS_FAMILY)
    // On iOS, there's an additional 8K clamp in CA (rdar://101936907).
    maxSize.clampToMaximumSize(fallbackMaxSurfaceDimension());
#endif

    if (maxSize.isZero())
        maxSize = fallbackMaxSurfaceDimension();

    return maxSize;
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

static WTF::Atomic<size_t>& surfaceBytesPerRowAlignment()
{
    static WTF::Atomic<size_t> alignment = 0;
    return alignment;
}

size_t IOSurface::bytesPerRowAlignment()
{
    auto alignment = surfaceBytesPerRowAlignment().load();
    if (!alignment) {
        alignment = IOSurfaceGetPropertyAlignment(kIOSurfaceBytesPerRow);

        // A return value for IOSurfaceGetPropertyAlignment(kIOSurfaceBytesPerRow) of 1 is invalid.
        // See https://developer.apple.com/documentation/iosurface/1419453-iosurfacegetpropertyalignment?language=objc
        // This likely means that the sandbox is blocking access to the IOSurface IOKit class,
        // and that IOSurface::bytesPerRowAlignment() has been called before IOSurface::setBytesPerRowAlignment.
        if (alignment <= 1) {
            RELEASE_LOG_ERROR(Layers, "Sandbox does not allow IOSurface IOKit access.");
            // 64 bytes is currently the alignment on all platforms.
            alignment = 64;
        }

        surfaceBytesPerRowAlignment().store(alignment);
    }
    return alignment;
}

void IOSurface::setBytesPerRowAlignment(size_t bytesPerRowAlignment)
{
    surfaceBytesPerRowAlignment().store(bytesPerRowAlignment);
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

IOSurface::BitmapConfiguration IOSurface::bitmapConfiguration() const
{
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);

    size_t bitsPerComponent = 8;
    
    switch (format()) {
    case Format::BGRA:
        break;
#if HAVE(IOSURFACE_RGB10)
    case Format::RGB10:
    case Format::RGB10A8:
        // A half-float format will be used if CG needs to read back the IOSurface contents,
        // but for an IOSurface-to-IOSurface copy, there should be no conversion.
        bitsPerComponent = 16;
        bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents);
        break;
#endif
    case Format::YUV422:
        ASSERT_NOT_REACHED();
        break;
    }

    return { bitmapInfo, bitsPerComponent };
}

RetainPtr<CGContextRef> IOSurface::createCompatibleBitmap(unsigned width, unsigned height)
{
    auto configuration = bitmapConfiguration();
    auto bitsPerPixel = configuration.bitsPerComponent * 4;
    auto bytesPerRow = width * bitsPerPixel;

    ensureColorSpace();
    return adoptCF(CGBitmapContextCreate(NULL, width, height, configuration.bitsPerComponent, bytesPerRow, m_colorSpace->platformColorSpace(), configuration.bitmapInfo));
}

CGContextRef IOSurface::ensurePlatformContext(PlatformDisplayID displayID)
{
    if (m_cgContext)
        return m_cgContext.get();

    auto configuration = bitmapConfiguration();
    auto bitsPerPixel = configuration.bitsPerComponent * 4;

    ensureColorSpace();
    m_cgContext = adoptCF(CGIOSurfaceContextCreate(m_surface.get(), m_size.width(), m_size.height(), configuration.bitsPerComponent, bitsPerPixel, m_colorSpace->platformColorSpace(), configuration.bitmapInfo));

#if PLATFORM(MAC)
    if (auto displayMask = primaryOpenGLDisplayMask()) {
        if (displayID)
            displayMask = displayMaskForDisplay(displayID);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CGIOSurfaceContextSetDisplayMask(m_cgContext.get(), displayMask);
ALLOW_DEPRECATED_DECLARATIONS_END
    }
#else
    UNUSED_PARAM(displayID);
#endif
#if HAVE(CG_CONTEXT_SET_OWNER_IDENTITY)
    if (m_resourceOwner && CGContextSetOwnerIdentity)
        CGContextSetOwnerIdentity(m_cgContext.get(), m_resourceOwner.taskIdToken());
#endif
    return m_cgContext.get();
}

GraphicsContext& IOSurface::ensureGraphicsContext()
{
    if (m_graphicsContext)
        return *m_graphicsContext;

    m_graphicsContext = makeUnique<GraphicsContextCG>(ensurePlatformContext());
    m_graphicsContext->setIsAcceleratedContext(true);

    return *m_graphicsContext;
}

SetNonVolatileResult IOSurface::state() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState == kIOSurfacePurgeableEmpty ? SetNonVolatileResult::Empty : SetNonVolatileResult::Valid;
}

IOSurfaceSeed IOSurface::seed() const
{
    return IOSurfaceGetSeed(m_surface.get());
}

bool IOSurface::isVolatile() const
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), kIOSurfacePurgeableKeepCurrent, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);
    return previousState != kIOSurfacePurgeableNonVolatile;
}

SetNonVolatileResult IOSurface::setVolatile(bool isVolatile)
{
    uint32_t previousState = 0;
    IOReturn ret = IOSurfaceSetPurgeable(m_surface.get(), isVolatile ? kIOSurfacePurgeableVolatile : kIOSurfacePurgeableNonVolatile, &previousState);
    ASSERT_UNUSED(ret, ret == kIOReturnSuccess);

    if (previousState == kIOSurfacePurgeableEmpty)
        return SetNonVolatileResult::Empty;

    return SetNonVolatileResult::Valid;
}

DestinationColorSpace IOSurface::colorSpace()
{
    ensureColorSpace();
    return *m_colorSpace;
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

void IOSurface::convertToFormat(IOSurfacePool* pool, std::unique_ptr<IOSurface>&& inSurface, Format format, WTF::Function<void(std::unique_ptr<IOSurface>)>&& callback)
{
    static IOSurfaceAcceleratorRef accelerator;
    if (!accelerator) {
        IOSurfaceAcceleratorCreate(nullptr, nullptr, &accelerator);

        if (!accelerator) {
            callback(nullptr);
            return;
        }

        auto runLoopSource = IOSurfaceAcceleratorGetRunLoopSource(accelerator);
        CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopDefaultMode);
    }

    if (inSurface->format() == format) {
        callback(WTFMove(inSurface));
        return;
    }

    auto destinationSurface = IOSurface::create(pool, inSurface->size(), inSurface->colorSpace(), format);
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

void IOSurface::setOwnershipIdentity(const ProcessIdentity& resourceOwner)
{
    ASSERT(resourceOwner);
    m_resourceOwner = resourceOwner;
    setOwnershipIdentity(m_surface.get(), resourceOwner);
#if HAVE(CG_CONTEXT_SET_OWNER_IDENTITY)
    if (m_cgContext && CGContextSetOwnerIdentity)
        CGContextSetOwnerIdentity(m_cgContext.get(), m_resourceOwner.taskIdToken());
#endif
}

void IOSurface::setOwnershipIdentity(IOSurfaceRef surface, const ProcessIdentity& resourceOwner)
{
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
    ASSERT(resourceOwner);
    ASSERT(surface);
    task_id_token_t ownerTaskIdToken = resourceOwner.taskIdToken();
    auto result = IOSurfaceSetOwnershipIdentity(surface, ownerTaskIdToken, kIOSurfaceMemoryLedgerTagGraphics, 0);
    if (result != kIOReturnSuccess)
        RELEASE_LOG_ERROR(IOSurface, "IOSurface::setOwnershipIdentity: Failed to claim ownership of IOSurface %p, task id token: %d, error: %d", surface, (int)ownerTaskIdToken, result);
#else
    UNUSED_PARAM(surface);
    UNUSED_PARAM(resourceOwner);
#endif
}

void IOSurface::setColorSpaceProperty()
{
    ASSERT(m_colorSpace);
    auto colorSpaceProperties = adoptCF(CGColorSpaceCopyPropertyList(m_colorSpace->platformColorSpace()));
    IOSurfaceSetValue(m_surface.get(), kIOSurfaceColorSpace, colorSpaceProperties.get());
}

void IOSurface::ensureColorSpace()
{
    if (m_colorSpace)
        return;

    m_colorSpace = surfaceColorSpace().value_or(DestinationColorSpace::SRGB());
}

std::optional<DestinationColorSpace> IOSurface::surfaceColorSpace() const
{
    auto propertyList = adoptCF(IOSurfaceCopyValue(m_surface.get(), kIOSurfaceColorSpace));
    if (!propertyList)
        return { };
    
    auto colorSpaceCF = adoptCF(CGColorSpaceCreateWithPropertyList(propertyList.get()));
    if (!colorSpaceCF)
        return { };
    
    return DestinationColorSpace { colorSpaceCF };
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

TextStream& operator<<(TextStream& ts, IOSurface::Format format)
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

static TextStream& operator<<(TextStream& ts, SetNonVolatileResult state)
{
    switch (state) {
    case SetNonVolatileResult::Valid:
        ts << "valid";
        break;
    case SetNonVolatileResult::Empty:
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
