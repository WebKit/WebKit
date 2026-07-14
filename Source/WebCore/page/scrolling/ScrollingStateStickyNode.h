/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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

#if ENABLE(ASYNC_SCROLLING)

#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingStateNode.h>

#include <wtf/Forward.h>

namespace WebCore {

class StickyPositionViewportConstraints;

class ScrollingStateStickyNode final : public ScrollingStateNode {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ScrollingStateStickyNode, WEBCORE_EXPORT);
public:
    template<typename... Args> static Ref<ScrollingStateStickyNode> create(Args&&... args) { return adoptRef(*new ScrollingStateStickyNode(std::forward<Args>(args)...)); }

    Ref<ScrollingStateNode> clone(ScrollingStateTree&) override;

    virtual ~ScrollingStateStickyNode();

    WEBCORE_EXPORT void updateConstraints(const StickyPositionViewportConstraints&);
    const StickyPositionViewportConstraints& viewportConstraints() const LIFETIME_BOUND { return m_staticLayoutData->constraints; }

    const LayerRepresentation& viewportAnchorLayer() const LIFETIME_BOUND { return m_staticConfigData->viewportAnchorLayer; }
    WEBCORE_EXPORT void setViewportAnchorLayer(const LayerRepresentation&);

private:
    WEBCORE_EXPORT ScrollingStateStickyNode(ScrollingNodeID, Vector<Ref<ScrollingStateNode>>&&, OptionSet<ScrollingStateNodeProperty>, std::optional<PlatformLayerIdentifier>, StickyPositionViewportConstraints&&, LayerRepresentation&&);
    ScrollingStateStickyNode(ScrollingStateTree&, ScrollingNodeID);
    ScrollingStateStickyNode(const ScrollingStateStickyNode&, ScrollingStateTree&);

    FloatPoint computeClippingLayerPosition(const LayoutRect& viewportRect) const;
    FloatPoint computeAnchorLayerPosition(const LayoutRect& viewportRect) const;
    void reconcileLayerPositionForViewportRect(const LayoutRect& viewportRect, ScrollingLayerPositionAction) final;
    FloatSize scrollDeltaSinceLastCommit(const LayoutRect& viewportRect) const;

    void dumpProperties(WTF::TextStream&, OptionSet<ScrollingStateTreeAsTextBehavior>) const final;
    OptionSet<ScrollingStateNode::Property> applicableProperties() const final;
    bool hasUnchangedGroupsAs(const ScrollingStateNode&) const final;
    void clearLayerFieldsForUnchangedProperties() final;
#if ASSERT_ENABLED
    void verifyClearedLayerFieldsForUnchangedProperties() const final;
#endif

    bool hasViewportClippingLayer() const;

    // Layout-derived sticky-node state. CoW-shared via DataRef.
    class StaticLayoutStickyState : public ThreadSafeRefCounted<StaticLayoutStickyState> {
    public:
        static Ref<StaticLayoutStickyState> create() { return adoptRef(*new StaticLayoutStickyState); }
        Ref<StaticLayoutStickyState> copy() const { return adoptRef(*new StaticLayoutStickyState(*this)); }

        StickyPositionViewportConstraints constraints;

    private:
        StaticLayoutStickyState() = default;
        StaticLayoutStickyState(const StaticLayoutStickyState& other)
            : ThreadSafeRefCounted<StaticLayoutStickyState>()
            , constraints(other.constraints)
        {
        }
    };

    // Long-lived sticky-node configuration. CoW-shared via DataRef.
    class StaticConfigurationStickyState : public ThreadSafeRefCounted<StaticConfigurationStickyState> {
    public:
        static Ref<StaticConfigurationStickyState> create() { return adoptRef(*new StaticConfigurationStickyState); }
        Ref<StaticConfigurationStickyState> copy() const { return adoptRef(*new StaticConfigurationStickyState(*this)); }

        LayerRepresentation viewportAnchorLayer;

    private:
        StaticConfigurationStickyState() = default;
        StaticConfigurationStickyState(const StaticConfigurationStickyState& other)
            : ThreadSafeRefCounted<StaticConfigurationStickyState>()
            , viewportAnchorLayer(other.viewportAnchorLayer)
        {
        }
    };

    DataRef<StaticLayoutStickyState> m_staticLayoutData { StaticLayoutStickyState::create() };
    DataRef<StaticConfigurationStickyState> m_staticConfigData { StaticConfigurationStickyState::create() };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_STATE_NODE(ScrollingStateStickyNode, isStickyNode())

#endif // ENABLE(ASYNC_SCROLLING)
