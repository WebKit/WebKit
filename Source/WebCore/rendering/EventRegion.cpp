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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EventRegionContext);

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
    m_eventRegion.unite(region, renderer, style, overrideUserModifyIsEditable);

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    auto rect = roundedRect.rect();
    if (auto* modelObject = dynamicDowncast<RenderLayerModelObject>(renderer))
        rect = snapRectToDevicePixelsIfNeeded(rect, *modelObject);
    auto layerBounds = transformAndClipIfNeeded(rect, [](auto affineTransform, auto rect) {
        return affineTransform.mapRect(rect);
    });

    // Same transform as `transformAndClipIfNeeded`.
    std::optional<AffineTransform> transform;
    if (!m_transformStack.isEmpty()) {
        transform = m_transformStack.last();
        rect = transform->mapRect(rect);
    }

    // The paths we generate to match shapes are complete and relative to the bounds.
    // But the layerBounds we pass are already clipped.
    // Keep track of the offset so we can adjust the paths location if needed.
    auto clipOffset = rect.location() - layerBounds.location();

    uniteInteractionRegions(renderer, layerBounds, clipOffset, transform);
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

static std::optional<FloatRect> guardRectForRegionBounds(const InteractionRegion& region)
{
    constexpr int minimumSize = 20;
    constexpr int occlusionMargin = 10;
    constexpr int complexSegmentsCount = 20;

    bool isSmallRect = false;
    bool isComplexShape =  region.clipPath
        && region.clipPath->segmentsIfExists()
        && region.clipPath->segmentsIfExists()->size() > complexSegmentsCount;

    auto guardRect = region.rectInLayerCoordinates;

    if (guardRect.width() < minimumSize) {
        guardRect.inflateX(occlusionMargin);
        isSmallRect = true;
    }

    if (guardRect.height() < minimumSize) {
        guardRect.inflateY(occlusionMargin);
        isSmallRect = true;
    }

    if (isSmallRect || isComplexShape)
        return guardRect;

    return std::nullopt;
}

void EventRegionContext::uniteInteractionRegions(RenderObject& renderer, const FloatRect& layerBounds, const FloatSize& clipOffset, const std::optional<AffineTransform>& transform)
{
    if (!renderer.page().shouldBuildInteractionRegions())
        return;

    if (auto interactionRegion = interactionRegionForRenderedRegion(renderer, layerBounds, clipOffset, transform)) {
        auto rectForTracking = enclosingIntRect(interactionRegion->rectInLayerCoordinates);
        
        if (interactionRegion->type == InteractionRegion::Type::Occlusion) {
            if (m_occlusionRects.contains(rectForTracking))
                return;
            m_occlusionRects.add(rectForTracking);
            
            m_interactionRegions.append(*interactionRegion);
            return;
        }

        if (interactionRegion->type == InteractionRegion::Type::Guard) {
            if (m_guardRects.contains(rectForTracking))
                return;
            m_guardRects.set(rectForTracking, Inflated::No);

            m_interactionRegions.append(*interactionRegion);
            return;
        }
        
        
        if (m_interactionRectsAndContentHints.contains(rectForTracking)) {
            m_interactionRectsAndContentHints.set(rectForTracking, interactionRegion->contentHint);
            return;
        }

        bool defaultContentHint = interactionRegion->contentHint == InteractionRegion::ContentHint::Default;
        if (defaultContentHint && shouldConsolidateInteractionRegion(renderer, rectForTracking, interactionRegion->elementIdentifier))
            return;

        // This region might be a container we can remove later.
        bool hasNoVisualBorders = !renderer.hasVisibleBoxDecorations();
        if (hasNoVisualBorders) {
            if (auto* renderElement = dynamicDowncast<RenderElement>(renderer))
                m_containerRemovalCandidates.add(renderElement->element()->identifier());
        }

        m_interactionRectsAndContentHints.add(rectForTracking, interactionRegion->contentHint);

        auto discoveredAddResult = m_discoveredRegionsByElement.add(interactionRegion->elementIdentifier, Vector<InteractionRegion>());
        discoveredAddResult.iterator->value.append(*interactionRegion);
        if (!discoveredAddResult.isNewEntry)
            return;

        auto guardRect = guardRectForRegionBounds(*interactionRegion);
        if (guardRect) {
            auto result = m_guardRects.add(enclosingIntRect(guardRect.value()), Inflated::Yes);
            if (result.isNewEntry) {
                m_interactionRegions.append({
                    InteractionRegion::Type::Guard,
                    interactionRegion->elementIdentifier,
                    guardRect.value()
                });
            }
        }

        m_interactionRegions.append(*interactionRegion);
    }
}

