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
#import "HostWindow.h"
#import "IOSurfacePool.h"
#import "ImageBufferBackend.h"
#import "Logging.h"
#import "PlatformScreen.h"
#import "ProcessCapabilities.h"
#import "ProcessIdentity.h"
#import "SharedMemory.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/Assertions.h>
#import <wtf/EnumTraits.h>
#import <wtf/MachSendRight.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/TextStream.h>

#import "CoreVideoSoftLink.h"
#import <pal/cg/CoreGraphicsSoftLink.h>
#import <pal/cocoa/QuartzCoreSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IOSurface);

static auto surfaceNameToNSString(IOSurface::Name name)
{
    switch (name) {
    case IOSurface::Name::Default:
        return @"WebKit";
    case IOSurface::Name::DOM:
        return @"WebKit DOM";
    case IOSurface::Name::Canvas:
        return @"WebKit Canvas";
    case IOSurface::Name::GraphicsContextGL:
        return @"WebKit GraphicsContextGL";
    case IOSurface::Name::ImageBuffer:
        return @"WebKit ImageBuffer";
    case IOSurface::Name::ImageBufferShareableMapped:
        return @"WebKit ImageBufferShareableMapped";
    case IOSurface::Name::LayerBacking:
        return @"WebKit LayerBacking";
    case IOSurface::Name::MediaPainting:
        return @"WebKit MediaPainting";
    case IOSurface::Name::Snapshot:
        return @"WKWebView Snapshot";
    case IOSurface::Name::ShareableSnapshot:
        return @"WKWebView Snapshot (shareable)";
    case IOSurface::Name::ShareableLocalSnapshot:
        return @"WKWebView Snapshot (shareable local)";
    case IOSurface::Name::WebGPU:
        return @"WebKit WebGPU";
    }
}

std::unique_ptr<IOSurface> IOSurface::create(IOSurfacePool* pool, IntSize size, const DestinationColorSpace& colorSpace, IOSurface::Name name, Format pixelFormat)
{
    ASSERT(ProcessCapabilities::canUseAcceleratedBuffers());

    if (pool) {
        if (auto cachedSurface = pool->takeSurface(size, colorSpace, pixelFormat)) {
            LOG_WITH_STREAM(IOSurface, stream << "IOSurface::create took from pool: " << *cachedSurface);
            if (cachedSurface->name() != name) {
                IOSurfaceSetValue(cachedSurface->surface(), kIOSurfaceName, surfaceNameToNSString(name));
                cachedSurface->setName(name);
            }
            return cachedSurface;
        }
    }

    bool success = false;
    auto surface = std::unique_ptr<IOSurface>(new IOSurface(size, colorSpace, name, pixelFormat, success));
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

    auto surface = IOSurface::create(pool, IntSize(width, height), DestinationColorSpace { CGImageGetColorSpace(image) }, Name::ImageBuffer);
    if (!surface)
        return nullptr;
    auto context = surface->createPlatformContext();
    CGContextDrawImage(context.get(), CGRectMake(0, 0, width, height), image);
    return surface;
}

void IOSurface::moveToPool(std::unique_ptr<IOSurface>&& surface, IOSurfacePool* pool)
{
    if (pool)
        pool->addSurface(WTFMove(surface));
}

static NSDictionary *optionsForBiplanarSurface(IntSize size, unsigned pixelFormat, size_t firstPlaneBytesPerPixel, size_t secondPlaneBytesPerPixel, IOSurface::Name name)
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
        (id)kIOSurfaceName: surfaceNameToNSString(name)
    };
}

static NSDictionary *optionsForSurface(IntSize size, unsigned bitsPerPixel, unsigned pixelFormat, IOSurface::Name name)
{
    int width = size.width();
    int height = size.height();

    unsigned bytesPerElement = 4 * (bitsPerPixel / 32);
    unsigned bytesPerPixel = bytesPerElement;

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
        (id)kIOSurfaceElementHeight: @(1),
        (id)kIOSurfaceName: surfaceNameToNSString(name)
    };
}

static NSDictionary *optionsFor32BitSurface(IntSize size, unsigned pixelFormat, IOSurface::Name name)
{
    return optionsForSurface(size, 32, pixelFormat, name);
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)
static NSDictionary *optionsFor64BitSurface(IntSize size, unsigned pixelFormat, IOSurface::Name name)
{
    return optionsForSurface(size, 64, pixelFormat, name);
}
#endif

