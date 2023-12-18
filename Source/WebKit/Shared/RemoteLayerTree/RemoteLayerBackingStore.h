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

#include "BufferAndBackendInfo.h"
#include "BufferIdentifierSet.h"
#include "ImageBufferBackendHandle.h"
#include <WebCore/FloatRect.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/Region.h>
#include <wtf/MachSendRight.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WeakPtr.h>

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
class RemoteLayerTreeHost;
enum class SwapBuffersDisplayRequirement : uint8_t;

enum class BackingStoreNeedsDisplayReason : uint8_t {
    None,
    NoFrontBuffer,
    FrontBufferIsVolatile,
    FrontBufferHasNoSharingHandle,
    HasDirtyRegion,
};

class RemoteLayerBackingStore : public CanMakeWeakPtr<RemoteLayerBackingStore> {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStore);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerBackingStore(PlatformCALayerRemote*);
    virtual ~RemoteLayerBackingStore();

    enum class Type : bool {
        IOSurface,
        Bitmap
    };

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    enum class IncludeDisplayList : bool { No, Yes };
#endif

    virtual bool isRemoteLayerWithRemoteRenderingBackingStore() const { return false; }
    virtual bool isRemoteLayerWithInProcessRenderingBackingStore() const { return false; }

    struct Parameters {
        Type type { Type::Bitmap };
        WebCore::FloatSize size;
        WebCore::DestinationColorSpace colorSpace { WebCore::DestinationColorSpace::SRGB() };
        float scale { 1.0f };
        bool deepColor { false };
        bool isOpaque { false };

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        IncludeDisplayList includeDisplayList { IncludeDisplayList::No };
#endif

        friend bool operator==(const Parameters&, const Parameters&) = default;
    };

    virtual void ensureBackingStore(const Parameters&);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay();

    void setDelegatedContents(const WebCore::PlatformCALayerDelegatedContents&);

    // Returns true if we need to encode the buffer.
    bool layerWillBeDisplayed();
    bool needsDisplay() const;

    bool performDelegatedLayerDisplay();

    void paintContents();
    virtual void createContextAndPaintContents() = 0;

    virtual Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> createFlushers() = 0;

    WebCore::FloatSize size() const { return m_parameters.size; }
    float scale() const { return m_parameters.scale; }
    bool usesDeepColorBackingStore() const;
    WebCore::DestinationColorSpace colorSpace() const { return m_parameters.colorSpace; }
    WebCore::PixelFormat pixelFormat() const;
    Type type() const { return m_parameters.type; }
    bool isOpaque() const { return m_parameters.isOpaque; }
    unsigned bytesPerPixel() const;
    bool supportsPartialRepaint() const;
    bool drawingRequiresClearedPixels() const;

    PlatformCALayerRemote* layer() const { return m_layer; }

    void encode(IPC::Encoder&) const;

    void enumerateRectsBeingDrawn(WebCore::GraphicsContext&, void (^)(WebCore::FloatRect));

    virtual bool hasFrontBuffer() const = 0;
    virtual bool frontBufferMayBeVolatile() const = 0;

    virtual void encodeBufferAndBackendInfos(IPC::Encoder&) const = 0;

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> takePendingFlushers();

    enum class BufferType {
        Front,
        Back,
        SecondaryBack
    };

    const WebCore::Region& dirtyRegion() { return m_dirtyRegion; }
    bool hasEmptyDirtyRegion() const { return m_dirtyRegion.isEmpty() || m_parameters.size.isEmpty(); }

    MonotonicTime lastDisplayTime() const { return m_lastDisplayTime; }

    virtual void clearBackingStore() = 0;

    virtual std::optional<ImageBufferBackendHandle> frontBufferHandle() const = 0;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    virtual std::optional<ImageBufferBackendHandle> displayListHandle() const  { return std::nullopt; }
#endif

    virtual void dump(WTF::TextStream&) const = 0;

protected:
    RemoteLayerBackingStoreCollection* backingStoreCollection() const;

    void drawInContext(WebCore::GraphicsContext&);

    void dirtyRepaintCounterIfNecessary();

    WebCore::IntRect layerBounds() const;

    PlatformCALayerRemote* m_layer;

    Parameters m_parameters;

    WebCore::Region m_dirtyRegion;

    std::optional<WebCore::IntRect> m_previouslyPaintedRect;

    // FIXME: This should be removed and m_bufferHandle should be used to ref the buffer once ShareableBitmap::Handle
    // can be encoded multiple times. http://webkit.org/b/234169
    std::optional<MachSendRight> m_contentsBufferHandle;
    std::optional<WebCore::RenderingResourceIdentifier> m_contentsRenderingResourceIdentifier;

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
    void applyBackingStoreToLayer(CALayer *, LayerContentsType, std::optional<WebCore::RenderingResourceIdentifier>, bool replayDynamicContentScalingDisplayListsIntoBackingStore);

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
    std::optional<WebCore::RenderingResourceIdentifier> m_contentsRenderingResourceIdentifier;

    std::optional<WebCore::IntRect> m_paintedRect;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<ImageBufferBackendHandle> m_displayListBufferHandle;
#endif

    bool m_isOpaque;
    RemoteLayerBackingStore::Type m_type;
};

WTF::TextStream& operator<<(WTF::TextStream&, BackingStoreNeedsDisplayReason);
WTF::TextStream& operator<<(WTF::TextStream&, const RemoteLayerBackingStore&);
WTF::TextStream& operator<<(WTF::TextStream&, const RemoteLayerBackingStoreProperties&);

} // namespace WebKit
