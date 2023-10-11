/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include <WebCore/CopyImageOptions.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformImage.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class GraphicsContext;
class Image;
class NativeImage;
}

namespace WebKit {

class ShareableBitmapConfiguration {
public:
    ShareableBitmapConfiguration() = default;

    ShareableBitmapConfiguration(const WebCore::IntSize&, std::optional<WebCore::DestinationColorSpace> = std::nullopt, bool isOpaque = false);
    ShareableBitmapConfiguration(const WebCore::IntSize&, std::optional<WebCore::DestinationColorSpace>, bool isOpaque, unsigned bytesPerPixel, unsigned bytesPerRow
#if USE(CG)
        , CGBitmapInfo
#endif
    );
#if USE(CG)
    ShareableBitmapConfiguration(WebCore::NativeImage&);
#endif

    WebCore::IntSize size() const { return m_size; }
    const WebCore::DestinationColorSpace& colorSpace() const { return m_colorSpace ? *m_colorSpace : WebCore::DestinationColorSpace::SRGB(); }
    WebCore::PlatformColorSpaceValue platformColorSpace() const { return colorSpace().platformColorSpace(); }
    bool isOpaque() const { return m_isOpaque; }

    unsigned bytesPerPixel() const { ASSERT(!m_bytesPerPixel.hasOverflowed()); return m_bytesPerPixel; }
    unsigned bytesPerRow() const { ASSERT(!m_bytesPerRow.hasOverflowed()); return m_bytesPerRow; }
#if USE(CG)
    CGBitmapInfo bitmapInfo() const { return m_bitmapInfo; }
#endif

    CheckedUint32 sizeInBytes() const { return m_bytesPerRow * m_size.height(); }

    static CheckedUint32 calculateBytesPerRow(const WebCore::IntSize&, const WebCore::DestinationColorSpace&);
    static CheckedUint32 calculateSizeInBytes(const WebCore::IntSize&, const WebCore::DestinationColorSpace&);

private:
    friend struct IPC::ArgumentCoder<ShareableBitmapConfiguration, void>;

    static std::optional<WebCore::DestinationColorSpace> validateColorSpace(std::optional<WebCore::DestinationColorSpace>);
    static CheckedUint32 calculateBytesPerPixel(const WebCore::DestinationColorSpace&);
#if USE(CG)
    static CGBitmapInfo calculateBitmapInfo(const WebCore::DestinationColorSpace&, bool isOpaque);
#endif

    WebCore::IntSize m_size;
    std::optional<WebCore::DestinationColorSpace> m_colorSpace;
    bool m_isOpaque { false };

    CheckedUint32 m_bytesPerPixel;
    CheckedUint32 m_bytesPerRow;
#if USE(CG)
    CGBitmapInfo m_bitmapInfo { 0 };
#endif
};

class ShareableBitmapHandle  {
public:
    ShareableBitmapHandle();
    ShareableBitmapHandle(ShareableBitmapHandle&&) = default;
    explicit ShareableBitmapHandle(const ShareableBitmapHandle&) = default;
    ShareableBitmapHandle(SharedMemory::Handle&&, const ShareableBitmapConfiguration&);

    ShareableBitmapHandle& operator=(ShareableBitmapHandle&&) = default;

    bool isNull() const { return m_handle.isNull(); }

    SharedMemory::Handle& handle() { return m_handle; }

    // Take ownership of the memory for process memory accounting purposes.
    void takeOwnershipOfMemory(MemoryLedger) const;

private:
    friend struct IPC::ArgumentCoder<ShareableBitmapHandle, void>;
    friend class ShareableBitmap;

    SharedMemory::Handle m_handle;
    ShareableBitmapConfiguration m_configuration;
};

class ShareableBitmap : public ThreadSafeRefCounted<ShareableBitmap> {
public:
    using Handle = ShareableBitmapHandle;

    // Create a shareable bitmap whose backing memory can be shared with another process.
    static RefPtr<ShareableBitmap> create(const ShareableBitmapConfiguration&);

    // Create a shareable bitmap from an already existing shared memory block.
    static RefPtr<ShareableBitmap> create(const ShareableBitmapConfiguration&, Ref<SharedMemory>&&);

    // Create a shareable bitmap from a NativeImage.
#if USE(CG)
    static RefPtr<ShareableBitmap> createFromImagePixels(WebCore::NativeImage&);
#endif
    static RefPtr<ShareableBitmap> createFromImageDraw(WebCore::NativeImage&);

    // Create a shareable bitmap from a handle.
    static RefPtr<ShareableBitmap> create(Handle&&, SharedMemory::Protection = SharedMemory::Protection::ReadWrite);
    
    // Create a shareable bitmap from a ReadOnly handle.
    static std::optional<Ref<ShareableBitmap>> createReadOnly(std::optional<Handle>&&);

    std::optional<Handle> createHandle(SharedMemory::Protection = SharedMemory::Protection::ReadWrite) const;
    
    // Create a ReadOnly handle.
    std::optional<Handle> createReadOnlyHandle() const;

    WebCore::IntSize size() const { return m_configuration.size(); }
    WebCore::IntRect bounds() const { return WebCore::IntRect(WebCore::IntPoint(), size()); }

    void* data() const;
    size_t bytesPerRow() const { return m_configuration.bytesPerRow(); }
    size_t sizeInBytes() const { return m_configuration.sizeInBytes(); }

    // Create a graphics context that can be used to paint into the backing store.
    std::unique_ptr<WebCore::GraphicsContext> createGraphicsContext();

    // Paint the backing store into the given context.
    void paint(WebCore::GraphicsContext&, const WebCore::IntPoint& destination, const WebCore::IntRect& source);
    void paint(WebCore::GraphicsContext&, float scaleFactor, const WebCore::IntPoint& destination, const WebCore::IntRect& source);

    // This creates a bitmap image that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    RefPtr<WebCore::Image> createImage();

#if USE(CG)
    // This creates a copied CGImageRef (most likely a copy-on-write) of the shareable bitmap.
    RetainPtr<CGImageRef> makeCGImageCopy();

    // This creates a CGImageRef that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    RetainPtr<CGImageRef> makeCGImage(WebCore::ShouldInterpolate = WebCore::ShouldInterpolate::No);

    WebCore::PlatformImagePtr createPlatformImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore, WebCore::ShouldInterpolate = WebCore::ShouldInterpolate::No);
#elif USE(CAIRO)
    // This creates a BitmapImage that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    RefPtr<cairo_surface_t> createPersistentCairoSurface();
    RefPtr<cairo_surface_t> createCairoSurface();

    WebCore::PlatformImagePtr createPlatformImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore, WebCore::ShouldInterpolate = WebCore::ShouldInterpolate::No) { return createCairoSurface(); }
#endif

private:
    ShareableBitmap(ShareableBitmapConfiguration, Ref<SharedMemory>&&);

#if USE(CG)
    RetainPtr<CGImageRef> createCGImage(CGDataProviderRef, WebCore::ShouldInterpolate) const;
    static void releaseBitmapContextData(void* typelessBitmap, void* typelessData);
#endif

#if USE(CAIRO)
    static void releaseSurfaceData(void* typelessBitmap);
#endif

#if USE(CG)
    bool m_releaseBitmapContextDataCalled { false };
#endif

    ShareableBitmapConfiguration m_configuration;
    Ref<SharedMemory> m_sharedMemory;
};

} // namespace WebKit