IOSurface::IOSurface(IntSize size, const DestinationColorSpace& colorSpace, IOSurface::Name name, Format format, bool& success)
    : m_format(format)
    , m_colorSpace(colorSpace)
    , m_size(size)
    , m_name(name)
{
    ASSERT(!success);
    ASSERT(!size.isEmpty());

    NSDictionary *options;

    switch (format) {
    case Format::BGRX:
    case Format::BGRA:
        options = optionsFor32BitSurface(size, 'BGRA', name);
        break;
    case Format::YUV422:
        options = optionsForBiplanarSurface(size, '422f', 1, 1, name);
        break;
    case Format::RGBX:
    case Format::RGBA:
        options = optionsFor32BitSurface(size, 'RGBA', name);
        break;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case Format::RGB10:
        options = optionsFor32BitSurface(size, 'w30r', name);
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case Format::RGB10A8:
        options = optionsForBiplanarSurface(size, 'b3a8', 4, 1, name);
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case Format::RGBA16F:
        options = optionsFor64BitSurface(size, 'RGhA', name);
        break;
#endif
    }
    m_surface = adoptCF(IOSurfaceCreate((CFDictionaryRef)options));
    success = !!m_surface;
    if (success) {
        setColorSpaceProperty();
        m_totalBytes = IOSurfaceGetAllocSize(m_surface.get());
    } else
        RELEASE_LOG_ERROR(Layers, "IOSurface creation failed for size: (%d %d) and format: (%d)", size.width(), size.height(), enumToUnderlyingType(*m_format));
}

static std::optional<IOSurface::Format> formatFromSurface(IOSurfaceRef surface)
{
    unsigned pixelFormat = IOSurfaceGetPixelFormat(surface);
    if (pixelFormat == 'BGRA')
        return IOSurface::Format::BGRA;
    if (pixelFormat == '422f')
        return IOSurface::Format::YUV422;
    if (pixelFormat == 'RGBA')
        return IOSurface::Format::RGBA;
#if ENABLE(PIXEL_FORMAT_RGB10)
    if (pixelFormat == 'w30r')
        return IOSurface::Format::RGB10;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    if (pixelFormat == 'b3a8')
        return IOSurface::Format::RGB10A8;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (pixelFormat == 'RGhA')
        return IOSurface::Format::RGBA16F;
#endif

    return { };
}

IOSurface::IOSurface(IOSurfaceRef surface, std::optional<DestinationColorSpace>&& colorSpace)
    : m_format(formatFromSurface(surface))
    , m_colorSpace(WTFMove(colorSpace))
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

    // On iOS, there's an additional 8K clamp in CA (rdar://101936907).
    // On some macOS VMs, IOSurfaceGetPropertyMaximum() returns INT_MAX (rdar://113661708).
    maxSize.clampToMaximumSize(fallbackMaxSurfaceDimension());

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

RetainPtr<id> IOSurface::asCAIOSurfaceLayerContents() const
{
    // CAIOSurface keeps most of the server-side rendering ojects alive,
    // but doesn't mark the IOSurface as in-use. We can retain it for efficiency
    // without breaking use-counting.
    if (PAL::canLoad_QuartzCore_CAIOSurfaceCreate()) {
        auto result = adoptCF(CAIOSurfaceCreate(m_surface.get()));
#if HAVE(SUPPORT_HDR_DISPLAY)
        // Force CA to reload the headroom, since this doesn't happen automatically.
        if (m_contentEDRHeadroom && *m_contentEDRHeadroom != 1 && PAL::canLoad_QuartzCore_CAIOSurfaceReloadColorAttributes())
            CAIOSurfaceReloadColorAttributes(result.get());
#endif
        return bridge_id_cast(result);
    }
    return asLayerContents();
}

RetainPtr<CGImageRef> IOSurface::createImage(CGContextRef context)
{
    ASSERT(CGIOSurfaceContextGetSurface(context) == m_surface);
    return adoptCF(CGIOSurfaceContextCreateImage(context));
}

RetainPtr<CGImageRef> IOSurface::sinkIntoImage(std::unique_ptr<IOSurface> surface, RetainPtr<CGContextRef> context)
{
    if (!context)
        context = surface->createPlatformContext();
    ASSERT(CGIOSurfaceContextGetSurface(context.get()) == surface->m_surface);
    UNUSED_PARAM(surface);
    return adoptCF(CGIOSurfaceContextCreateImageReference(context.get()));
}

