/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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

#include <WebCore/CopyImageOptions.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformImage.h>
#include <WebCore/SharedMemory.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/ExportMacros.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class GraphicsContext;
class Image;
class NativeImage;

class ShareableBitmapConfiguration {
public:
#if USE(CG)
    WEBCORE_EXPORT static std::optional<ShareableBitmapConfiguration> create(const IntSize&, PlatformColorSpace&&, size_t bytesPerRow, Headroom, CGBitmapInfo, size_t bitsPerComponent, size_t bitsPerPixel);
    Headroom headroom() const { return m_headroom; }
    CGBitmapInfo bitmapInfo() const { return m_bitmapInfo; }
    size_t bitsPerComponent() const { return m_bitsPerComponent; }
    size_t bitsPerPixel() const { return m_bitsPerPixel; }
#else
    WEBCORE_EXPORT static std::optional<ShareableBitmapConfiguration> create(const IntSize&, PlatformColorSpace&&, size_t bytesPerRow, bool isOpaque);
#endif

    IntSize size() const { return m_size; }
    PlatformColorSpace colorSpace() const { return m_colorSpace; }
    uint32_t bytesPerRow() const { return m_bytesPerRow; }
    uint32_t sizeInBytes() const { return m_bytesPerRow * m_size.height(); }

    WEBCORE_EXPORT static CheckedUint32 calculateBytesPerRow(const IntSize&, const DestinationColorSpace&);

private:
#if USE(CG)
    ShareableBitmapConfiguration(const IntSize&, PlatformColorSpace&&, size_t bytesPerRow, Headroom, CGBitmapInfo, uint8_t bitsPerComponent, uint8_t bitsPerPixel);
#else
    ShareableBitmapConfiguration(const IntSize&, PlatformColorSpace&&, size_t bytesPerRow, bool isOpaque);
#endif

    friend struct IPC::ArgumentCoder<ShareableBitmapConfiguration, void>;

    IntSize m_size;
    PlatformColorSpace m_colorSpace;
    uint32_t m_bytesPerRow;
#if USE(CG)
    Headroom m_headroom { Headroom::None };
    CGBitmapInfo m_bitmapInfo;
    uint8_t m_bitsPerComponent;
    uint8_t m_bitsPerPixel;
#else
    bool m_isOpaque;
#endif
};

class ShareableBitmapHandle  {
public:
    ShareableBitmapHandle(ShareableBitmapHandle&&) = default;
    explicit ShareableBitmapHandle(const ShareableBitmapHandle&) = default;
    WEBCORE_EXPORT ShareableBitmapHandle(SharedMemory::Handle&&, const ShareableBitmapConfiguration&);

    ShareableBitmapHandle& operator=(ShareableBitmapHandle&&) = default;

    SharedMemory::Handle& handle() { return m_handle; }

    // Take ownership of the memory for process memory accounting purposes.
    WEBCORE_EXPORT void takeOwnershipOfMemory(MemoryLedger) const;
    // Transfer ownership of the memory for process memory accounting purposes.
    WEBCORE_EXPORT void setOwnershipOfMemory(const ProcessIdentity&, MemoryLedger) const;

private:
    friend struct IPC::ArgumentCoder<ShareableBitmapHandle, void>;
    friend class ShareableBitmap;

    SharedMemory::Handle m_handle;
    ShareableBitmapConfiguration m_configuration;
};

class ShareableBitmap : public ThreadSafeRefCounted<ShareableBitmap> {
public:
    using Handle = ShareableBitmapHandle;

    // Creates a non-opaque SRGB destination drawable bitmap.
    static RefPtr<ShareableBitmap> create(const IntSize&);

    // Creates a destination drawable bitmap bitmap with compatible colorspace.
#if USE(CG)
    static WEBCORE_EXPORT RefPtr<ShareableBitmap> create(const IntSize&, const DestinationColorSpace&, Headroom = Headroom::None, bool isOpaque = false);
#else
    static WEBCORE_EXPORT RefPtr<ShareableBitmap> create(const IntSize&, const DestinationColorSpace&, bool isOpaque = false);
#endif

