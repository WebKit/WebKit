/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#pragma once

#if HAVE(IOSURFACE)

#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/IntSize.h>
#include <WebCore/PixelFormat.h>
#include <WebCore/ProcessIdentity.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WTF {
class MachSendRight;
class TextStream;
}

namespace WebCore {

class IOSurfacePool;

enum class RenderingPurpose : uint8_t;
enum class SetNonVolatileResult : uint8_t;

using IOSurfaceSeed = uint32_t;
using PlatformDisplayID = uint32_t;

class IOSurface final {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(IOSurface, WEBCORE_EXPORT);
public:
    enum class Name : uint8_t {
        Default,
        DOM,
        Canvas,
        GraphicsContextGL,
        ImageBuffer,
        ImageBufferShareableMapped,
        LayerBacking,
        MediaPainting,
        Snapshot,
        ShareableSnapshot,
        ShareableLocalSnapshot,
        WebGPU,
    };

    enum class Format {
        BGRX,
        BGRA,
        YUV422,
        RGBA,
        RGBX,
#if ENABLE(PIXEL_FORMAT_RGB10)
        RGB10,
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
        RGB10A8,
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        RGBA16F,
#endif
    };

    struct UsedFormat {
        Format format;
        UseLosslessCompression useLosslessCompression;
        bool operator==(const UsedFormat&) const = default;
    };

    enum class AccessMode : uint32_t {
        ReadWrite = 0,
        ReadOnly = kIOSurfaceLockReadOnly
    };
    template <AccessMode Mode>
    class Locker {
    public:
        static Locker adopt(RetainPtr<IOSurfaceRef> surface)
        {
            return Locker { WTFMove(surface) };
        }

        Locker(Locker&& other)
            : m_surface(std::exchange(other.m_surface, nullptr))
        {
        }

        ~Locker()
        {
            if (!m_surface)
                return;
            IOSurfaceUnlock(m_surface.get(), static_cast<uint32_t>(Mode), nullptr);
        }

        Locker& operator=(Locker&& other)
        {
            m_surface = std::exchange(other.m_surface, nullptr);
            return *this;
        }

        void* surfaceBaseAddress() const
        {
            return IOSurfaceGetBaseAddress(m_surface.get());
        }

        std::span<uint8_t> surfaceSpan()
        {
            return unsafeMakeSpan(static_cast<uint8_t*>(IOSurfaceGetBaseAddress(m_surface.get())), IOSurfaceGetAllocSize(m_surface.get()));
        }

        std::span<const uint8_t> surfaceSpan() const
        {
            return unsafeMakeSpan(static_cast<const uint8_t*>(IOSurfaceGetBaseAddress(m_surface.get())), IOSurfaceGetAllocSize(m_surface.get()));
        }

    private:
        explicit Locker(RetainPtr<IOSurfaceRef> surface)
            : m_surface(WTFMove(surface))
        {
        }

        RetainPtr<IOSurfaceRef> m_surface;
    };

    WEBCORE_EXPORT static std::unique_ptr<IOSurface> create(IOSurfacePool*, IntSize, const DestinationColorSpace&, Name = Name::Default, Format = Format::BGRA, UseLosslessCompression = UseLosslessCompression::No);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromImage(IOSurfacePool*, CGImageRef);

    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSendRight(const WTF::MachSendRight&&);
    // If the colorSpace argument is non-null, it replaces any colorspace metadata on the surface.
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSurface(IOSurfaceRef, std::optional<DestinationColorSpace>&&);

    WEBCORE_EXPORT static void moveToPool(std::unique_ptr<IOSurface>&&, IOSurfacePool*);

    WEBCORE_EXPORT ~IOSurface();

    WEBCORE_EXPORT static IntSize maximumSize();
    WEBCORE_EXPORT static void setMaximumSize(IntSize);

    WEBCORE_EXPORT static size_t bytesPerRowAlignment();
    WEBCORE_EXPORT static void setBytesPerRowAlignment(size_t);

    WEBCORE_EXPORT WTF::MachSendRight createSendRight() const;

    // Any images created from a surface need to be released before releasing
    // the context, or an expensive GPU readback can result.
    // Passed in context is the context through which the contents was drawn.
    WEBCORE_EXPORT RetainPtr<CGImageRef> createImage(CGContextRef);
    // Passed in context is the context through which the contents was drawn.
    WEBCORE_EXPORT static RetainPtr<CGImageRef> sinkIntoImage(std::unique_ptr<IOSurface>, RetainPtr<CGContextRef> = nullptr);

    WEBCORE_EXPORT static Name nameForRenderingPurpose(RenderingPurpose);
    Name name() const { return m_name; }

#ifdef __OBJC__
    id asLayerContents() const { return (__bridge id)m_surface.get(); }
#endif
    WEBCORE_EXPORT RetainPtr<id> asCAIOSurfaceLayerContents() const;

    IOSurfaceRef surface() const { return m_surface.get(); }
    RetainPtr<IOSurfaceRef> protectedSurface() const { return surface(); }

    WEBCORE_EXPORT RetainPtr<CGContextRef> createPlatformContext(PlatformDisplayID = 0, std::optional<CGImageAlphaInfo> = std::nullopt);

