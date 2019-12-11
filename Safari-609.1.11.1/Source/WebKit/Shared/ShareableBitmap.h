/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SharedMemory.h"
#include <WebCore/IntRect.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(CG)
#include "ColorSpaceData.h"
#include <wtf/RetainPtr.h>
#endif

#if USE(CAIRO)
#include <WebCore/RefPtrCairo.h>
#endif

#if USE(DIRECT2D)
interface ID2D1Bitmap;
interface ID2D1RenderTarget;
interface ID3D11Device1;
interface IDXGIKeyedMutex;
interface IDXGISurface1;

#include <WebCore/COMPtr.h>
#endif

namespace WebCore {
class Image;
class GraphicsContext;
}

namespace WebKit {
    
class ShareableBitmap : public RefCounted<ShareableBitmap> {
public:
    struct Configuration {
        bool isOpaque { false };
#if PLATFORM(COCOA)
        ColorSpaceData colorSpace;
#endif
#if USE(DIRECT2D)
        mutable HANDLE sharedResourceHandle { nullptr };
#endif

        void encode(IPC::Encoder&) const;
        static bool decode(IPC::Decoder&, Configuration&);
    };

    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    public:
        Handle();
        Handle(Handle&&) = default;
        Handle& operator=(Handle&&) = default;

        bool isNull() const { return m_handle.isNull(); }

        void clear();

        void encode(IPC::Encoder&) const;
        static bool decode(IPC::Decoder&, Handle&);

    private:
        friend class ShareableBitmap;

        mutable SharedMemory::Handle m_handle;
        WebCore::IntSize m_size;
        Configuration m_configuration;
    };

    // Create a shareable bitmap that uses malloced memory.
    static RefPtr<ShareableBitmap> create(const WebCore::IntSize&, Configuration);

    // Create a shareable bitmap whose backing memory can be shared with another process.
    static RefPtr<ShareableBitmap> createShareable(const WebCore::IntSize&, Configuration);

    // Create a shareable bitmap from an already existing shared memory block.
    static RefPtr<ShareableBitmap> create(const WebCore::IntSize&, Configuration, RefPtr<SharedMemory>);

    // Create a shareable bitmap from a handle.
    static RefPtr<ShareableBitmap> create(const Handle&, SharedMemory::Protection = SharedMemory::Protection::ReadWrite);

    // Create a handle.
    bool createHandle(Handle&, SharedMemory::Protection = SharedMemory::Protection::ReadWrite) const;

    ~ShareableBitmap();

    const WebCore::IntSize& size() const { return m_size; }
    WebCore::IntRect bounds() const { return WebCore::IntRect(WebCore::IntPoint(), size()); }

    // Create a graphics context that can be used to paint into the backing store.
    std::unique_ptr<WebCore::GraphicsContext> createGraphicsContext();

    // Paint the backing store into the given context.
    void paint(WebCore::GraphicsContext&, const WebCore::IntPoint& destination, const WebCore::IntRect& source);
    void paint(WebCore::GraphicsContext&, float scaleFactor, const WebCore::IntPoint& destination, const WebCore::IntRect& source);

    bool isBackedBySharedMemory() const { return m_sharedMemory; }

    // This creates a bitmap image that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    RefPtr<WebCore::Image> createImage();

#if USE(CG)
    // This creates a copied CGImageRef (most likely a copy-on-write) of the shareable bitmap.
    RetainPtr<CGImageRef> makeCGImageCopy();

    // This creates a CGImageRef that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    RetainPtr<CGImageRef> makeCGImage();
#elif USE(CAIRO)
    // This creates a BitmapImage that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    RefPtr<cairo_surface_t> createCairoSurface();
#elif USE(DIRECT2D)
    COMPtr<ID2D1Bitmap> createDirect2DSurface(ID3D11Device1*, ID2D1RenderTarget*);
    IDXGISurface1* dxSurface() { return m_surface.get(); }
    void createSharedResource();
    void disposeSharedResource();
    void leakSharedResource();
#endif

private:
    ShareableBitmap(const WebCore::IntSize&, Configuration, void*);
    ShareableBitmap(const WebCore::IntSize&, Configuration, RefPtr<SharedMemory>);

    static Checked<unsigned, RecordOverflow> numBytesForSize(WebCore::IntSize, const ShareableBitmap::Configuration&);
    static Checked<unsigned, RecordOverflow> calculateBytesPerRow(WebCore::IntSize, const Configuration&);
    static unsigned calculateBytesPerPixel(const Configuration&);

#if USE(CG)
    RetainPtr<CGImageRef> createCGImage(CGDataProviderRef) const;
    static void releaseBitmapContextData(void* typelessBitmap, void* typelessData);
    static void releaseDataProviderData(void* typelessBitmap, const void* typelessData, size_t);
#endif

#if USE(CAIRO)
    static void releaseSurfaceData(void* typelessBitmap);
#endif

public:
    void* data() const;
private:
    size_t sizeInBytes() const { return numBytesForSize(m_size, m_configuration).unsafeGet(); }

    WebCore::IntSize m_size;
    Configuration m_configuration;

#if USE(DIRECT2D)
    COMPtr<IDXGISurface1> m_surface;
    COMPtr<IDXGIKeyedMutex> m_surfaceMutex;
    COMPtr<ID2D1Bitmap> m_bitmap;
#endif

    // If the shareable bitmap is backed by shared memory, this points to the shared memory object.
    RefPtr<SharedMemory> m_sharedMemory;

    // If the shareable bitmap is backed by fastMalloced memory, this points to the data.
    void* m_data { nullptr };
};

} // namespace WebKit

