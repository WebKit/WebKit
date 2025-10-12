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

#include "EventTrackingRegions.h"
#include "HTMLFormControlElement.h"
#include "Logging.h"
#include "Path.h"
#include "PathUtilities.h"
#include "RenderAncestorIterator.h"
#include "RenderBox.h"
#include "RenderObjectInlines.h"
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

void EventRegionContext::unite(const FloatRoundedRect& roundedRect, const RenderObject& renderer, const RenderStyle& style, bool overrideUserModifyIsEditable)
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

void EventRegionContext::uniteInteractionRegions(const RenderObject& renderer, const FloatRect& layerBounds, const FloatSize& clipOffset, const std::optional<AffineTransform>& transform)
{
    if (!renderer.page().shouldBuildInteractionRegions())
        return;

    if (auto interactionRegion = interactionRegionForRenderedRegion(renderer, layerBounds, clipOffset, transform)) {
        auto rectForTracking = enclosingIntRect(interactionRegion->rectInLayerCoordinates);

        if (interactionRegion->type == InteractionRegion::Type::Occlusion) {
            auto result = m_occlusionRects.add(rectForTracking);
            if (!result.isNewEntry)
                return;

            m_interactionRegions.append(*interactionRegion);
            return;
        }

        if (interactionRegion->type == InteractionRegion::Type::Guard) {
            auto result = m_guardRects.add(rectForTracking, Inflated::No);
            if (!result.isNewEntry)
                return;

            m_interactionRegions.append(*interactionRegion);
            return;
        }

        auto result = m_interactionRectsAndContentHints.set(rectForTracking, interactionRegion->contentHint);
        if (!result.isNewEntry)
            return;

        bool defaultContentHint = interactionRegion->contentHint == InteractionRegion::ContentHint::Default;
        if (defaultContentHint && shouldConsolidateInteractionRegion(renderer, rectForTracking, interactionRegion->nodeIdentifier))
            return;

        // This region might be a container we can remove later.
        bool hasNoVisualBorders = !renderer.hasVisibleBoxDecorations();
        if (hasNoVisualBorders) {
            if (auto* renderElement = dynamicDowncast<RenderElement>(renderer))
                m_containerRemovalCandidates.add(renderElement->element()->nodeIdentifier());
        }

        auto discoveredAddResult = m_discoveredRegionsByElement.add(interactionRegion->nodeIdentifier, Vector<InteractionRegion>());
        discoveredAddResult.iterator->value.append(*interactionRegion);
        if (!discoveredAddResult.isNewEntry)
            return;

        auto guardRect = guardRectForRegionBounds(*interactionRegion);
        if (guardRect) {
            auto result = m_guardRects.add(enclosingIntRect(guardRect.value()), Inflated::Yes);
            if (result.isNewEntry) {
                m_interactionRegions.append({
                    InteractionRegion::Type::Guard,
                    interactionRegion->nodeIdentifier,
                    guardRect.value()
                });
            }
        }

        m_interactionRegions.append(*interactionRegion);
    }
}

bool EventRegionContext::shouldConsolidateInteractionRegion(const RenderObject& renderer, const IntRect& bounds, const NodeIdentifier& nodeIdentifier)
{
    for (auto& ancestor : ancestorsOfType<RenderElement>(renderer)) {
        if (!ancestor.element())
            continue;

        auto ancestorElementIdentifier = ancestor.element()->nodeIdentifier();
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
            && (majorOverlap || nodeIdentifier == ancestorElementIdentifier);

        // We're consolidating the region based on this ancestor, it shouldn't be removed or candidate for removal.
        if (canConsolidate) {
            m_containerRemovalCandidates.remove(ancestorElementIdentifier);
            m_containersToRemove.remove(ancestorElementIdentifier);
            return true;
        }

        // We found a region nested inside a container candidate for removal, flag it for removal.
        if (m_containerRemovalCandidates.remove(ancestorElementIdentifier))
            m_containersToRemove.add(ancestorElementIdentifier);

        return false;
    }

    return false;
}