    struct LockAndContext {
        IOSurface::Locker<AccessMode::ReadWrite> lock;
        RetainPtr<CGContextRef> context;
    };
    WEBCORE_EXPORT std::optional<LockAndContext> createBitmapPlatformContext();
    template<AccessMode Mode> std::optional<Locker<Mode>> lock();

    // Querying volatility can be expensive, so in cases where the surface is
    // going to be used immediately, use the return value of setVolatile to
    // determine whether the data was purged, instead of first calling state() or isVolatile().
    SetNonVolatileResult state() const;
    WEBCORE_EXPORT bool isVolatile() const;

    WEBCORE_EXPORT SetNonVolatileResult setVolatile(bool);

    bool hasFormat(UsedFormat format) const { return m_format && *m_format == format; }
    std::optional<Format> pixelFormat() const
    {
        if (m_format)
            return m_format->format;
        return std::nullopt;
    }

    std::optional<UseLosslessCompression> usesLosslessCompression() const
    {
        if (m_format)
            return m_format->useLosslessCompression;
        return std::nullopt;
    }

    IntSize size() const { return m_size; }
    size_t totalBytes() const { return m_totalBytes; }

#if HAVE(SUPPORT_HDR_DISPLAY)
    WEBCORE_EXPORT void setContentEDRHeadroom(float);
    WEBCORE_EXPORT std::optional<float> contentEDRHeadroom() const;
    WEBCORE_EXPORT void loadContentEDRHeadroom();
#endif

    WEBCORE_EXPORT DestinationColorSpace colorSpace();
    WEBCORE_EXPORT IOSurfaceID surfaceID() const;
    WEBCORE_EXPORT size_t bytesPerRow() const;

    WEBCORE_EXPORT IOSurfaceSeed seed() const;

    WEBCORE_EXPORT bool isInUse() const;

#if HAVE(IOSURFACE_ACCELERATOR)
    WEBCORE_EXPORT static bool allowConversionFromFormatToFormat(Format, Format);
    WEBCORE_EXPORT static void convertToFormat(IOSurfacePool*, std::unique_ptr<WebCore::IOSurface>&& inSurface, Name, Format, Function<void(std::unique_ptr<WebCore::IOSurface>)>&&);
#endif // HAVE(IOSURFACE_ACCELERATOR)

    WEBCORE_EXPORT void setOwnershipIdentity(const ProcessIdentity&);
    WEBCORE_EXPORT static void setOwnershipIdentity(IOSurfaceRef, const ProcessIdentity&);

    RetainPtr<CGContextRef> createCompatibleBitmap(unsigned width, unsigned height);

private:
    IOSurface(IntSize, const DestinationColorSpace&, Name, Format, UseLosslessCompression, bool& success);
    IOSurface(IOSurfaceRef, std::optional<DestinationColorSpace>&&);

    void setColorSpaceProperty();
    void ensureColorSpace();
    std::optional<DestinationColorSpace> surfaceColorSpace() const;

    void setName(Name name) { m_name = name; }

    struct BitmapConfiguration {
        CGBitmapInfo bitmapInfo;
        size_t bitsPerComponent;
    };

    BitmapConfiguration bitmapConfiguration() const;

    std::optional<UsedFormat> m_format;
    std::optional<DestinationColorSpace> m_colorSpace;
    IntSize m_size;
    size_t m_totalBytes;
#if HAVE(SUPPORT_HDR_DISPLAY)
    std::optional<float> m_contentEDRHeadroom;
#endif

    ProcessIdentity m_resourceOwner;

    RetainPtr<IOSurfaceRef> m_surface;

    static std::optional<IntSize> s_maximumSize;

    Name m_name;

    WEBCORE_EXPORT friend WTF::TextStream& operator<<(WTF::TextStream&, const WebCore::IOSurface&);
};

template<IOSurface::AccessMode Mode>
std::optional<IOSurface::Locker<Mode>> IOSurface::lock()
{
    if (IOSurfaceLock(m_surface.get(), static_cast<uint32_t>(Mode), nullptr) != kIOReturnSuccess)
        return std::nullopt;
    return IOSurface::Locker<Mode>::adopt(m_surface);
}

constexpr IOSurface::Format convertToIOSurfaceFormat(PixelFormat format)
{
    switch (format) {
    case PixelFormat::RGBA8:
        return IOSurface::Format::RGBA;
    case PixelFormat::BGRX8:
        return IOSurface::Format::BGRX;
    case PixelFormat::BGRA8:
        return IOSurface::Format::BGRA;
#if ENABLE(PIXEL_FORMAT_RGB10)
    case PixelFormat::RGB10:
        return IOSurface::Format::RGB10;
#endif
#if ENABLE(PIXEL_FORMAT_RGB10A8)
    case PixelFormat::RGB10A8:
        return IOSurface::Format::RGB10A8;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F:
        return IOSurface::Format::RGBA16F;
#endif
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return IOSurface::Format::BGRA;
    }
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WebCore::IOSurface::Format);

} // namespace WebCore

#endif // HAVE(IOSURFACE)