    // Create a shareable bitmap whose backing memory can be shared with another process.
    // The bitmap may be not be drawable if the colorspace or pixel format is not drawable.
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> create(const ShareableBitmapConfiguration&);

    // Create a shareable bitmap from an already existing shared memory block.
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> create(const ShareableBitmapConfiguration&, Ref<SharedMemory>&&);

    // Create a shareable bitmap from a NativeImage.
#if USE(CG)
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> createFromImagePixels(NativeImage&);
#endif
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> createFromImageDraw(NativeImage&, const DestinationColorSpace&);
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> createFromImageDraw(NativeImage&, const DestinationColorSpace&, const IntSize&);
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> createFromImageDraw(NativeImage&, const DestinationColorSpace&, const IntSize& destinationSize, const IntSize& sourceSize);

    // Create a shareable bitmap from a handle.
    WEBCORE_EXPORT static RefPtr<ShareableBitmap> create(Handle&&, SharedMemory::Protection = SharedMemory::Protection::ReadWrite);

    // Create a shareable bitmap from a ReadOnly handle.
    WEBCORE_EXPORT static std::optional<Ref<ShareableBitmap>> createReadOnly(std::optional<Handle>&&);

    WEBCORE_EXPORT std::optional<Handle> createHandle(SharedMemory::Protection = SharedMemory::Protection::ReadWrite) const;

    // Create a ReadOnly handle.
    WEBCORE_EXPORT std::optional<Handle> createReadOnlyHandle() const;

    WEBCORE_EXPORT void setOwnershipOfMemory(const ProcessIdentity&);

    IntSize size() const { return m_configuration.size(); }
    IntRect bounds() const { return IntRect(IntPoint(), size()); }

    WEBCORE_EXPORT std::span<const uint8_t> span() const LIFETIME_BOUND;
    WEBCORE_EXPORT std::span<uint8_t> mutableSpan() LIFETIME_BOUND;
    size_t bytesPerRow() const { return m_configuration.bytesPerRow(); }
    size_t sizeInBytes() const { return m_configuration.sizeInBytes(); }

    // Create a graphics context that can be used to paint into the backing store.
    WEBCORE_EXPORT std::unique_ptr<GraphicsContext> createGraphicsContext();

    // Paint the backing store into the given context.
    WEBCORE_EXPORT void paint(GraphicsContext&, const IntPoint& destination, const IntRect& source);
    WEBCORE_EXPORT void paint(GraphicsContext&, float scaleFactor, const IntPoint& destination, const IntRect& source);

    // This creates a bitmap image that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    WEBCORE_EXPORT RefPtr<Image> createImage();

    WEBCORE_EXPORT PlatformImagePtr createPlatformImage(BackingStoreCopy = CopyBackingStore, ShouldInterpolate = ShouldInterpolate::No);

#if USE(CAIRO)
    // This creates a BitmapImage that directly references the shared bitmap data.
    // This is only safe to use when we know that the contents of the shareable bitmap won't change.
    WEBCORE_EXPORT RefPtr<cairo_surface_t> createPersistentCairoSurface();
    WEBCORE_EXPORT RefPtr<cairo_surface_t> createCairoSurface();
#endif

private:
    ShareableBitmap(ShareableBitmapConfiguration, Ref<SharedMemory>&&);

#if USE(CG)
    static void releaseBitmapContextData(void* typelessBitmap, void* typelessData);
#endif

#if USE(CAIRO)
    static void releaseSurfaceData(void* typelessBitmap);
#endif

    ShareableBitmapConfiguration m_configuration;
    const Ref<SharedMemory> m_sharedMemory;
#if USE(CG)
    std::optional<SharedMemoryHandle> m_ownershipHandle;
    bool m_releaseBitmapContextDataCalled : 1 { false };
#endif
};

inline RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size)
{
    return create(size, DestinationColorSpace::SRGB());
}

} // namespace WebCore
