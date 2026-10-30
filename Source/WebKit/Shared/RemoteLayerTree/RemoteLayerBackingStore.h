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
#include "ImageBufferSetIdentifier.h"
#include "RemoteImageBufferSetProxy.h"
#include <WebCore/FloatRect.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/PlaceholderFrameIdentifier.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/Region.h>
#include <wtf/MachSendRight.h>
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/WeakRef.h>

OBJC_CLASS CALayer;
OBJC_CLASS UIView;

namespace WebCore {
class NativeImage;
struct PlatformCALayerDelegatedContents;
struct PlatformCALayerDelegatedContentsFinishedEvent;
}

namespace WebKit {

class PlatformCALayerRemote;
class RemoteLayerBackingStoreCollection;
class RemoteLayerTreeNode;
class RemoteLayerTreeHost;
class ThreadSafeImageBufferSetFlusher;
enum class SwapBuffersDisplayRequirement : uint8_t;
struct PlatformCALayerRemoteDelegatedContents;

enum class BackingStoreNeedsDisplayReason : uint8_t {
    None,
    NoFrontBuffer,
    FrontBufferIsVolatile,
    FrontBufferHasNoSharingHandle,
    HasDirtyRegion,
};

class RemoteLayerBackingStore final : public CanMakeWeakPtr<RemoteLayerBackingStore>, public CanMakeCheckedPtr<RemoteLayerBackingStore>, public ImageBufferSetClient {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerBackingStore);
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStore);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerBackingStore);
public:
    RemoteLayerBackingStore(PlatformCALayerRemote&);
    ~RemoteLayerBackingStore();

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    enum class Type : bool {
        IOSurface,
        Bitmap
    };

    struct Parameters {
        Type type { Type::Bitmap };
        WebCore::FloatSize size;
        WebCore::ColorSpace colorSpace { WebCore::ColorSpace::SRGB() };
        WebCore::ContentsFormat contentsFormat { WebCore::ContentsFormat::RGBA8 };
        float scale { 1.0f };
        bool isOpaque { false };

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        WebCore::IncludeDynamicContentScalingDisplayList includeDisplayList { WebCore::IncludeDynamicContentScalingDisplayList::No };
#endif

        friend bool operator==(const Parameters&, const Parameters&) = default;
    };

    void ensureBackingStore(const Parameters&);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay() final;

#if HAVE(SUPPORT_HDR_DISPLAY)
    bool setNeedsDisplayIfEDRHeadroomExceeds(float);
#endif

    void setDelegatedContents(const PlatformCALayerRemoteDelegatedContents&);

    // Returns true if we need to encode the buffer.
    bool layerWillBeDisplayed();
    bool layerWillBeDisplayedWithRenderingSuppression();
    bool needsDisplay() const;

    bool performDelegatedLayerDisplay();

    void paintContents();
    void flush();
    void prepareToDisplay();
    void createContextAndPaintContents();

    RemoteImageBufferSetProxy* bufferSet() { return m_bufferSet.get(); }

    std::unique_ptr<ThreadSafeImageBufferSetFlusher> createFlusher(ThreadSafeImageBufferSetFlusher::FlushType = ThreadSafeImageBufferSetFlusher::FlushType::BackendHandlesAndDrawing);
    void submitDrawingCommands();

    WebCore::FloatSize size() const { return m_parameters.size; }
    float scale() const { return m_parameters.scale; }
    WebCore::ContentsFormat contentsFormat() const { return m_parameters.contentsFormat; }
    WebCore::ColorSpace colorSpace() const { return m_parameters.colorSpace; }
    WebCore::PixelFormat LEGACY_NODELETE pixelFormat() const;
    Type type() const { return m_parameters.type; }
    bool isOpaque() const { return m_parameters.isOpaque; }
    unsigned LEGACY_NODELETE bytesPerPixel() const;
    bool supportsPartialRepaint() const;
    bool drawingRequiresClearedPixels() const;

    PlatformCALayerRemote& LEGACY_NODELETE layer() const;

    void encode(IPC::Encoder&) const;

    void enumerateRectsBeingDrawn(WebCore::GraphicsContext&, void (^)(WebCore::FloatRect));

    bool hasFrontBuffer() const;
    bool frontBufferMayBeVolatile() const;

    Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>> takePendingFlushers();

    const WebCore::Region& dirtyRegion() LIFETIME_BOUND { return m_dirtyRegion; }
    bool hasEmptyDirtyRegion() const { return m_dirtyRegion.isEmpty() || m_parameters.size.isEmpty(); }

    MonotonicTime lastDisplayTime() const { return m_lastDisplayTime; }

    void clearBackingStore();

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> displayListHandle() const;
#endif
    std::optional<ImageBufferSetIdentifier> bufferSetIdentifier() const;

    void dump(WTF::TextStream&) const;

    void purgeFrontBufferForTesting();
    void purgeBackBufferForTesting();
    void markFrontBufferVolatileForTesting();

