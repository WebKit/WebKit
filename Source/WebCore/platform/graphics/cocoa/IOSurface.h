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

#pragma once

#if HAVE(IOSURFACE)

#include "DestinationColorSpace.h"
#include "IntSize.h"
#include "ProcessIdentity.h"
#include <objc/objc.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#define HAVE_IOSURFACE_RGB10 1
#endif

namespace WTF {
class MachSendRight;
class TextStream;
}

namespace WebCore {

class GraphicsContext;
class HostWindow;
class IOSurfacePool;

enum class PixelFormat : uint8_t;
enum class SetNonVolatileResult : uint8_t;

using IOSurfaceSeed = uint32_t;

class IOSurface final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Format {
        BGRA,
        YUV422,
#if HAVE(IOSURFACE_RGB10)
        RGB10,
        RGB10A8,
#endif
    };

    WEBCORE_EXPORT static IOSurface::Format formatForPixelFormat(WebCore::PixelFormat);
    
    class Locker {
    public:
        enum class AccessMode {
            ReadOnly,
            ReadWrite
        };

        Locker(IOSurface& surface, AccessMode mode = AccessMode::ReadOnly)
            : m_surface(surface)
            , m_flags(flagsFromMode(mode))
        {
            IOSurfaceLock(m_surface.surface(), m_flags, nullptr);
        }

        ~Locker()
        {
            IOSurfaceUnlock(m_surface.surface(), m_flags, nullptr);
        }

        void * surfaceBaseAddress() const
        {
            return IOSurfaceGetBaseAddress(m_surface.surface());
        }

    private:
        static uint32_t flagsFromMode(AccessMode mode)
        {
            return mode == AccessMode::ReadOnly ? kIOSurfaceLockReadOnly : 0;
        }
        IOSurface& m_surface;
        uint32_t m_flags;
    };

    WEBCORE_EXPORT static std::unique_ptr<IOSurface> create(IOSurfacePool*, IntSize, const DestinationColorSpace&, Format = Format::BGRA);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromImage(IOSurfacePool*, CGImageRef);

    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSendRight(const WTF::MachSendRight&&, const DestinationColorSpace&);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSurface(IOSurfaceRef, const DestinationColorSpace&);

    WEBCORE_EXPORT static void moveToPool(std::unique_ptr<IOSurface>&&, IOSurfacePool*);

    WEBCORE_EXPORT ~IOSurface();

    WEBCORE_EXPORT static IntSize maximumSize();
    WEBCORE_EXPORT static void setMaximumSize(IntSize);

    WEBCORE_EXPORT static size_t bytesPerRowAlignment();
    WEBCORE_EXPORT static void setBytesPerRowAlignment(size_t);

    WEBCORE_EXPORT WTF::MachSendRight createSendRight() const;

    // Any images created from a surface need to be released before releasing
    // the surface, or an expensive GPU readback can result.
    WEBCORE_EXPORT RetainPtr<CGImageRef> createImage();
    WEBCORE_EXPORT static RetainPtr<CGImageRef> sinkIntoImage(std::unique_ptr<IOSurface>);

#ifdef __OBJC__
    id asLayerContents() const { return (__bridge id)m_surface.get(); }
#endif
    IOSurfaceRef surface() const { return m_surface.get(); }
    WEBCORE_EXPORT GraphicsContext& ensureGraphicsContext();
    WEBCORE_EXPORT CGContextRef ensurePlatformContext(const HostWindow* = nullptr);

    // Querying volatility can be expensive, so in cases where the surface is
    // going to be used immediately, use the return value of setVolatile to
    // determine whether the data was purged, instead of first calling state() or isVolatile().
    SetNonVolatileResult state() const;
    bool isVolatile() const;

    WEBCORE_EXPORT SetNonVolatileResult setVolatile(bool);

    IntSize size() const { return m_size; }
    size_t totalBytes() const { return m_totalBytes; }

    const DestinationColorSpace& colorSpace() const { return m_colorSpace; }
    WEBCORE_EXPORT Format format() const;
    WEBCORE_EXPORT IOSurfaceID surfaceID() const;
    size_t bytesPerRow() const;

    WEBCORE_EXPORT IOSurfaceSeed seed() const;

    WEBCORE_EXPORT bool isInUse() const;

    // The graphics context cached on the surface counts as a "user", so to get
    // an accurate result from isInUse(), it needs to be released.
    WEBCORE_EXPORT void releaseGraphicsContext();

#if HAVE(IOSURFACE_ACCELERATOR)
    WEBCORE_EXPORT static bool allowConversionFromFormatToFormat(Format, Format);
    WEBCORE_EXPORT static void convertToFormat(IOSurfacePool*, std::unique_ptr<WebCore::IOSurface>&& inSurface, Format, Function<void(std::unique_ptr<WebCore::IOSurface>)>&&);
#endif // HAVE(IOSURFACE_ACCELERATOR)

    WEBCORE_EXPORT void setOwnershipIdentity(const ProcessIdentity&);
    WEBCORE_EXPORT static void setOwnershipIdentity(IOSurfaceRef, const ProcessIdentity&);

    void migrateColorSpaceToProperties();

    RetainPtr<CGContextRef> createCompatibleBitmap(unsigned width, unsigned height);

private:
    IOSurface(IntSize, const DestinationColorSpace&, Format, bool& success);
    IOSurface(IOSurfaceRef, const DestinationColorSpace&);

    struct BitmapConfiguration {
        CGBitmapInfo bitmapInfo;
        size_t bitsPerComponent;
    };

    BitmapConfiguration bitmapConfiguration() const;

    DestinationColorSpace m_colorSpace;
    IntSize m_size;
    size_t m_totalBytes;

    ProcessIdentity m_resourceOwner;
    std::unique_ptr<GraphicsContext> m_graphicsContext;
    RetainPtr<CGContextRef> m_cgContext;

    RetainPtr<IOSurfaceRef> m_surface;

    static std::optional<IntSize> s_maximumSize;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, WebCore::IOSurface::Format);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const WebCore::IOSurface&);

} // namespace WebCore

#endif // HAVE(IOSURFACE)

