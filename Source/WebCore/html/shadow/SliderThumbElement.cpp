/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "SliderThumbElement.h"

#include "CSSValueKeywords.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "MouseEvent.h"
#include "RenderFlexibleBox.h"
#include "RenderSlider.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>

#if ENABLE(IOS_TOUCH_EVENTS)
#include "Document.h"
#include "Page.h"
#include "TouchEvent.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(SliderThumbElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(SliderContainerElement);
WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSliderThumb);

inline static Decimal sliderPosition(HTMLInputElement& element)
{
    const StepRange stepRange(element.createStepRange(RejectAny));
    const Decimal oldValue = parseToDecimalForNumberType(element.value(), stepRange.defaultValue());
    return stepRange.proportionFromValue(stepRange.clampValue(oldValue));
}

inline static bool hasVerticalAppearance(HTMLInputElement& input)
{
    ASSERT(input.renderer());
    const RenderStyle& sliderStyle = input.renderer()->style();

#if ENABLE(VIDEO)
    if (sliderStyle.appearance() == MediaVolumeSliderPart && input.renderer()->theme().usesVerticalVolumeSlider())
        return true;
#endif

    return sliderStyle.appearance() == SliderVerticalPart;
}

// --------------------------------

RenderSliderThumb::RenderSliderThumb(SliderThumbElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

void RenderSliderThumb::updateAppearance(const RenderStyle* parentStyle)
{
    if (parentStyle->appearance() == SliderVerticalPart)
        mutableStyle().setAppearance(SliderThumbVerticalPart);
    else if (parentStyle->appearance() == SliderHorizontalPart)
        mutableStyle().setAppearance(SliderThumbHorizontalPart);
    else if (parentStyle->appearance() == MediaSliderPart)
        mutableStyle().setAppearance(MediaSliderThumbPart);
    else if (parentStyle->appearance() == MediaVolumeSliderPart)
        mutableStyle().setAppearance(MediaVolumeSliderThumbPart);
    else if (parentStyle->appearance() == MediaFullScreenVolumeSliderPart)
        mutableStyle().setAppearance(MediaFullScreenVolumeSliderThumbPart);
    if (style().hasAppearance()) {
        ASSERT(element());
        theme().adjustSliderThumbSize(mutableStyle(), element());
    }
}

bool RenderSliderThumb::isSliderThumb() const
{
    return true;
}

// --------------------------------

// FIXME: Find a way to cascade appearance and adjust heights, and get rid of this class.
// http://webkit.org/b/62535
class RenderSliderContainer final : public RenderFlexibleBox {
    WTF_MAKE_ISO_ALLOCATED_INLINE(RenderSliderContainer);
public:
    RenderSliderContainer(SliderContainerElement& element, RenderStyle&& style)
        : RenderFlexibleBox(element, WTFMove(style))
    {
    }

public:
    RenderBox::LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

private:
    void layout() override;
    bool isFlexibleBoxImpl() const override { return true; }
};

RenderBox::LogicalExtentComputedValues RenderSliderContainer::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    ASSERT(element()->shadowHost());
    auto& input = downcast<HTMLInputElement>(*element()->shadowHost());
    bool isVertical = hasVerticalAppearance(input);

#if ENABLE(DATALIST_ELEMENT)
    if (input.renderer()->isSlider() && !isVertical && input.list()) {
        int offsetFromCenter = theme().sliderTickOffsetFromTrackCenter();
        LayoutUnit trackHeight;
        if (offsetFromCenter < 0)
            trackHeight = -2 * offsetFromCenter;
        else {
            int tickLength = theme().sliderTickSize().height();
            trackHeight = 2 * (offsetFromCenter + tickLength);
        }
        float zoomFactor = style().effectiveZoom();
        if (zoomFactor != 1.0)
            trackHeight *= zoomFactor;

        return RenderBox::computeLogicalHeight(trackHeight, logicalTop);
    }
#endif
    if (isVertical)
        logicalHeight = RenderSlider::defaultTrackLength;
    return RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
}

