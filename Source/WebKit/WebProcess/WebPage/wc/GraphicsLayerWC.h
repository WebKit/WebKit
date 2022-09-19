/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_WC)

#include "WCUpateInfo.h"
#include <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#include <wtf/DoublyLinkedList.h>

namespace WebCore {
class TransformState;
}

namespace WebKit {
class WCTiledBacking;

class GraphicsLayerWC final : public WebCore::GraphicsLayer, public DoublyLinkedListNode<GraphicsLayerWC> {
public:
    struct Observer {
        virtual void graphicsLayerAdded(GraphicsLayerWC&) = 0;
        virtual void graphicsLayerRemoved(GraphicsLayerWC&) = 0;
        virtual void commitLayerUpateInfo(WCLayerUpateInfo&&) = 0;
        virtual RefPtr<WebCore::ImageBuffer> createImageBuffer(WebCore::FloatSize) = 0;
    };

    GraphicsLayerWC(Type layerType, WebCore::GraphicsLayerClient&, Observer&);
    ~GraphicsLayerWC() override;

    void clearObserver() { m_observer = nullptr; }

    // GraphicsLayer
    WebCore::GraphicsLayer::PlatformLayerID primaryLayerID() const override;
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::FloatRect&, ShouldClipToLayer) override;
    void setContentsNeedsDisplay() override;
    bool setChildren(Vector<Ref<GraphicsLayer>>&&) override;
    void addChild(Ref<GraphicsLayer>&&) override;
    void addChildAtIndex(Ref<GraphicsLayer>&&, int index) override;
    void addChildAbove(Ref<GraphicsLayer>&&, GraphicsLayer* sibling) override;
    void addChildBelow(Ref<GraphicsLayer>&&, GraphicsLayer* sibling) override;
    bool replaceChild(GraphicsLayer* oldChild, Ref<GraphicsLayer>&& newChild) override;
    void removeFromParent() override;
    void setMaskLayer(RefPtr<GraphicsLayer>&&) override;
    void setReplicatedLayer(GraphicsLayer*) override;
    void setReplicatedByLayer(RefPtr<GraphicsLayer>&&) override;
    void setPosition(const WebCore::FloatPoint&) override;
    void setAnchorPoint(const WebCore::FloatPoint3D&) override;
    void setSize(const WebCore::FloatSize&) override;
    void setBoundsOrigin(const WebCore::FloatPoint&) override;
    void setTransform(const WebCore::TransformationMatrix&) override;
    void setChildrenTransform(const WebCore::TransformationMatrix&) override;
    void setPreserves3D(bool) override;
    void setMasksToBounds(bool) override;
    void setBackgroundColor(const WebCore::Color&) override;
    void setOpacity(float) override;
    void setContentsRect(const WebCore::FloatRect&) override;
    void setContentsClippingRect(const WebCore::FloatRoundedRect&) override;
    void setContentsRectClipsDescendants(bool) override;
    void setDrawsContent(bool) override;
    void setContentsVisible(bool) override;
    void setBackfaceVisibility(bool) override;
    void setContentsToSolidColor(const WebCore::Color&) override;
    void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    void setContentsDisplayDelegate(RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate>&&, ContentsLayerPurpose) override;
    bool shouldDirectlyCompositeImage(WebCore::Image*) const override { return false; }
    bool usesContentsLayer() const override;
    void setShowDebugBorder(bool) override;
    void setDebugBorder(const WebCore::Color&, float width) override;
    void setShowRepaintCounter(bool) override;
    bool setFilters(const WebCore::FilterOperations&) override;
    bool setBackdropFilters(const WebCore::FilterOperations&) override;
    void setBackdropFiltersRect(const WebCore::FloatRoundedRect&) override;
    void flushCompositingState(const WebCore::FloatRect& clipRect) override;
    void flushCompositingStateForThisLayerOnly() override;
    WebCore::TiledBacking* tiledBacking() const override;

protected:
    friend WCTiledBacking;

    RefPtr<WebCore::ImageBuffer> createImageBuffer(WebCore::FloatSize);
    
private:
    struct VisibleAndCoverageRects {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WebCore::FloatRect visibleRect;
        WebCore::FloatRect coverageRect;
        WebCore::TransformationMatrix animatingTransform;
    };

    enum ScheduleFlushOrNot { ScheduleFlush, DontScheduleFlush };
    void noteLayerPropertyChanged(OptionSet<WCLayerChange>, ScheduleFlushOrNot = ScheduleFlush);
    WebCore::TransformationMatrix transformByApplyingAnchorPoint(const WebCore::TransformationMatrix&) const;
    WebCore::TransformationMatrix layerTransform(const WebCore::FloatPoint&, const WebCore::TransformationMatrix* = nullptr) const;
    VisibleAndCoverageRects computeVisibleAndCoverageRect(WebCore::TransformState&, bool preserves3D) const;
    void recursiveCommitChanges(const WebCore::TransformState&);

    static GraphicsLayer::PlatformLayerID generateLayerID();

    friend class WTF::DoublyLinkedListNode<GraphicsLayerWC>;

    GraphicsLayerWC* m_prev;
    GraphicsLayerWC* m_next;
    WebCore::GraphicsLayer::PlatformLayerID m_layerID { generateLayerID() };
    Observer* m_observer;
    std::unique_ptr<WCTiledBacking> m_tiledBacking;
    PlatformLayer* m_platformLayer { nullptr };
    WebCore::Color m_solidColor;
    WebCore::Color m_debugBorderColor;
    OptionSet<WCLayerChange> m_uncommittedChanges;
    float m_debugBorderWidth { 0 };
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
