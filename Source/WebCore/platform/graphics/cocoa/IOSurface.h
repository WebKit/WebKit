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

#ifndef IOSurface_h
#define IOSurface_h

#if USE(IOSURFACE)

#include "GraphicsContext.h"
#include "IntSize.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class MachSendRight;

class IOSurface final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Format {
        RGBA,
        YUV422,
        RGB10,
        RGB10A8,
    };

    WEBCORE_EXPORT static std::unique_ptr<IOSurface> create(IntSize, ColorSpace, Format = Format::RGBA);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> create(IntSize, IntSize contextSize, ColorSpace, Format = Format::RGBA);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromSendRight(const MachSendRight&, ColorSpace);
    static std::unique_ptr<IOSurface> createFromSurface(IOSurfaceRef, ColorSpace);
    WEBCORE_EXPORT static std::unique_ptr<IOSurface> createFromImage(CGImageRef);
    
    static std::unique_ptr<IOSurface> createFromImageBuffer(std::unique_ptr<ImageBuffer>);

    WEBCORE_EXPORT static void moveToPool(std::unique_ptr<IOSurface>&&);

    static IntSize maximumSize();

    WEBCORE_EXPORT MachSendRight createSendRight() const;

    // Any images created from a surface need to be released before releasing
    // the surface, or an expensive GPU readback can result.
    WEBCORE_EXPORT RetainPtr<CGImageRef> createImage();
    static RetainPtr<CGImageRef> sinkIntoImage(std::unique_ptr<IOSurface>);

    IOSurfaceRef surface() const { return m_surface.get(); }
    WEBCORE_EXPORT GraphicsContext& ensureGraphicsContext();
    WEBCORE_EXPORT CGContextRef ensurePlatformContext();

    enum class SurfaceState {
        Valid,
        Empty
    };

    // Querying volatility can be expensive, so in cases where the surface is
    // going to be used immediately, use the return value of setIsVolatile to
    // determine whether the data was purged, instead of first calling state() or isVolatile().
    SurfaceState state() const;
    bool isVolatile() const;

    // setIsVolatile only has an effect on iOS and OS 10.9 and above.
    WEBCORE_EXPORT SurfaceState setIsVolatile(bool);

    IntSize size() const { return m_size; }
    size_t totalBytes() const { return m_totalBytes; }
    ColorSpace colorSpace() const { return m_colorSpace; }
    WEBCORE_EXPORT Format format() const;

    WEBCORE_EXPORT bool isInUse() const;

    // The graphics context cached on the surface counts as a "user", so to get
    // an accurate result from isInUse(), it needs to be released.
    WEBCORE_EXPORT void releaseGraphicsContext();

#if PLATFORM(IOS)
    WEBCORE_EXPORT static bool allowConversionFromFormatToFormat(Format, Format);
    WEBCORE_EXPORT static void convertToFormat(std::unique_ptr<WebCore::IOSurface>&& inSurface, Format, std::function<void(std::unique_ptr<WebCore::IOSurface>)>);
#endif

private:
    IOSurface(IntSize, ColorSpace, Format);
    IOSurface(IntSize, IntSize contextSize, ColorSpace, Format);
    IOSurface(IOSurfaceRef, ColorSpace);

    static std::unique_ptr<IOSurface> surfaceFromPool(IntSize, IntSize contextSize, ColorSpace, Format);
    IntSize contextSize() const { return m_contextSize; }
    void setContextSize(IntSize);

    ColorSpace m_colorSpace;
    IntSize m_size;
    IntSize m_contextSize;
    size_t m_totalBytes;

    std::unique_ptr<GraphicsContext> m_graphicsContext;
    RetainPtr<CGContextRef> m_cgContext;

    RetainPtr<IOSurfaceRef> m_surface;
};

} // namespace WebCore

#endif // USE(IOSURFACE)

#endif // IOSurface_h