void RenderSliderContainer::layout()
{
    ASSERT(element()->shadowHost());
    auto& input = downcast<HTMLInputElement>(*element()->shadowHost());
    bool isVertical = hasVerticalAppearance(input);
    mutableStyle().setFlexDirection(isVertical ? FlexDirection::Column : FlexDirection::Row);
    TextDirection oldTextDirection = style().direction();
    if (isVertical) {
        // FIXME: Work around rounding issues in RTL vertical sliders. We want them to
        // render identically to LTR vertical sliders. We can remove this work around when
        // subpixel rendering is enabled on all ports.
        mutableStyle().setDirection(TextDirection::LTR);
    }

    RenderBox* thumb = input.sliderThumbElement() ? input.sliderThumbElement()->renderBox() : nullptr;
    RenderBox* track = input.sliderTrackElement() ? input.sliderTrackElement()->renderBox() : nullptr;
    // Force a layout to reset the position of the thumb so the code below doesn't move the thumb to the wrong place.
    // FIXME: Make a custom Render class for the track and move the thumb positioning code there.
    if (track)
        track->setChildNeedsLayout(MarkOnlyThis);

    RenderFlexibleBox::layout();

    mutableStyle().setDirection(oldTextDirection);
    // These should always exist, unless someone mutates the shadow DOM (e.g., in the inspector).
    if (!thumb || !track)
        return;

    double percentageOffset = sliderPosition(input).toDouble();
    LayoutUnit availableExtent = isVertical ? track->contentHeight() : track->contentWidth();
    availableExtent -= isVertical ? thumb->height() : thumb->width();
    LayoutUnit offset = percentageOffset * availableExtent;
    LayoutPoint thumbLocation = thumb->location();
    if (isVertical)
        thumbLocation.setY(thumbLocation.y() + track->contentHeight() - thumb->height() - offset);
    else if (style().isLeftToRightDirection())
        thumbLocation.setX(thumbLocation.x() + offset);
    else
        thumbLocation.setX(thumbLocation.x() - offset);
    thumb->setLocation(thumbLocation);
    thumb->repaint();
}

// --------------------------------

SliderThumbElement::SliderThumbElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

void SliderThumbElement::setPositionFromValue()
{
    // Since the code to calculate position is in the RenderSliderThumb layout
    // path, we don't actually update the value here. Instead, we poke at the
    // renderer directly to trigger layout.
    if (renderer())
        renderer()->setNeedsLayout();
}

RenderPtr<RenderElement> SliderThumbElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSliderThumb>(*this, WTFMove(style));
}

bool SliderThumbElement::isDisabledFormControl() const
{
    auto input = hostInput();
    return !input || input->isDisabledFormControl();
}

bool SliderThumbElement::matchesReadWritePseudoClass() const
{
    auto input = hostInput();
    return input && input->matchesReadWritePseudoClass();
}

RefPtr<Element> SliderThumbElement::focusDelegate()
{
    return hostInput();
}

void SliderThumbElement::dragFrom(const LayoutPoint& point)
{
    Ref<SliderThumbElement> protectedThis(*this);
    setPositionFromPoint(point);
#if !PLATFORM(IOS_FAMILY)
    startDragging();
#endif
}

