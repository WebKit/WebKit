/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "ImageBufferBackendHandle.h"
#include <WebCore/FloatRect.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/Region.h>
#include <wtf/MachSendRight.h>
#include <wtf/MonotonicTime.h>

OBJC_CLASS CALayer;

// FIXME: Make PlatformCALayerRemote.cpp Objective-C so we can include WebLayer.h here and share the typedef.
namespace WebCore {
class NativeImage;
class ThreadSafeImageBufferFlusher;
typedef Vector<WebCore::FloatRect, 5> RepaintRectList;
struct PlatformCALayerDelegatedContents;
struct PlatformCALayerDelegatedContentsFinishedEvent;
}

namespace WebKit {

class PlatformCALayerRemote;
class RemoteLayerBackingStoreCollection;
class RemoteLayerTreeNode;
enum class SwapBuffersDisplayRequirement : uint8_t;
struct PlatformCALayerRemoteDelegatedContents;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
using UseCGDisplayListImageCache = WebCore::ImageBufferCreationContext::UseCGDisplayListImageCache;
#endif

enum class BackingStoreNeedsDisplayReason : uint8_t {
    None,
    NoFrontBuffer,
    FrontBufferIsVolatile,
    FrontBufferHasNoSharingHandle,
    HasDirtyRegion,
};

struct BufferAndBackendInfo {
    WebCore::RenderingResourceIdentifier resourceIdentifier;
    unsigned backendGeneration { 0 };

    BufferAndBackendInfo() = default;
    BufferAndBackendInfo(const BufferAndBackendInfo&) = default;

    explicit BufferAndBackendInfo(WebCore::ImageBuffer& imageBuffer)
        : resourceIdentifier(imageBuffer.renderingResourceIdentifier())
        , backendGeneration(imageBuffer.backendGeneration())
    { }

    bool operator==(const BufferAndBackendInfo&) const = default;

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, BufferAndBackendInfo&);
};

class RemoteLayerBackingStore {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStore);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerBackingStore(PlatformCALayerRemote*);
    ~RemoteLayerBackingStore();

    enum class Type : bool {
        IOSurface,
        Bitmap
    };

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    enum class IncludeDisplayList : bool { No, Yes };
#endif

    struct Parameters {
        Type type { Type::Bitmap };
        WebCore::FloatSize size;
        WebCore::DestinationColorSpace colorSpace { WebCore::DestinationColorSpace::SRGB() };
        float scale { 1.0f };
        bool deepColor { false };
        bool isOpaque { false };

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        IncludeDisplayList includeDisplayList { IncludeDisplayList::No };
        UseCGDisplayListImageCache useCGDisplayListImageCache { UseCGDisplayListImageCache::No };
#endif

        bool operator==(const Parameters& other) const
        {
            return (type == other.type
                && size == other.size
                && colorSpace == other.colorSpace
                && scale == other.scale
                && deepColor == other.deepColor
                && isOpaque == other.isOpaque
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
                && includeDisplayList == other.includeDisplayList
                && useCGDisplayListImageCache == other.useCGDisplayListImageCache
#endif
                );
        }
    };

    void ensureBackingStore(const Parameters&);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay();

    void setDelegatedContents(const PlatformCALayerRemoteDelegatedContents&);

    // Returns true if we need to encode the buffer.
    bool layerWillBeDisplayed();
    bool needsDisplay() const;

    bool performDelegatedLayerDisplay();
    void prepareToDisplay();
    void paintContents();

    WebCore::FloatSize size() const { return m_parameters.size; }
    float scale() const { return m_parameters.scale; }
    bool usesDeepColorBackingStore() const;
    WebCore::DestinationColorSpace colorSpace() const { return m_parameters.colorSpace; }
    WebCore::PixelFormat pixelFormat() const;
    Type type() const { return m_parameters.type; }
    bool isOpaque() const { return m_parameters.isOpaque; }
    unsigned bytesPerPixel() const;

    PlatformCALayerRemote* layer() const { return m_layer; }

    void encode(IPC::Encoder&) const;

    void enumerateRectsBeingDrawn(WebCore::GraphicsContext&, void (^)(WebCore::FloatRect));

    bool hasFrontBuffer() const
    {
        return m_contentsBufferHandle || !!m_frontBuffer.imageBuffer;
    }

    bool hasNoBuffers() const
    {
        return !m_frontBuffer.imageBuffer && !m_backBuffer.imageBuffer && !m_secondaryBackBuffer.imageBuffer && !m_contentsBufferHandle;
    }

    // Just for RemoteBackingStoreCollection.
    void applySwappedBuffers(RefPtr<WebCore::ImageBuffer>&& front, RefPtr<WebCore::ImageBuffer>&& back, RefPtr<WebCore::ImageBuffer>&& secondaryBack, SwapBuffersDisplayRequirement);
    WebCore::SetNonVolatileResult swapToValidFrontBuffer();

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> takePendingFlushers();

    enum class BufferType {
        Front,
        Back,
        SecondaryBack
    };

    RefPtr<WebCore::ImageBuffer> bufferForType(BufferType) const;

    // Returns true if it was able to fulfill the request. This can fail when trying to mark an in-use surface as volatile.
    bool setBufferVolatile(BufferType);
    WebCore::SetNonVolatileResult setFrontBufferNonVolatile();

    bool hasEmptyDirtyRegion() const { return m_dirtyRegion.isEmpty() || m_parameters.size.isEmpty(); }
    bool supportsPartialRepaint() const;

    MonotonicTime lastDisplayTime() const { return m_lastDisplayTime; }

    void clearBackingStore();

