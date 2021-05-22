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

#include <objc/objc.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#include "IntSize.h"

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#define HAVE_IOSURFACE_RGB10 1
#endif

using CVPixelBufferRef = struct __CVBuffer*;

namespace WTF {
class MachSendRight;
class TextStream;
}

namespace WebCore {

class GraphicsContext;
class HostWindow;
    
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
class ImageBuffer;
#endif

enum class PixelFormat : uint8_t;
enum class VolatilityState : uint8_t;

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

    WEBCORE_EXPORT static std::unique_ptr<IOSurface> create(IntSize, CGColorSpaceRef, Format = Format::BGRA);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> create(IntSize, IntSize contextSize, CGColorSpaceRef, Format = Format::BGRA);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSendRight(const WTF::MachSendRight&&, CGColorSpaceRef);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSurface(IOSurfaceRef, CGColorSpaceRef);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromImage(CGImageRef);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromPixelBuffer(CVPixelBufferRef);
    
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    static std::unique_ptr<IOSurface> createFromImageBuffer(RefPtr<ImageBuffer>);
#endif

    WEBCORE_EXPORT ~IOSurface();

    WEBCORE_EXPORT static void moveToPool(std::unique_ptr<IOSurface>&&);

    WEBCORE_EXPORT static IntSize maximumSize();
    WEBCORE_EXPORT static void setMaximumSize(IntSize);

    WEBCORE_EXPORT WTF::MachSendRight createSendRight() const;

    // Any images created from a surface need to be released before releasing
    // the surface, or an expensive GPU readback can result.
    WEBCORE_EXPORT RetainPtr<CGImageRef> createImage();
    WEBCORE_EXPORT static RetainPtr<CGImageRef> sinkIntoImage(std::unique_ptr<IOSurface>);

    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> createPixelBuffer();

#ifdef __OBJC__
    id asLayerContents() const { return (__bridge id)m_surface.get(); }
#endif
    IOSurfaceRef surface() const { return m_surface.get(); }
    WEBCORE_EXPORT GraphicsContext& ensureGraphicsContext();
    WEBCORE_EXPORT CGContextRef ensurePlatformContext(const HostWindow* = nullptr);

    // Querying volatility can be expensive, so in cases where the surface is
    // going to be used immediately, use the return value of setVolatile to
    // determine whether the data was purged, instead of first calling state() or isVolatile().
    VolatilityState state() const;
    bool isVolatile() const;

    WEBCORE_EXPORT VolatilityState setVolatile(bool);

    IntSize size() const { return m_size; }
    size_t totalBytes() const { return m_totalBytes; }

    CGColorSpaceRef colorSpace() const { return m_colorSpace.get(); }
    WEBCORE_EXPORT Format format() const;
    WEBCORE_EXPORT IOSurfaceID surfaceID() const;
    size_t bytesPerRow() const;

    WEBCORE_EXPORT bool isInUse() const;

    // The graphics context cached on the surface counts as a "user", so to get
    // an accurate result from isInUse(), it needs to be released.
    WEBCORE_EXPORT void releaseGraphicsContext();

#if HAVE(IOSURFACE_ACCELERATOR)
    WEBCORE_EXPORT static bool allowConversionFromFormatToFormat(Format, Format);
    WEBCORE_EXPORT static void convertToFormat(std::unique_ptr<WebCore::IOSurface>&& inSurface, Format, WTF::Function<void(std::unique_ptr<WebCore::IOSurface>)>&&);
#endif // HAVE(IOSURFACE_ACCELERATOR)

#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
    WEBCORE_EXPORT void setOwnershipIdentity(task_id_token_t newOwner);
#endif

    void migrateColorSpaceToProperties();

private:
    IOSurface(IntSize, IntSize contextSize, CGColorSpaceRef, Format, bool& success);
    IOSurface(IOSurfaceRef, CGColorSpaceRef);

    static std::unique_ptr<IOSurface> surfaceFromPool(IntSize, IntSize contextSize, CGColorSpaceRef, Format);
    IntSize contextSize() const { return m_contextSize; }
    void setContextSize(IntSize);

    RetainPtr<CGColorSpaceRef> m_colorSpace;
    IntSize m_size;
    IntSize m_contextSize;
    size_t m_totalBytes;

    std::unique_ptr<GraphicsContext> m_graphicsContext;
    RetainPtr<CGContextRef> m_cgContext;

    RetainPtr<IOSurfaceRef> m_surface;

    static WTF::Optional<IntSize> s_maximumSize;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const WebCore::IOSurface&);

} // namespace WebCore

#endif // HAVE(IOSURFACE)