bool EventRegionContext::shouldConsolidateInteractionRegion(RenderObject& renderer, const IntRect& bounds, const ElementIdentifier& elementIdentifier)
{
    for (auto& ancestor : ancestorsOfType<RenderElement>(renderer)) {
        if (!ancestor.element())
            continue;

        auto ancestorElementIdentifier = ancestor.element()->identifier();
        auto discoveredIterator = m_discoveredRegionsByElement.find(ancestorElementIdentifier);

        // The ancestor has no known InteractionRegion, we can skip it.
        if (discoveredIterator == m_discoveredRegionsByElement.end()) {
            // If it has a border / background, stop the search.
            if (ancestor.hasVisibleBoxDecorations())
                return false;
            continue;
        }

        // The ancestor has multiple known rects (e.g. multi-line links), we can skip it.
        if (discoveredIterator->value.size() > 1)
            continue;

        auto& ancestorBounds = discoveredIterator->value.first().rectInLayerCoordinates;

        constexpr float looseContainmentMargin = 3.0;
        FloatRect ancestorBoundsForLooseContainmentCheck = ancestorBounds;
        ancestorBoundsForLooseContainmentCheck.inflate(looseContainmentMargin);

        // The ancestor's InteractionRegion does not contain ours, we don't consolidate and stop the search.
        if (!ancestorBoundsForLooseContainmentCheck.contains(bounds))
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

        bool hasNoVisualBorders = !renderer.hasVisibleBoxDecorations();

        bool canConsolidate = hasNoVisualBorders
            && (majorOverlap || elementIdentifier == ancestorElementIdentifier);

        // We're consolidating the region based on this ancestor, it shouldn't be removed or candidate for removal.
        if (canConsolidate) {
            m_containerRemovalCandidates.remove(ancestorElementIdentifier);
            m_containersToRemove.remove(ancestorElementIdentifier);
            return true;
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

void EventRegionContext::convertGuardContainersToInterationIfNeeded(float minimumCornerRadius)
{
    for (auto& region : m_interactionRegions) {
        if (region.type != InteractionRegion::Type::Guard)
            continue;

        if (!m_discoveredRegionsByElement.contains(region.elementIdentifier)) {
            auto rectForTracking = enclosingIntRect(region.rectInLayerCoordinates);
            auto result = m_interactionRectsAndContentHints.add(rectForTracking, region.contentHint);
            if (result.isNewEntry) {
                region.type = InteractionRegion::Type::Interaction;
                region.cornerRadius = minimumCornerRadius;
                m_discoveredRegionsByElement.add(region.elementIdentifier, Vector<InteractionRegion>({ region }));
            }
        }
    }
}

void EventRegionContext::shrinkWrapInteractionRegions()
{
    for (size_t i = 0; i < m_interactionRegions.size(); ++i) {
        auto& region = m_interactionRegions[i];
        if (region.type != InteractionRegion::Type::Interaction)
            continue;

        auto discoveredIterator = m_discoveredRegionsByElement.find(region.elementIdentifier);
        if (discoveredIterator == m_discoveredRegionsByElement.end())
            continue;

        auto discoveredRegions = discoveredIterator->value;
        if (discoveredRegions.size() == 1) {
            auto rectForTracking = enclosingIntRect(region.rectInLayerCoordinates);
            region.contentHint = m_interactionRectsAndContentHints.get(rectForTracking);
            continue;
        }

        FloatRect layerBounds;
        bool canUseSingleRect = true;
        Vector<InteractionRegion> toAddAfterMerge;
        Vector<FloatRect> discoveredRects;
        discoveredRects.reserveInitialCapacity(discoveredRegions.size());
        for (const auto& discoveredRegion : discoveredRegions) {
            auto previousArea = layerBounds.area();
            auto rect = discoveredRegion.rectInLayerCoordinates;
            auto overlap = rect;
            overlap.intersect(layerBounds);
            layerBounds.unite(rect);
            auto growth = layerBounds.area() - previousArea;
            if (growth > rect.area() - overlap.area() + std::numeric_limits<float>::epsilon())
                canUseSingleRect = false;

            auto rectForTracking = enclosingIntRect(rect);
            auto hint = m_interactionRectsAndContentHints.get(rectForTracking);
            if (hint != region.contentHint)
                toAddAfterMerge.append(discoveredRegion);
            else if (growth > std::numeric_limits<float>::epsilon())
                discoveredRects.append(rect);
        }

        if (canUseSingleRect)
            region.rectInLayerCoordinates = layerBounds;
        else {
            Path path = PathUtilities::pathWithShrinkWrappedRects(discoveredRects, region.cornerRadius);
            region.rectInLayerCoordinates = layerBounds;
            path.translate(-toFloatSize(layerBounds.location()));
            region.clipPath = path;
            region.cornerRadius = 0;
        }

        auto finalRegionRectForTracking = enclosingIntRect(region.rectInLayerCoordinates);
        for (auto& extraRegion : toAddAfterMerge) {
            auto extraRectForTracking = enclosingIntRect(extraRegion.rectInLayerCoordinates);
            // Do not insert a new region if it creates a duplicated Interaction Rect.
            if (finalRegionRectForTracking == extraRectForTracking) {
                region.contentHint = m_interactionRectsAndContentHints.get(extraRectForTracking);
                continue;
            }
            extraRegion.contentHint = m_interactionRectsAndContentHints.get(extraRectForTracking);
            m_interactionRegions.insert(++i, WTFMove(extraRegion));
        }
    }
}

void EventRegionContext::removeSuperfluousInteractionRegions()
{
    m_interactionRegions.removeAllMatching([&] (auto& region) {
        if (region.type != InteractionRegion::Type::Guard)
            return m_containersToRemove.contains(region.elementIdentifier);

        auto guardRect = enclosingIntRect(region.rectInLayerCoordinates);
        auto guardIterator = m_guardRects.find(guardRect);
        if (guardIterator != m_guardRects.end() && guardIterator->value == Inflated::No)
            return false;
        for (const auto& interactionRect : m_interactionRectsAndContentHints.keys()) {
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

void EventRegionContext::copyInteractionRegionsToEventRegion(float minimumCornerRadius)
{
    convertGuardContainersToInterationIfNeeded(minimumCornerRadius);
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

void EventRegion::unite(const Region& region, RenderObject& renderer, const RenderStyle& style, bool overrideUserModifyIsEditable)
{
    if (renderer.usedPointerEvents() == PointerEvents::None)
        return;

    m_region.unite(region);

#if ENABLE(TOUCH_ACTION_REGIONS)
    uniteTouchActions(region, style.usedTouchActions());
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    uniteEventListeners(region, style.eventListenerRegionTypes());
#endif

#if ENABLE(EDITABLE_REGION)
    if (m_editableRegion && (overrideUserModifyIsEditable || style.usedUserModify() != UserModify::ReadOnly)) {
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
