/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#include "RemoteLayerBackingStore.h"
#include <WebCore/EventRegion.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS CALayer;
OBJC_CLASS NSString;
#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIView;
#endif

namespace WebKit {

class RemoteLayerTreeScrollbars;

class RemoteLayerTreeNode : public CanMakeWeakPtr<RemoteLayerTreeNode> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier, Markable<WebCore::LayerHostingContextIdentifier>, RetainPtr<CALayer>);
#if PLATFORM(IOS_FAMILY)
    RemoteLayerTreeNode(WebCore::PlatformLayerIdentifier, Markable<WebCore::LayerHostingContextIdentifier>, RetainPtr<UIView>);
#endif
    ~RemoteLayerTreeNode();

    static std::unique_ptr<RemoteLayerTreeNode> createWithPlainLayer(WebCore::PlatformLayerIdentifier);

    CALayer *layer() const { return m_layer.get(); }
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    CALayer *interactionRegionsLayer() const { return m_interactionRegionsLayer.get(); }

    struct VisibleRectMarkableTraits {
        static bool isEmptyValue(const WebCore::FloatRect& value)
        {
            return value.isEmpty();
        }

        static WebCore::FloatRect emptyValue()
        {
            return { };
        }
    };

    const Markable<WebCore::FloatRect, VisibleRectMarkableTraits> visibleRect() const { return m_visibleRect; }
    void setVisibleRect(const WebCore::FloatRect& value) { m_visibleRect = value; }
#endif
#if PLATFORM(IOS_FAMILY)
    UIView *uiView() const { return m_uiView.get(); }
#endif

    WebCore::PlatformLayerIdentifier layerID() const { return m_layerID; }

    const WebCore::EventRegion& eventRegion() const { return m_eventRegion; }
    void setEventRegion(const WebCore::EventRegion&);

    // Non-ancestor scroller that controls positioning of the layer.
    WebCore::PlatformLayerIdentifier actingScrollContainerID() const { return m_actingScrollContainerID; }
    // Ancestor scrollers that don't affect positioning of the layer.
    const Vector<WebCore::PlatformLayerIdentifier>& stationaryScrollContainerIDs() const { return m_stationaryScrollContainerIDs; }

    void setActingScrollContainerID(WebCore::PlatformLayerIdentifier value) { m_actingScrollContainerID = value; }
    void setStationaryScrollContainerIDs(Vector<WebCore::PlatformLayerIdentifier>&& value) { m_stationaryScrollContainerIDs = WTFMove(value); }

    void detachFromParent();

    static WebCore::PlatformLayerIdentifier layerID(CALayer *);
    static RemoteLayerTreeNode* forCALayer(CALayer *);

    static NSString *appendLayerDescription(NSString *description, CALayer *);

#if ENABLE(SCROLLING_THREAD)
    WebCore::ScrollingNodeID scrollingNodeID() const { return m_scrollingNodeID; }
    void setScrollingNodeID(WebCore::ScrollingNodeID nodeID) { m_scrollingNodeID = nodeID; }
#endif

    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostingIdentifier() const { return m_remoteContextHostingIdentifier; }
    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostedIdentifier() const { return m_remoteContextHostedIdentifier; }
    void setRemoteContextHostedIdentifier(WebCore::LayerHostingContextIdentifier identifier) { m_remoteContextHostedIdentifier = identifier; }

    // A cached CAIOSurface object to retain CA render resources.
    struct CachedContentsBuffer {
        BufferAndBackendInfo imageBufferInfo;
        RetainPtr<id> buffer;
    };

    Vector<CachedContentsBuffer> takeCachedContentsBuffers() { return std::exchange(m_cachedContentsBuffers, { }); }
    void setCachedContentsBuffers(Vector<CachedContentsBuffer>&& buffers)
    {
        m_cachedContentsBuffers = WTFMove(buffers);
    }

    std::optional<WebCore::RenderingResourceIdentifier> asyncContentsIdentifier() const
    {
        return m_asyncContentsIdentifier;
    }

    void setAsyncContentsIdentifier(const WebCore::RenderingResourceIdentifier& identifier)
    {
        m_asyncContentsIdentifier = identifier;
    }

private:
    void initializeLayer();

    WebCore::PlatformLayerIdentifier m_layerID;
    Markable<WebCore::LayerHostingContextIdentifier> m_remoteContextHostingIdentifier;
    Markable<WebCore::LayerHostingContextIdentifier> m_remoteContextHostedIdentifier;

    RetainPtr<CALayer> m_layer;
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    RetainPtr<CALayer> m_interactionRegionsLayer;
    Markable<WebCore::FloatRect, VisibleRectMarkableTraits> m_visibleRect;
#endif
#if PLATFORM(IOS_FAMILY)
    RetainPtr<UIView> m_uiView;
#endif

    WebCore::EventRegion m_eventRegion;

#if ENABLE(SCROLLING_THREAD)
    WebCore::ScrollingNodeID m_scrollingNodeID { 0 };
#endif

    WebCore::PlatformLayerIdentifier m_actingScrollContainerID;
    Vector<WebCore::PlatformLayerIdentifier> m_stationaryScrollContainerIDs;

    Vector<CachedContentsBuffer> m_cachedContentsBuffers;
    std::optional<WebCore::RenderingResourceIdentifier> m_asyncContentsIdentifier;
};

}
