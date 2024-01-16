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

#include "config.h"
#include "EventRegion.h"

#include "HTMLFormControlElement.h"
#include "Logging.h"
#include "Path.h"
#include "PathUtilities.h"
#include "RenderAncestorIterator.h"
#include "RenderBox.h"
#include "RenderStyleInlines.h"
#include "SimpleRange.h"
#include "WindRule.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

EventRegionContext::EventRegionContext(EventRegion& eventRegion)
    : m_eventRegion(eventRegion)
{
}

EventRegionContext::~EventRegionContext() = default;

void EventRegionContext::unite(const FloatRoundedRect& roundedRect, RenderObject& renderer, const RenderStyle& style, bool overrideUserModifyIsEditable)
{
    auto transformAndClipIfNeeded = [&](auto input, auto transform) {
        if (m_transformStack.isEmpty() && m_clipStack.isEmpty())
            return input;

        auto transformedAndClippedInput = m_transformStack.isEmpty() ? input : transform(m_transformStack.last(), input);
        if (!m_clipStack.isEmpty())
            transformedAndClippedInput.intersect(m_clipStack.last());

        return transformedAndClippedInput;
    };

    auto region = transformAndClipIfNeeded(approximateAsRegion(roundedRect), [](auto affineTransform, auto region) {
        return affineTransform.mapRegion(region);
    });
    m_eventRegion.unite(region, style, overrideUserModifyIsEditable);

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    auto rect = roundedRect.rect();
    if (auto* modelObject = dynamicDowncast<RenderLayerModelObject>(renderer))
        rect = snapRectToDevicePixelsIfNeeded(rect, *modelObject);
    auto layerBounds = transformAndClipIfNeeded(rect, [](auto affineTransform, auto rect) {
        return affineTransform.mapRect(rect);
    });
    uniteInteractionRegions(renderer, layerBounds);
#else
    UNUSED_PARAM(renderer);
#endif
}

bool EventRegionContext::contains(const IntRect& rect) const
{
    if (m_transformStack.isEmpty())
        return m_eventRegion.contains(rect);

    return m_eventRegion.contains(m_transformStack.last().mapRect(rect));
}

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

static std::optional<FloatRect> guardRectForRegionBounds(const FloatRect& regionBounds)
{
    constexpr int minimumSize = 20;
    constexpr int occlusionMargin = 10;

    bool isSmallRect = false;
    auto occlusionRect = regionBounds;

    if (occlusionRect.width() < minimumSize) {
        occlusionRect.inflateX(occlusionMargin);
        isSmallRect = true;
    }

    if (occlusionRect.height() < minimumSize) {
        occlusionRect.inflateY(occlusionMargin);
        isSmallRect = true;
    }

    if (isSmallRect)
        return occlusionRect;
    return std::nullopt;
}

void EventRegionContext::uniteInteractionRegions(RenderObject& renderer, const FloatRect& layerBounds)
{
    if (!renderer.page().shouldBuildInteractionRegions())
        return;

    if (auto interactionRegion = interactionRegionForRenderedRegion(renderer, layerBounds)) {
        auto rectForTracking = enclosingIntRect(interactionRegion->rectInLayerCoordinates);
        
        if (interactionRegion->type == InteractionRegion::Type::Occlusion) {
            if (m_occlusionRects.contains(rectForTracking))
                return;
            m_occlusionRects.add(rectForTracking);
            
            m_interactionRegions.append(*interactionRegion);
            return;
        }
        
        
        if (m_interactionRects.contains(rectForTracking))
            return;

        if (shouldConsolidateInteractionRegion(renderer, rectForTracking))
            return;

        m_interactionRects.add(rectForTracking);

        auto discoveredIterator = m_discoveredRectsByElement.find(interactionRegion->elementIdentifier);
        if (discoveredIterator != m_discoveredRectsByElement.end()) {
            discoveredIterator->value.append(interactionRegion->rectInLayerCoordinates);
            return;
        }

        Vector<FloatRect, 1> discoveredRects;
        discoveredRects.append(interactionRegion->rectInLayerCoordinates);
        m_discoveredRectsByElement.add(interactionRegion->elementIdentifier, discoveredRects);
    
        auto guardRect = guardRectForRegionBounds(interactionRegion->rectInLayerCoordinates);
        if (guardRect) {
            m_interactionRegions.append({
                InteractionRegion::Type::Guard,
                interactionRegion->elementIdentifier,
                guardRect.value()
            });
        }

        m_interactionRegions.append(*interactionRegion);
    }
}