void SliderThumbElement::setPositionFromPoint(const LayoutPoint& absolutePoint)
{
    auto input = hostInput();
    if (!input)
        return;

    auto* inputRenderer = input->renderBox();
    if (!inputRenderer)
        return;

    auto* thumbRenderer = renderBox();
    if (!thumbRenderer)
        return;

    ASSERT(input->sliderTrackElement());
    auto* trackRenderer = input->sliderTrackElement()->renderBox();
    if (!trackRenderer)
        return;

    // Do all the tracking math relative to the input's renderer's box.

    bool isVertical = hasVerticalAppearance(*input);
    bool isLeftToRightDirection = thumbRenderer->style().isLeftToRightDirection();
    
    auto offset = inputRenderer->absoluteToLocal(absolutePoint, UseTransforms);
    auto trackBoundingBox = trackRenderer->localToContainerQuad(FloatRect { { }, trackRenderer->size() }, inputRenderer).enclosingBoundingBox();

    LayoutUnit trackLength;
    LayoutUnit position;
    if (isVertical) {
        trackLength = trackRenderer->contentHeight() - thumbRenderer->height();
        position = offset.y() - thumbRenderer->height() / 2 - trackBoundingBox.y() - thumbRenderer->marginBottom();
    } else {
        trackLength = trackRenderer->contentWidth() - thumbRenderer->width();
        position = offset.x() - thumbRenderer->width() / 2 - trackBoundingBox.x();
        position -= isLeftToRightDirection ? thumbRenderer->marginLeft() : thumbRenderer->marginRight();
    }

    position = std::max<LayoutUnit>(0, std::min(position, trackLength));
    auto ratio = Decimal::fromDouble(static_cast<double>(position) / trackLength);
    auto fraction = isVertical || !isLeftToRightDirection ? Decimal(1) - ratio : ratio;
    auto stepRange = input->createStepRange(RejectAny);
    auto value = stepRange.clampValue(stepRange.valueFromProportion(fraction));

#if ENABLE(DATALIST_ELEMENT)
    const LayoutUnit snappingThreshold = renderer()->theme().sliderTickSnappingThreshold();
    if (snappingThreshold > 0) {
        if (std::optional<Decimal> closest = input->findClosestTickMarkValue(value)) {
            double closestFraction = stepRange.proportionFromValue(*closest).toDouble();
            double closestRatio = isVertical || !isLeftToRightDirection ? 1.0 - closestFraction : closestFraction;
            LayoutUnit closestPosition = trackLength * closestRatio;
            if ((closestPosition - position).abs() <= snappingThreshold)
                value = *closest;
        }
    }
#endif

    String valueString = serializeForNumberType(value);
    if (valueString == input->value())
        return;

    // FIXME: This is no longer being set from renderer. Consider updating the method name.
    input->setValueFromRenderer(valueString);
    if (renderer())
        renderer()->setNeedsLayout();
}

void SliderThumbElement::startDragging()
{
    if (RefPtr<Frame> frame = document().frame()) {
        frame->eventHandler().setCapturingMouseEventsElement(this);
        m_inDragMode = true;
    }
}

void SliderThumbElement::stopDragging()
{
    if (!m_inDragMode)
        return;

    if (RefPtr<Frame> frame = document().frame())
        frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    m_inDragMode = false;
    if (renderer())
        renderer()->setNeedsLayout();
}