IOSurface::BitmapConfiguration IOSurface::bitmapConfiguration() const
{
    auto bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);

    size_t bitsPerComponent = 8;

    ASSERT(m_format);

    switch (m_format.value_or(Format::BGRA)) {
    case Format::BGRX:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);
        break;
    case Format::BGRA:
        break;
    case Format::YUV422:
        ASSERT_NOT_REACHED();
        break;
    case Format::RGBX:
        bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Host);
        break;
    case Format::RGBA:
        break;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case Format::RGB10:
        // A half-float format will be used if CG needs to read back the IOSurface contents,
        // but for an IOSurface-to-IOSurface copy, there should be no conversion.
        bitsPerComponent = 16;
        bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents);
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case Format::RGB10A8:
        // A half-float format will be used if CG needs to read back the IOSurface contents,
        // but for an IOSurface-to-IOSurface copy, there should be no conversion.
        bitsPerComponent = 16;
        bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents);
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case Format::RGBA16F:
        bitsPerComponent = 16;
        bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder16Host) | static_cast<CGBitmapInfo>(kCGBitmapFloatComponents);
        break;
#endif
    }

    return { bitmapInfo, bitsPerComponent };
}

RetainPtr<CGContextRef> IOSurface::createCompatibleBitmap(unsigned width, unsigned height)
{
    auto configuration = bitmapConfiguration();
    auto bitsPerPixel = configuration.bitsPerComponent * 4;
    auto bytesPerRow = roundUpToMultipleOfNonPowerOfTwo(bytesPerRowAlignment(), width * (bitsPerPixel / 8));

    ensureColorSpace();
    return adoptCF(CGBitmapContextCreate(NULL, width, height, configuration.bitsPerComponent, bytesPerRow, m_colorSpace->platformColorSpace(), configuration.bitmapInfo));
}

RetainPtr<CGContextRef> IOSurface::createPlatformContext(PlatformDisplayID displayID, std::optional<CGImageAlphaInfo> overrideAlphaInfo)
{
    auto configuration = bitmapConfiguration();
    if (overrideAlphaInfo)
        configuration.bitmapInfo = (configuration.bitmapInfo & ~kCGBitmapAlphaInfoMask) | *overrideAlphaInfo;
    auto bitsPerPixel = configuration.bitsPerComponent * 4;

    ensureColorSpace();
    auto cgContext = adoptCF(CGIOSurfaceContextCreate(m_surface.get(), m_size.width(), m_size.height(), configuration.bitsPerComponent, bitsPerPixel, m_colorSpace->platformColorSpace(), configuration.bitmapInfo));

#if PLATFORM(MAC)
    if (auto displayMask = primaryOpenGLDisplayMask()) {
        if (displayID)
            displayMask = displayMaskForDisplay(displayID);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CGIOSurfaceContextSetDisplayMask(cgContext.get(), displayMask); // NOLINT
ALLOW_DEPRECATED_DECLARATIONS_END
    }
#else
    UNUSED_PARAM(displayID);
#endif
#if HAVE(CG_CONTEXT_SET_OWNER_IDENTITY)
    if (m_resourceOwner && CGContextSetOwnerIdentity)
        CGContextSetOwnerIdentity(cgContext.get(), m_resourceOwner.taskIdToken());
#endif
    return cgContext;
}

std::optional<IOSurface::LockAndContext> IOSurface::createBitmapPlatformContext()
{
    auto locker = lock<AccessMode::ReadWrite>();
    if (!locker)
        return std::nullopt;
    auto configuration = bitmapConfiguration();
    auto size = this->size();
    auto context = adoptCF(CGBitmapContextCreate(locker->surfaceBaseAddress(), size.width(), size.height(), configuration.bitsPerComponent, bytesPerRow(), colorSpace().platformColorSpace(), configuration.bitmapInfo));
    if (!context)
        return std::nullopt;
    return LockAndContext { WTFMove(*locker), WTFMove(context) };
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

IOSurfaceID IOSurface::surfaceID() const
{
    return IOSurfaceGetID(m_surface.get());
}

size_t IOSurface::bytesPerRow() const
{
    return IOSurfaceGetBytesPerRow(m_surface.get());
}

bool IOSurface::isInUse() const
{
    return IOSurfaceIsInUse(m_surface.get());
}

#if HAVE(IOSURFACE_ACCELERATOR)

bool IOSurface::allowConversionFromFormatToFormat(Format sourceFormat, Format destFormat)
{
#if ENABLE(PIXEL_FORMAT_RGB10)
    if (sourceFormat == Format::RGB10 && destFormat == Format::YUV422)
        return false;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    if (sourceFormat == Format::RGB10A8 && destFormat == Format::YUV422)
        return false;
#endif

    return true;
}

void IOSurface::convertToFormat(IOSurfacePool* pool, std::unique_ptr<IOSurface>&& inSurface, Name name, Format format, WTF::Function<void(std::unique_ptr<IOSurface>)>&& callback)
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

    if (inSurface->hasFormat(format)) {
        callback(WTFMove(inSurface));
        return;
    }

    auto destinationSurface = IOSurface::create(pool, inSurface->size(), inSurface->colorSpace(), name, format);
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
}