bool EventRegionContext::shouldConsolidateInteractionRegion(RenderObject& renderer, const IntRect& bounds)
{
    for (auto& ancestor : ancestorsOfType<RenderElement>(renderer)) {
        if (!ancestor.element())
            continue;

        auto ancestorElementIdentifier = ancestor.element()->identifier();
        auto discoveredIterator = m_discoveredRectsByElement.find(ancestorElementIdentifier);

        // The ancestor has no known InteractionRegion, we can skip it.
        if (discoveredIterator == m_discoveredRectsByElement.end()) {
            // If it has a border / background, stop the search.
            if (ancestor.hasVisibleBoxDecorations())
                return false;
            continue;
        }

        // The ancestor has multiple known rects (e.g. multi-line links), we can skip it.
        if (discoveredIterator->value.size() > 1)
            continue;

        auto& ancestorBounds = discoveredIterator->value.first();

        // The ancestor's InteractionRegion does not contain ours, we don't consolidate and stop the search.
        if (!ancestorBounds.contains(bounds))
            return false;

        constexpr auto maxMargin = 50;
        float marginLeft = bounds.x() - ancestorBounds.x();
        float marginRight = ancestorBounds.maxX() - bounds.maxX();
        float marginTop = bounds.y() - ancestorBounds.y();
        float marginBottom = ancestorBounds.maxY() - bounds.maxY();
        bool majorOverlap = marginLeft <= maxMargin
            && marginRight <= maxMargin
            && marginTop <= maxMargin
            && marginBottom <= maxMargin;

        constexpr auto offCenterThreshold = 2;
        bool centered = std::abs(bounds.center().x() - ancestorBounds.center().x()) < offCenterThreshold
            || std::abs(bounds.center().y() - ancestorBounds.center().y()) < offCenterThreshold;

        bool hasNoVisualBorders = !renderer.hasVisibleBoxDecorations();

        bool canConsolidate = hasNoVisualBorders
            && (majorOverlap || (centered && elementMatchesHoverRules(*ancestor.element())));

        // We're consolidating the region based on this ancestor, it shouldn't be removed or candidate for removal.
        if (canConsolidate) {
            m_containerRemovalCandidates.remove(ancestorElementIdentifier);
            m_containersToRemove.remove(ancestorElementIdentifier);
            return true;
        }

        // We can't consolidate this region but it might be a container we can remove later.
        if (hasNoVisualBorders) {
            if (auto* renderElement = dynamicDowncast<RenderElement>(renderer))
                m_containerRemovalCandidates.add(renderElement->element()->identifier());
        }

        // We found a region nested inside a container candidate for removal, flag it for removal.
        if (m_containerRemovalCandidates.contains(ancestorElementIdentifier)) {
            m_containerRemovalCandidates.remove(ancestorElementIdentifier);
            m_containersToRemove.add(ancestorElementIdentifier);
        }

        return false;
    }

    return false;
}

void EventRegionContext::shrinkWrapInteractionRegions()
{
    for (auto& region : m_interactionRegions) {
        if (region.type != InteractionRegion::Type::Interaction)
            continue;

        auto discoveredIterator = m_discoveredRectsByElement.find(region.elementIdentifier);
        if (discoveredIterator == m_discoveredRectsByElement.end())
            continue;

        auto discoveredRects = discoveredIterator->value;
        if (discoveredRects.size() == 1)
            continue;

        FloatRect layerBounds;
        for (const auto& rect : discoveredRects)
            layerBounds.unite(rect);

        Path path = PathUtilities::pathWithShrinkWrappedRects(discoveredRects, region.cornerRadius);
        region.rectInLayerCoordinates = layerBounds;
        path.translate(-toFloatSize(layerBounds.location()));
        region.clipPath = path;
        region.cornerRadius = 0;
    }
}

void EventRegionContext::removeSuperfluousInteractionRegions()
{
    m_interactionRegions.removeAllMatching([&] (auto& region) {
        if (region.type != InteractionRegion::Type::Guard)
            return m_containersToRemove.contains(region.elementIdentifier);

        auto guardRect = enclosingIntRect(region.rectInLayerCoordinates);
        for (const auto& interactionRect : m_interactionRects) {
            auto intersection = interactionRect;
            intersection.intersect(guardRect);

            if (intersection.isEmpty())
                continue;

            // This is an interactive container of the guarded region.
            if (intersection.contains(guardRect))
                continue;

            // This is probably the element being guarded.
            if (intersection.contains(interactionRect) && guardRect.center() == interactionRect.center())
                continue;

            bool tooMuchOverlap = interactionRect.width() / 2 < intersection.width()
                || interactionRect.height() / 2 < intersection.height();

            if (tooMuchOverlap)
                return true;
        }

        return false;
    });
}

void EventRegionContext::copyInteractionRegionsToEventRegion()
{
    removeSuperfluousInteractionRegions();
    shrinkWrapInteractionRegions();
    m_eventRegion.appendInteractionRegions(m_interactionRegions);
}

#endif