#if !PLATFORM(IOS_FAMILY)
void SliderThumbElement::defaultEventHandler(Event& event)
{
    if (!is<MouseEvent>(event)) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // FIXME: Should handle this readonly/disabled check in more general way.
    // Missing this kind of check is likely to occur elsewhere if adding it in each shadow element.
    auto input = hostInput();
    if (!input || input->isDisabledFormControl()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent& mouseEvent = downcast<MouseEvent>(event);
    bool isLeftButton = mouseEvent.button() == LeftButton;
    const AtomicString& eventType = mouseEvent.type();

    // We intentionally do not call event->setDefaultHandled() here because
    // MediaControlTimelineElement::defaultEventHandler() wants to handle these
    // mouse events.
    if (eventType == eventNames().mousedownEvent && isLeftButton) {
        startDragging();
        return;
    } else if (eventType == eventNames().mouseupEvent && isLeftButton) {
        input->dispatchFormControlChangeEvent();
        stopDragging();
        return;
    } else if (eventType == eventNames().mousemoveEvent) {
        if (m_inDragMode)
            setPositionFromPoint(mouseEvent.absoluteLocation());
        return;
    }

    HTMLDivElement::defaultEventHandler(mouseEvent);
}
#endif

#if !PLATFORM(IOS_FAMILY)

bool SliderThumbElement::willRespondToMouseMoveEvents()
{
    const auto input = hostInput();
    if (input && !input->isDisabledFormControl() && m_inDragMode)
        return true;

    return HTMLDivElement::willRespondToMouseMoveEvents();
}

bool SliderThumbElement::willRespondToMouseClickEvents()
{
    const auto input = hostInput();
    if (input && !input->isDisabledFormControl())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

#endif // !PLATFORM(IOS_FAMILY)

void SliderThumbElement::willDetachRenderers()
{
    if (m_inDragMode) {
        if (RefPtr<Frame> frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    }
#if ENABLE(IOS_TOUCH_EVENTS)
    unregisterForTouchEvents();
#endif
}

#if ENABLE(IOS_TOUCH_EVENTS)

unsigned SliderThumbElement::exclusiveTouchIdentifier() const
{
    return m_exclusiveTouchIdentifier;
}

void SliderThumbElement::setExclusiveTouchIdentifier(unsigned identifier)
{
    ASSERT(m_exclusiveTouchIdentifier == NoIdentifier);
    m_exclusiveTouchIdentifier = identifier;
}

void SliderThumbElement::clearExclusiveTouchIdentifier()
{
    m_exclusiveTouchIdentifier = NoIdentifier;
}

static Touch* findTouchWithIdentifier(TouchList& list, unsigned identifier)
{
    unsigned length = list.length();
    for (unsigned i = 0; i < length; ++i) {
        RefPtr<Touch> touch = list.item(i);
        if (touch->identifier() == identifier)
            return touch.get();
    }
    return nullptr;
}

void SliderThumbElement::handleTouchStart(TouchEvent& touchEvent)
{
    RefPtr<TouchList> targetTouches = touchEvent.targetTouches();
    if (!targetTouches)
        return;

    if (targetTouches->length() != 1)
        return;

    RefPtr<Touch> touch = targetTouches->item(0);
    if (!renderer())
        return;
    IntRect boundingBox = renderer()->absoluteBoundingBoxRect();
    // Ignore the touch if it is not really inside the thumb.
    if (!boundingBox.contains(touch->pageX(), touch->pageY()))
        return;

    setExclusiveTouchIdentifier(touch->identifier());

    startDragging();
    touchEvent.setDefaultHandled();
}

void SliderThumbElement::handleTouchMove(TouchEvent& touchEvent)
{
    unsigned identifier = exclusiveTouchIdentifier();
    if (identifier == NoIdentifier)
        return;

    RefPtr<TouchList> targetTouches = touchEvent.targetTouches();
    if (!targetTouches)
        return;

    RefPtr<Touch> touch = findTouchWithIdentifier(*targetTouches, identifier);
    if (!touch)
        return;

    if (m_inDragMode)
        setPositionFromPoint(IntPoint(touch->pageX(), touch->pageY()));
    touchEvent.setDefaultHandled();
}

void SliderThumbElement::handleTouchEndAndCancel(TouchEvent& touchEvent)
{
    unsigned identifier = exclusiveTouchIdentifier();
    if (identifier == NoIdentifier)
        return;

    RefPtr<TouchList> targetTouches = touchEvent.targetTouches();
    if (!targetTouches)
        return;
    // If our exclusive touch still exists, it was not the touch
    // that ended, so we should not stop dragging.
    RefPtr<Touch> exclusiveTouch = findTouchWithIdentifier(*targetTouches, identifier);
    if (exclusiveTouch)
        return;

    clearExclusiveTouchIdentifier();

    auto input = hostInput();
    if (input)
        input->dispatchFormControlChangeEvent();
    stopDragging();
}

void SliderThumbElement::didAttachRenderers()
{
    if (shouldAcceptTouchEvents())
        registerForTouchEvents();
}

void SliderThumbElement::handleTouchEvent(TouchEvent& touchEvent)
{
    auto input = hostInput();
    ASSERT(input);
    if (input->isReadOnly() || input->isDisabledFormControl()) {
        clearExclusiveTouchIdentifier();
        stopDragging();
        touchEvent.setDefaultHandled();
        HTMLDivElement::defaultEventHandler(touchEvent);
        return;
    }

    const AtomicString& eventType = touchEvent.type();
    if (eventType == eventNames().touchstartEvent) {
        handleTouchStart(touchEvent);
        return;
    }
    if (eventType == eventNames().touchendEvent || eventType == eventNames().touchcancelEvent) {
        handleTouchEndAndCancel(touchEvent);
        return;
    }
    if (eventType == eventNames().touchmoveEvent) {
        handleTouchMove(touchEvent);
        return;
    }

    HTMLDivElement::defaultEventHandler(touchEvent);
}

bool SliderThumbElement::shouldAcceptTouchEvents()
{
    return renderer() && !isDisabledFormControl();
}

void SliderThumbElement::registerForTouchEvents()
{
    if (m_isRegisteredAsTouchEventListener)
        return;

    ASSERT(shouldAcceptTouchEvents());

    document().addTouchEventHandler(*this);
    m_isRegisteredAsTouchEventListener = true;
}

void SliderThumbElement::unregisterForTouchEvents()
{
    if (!m_isRegisteredAsTouchEventListener)
        return;

    clearExclusiveTouchIdentifier();
    stopDragging();

    document().removeTouchEventHandler(*this);
    m_isRegisteredAsTouchEventListener = false;
}

#endif // ENABLE(IOS_TOUCH_EVENTS)

void SliderThumbElement::hostDisabledStateChanged()
{
    if (isDisabledFormControl())
        stopDragging();

#if ENABLE(IOS_TOUCH_EVENTS)
    if (shouldAcceptTouchEvents())
        registerForTouchEvents();
    else
        unregisterForTouchEvents();
#endif
}

RefPtr<HTMLInputElement> SliderThumbElement::hostInput() const
{
    // Only HTMLInputElement creates SliderThumbElement instances as its shadow nodes.
    // So, shadowHost() must be an HTMLInputElement.
    return downcast<HTMLInputElement>(shadowHost());
}

std::optional<ElementStyle> SliderThumbElement::resolveCustomStyle(const RenderStyle&, const RenderStyle* hostStyle)
{
    // This doesn't actually compute style. This is just a hack to pick shadow pseudo id when host style is known.

    static NeverDestroyed<const AtomicString> sliderThumbShadowPseudoId("-webkit-slider-thumb", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> mediaSliderThumbShadowPseudoId("-webkit-media-slider-thumb", AtomicString::ConstructFromLiteral);

    if (!hostStyle)
        return std::nullopt;

    switch (hostStyle->appearance()) {
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaFullScreenVolumeSliderPart:
    case MediaFullScreenVolumeSliderThumbPart:
        m_shadowPseudoId = mediaSliderThumbShadowPseudoId;
        break;
    default:
        m_shadowPseudoId = sliderThumbShadowPseudoId;
    }

    return std::nullopt;
}

const AtomicString& SliderThumbElement::shadowPseudoId() const
{
    return m_shadowPseudoId;
}

Ref<Element> SliderThumbElement::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return create(targetDocument);
}

// --------------------------------

inline SliderContainerElement::SliderContainerElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<SliderContainerElement> SliderContainerElement::create(Document& document)
{
    return adoptRef(*new SliderContainerElement(document));
}

RenderPtr<RenderElement> SliderContainerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderSliderContainer>(*this, WTFMove(style));
}

std::optional<ElementStyle> SliderContainerElement::resolveCustomStyle(const RenderStyle&, const RenderStyle* hostStyle)
{
    // This doesn't actually compute style. This is just a hack to pick shadow pseudo id when host style is known.

    static NeverDestroyed<const AtomicString> mediaSliderContainer("-webkit-media-slider-container", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> sliderContainer("-webkit-slider-container", AtomicString::ConstructFromLiteral);

    if (!hostStyle)
        return std::nullopt;

    switch (hostStyle->appearance()) {
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaFullScreenVolumeSliderPart:
    case MediaFullScreenVolumeSliderThumbPart:
        m_shadowPseudoId = mediaSliderContainer;
        break;
    default:
        m_shadowPseudoId = sliderContainer;
    }

    return std::nullopt;
}

const AtomicString& SliderContainerElement::shadowPseudoId() const
{
    return m_shadowPseudoId;
}

}