void IOSurface::setOwnershipIdentity(IOSurfaceRef surface, const ProcessIdentity& resourceOwner)
{
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
#if ASSERT_ENABLED
    ASSERT(surface);
    if (!isMemoryAttributionDisabled())
        ASSERT(resourceOwner);
#endif

    if (!resourceOwner)
        return;
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

#if HAVE(SUPPORT_HDR_DISPLAY)
void IOSurface::setContentEDRHeadroom(float headroom)
{
    if (m_contentEDRHeadroom && headroom == m_contentEDRHeadroom)
        return;

    LOG_WITH_STREAM(HDR, stream << "IOSurface::setContentEDRHeadroom " << this << " " << headroom);
    m_contentEDRHeadroom = headroom;
    IOSurfaceSetValue(m_surface.get(), kIOSurfaceContentHeadroom, @(headroom));
}

std::optional<float> IOSurface::contentEDRHeadroom() const
{
    return m_contentEDRHeadroom;
}

void IOSurface::loadContentEDRHeadroom()
{
    m_contentEDRHeadroom = 1;
    if (auto valueNumber = dynamic_cf_cast<CFNumberRef>(adoptCF(IOSurfaceCopyValue(m_surface.get(), kIOSurfaceContentHeadroom))))
        CFNumberGetValue(valueNumber.get(), kCFNumberFloat32Type, &m_contentEDRHeadroom.value());
}
#endif

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

IOSurface::Name IOSurface::nameForRenderingPurpose(RenderingPurpose purpose)
{
    switch (purpose) {
    case RenderingPurpose::Unspecified:
        return Name::ImageBufferShareableMapped;

    case RenderingPurpose::Canvas:
        return Name::Canvas;

    case RenderingPurpose::DOM:
        return Name::DOM;

    case RenderingPurpose::LayerBacking:
        return Name::LayerBacking;

    case RenderingPurpose::Snapshot:
        return Name::Snapshot;

    case RenderingPurpose::ShareableSnapshot:
        return Name::ShareableSnapshot;

    case RenderingPurpose::ShareableLocalSnapshot:
        return Name::ShareableLocalSnapshot;

    case RenderingPurpose::MediaPainting:
        return Name::MediaPainting;
    }

    return Name::Default;
}

TextStream& operator<<(TextStream& ts, IOSurface::Format format)
{
    switch (format) {
    case IOSurface::Format::BGRX:
        ts << "BGRX"_s;
        break;
    case IOSurface::Format::BGRA:
        ts << "BGRA"_s;
        break;
    case IOSurface::Format::YUV422:
        ts << "YUV422"_s;
        break;
    case IOSurface::Format::RGBX:
        ts << "RGBX"_s;
        break;
    case IOSurface::Format::RGBA:
        ts << "RGBA"_s;
        break;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case IOSurface::Format::RGB10:
        ts << "RGB10"_s;
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case IOSurface::Format::RGB10A8:
        ts << "RGB10A8"_s;
        break;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case IOSurface::Format::RGBA16F:
        ts << "RGBA16F"_s;
        break;
#endif
    }
    return ts;
}

static TextStream& operator<<(TextStream& ts, SetNonVolatileResult state)
{
    switch (state) {
    case SetNonVolatileResult::Valid:
        ts << "valid"_s;
        break;
    case SetNonVolatileResult::Empty:
        ts << "empty"_s;
        break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const IOSurface& surface)
{
    return ts << "IOSurface "_s << surface.surfaceID() << " name "_s << [surfaceNameToNSString(surface.name()) UTF8String] << " size "_s << surface.size() << " format "_s << surface.m_format << " state "_s << surface.state();
}

} // namespace WebCore
