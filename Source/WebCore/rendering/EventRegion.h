/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <WebCore/AffineTransform.h>
#include <WebCore/EventTrackingRegions.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntRectHash.h>
#include <WebCore/InteractionRegion.h>
#include <WebCore/NodeIdentifier.h>
#include <WebCore/Region.h>
#include <WebCore/RegionContext.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/TouchAction.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class EventRegion;
class Path;
class RenderObject;
class RenderStyle;
enum class TrackingType : uint8_t;

class EventRegionContext final : public RegionContext {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(EventRegionContext, WEBCORE_EXPORT);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(EventRegionContext);
public:
    WEBCORE_EXPORT explicit EventRegionContext(EventRegion&);
    WEBCORE_EXPORT virtual ~EventRegionContext();

    bool isEventRegionContext() const final { return true; }

    WEBCORE_EXPORT void unite(const FloatRoundedRect&, const RenderObject&, const RenderStyle&, bool overrideUserModifyIsEditable = false);
    bool contains(const IntRect&) const;

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    void uniteInteractionRegions(const RenderObject&, const FloatRect&, const FloatSize&, const std::optional<AffineTransform>&);
    bool shouldConsolidateInteractionRegion(const RenderObject&, const IntRect&, const NodeIdentifier&);
    void convertGuardContainersToInterationIfNeeded(float minimumCornerRadius);
    void removeSuperfluousInteractionRegions();
    void shrinkWrapInteractionRegions();
    void copyInteractionRegionsToEventRegion(float minimumCornerRadius);
    void reserveCapacityForInteractionRegions(size_t);
#endif

private:
    EventRegion& m_eventRegion;

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    Vector<InteractionRegion> m_interactionRegions;
    HashMap<IntRect, InteractionRegion::ContentHint> m_interactionRectsAndContentHints;
    HashSet<IntRect> m_occlusionRects;
    enum class Inflated : bool { No, Yes };
    HashMap<IntRect, Inflated> m_guardRects;
    HashSet<NodeIdentifier> m_containerRemovalCandidates;
    HashSet<NodeIdentifier> m_containersToRemove;
    HashMap<NodeIdentifier, Vector<InteractionRegion>> m_discoveredRegionsByElement;
#endif
};

#if ENABLE(TOUCH_EVENT_REGIONS)
struct TouchEventListenerRegion {
    bool operator==(const TouchEventListenerRegion&) const = default;

    bool isEmpty() const { return start.isEmpty() && end.isEmpty() && move.isEmpty() && cancel.isEmpty(); }

    Region start;
    Region end;
    Region move;
    Region cancel;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const TouchEventListenerRegion&);
#endif

class EventRegion {
public:
    WEBCORE_EXPORT EventRegion();
    WEBCORE_EXPORT EventRegion(Region&&
#if ENABLE(TOUCH_ACTION_REGIONS)
    , Vector<WebCore::Region> touchActionRegions
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
    , WebCore::Region wheelEventListenerRegion
    , WebCore::Region nonPassiveWheelEventListenerRegion
#endif
#if ENABLE(TOUCH_EVENT_REGIONS)
    , EventTrackingRegions touchEventListenerRegion
#endif
#if ENABLE(EDITABLE_REGION)
    , std::optional<WebCore::Region>
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    , Vector<WebCore::InteractionRegion>
#endif
    );

    EventRegionContext makeContext() { return EventRegionContext(*this); }

    bool isEmpty() const { return m_region.isEmpty(); }

    friend bool operator==(const EventRegion&, const EventRegion&) = default;

    void unite(const Region&, const RenderObject&, const RenderStyle&, bool overrideUserModifyIsEditable = false);
    void translate(const IntSize&);

    bool contains(const IntPoint& point) const { return m_region.contains(point); }
    bool contains(const IntRect& rect) const { return m_region.contains(rect); }
    bool intersects(const IntRect& rect) const { return m_region.intersects(rect); }

    const Region& region() const { return m_region; }

#if ENABLE(TOUCH_ACTION_REGIONS)
    bool hasTouchActions() const { return !m_touchActionRegions.isEmpty(); }
    WEBCORE_EXPORT OptionSet<TouchAction> touchActionsForPoint(const IntPoint&) const;
    const Region* regionForTouchAction(TouchAction) const;
#endif

#if ENABLE(TOUCH_EVENT_REGIONS)
    WEBCORE_EXPORT TrackingType eventTrackingTypeForPoint(EventTrackingRegionsEventType, const IntPoint&) const;
    const EventTrackingRegions& touchEventListenerRegion() const { return m_touchEventListenerRegion; }
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    WEBCORE_EXPORT OptionSet<EventListenerRegionType> eventListenerRegionTypesForPoint(const IntPoint&) const;
    const Region& eventListenerRegionForType(EventListenerRegionType) const;
#endif

#if ENABLE(EDITABLE_REGION)
    void ensureEditableRegion();
    bool hasEditableRegion() const { return m_editableRegion.has_value(); }
    WEBCORE_EXPORT bool containsEditableElementsInRect(const IntRect&) const;
    Vector<IntRect, 1> rectsForEditableElements() const { return m_editableRegion ? m_editableRegion->rects() : Vector<IntRect, 1> { }; }
#endif

    void dump(TextStream&) const;

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    const Vector<InteractionRegion>& interactionRegions() const { return m_interactionRegions; }
    void appendInteractionRegions(const Vector<InteractionRegion>&);
    void clearInteractionRegions();
#endif

private:
    friend struct IPC::ArgumentCoder<EventRegion>;
#if ENABLE(TOUCH_ACTION_REGIONS)
    void uniteTouchActions(const Region&, OptionSet<TouchAction>);
#endif
    void uniteEventListeners(const Region&, OptionSet<EventListenerRegionType>);

    Region m_region;
#if ENABLE(TOUCH_ACTION_REGIONS)
    Vector<Region> m_touchActionRegions;
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
    Region m_wheelEventListenerRegion;
    Region m_nonPassiveWheelEventListenerRegion;
#endif
#if ENABLE(TOUCH_EVENT_REGIONS)
    EventTrackingRegions m_touchEventListenerRegion;
#endif
#if ENABLE(EDITABLE_REGION)
    std::optional<Region> m_editableRegion;
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    Vector<InteractionRegion> m_interactionRegions;
#endif
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const EventRegion&);

#if ENABLE(EDITABLE_REGION)

inline void EventRegion::ensureEditableRegion()
{
    if (!m_editableRegion)
        m_editableRegion.emplace();
}

#endif

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::EventRegionContext)
    static bool isType(const WebCore::RegionContext& regionContext) { return regionContext.isEventRegionContext(); }
SPECIALIZE_TYPE_TRAITS_END()