EventRegion::EventRegion() = default;

EventRegion::EventRegion(Region&& region
#if ENABLE(TOUCH_ACTION_REGIONS)
    , Vector<WebCore::Region> touchActionRegions
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
    , WebCore::Region wheelEventListenerRegion
    , WebCore::Region nonPassiveWheelEventListenerRegion
#endif
#if ENABLE(EDITABLE_REGION)
    , std::optional<WebCore::Region> editableRegion
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    , Vector<WebCore::InteractionRegion> interactionRegions
#endif
    )
    : m_region(WTFMove(region))
#if ENABLE(TOUCH_ACTION_REGIONS)
    , m_touchActionRegions(WTFMove(touchActionRegions))
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
    , m_wheelEventListenerRegion(WTFMove(wheelEventListenerRegion))
    , m_nonPassiveWheelEventListenerRegion(WTFMove(nonPassiveWheelEventListenerRegion))
#endif
#if ENABLE(EDITABLE_REGION)
    , m_editableRegion(WTFMove(editableRegion))
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    , m_interactionRegions(WTFMove(interactionRegions))
#endif
{
}

void EventRegion::unite(const Region& region, const RenderStyle& style, bool overrideUserModifyIsEditable)
{
    if (style.effectivePointerEvents() == PointerEvents::None)
        return;

    m_region.unite(region);

#if ENABLE(TOUCH_ACTION_REGIONS)
    uniteTouchActions(region, style.effectiveTouchActions());
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    uniteEventListeners(region, style.eventListenerRegionTypes());
#endif

#if ENABLE(EDITABLE_REGION)
    if (m_editableRegion && (overrideUserModifyIsEditable || style.effectiveUserModify() != UserModify::ReadOnly)) {
        m_editableRegion->unite(region);
        LOG_WITH_STREAM(EventRegions, stream << " uniting editable region");
    }
#else
    UNUSED_PARAM(overrideUserModifyIsEditable);
#endif

#if !ENABLE(TOUCH_ACTION_REGIONS) && !ENABLE(WHEEL_EVENT_REGIONS) && !ENABLE(EDITABLE_REGION)
    UNUSED_PARAM(style);
#endif
}

void EventRegion::translate(const IntSize& offset)
{
    m_region.translate(offset);

#if ENABLE(TOUCH_ACTION_REGIONS)
    for (auto& touchActionRegion : m_touchActionRegions)
        touchActionRegion.translate(offset);
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    m_wheelEventListenerRegion.translate(offset);
    m_nonPassiveWheelEventListenerRegion.translate(offset);
#endif

#if ENABLE(EDITABLE_REGION)
    if (m_editableRegion)
        m_editableRegion->translate(offset);
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    for (auto& region : m_interactionRegions)
        region.rectInLayerCoordinates.move(offset);
#endif
}