void EventRegionContext::convertGuardContainersToInterationIfNeeded(float minimumCornerRadius)
{
    for (auto& region : m_interactionRegions) {
        if (region.type != InteractionRegion::Type::Guard)
            continue;

        if (!m_discoveredRegionsByElement.contains(region.nodeIdentifier)) {
            auto rectForTracking = enclosingIntRect(region.rectInLayerCoordinates);
            auto result = m_interactionRectsAndContentHints.add(rectForTracking, region.contentHint);
            if (result.isNewEntry) {
                region.type = InteractionRegion::Type::Interaction;
                region.cornerRadius = minimumCornerRadius;
                m_discoveredRegionsByElement.add(region.nodeIdentifier, Vector<InteractionRegion>({ region }));
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

        auto discoveredIterator = m_discoveredRegionsByElement.find(region.nodeIdentifier);
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
        Vector<Path> discoveredClipPaths;

        discoveredRects.reserveInitialCapacity(discoveredRegions.size());
        discoveredClipPaths.reserveInitialCapacity(discoveredRegions.size());

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
            else if (growth > std::numeric_limits<float>::epsilon()) {
                // If the discovered region's shape should not be a rounded-rect
                // with uniform corner radii, its clipPath will be non-empty.
                if (auto clipPath = discoveredRegion.clipPath) {
                    AffineTransform transform;
                    transform.translate(discoveredRegion.rectInLayerCoordinates.location());

                    Path foundPath = *clipPath;
                    foundPath.transform(transform);

                    discoveredClipPaths.append(foundPath);
                } else if (discoveredRegion.useContinuousCorners) {
                    // If this region has continuous corners, we won't be able to
                    // shrink wrap it. Instead, find it's path so that it can be
                    // included in the final clip.
                    Path path;
                    path.addContinuousRoundedRect(discoveredRegion.rectInLayerCoordinates, discoveredRegion.cornerRadius);
                    discoveredClipPaths.append(path);
                } else
                    discoveredRects.append(rect);
            }
        }

        if (canUseSingleRect)
            region.rectInLayerCoordinates = layerBounds;
        else {
            Path shrinkWrappedRects = PathUtilities::pathWithShrinkWrappedRects(discoveredRects, region.cornerRadius);

            Path path;
            path.addPath(shrinkWrappedRects, { });
            for (Path clipPath : discoveredClipPaths)
                path.addPath(clipPath, { });

            path.translate(-toFloatSize(layerBounds.location()));

            region.clipPath = path;
            region.cornerRadius = 0;
            region.rectInLayerCoordinates = layerBounds;
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
            return m_containersToRemove.contains(region.nodeIdentifier);

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

void EventRegionContext::reserveCapacityForInteractionRegions(size_t previousSize)
{
    m_interactionRegions.reserveCapacity(previousSize);
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
#if ENABLE(TOUCH_EVENT_REGIONS)
    , EventTrackingRegions touchEventListenerRegion
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
#if ENABLE(TOUCH_EVENT_REGIONS)
    , m_touchEventListenerRegion(WTFMove(touchEventListenerRegion))
#endif
#if ENABLE(EDITABLE_REGION)
    , m_editableRegion(WTFMove(editableRegion))
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    , m_interactionRegions(WTFMove(interactionRegions))
#endif
{
}

void EventRegion::unite(const Region& region, const RenderObject& renderer, const RenderStyle& style, bool overrideUserModifyIsEditable)
{
    if (renderer.usedPointerEvents() == PointerEvents::None)
        return;

    m_region.unite(region);

#if ENABLE(TOUCH_ACTION_REGIONS)
    uniteTouchActions(region, style.usedTouchActions());
#endif

    uniteEventListeners(region, style.eventListenerRegionTypes());

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

#if ENABLE(TOUCH_EVENT_REGIONS)
OptionSet<EventListenerRegionType> touchEventTypes =
{
    EventListenerRegionType::TouchStart, EventListenerRegionType::NonPassiveTouchStart
    , EventListenerRegionType::TouchEnd, EventListenerRegionType::NonPassiveTouchEnd
    , EventListenerRegionType::TouchMove, EventListenerRegionType::NonPassiveTouchMove
    , EventListenerRegionType::TouchCancel, EventListenerRegionType::NonPassiveTouchCancel
    , EventListenerRegionType::PointerDown, EventListenerRegionType::NonPassivePointerDown
    , EventListenerRegionType::PointerEnter, EventListenerRegionType::NonPassivePointerEnter
    , EventListenerRegionType::PointerLeave, EventListenerRegionType::NonPassivePointerLeave
    , EventListenerRegionType::PointerMove, EventListenerRegionType::NonPassivePointerMove
    , EventListenerRegionType::PointerOut, EventListenerRegionType::NonPassivePointerOut
    , EventListenerRegionType::PointerOver, EventListenerRegionType::NonPassivePointerOver
    , EventListenerRegionType::PointerUp, EventListenerRegionType::NonPassivePointerUp
    , EventListenerRegionType::MouseMove, EventListenerRegionType::NonPassiveMouseMove
    , EventListenerRegionType::MouseDown, EventListenerRegionType::NonPassiveMouseDown
    , EventListenerRegionType::MouseMove, EventListenerRegionType::NonPassiveMouseMove
};

OptionSet<EventListenerRegionType> touchEventNonPassiveTypes =
{
    EventListenerRegionType::NonPassiveTouchStart
    , EventListenerRegionType::NonPassiveTouchEnd
    , EventListenerRegionType::NonPassiveTouchMove
    , EventListenerRegionType::NonPassiveTouchCancel
    , EventListenerRegionType::NonPassivePointerDown
    , EventListenerRegionType::NonPassivePointerEnter
    , EventListenerRegionType::NonPassivePointerLeave
    , EventListenerRegionType::NonPassivePointerMove
    , EventListenerRegionType::NonPassivePointerOut
    , EventListenerRegionType::NonPassivePointerOver
    , EventListenerRegionType::NonPassivePointerUp
    , EventListenerRegionType::NonPassiveMouseDown
    , EventListenerRegionType::NonPassiveMouseUp
    , EventListenerRegionType::NonPassiveMouseMove
};

static bool isNonPassiveTouchEventType(EventListenerRegionType eventListenerRegionType)
{
    return touchEventNonPassiveTypes.contains(eventListenerRegionType);
}

static bool containsTouchEventType(OptionSet<EventListenerRegionType> eventListenerRegionTypes)
{
    return eventListenerRegionTypes.containsAny(touchEventTypes);
}

static EventTrackingRegionsEventType eventTypeForEventListenerType(EventListenerRegionType eventType)
{
    switch (eventType) {
    case EventListenerRegionType::NonPassiveTouchStart:
        return EventTrackingRegionsEventType::Touchstart;
    case EventListenerRegionType::NonPassiveTouchEnd:
        return EventTrackingRegionsEventType::Touchend;
    case EventListenerRegionType::NonPassiveTouchMove:
        return EventTrackingRegionsEventType::Touchmove;
    case EventListenerRegionType::NonPassiveTouchCancel:
        return EventTrackingRegionsEventType::Touchforcechange;
    case EventListenerRegionType::NonPassivePointerDown:
        return EventTrackingRegionsEventType::Pointerdown;
    case EventListenerRegionType::NonPassivePointerEnter:
        return EventTrackingRegionsEventType::Pointerenter;
    case EventListenerRegionType::NonPassivePointerLeave:
        return EventTrackingRegionsEventType::Pointerleave;
    case EventListenerRegionType::NonPassivePointerMove:
        return EventTrackingRegionsEventType::Pointermove;
    case EventListenerRegionType::NonPassivePointerOut:
        return EventTrackingRegionsEventType::Pointerout;
    case EventListenerRegionType::NonPassivePointerOver:
        return EventTrackingRegionsEventType::Pointerover;
    case EventListenerRegionType::NonPassivePointerUp:
        return EventTrackingRegionsEventType::Pointerup;
    case EventListenerRegionType::NonPassiveMouseDown:
        return EventTrackingRegionsEventType::Mousedown;
    case EventListenerRegionType::NonPassiveMouseUp:
        return EventTrackingRegionsEventType::Mousemove;
    case EventListenerRegionType::NonPassiveMouseMove:
        return EventTrackingRegionsEventType::Mouseup;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return EventTrackingRegionsEventType::Touchend;
}
#endif

void EventRegion::uniteEventListeners(const Region& region, OptionSet<EventListenerRegionType> eventListenerRegionTypes)
{
#if ENABLE(WHEEL_EVENT_REGIONS)
    if (eventListenerRegionTypes.contains(EventListenerRegionType::Wheel)) {
        m_wheelEventListenerRegion.unite(region);
        LOG_WITH_STREAM(EventRegions, stream << " uniting for passive wheel event listener");
    }
    if (eventListenerRegionTypes.contains(EventListenerRegionType::NonPassiveWheel)) {
        m_nonPassiveWheelEventListenerRegion.unite(region);
        LOG_WITH_STREAM(EventRegions, stream << " uniting for active wheel event listener");
    }
#endif // ENABLE(WHEEL_EVENT_REGIONS)
#if ENABLE(TOUCH_EVENT_REGIONS)
    if (containsTouchEventType(eventListenerRegionTypes)) {
        m_touchEventListenerRegion.asynchronousDispatchRegion.unite(region);
        for (auto eventType : eventListenerRegionTypes) {
            if (!isNonPassiveTouchEventType(eventType))
                continue;
            m_touchEventListenerRegion.uniteSynchronousRegion(eventTypeForEventListenerType(eventType), region);
        }
        LOG_WITH_STREAM(EventRegions, stream << " uniting for touch event listener");
    }
#endif
#if !ENABLE(TOUCH_EVENT_REGIONS) && !ENABLE(WHEEL_EVENT_REGIONS)
    UNUSED_PARAM(region);
    UNUSED_PARAM(eventListenerRegionTypes);
#endif
}

#if ENABLE(TOUCH_EVENT_REGIONS)
TrackingType EventRegion::eventTrackingTypeForPoint(EventTrackingRegionsEventType event, const IntPoint& point) const
{
    return m_touchEventListenerRegion.trackingTypeForPoint(event, point);
}
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
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
    default:
            break;
    }
    ASSERT_NOT_REACHED();
    return m_wheelEventListenerRegion;
}
#endif

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
        ts << indent << "(touch-action\n"_s;
        for (unsigned i = 0; i < m_touchActionRegions.size(); ++i) {
            if (m_touchActionRegions[i].isEmpty())
                continue;
            TextStream::IndentScope indentScope(ts);
            ts << indent << '(' << toTouchAction(i);
            ts << indent << m_touchActionRegions[i];
            ts << indent << ")\n"_s;
        }
        ts << indent << ")\n"_s;
    }
#endif

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (!m_wheelEventListenerRegion.isEmpty()) {
        ts << indent << "(wheel event listener region"_s << m_wheelEventListenerRegion;
        if (!m_nonPassiveWheelEventListenerRegion.isEmpty()) {
            TextStream::IndentScope indentScope(ts);
            ts << indent << "(non-passive"_s << m_nonPassiveWheelEventListenerRegion;
            ts << indent << ")\n"_s;
        }
        ts << indent << ")\n"_s;
    }
#endif

#if ENABLE(TOUCH_EVENT_REGIONS)
    if (!m_touchEventListenerRegion.isEmpty())
        ts << indent << "(touch event listener region:"_s << m_touchEventListenerRegion << '\n';
#endif

#if ENABLE(EDITABLE_REGION)
    if (m_editableRegion && !m_editableRegion->isEmpty()) {
        ts << indent << "(editable region"_s << *m_editableRegion;
        ts << indent << ")\n"_s;
    }
#endif
    
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    if (!m_interactionRegions.isEmpty()) {
        ts.dumpProperty("interaction regions"_s, m_interactionRegions);
        ts << '\n';
    }
#endif
}

#if ENABLE(TOUCH_EVENT_REGIONS)
TextStream& operator<<(TextStream& ts, const TouchEventListenerRegion& region)
{
    if (!region.start.isEmpty())
        ts << " touchStart: "_s << region.start;
    if (!region.end.isEmpty())
        ts << " touchEnd: "_s << region.end;
    if (!region.cancel.isEmpty())
        ts << " touchCancel: "_s << region.cancel;
    if (!region.move.isEmpty())
        ts << " touchMove: "_s << region.move;
    return ts;
}
#endif

TextStream& operator<<(TextStream& ts, TouchAction touchAction)
{
    switch (touchAction) {
    case TouchAction::None:
        return ts << "none"_s;
    case TouchAction::Manipulation:
        return ts << "manipulation"_s;
    case TouchAction::PanX:
        return ts << "pan-x"_s;
    case TouchAction::PanY:
        return ts << "pan-y"_s;
    case TouchAction::PinchZoom:
        return ts << "pinch-zoom"_s;
    case TouchAction::Auto:
        return ts << "auto"_s;
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