private:
    RemoteLayerBackingStoreCollection* LEGACY_NODELETE backingStoreCollection() const LIFETIME_BOUND;

    void drawInContext(WebCore::GraphicsContext&);

    void dirtyRepaintCounterIfNecessary();

    WebCore::IntRect layerBounds() const;

    WeakRef<PlatformCALayerRemote> m_layer;
    const RefPtr<RemoteImageBufferSetProxy> m_bufferSet;

    Parameters m_parameters;

    WebCore::Region m_dirtyRegion;

    std::optional<WebCore::IntRect> m_previouslyPaintedRect;

    // FIXME: This should be removed and m_bufferHandle should be used to ref the buffer once ShareableBitmap::Handle
    // can be encoded multiple times. http://webkit.org/b/234169
    std::optional<ImageBufferBackendHandle> m_contentsBufferHandle;
    std::optional<WebCore::PlaceholderFrameIdentifier> m_contentsFrameIdentifier;

    Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>> m_frontBufferFlushers;

    PaintRectList m_paintingRects;

    MonotonicTime m_lastDisplayTime;

#if HAVE(SUPPORT_HDR_DISPLAY)
    float m_maxPaintedEDRHeadroom { 1 };
    float m_maxRequestedEDRHeadroom { 1 };
#endif
    bool m_needsFlush { false };
    bool m_cleared { true };
};

// The subset of RemoteLayerBackingStore that gets serialized into the UI
// process, and gets applied to the CALayer.
class RemoteLayerBackingStoreProperties {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerBackingStoreProperties);
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStoreProperties);
public:
    RemoteLayerBackingStoreProperties() = default;
    RemoteLayerBackingStoreProperties(RemoteLayerBackingStoreProperties&&) = default;
    RemoteLayerBackingStoreProperties(ImageBufferBackendHandle&&, WebCore::PlaceholderFrameIdentifier, bool opaque);

    // Returns false if the layer was left without the contents, because they could not be mapped.
    bool applyBackingStoreToNode(RemoteLayerTreeNode&, bool replayDynamicContentScalingDisplayListsIntoBackingStore, UIView* hostingView);

    const std::optional<ImageBufferBackendHandle>& bufferHandle() const LIFETIME_BOUND { return m_bufferHandle; };

    struct LayerContentsBufferInfo {
        RetainPtr<id> buffer;
        bool hasExtendedDynamicRange;
    };

    static LayerContentsBufferInfo layerContentsBufferFromBackendHandle(ImageBufferBackendHandle&&, bool isDelegatedDisplay);

    void dump(WTF::TextStream&) const;

    std::optional<ImageBufferSetIdentifier> bufferSetIdentifier() { return m_bufferSet; }
    void setBackendHandle(BufferSetBackendHandle&);

    std::optional<WebCore::PlaceholderFrameIdentifier> contentsFrameIdentifier() const { return m_contentsFrameIdentifier; };

private:
    friend struct IPC::ArgumentCoder<RemoteLayerBackingStoreProperties>;

    LayerContentsBufferInfo lookupCachedBuffer(RemoteLayerTreeNode&);

    std::optional<ImageBufferBackendHandle> m_bufferHandle;

    std::optional<ImageBufferSetIdentifier> m_bufferSet;

    std::optional<BufferAndBackendInfo> m_frontBufferInfo;
    std::optional<BufferAndBackendInfo> m_backBufferInfo;
    std::optional<BufferAndBackendInfo> m_secondaryBackBufferInfo;
    std::optional<WebCore::PlaceholderFrameIdentifier> m_contentsFrameIdentifier;

    std::optional<WebCore::IntRect> m_paintedRect;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> m_displayListBufferHandle;
#endif

    bool m_isOpaque { false };
    RemoteLayerBackingStore::Type m_type;
#if HAVE(SUPPORT_HDR_DISPLAY)
    float m_maxRequestedEDRHeadroom { 1 };
#endif
};

WTF::TextStream& operator<<(WTF::TextStream&, BackingStoreNeedsDisplayReason);
WTF::TextStream& operator<<(WTF::TextStream&, const RemoteLayerBackingStore&);
WTF::TextStream& operator<<(WTF::TextStream&, const RemoteLayerBackingStoreProperties&);

} // namespace WebKit