#if ENABLE(TOUCH_ACTION_REGIONS)
static inline unsigned toIndex(TouchAction touchAction)
{
    switch (touchAction) {
    case TouchAction::None:
        return 0;
    case TouchAction::Manipulation:
        return 1;
    case TouchAction::PanX:
        return 2;
    case TouchAction::PanY:
        return 3;
    case TouchAction::PinchZoom:
        return 4;
    case TouchAction::Auto:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static inline TouchAction toTouchAction(unsigned index)
{
    switch (index) {
    case 0:
        return TouchAction::None;
    case 1:
        return TouchAction::Manipulation;
    case 2:
        return TouchAction::PanX;
    case 3:
        return TouchAction::PanY;
    case 4:
        return TouchAction::PinchZoom;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return TouchAction::Auto;
}

void EventRegion::uniteTouchActions(const Region& touchRegion, OptionSet<TouchAction> touchActions)
{
    for (auto touchAction : touchActions) {
        if (touchAction == TouchAction::Auto)
            break;
        auto index = toIndex(touchAction);
        if (m_touchActionRegions.size() < index + 1)
            m_touchActionRegions.grow(index + 1);
    }

    for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
        auto regionTouchAction = toTouchAction(i);
        if (touchActions.contains(regionTouchAction)) {
            m_touchActionRegions[i].unite(touchRegion);
            LOG_WITH_STREAM(EventRegions, stream << " uniting for TouchAction " << regionTouchAction);
        } else {
            m_touchActionRegions[i].subtract(touchRegion);
            LOG_WITH_STREAM(EventRegions, stream << " subtracting for TouchAction " << regionTouchAction);
        }
    }
}

const Region* EventRegion::regionForTouchAction(TouchAction action) const
{
    unsigned actionIndex = toIndex(action);
    if (actionIndex >= m_touchActionRegions.size())
        return nullptr;

    return &m_touchActionRegions[actionIndex];
}

OptionSet<TouchAction> EventRegion::touchActionsForPoint(const IntPoint& point) const
{
    OptionSet<TouchAction> actions;

    for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
        if (m_touchActionRegions[i].contains(point)) {
            auto action = toTouchAction(i);
            actions.add(action);
            if (action == TouchAction::None || action == TouchAction::Manipulation)
                break;
        }
    }

    if (actions.isEmpty())
        return { TouchAction::Auto };

    return actions;
}
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
void EventRegion::uniteEventListeners(const Region& region, OptionSet<EventListenerRegionType> eventListenerRegionTypes)
{
    if (eventListenerRegionTypes.contains(EventListenerRegionType::Wheel)) {
        m_wheelEventListenerRegion.unite(region);
        LOG_WITH_STREAM(EventRegions, stream << " uniting for passive wheel event listener");
    }
    if (eventListenerRegionTypes.contains(EventListenerRegionType::NonPassiveWheel)) {
        m_nonPassiveWheelEventListenerRegion.unite(region);
        LOG_WITH_STREAM(EventRegions, stream << " uniting for active wheel event listener");
    }
}

OptionSet<EventListenerRegionType> EventRegion::eventListenerRegionTypesForPoint(const IntPoint& point) const
{
    OptionSet<EventListenerRegionType> regionTypes;
    if (m_wheelEventListenerRegion.contains(point))
        regionTypes.add(EventListenerRegionType::Wheel);
    if (m_nonPassiveWheelEventListenerRegion.contains(point))
        regionTypes.add(EventListenerRegionType::NonPassiveWheel);

    return regionTypes;
}

const Region& EventRegion::eventListenerRegionForType(EventListenerRegionType type) const
{
    switch (type) {
    case EventListenerRegionType::Wheel:
        return m_wheelEventListenerRegion;
    case EventListenerRegionType::NonPassiveWheel:
        return m_nonPassiveWheelEventListenerRegion;
    case EventListenerRegionType::MouseClick:
        break;
    }
    ASSERT_NOT_REACHED();
    return m_wheelEventListenerRegion;
}
#endif // ENABLE(WHEEL_EVENT_REGIONS)

#if ENABLE(EDITABLE_REGION)

bool EventRegion::containsEditableElementsInRect(const IntRect& rect) const
{
    return m_editableRegion && m_editableRegion->intersects(rect);
}

#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)

void EventRegion::appendInteractionRegions(const Vector<InteractionRegion>& interactionRegions)
{
    m_interactionRegions.appendVector(interactionRegions);
}

void EventRegion::clearInteractionRegions()
{
    m_interactionRegions.clear();
}

#endif

void EventRegion::dump(TextStream& ts) const
{
    ts << m_region;

#if ENABLE(TOUCH_ACTION_REGIONS)
    if (!m_touchActionRegions.isEmpty()) {
        TextStream::IndentScope indentScope(ts);
        ts << indent << "(touch-action\n";
        for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
            if (m_touchActionRegions[i].isEmpty())
                continue;
            TextStream::IndentScope indentScope(ts);
            ts << indent << "(" << toTouchAction(i);
            ts << indent << m_touchActionRegions[i];
            ts << indent << ")\n";
        }
        ts << indent << ")\n";
    }
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (!m_wheelEventListenerRegion.isEmpty()) {
        ts << indent << "(wheel event listener region" << m_wheelEventListenerRegion;
        if (!m_nonPassiveWheelEventListenerRegion.isEmpty()) {
            TextStream::IndentScope indentScope(ts);
            ts << indent << "(non-passive" << m_nonPassiveWheelEventListenerRegion;
            ts << indent << ")\n";
        }
        ts << indent << ")\n";
    }
#endif

#if ENABLE(EDITABLE_REGION)
    if (m_editableRegion && !m_editableRegion->isEmpty()) {
        ts << indent << "(editable region" << *m_editableRegion;
        ts << indent << ")\n";
    }
#endif
    
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    if (!m_interactionRegions.isEmpty()) {
        ts.dumpProperty("interaction regions", m_interactionRegions);
        ts << "\n";
    }
#endif
}

TextStream& operator<<(TextStream& ts, TouchAction touchAction)
{
    switch (touchAction) {
    case TouchAction::None:
        return ts << "none";
    case TouchAction::Manipulation:
        return ts << "manipulation";
    case TouchAction::PanX:
        return ts << "pan-x";
    case TouchAction::PanY:
        return ts << "pan-y";
    case TouchAction::PinchZoom:
        return ts << "pinch-zoom";
    case TouchAction::Auto:
        return ts << "auto";
    }
    ASSERT_NOT_REACHED();
    return ts;
}

TextStream& operator<<(TextStream& ts, const EventRegion& eventRegion)
{
    eventRegion.dump(ts);
    return ts;
}

}