private:
    RemoteLayerBackingStoreCollection* backingStoreCollection() const;

    void drawInContext(WebCore::GraphicsContext&, WTF::Function<void()>&& additionalContextSetupCallback = nullptr);

    struct Buffer {
        RefPtr<WebCore::ImageBuffer> imageBuffer;
        bool isCleared { false };

        explicit operator bool() const
        {
            return !!imageBuffer;
        }

        void discard();
        void encode(IPC::Encoder&) const;
    };

    bool setBufferVolatile(Buffer&);
    WebCore::SetNonVolatileResult setBufferNonVolatile(Buffer&);
    
    SwapBuffersDisplayRequirement prepareBuffers();
    void ensureFrontBuffer();
    void dirtyRepaintCounterIfNecessary();

    PlatformCALayerRemote* m_layer;

    Parameters m_parameters;

    WebCore::Region m_dirtyRegion;

    Buffer m_frontBuffer;
    Buffer m_backBuffer;
    Buffer m_secondaryBackBuffer;

    std::optional<WebCore::IntRect> m_previouslyPaintedRect;

    // FIXME: This should be removed and m_bufferHandle should be used to ref the buffer once ShareableBitmap::Handle
    // can be encoded multiple times. http://webkit.org/b/234169
    std::optional<ImageBufferBackendHandle> m_contentsBufferHandle;
    std::optional<WebCore::RenderingResourceIdentifier> m_contentsRenderingResourceIdentifier;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    RefPtr<WebCore::ImageBuffer> m_displayListBuffer;
#endif

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> m_frontBufferFlushers;

    WebCore::RepaintRectList m_paintingRects;

    MonotonicTime m_lastDisplayTime;
};

// The subset of RemoteLayerBackingStore that gets serialized into the UI
// process, and gets applied to the CALayer.
class RemoteLayerBackingStoreProperties {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStoreProperties);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerBackingStoreProperties() = default;
    RemoteLayerBackingStoreProperties(RemoteLayerBackingStoreProperties&&) = default;

    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, RemoteLayerBackingStoreProperties&);

    enum class LayerContentsType { IOSurface, CAMachPort, CachedIOSurface };
    void applyBackingStoreToLayer(CALayer *, LayerContentsType, std::optional<WebCore::RenderingResourceIdentifier>, bool replayCGDisplayListsIntoBackingStore);

    void updateCachedBuffers(RemoteLayerTreeNode&, LayerContentsType);

    const std::optional<ImageBufferBackendHandle>& bufferHandle() const { return m_bufferHandle; };

    bool isOpaque() const { return m_isOpaque; }

    static RetainPtr<id> layerContentsBufferFromBackendHandle(ImageBufferBackendHandle&&, LayerContentsType);

    void dump(WTF::TextStream&) const;

private:
    std::optional<ImageBufferBackendHandle> m_bufferHandle;
    RetainPtr<id> m_contentsBuffer;

    std::optional<BufferAndBackendInfo> m_frontBufferInfo;
    std::optional<BufferAndBackendInfo> m_backBufferInfo;
    std::optional<BufferAndBackendInfo> m_secondaryBackBufferInfo;

    std::optional<WebCore::IntRect> m_paintedRect;

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    std::optional<ImageBufferBackendHandle> m_displayListBufferHandle;
#endif

    bool m_isOpaque;
    RemoteLayerBackingStore::Type m_type;
};

WTF::TextStream& operator<<(WTF::TextStream&, SwapBuffersDisplayRequirement);
WTF::TextStream& operator<<(WTF::TextStream&, BackingStoreNeedsDisplayReason);
WTF::TextStream& operator<<(WTF::TextStream&, const RemoteLayerBackingStore&);
WTF::TextStream& operator<<(WTF::TextStream&, const RemoteLayerBackingStoreProperties&);

} // namespace WebKit
