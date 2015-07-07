/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "MouseEvent.h"
#include "RenderFlexibleBox.h"
#include "RenderSlider.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)
#include "Document.h"
#include "Page.h"
#include "TouchEvent.h"
#endif

namespace WebCore {

using namespace HTMLNames;

inline static Decimal sliderPosition(HTMLInputElement* element)
{
    const StepRange stepRange(element->createStepRange(RejectAny));
    const Decimal oldValue = parseToDecimalForNumberType(element->value(), stepRange.defaultValue());
    return stepRange.proportionFromValue(stepRange.clampValue(oldValue));
}

inline static bool hasVerticalAppearance(HTMLInputElement* input)
{
    ASSERT(input->renderer());
    const RenderStyle& sliderStyle = input->renderer()->style();

#if ENABLE(VIDEO)
    if (sliderStyle.appearance() == MediaVolumeSliderPart && input->renderer()->theme().usesVerticalVolumeSlider())
        return true;
#endif

    return sliderStyle.appearance() == SliderVerticalPart;
}

// --------------------------------

RenderSliderThumb::RenderSliderThumb(SliderThumbElement& element, PassRef<RenderStyle> style)
    : RenderBlockFlow(element, WTF::move(style))
{
}

void RenderSliderThumb::updateAppearance(RenderStyle* parentStyle)
{
    if (parentStyle->appearance() == SliderVerticalPart)
        style().setAppearance(SliderThumbVerticalPart);
    else if (parentStyle->appearance() == SliderHorizontalPart)
        style().setAppearance(SliderThumbHorizontalPart);
    else if (parentStyle->appearance() == MediaSliderPart)
        style().setAppearance(MediaSliderThumbPart);
    else if (parentStyle->appearance() == MediaVolumeSliderPart)
        style().setAppearance(MediaVolumeSliderThumbPart);
    else if (parentStyle->appearance() == MediaFullScreenVolumeSliderPart)
        style().setAppearance(MediaFullScreenVolumeSliderThumbPart);
    if (style().hasAppearance()) {
        ASSERT(element());
        theme().adjustSliderThumbSize(style(), *element());
    }
}

bool RenderSliderThumb::isSliderThumb() const
{
    return true;
}

// --------------------------------

// FIXME: Find a way to cascade appearance and adjust heights, and get rid of this class.
// http://webkit.org/b/62535
class RenderSliderContainer : public RenderFlexibleBox {
public:
    RenderSliderContainer(SliderContainerElement& element, PassRef<RenderStyle> style)
        : RenderFlexibleBox(element, WTF::move(style))
    {
    }

public:
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

private:
    virtual void layout() override;
};

void RenderSliderContainer::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    HTMLInputElement* input = element()->shadowHost()->toInputElement();
    bool isVertical = hasVerticalAppearance(input);

#if ENABLE(DATALIST_ELEMENT)
    if (input->renderer()->isSlider() && !isVertical && input->list()) {
        int offsetFromCenter = theme().sliderTickOffsetFromTrackCenter();
        LayoutUnit trackHeight = 0;
        if (offsetFromCenter < 0)
            trackHeight = -2 * offsetFromCenter;
        else {
            int tickLength = theme().sliderTickSize().height();
            trackHeight = 2 * (offsetFromCenter + tickLength);
        }
        float zoomFactor = style().effectiveZoom();
        if (zoomFactor != 1.0)
            trackHeight *= zoomFactor;

        RenderBox::computeLogicalHeight(trackHeight, logicalTop, computedValues);
        return;
    }
#endif
    if (isVertical)
        logicalHeight = RenderSlider::defaultTrackLength;
    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void RenderSliderContainer::layout()
{
    HTMLInputElement* input = element()->shadowHost()->toInputElement();
    bool isVertical = hasVerticalAppearance(input);
    style().setFlexDirection(isVertical ? FlowColumn : FlowRow);
    TextDirection oldTextDirection = style().direction();
    if (isVertical) {
        // FIXME: Work around rounding issues in RTL vertical sliders. We want them to
        // render identically to LTR vertical sliders. We can remove this work around when
        // subpixel rendering is enabled on all ports.
        style().setDirection(LTR);
    }

    RenderBox* thumb = input->sliderThumbElement() ? input->sliderThumbElement()->renderBox() : 0;
    RenderBox* track = input->sliderTrackElement() ? input->sliderTrackElement()->renderBox() : 0;
    // Force a layout to reset the position of the thumb so the code below doesn't move the thumb to the wrong place.
    // FIXME: Make a custom Render class for the track and move the thumb positioning code there.
    if (track)
        track->setChildNeedsLayout(MarkOnlyThis);

    RenderFlexibleBox::layout();

    style().setDirection(oldTextDirection);
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
}

// --------------------------------

SliderThumbElement::SliderThumbElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document)
    , m_inDragMode(false)
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)
    , m_exclusiveTouchIdentifier(NoIdentifier)
    , m_isRegisteredAsTouchEventListener(false)
#endif
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

RenderPtr<RenderElement> SliderThumbElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSliderThumb>(*this, WTF::move(style));
}

bool SliderThumbElement::isDisabledFormControl() const
{
    HTMLInputElement* input = hostInput();
    return !input || input->isDisabledFormControl();
}

bool SliderThumbElement::matchesReadOnlyPseudoClass() const
{
    HTMLInputElement* input = hostInput();
    return input && input->matchesReadOnlyPseudoClass();
}

bool SliderThumbElement::matchesReadWritePseudoClass() const
{
    HTMLInputElement* input = hostInput();
    return input && input->matchesReadWritePseudoClass();
}

Element* SliderThumbElement::focusDelegate()
{
    return hostInput();
}

void SliderThumbElement::dragFrom(const LayoutPoint& point)
{
    Ref<SliderThumbElement> protect(*this);
    setPositionFromPoint(point);
#if !PLATFORM(IOS)
    startDragging();
#endif
}

void SliderThumbElement::setPositionFromPoint(const LayoutPoint& absolutePoint)
{
    RefPtr<HTMLInputElement> input = hostInput();
    if (!input || !input->renderer() || !renderBox())
        return;

    HTMLElement* trackElement = input->sliderTrackElement();
    if (!trackElement->renderBox())
        return;

    // Do all the tracking math relative to the input's renderer's box.
    RenderBox& inputRenderer = *toRenderBox(input->renderer());
    RenderBox& trackRenderer = *trackElement->renderBox();

    bool isVertical = hasVerticalAppearance(input.get());
    bool isLeftToRightDirection = renderBox()->style().isLeftToRightDirection();
    
    LayoutPoint offset = roundedLayoutPoint(inputRenderer.absoluteToLocal(absolutePoint, UseTransforms));
    FloatRect trackBoundingBox = trackRenderer.localToContainerQuad(FloatRect(0, 0, trackRenderer.width(), trackRenderer.height()), &inputRenderer).enclosingBoundingBox();

    LayoutUnit trackLength;
    LayoutUnit position;
    if (isVertical) {
        trackLength = trackRenderer.contentHeight() - renderBox()->height();
        position = offset.y() - renderBox()->height() / 2 - trackBoundingBox.y() - renderBox()->marginBottom();
    } else {
        trackLength = trackRenderer.contentWidth() - renderBox()->width();
        position = offset.x() - renderBox()->width() / 2 - trackBoundingBox.x();
        position -= isLeftToRightDirection ? renderBox()->marginLeft() : renderBox()->marginRight();
    }

    position = std::max<LayoutUnit>(0, std::min(position, trackLength));
    const Decimal ratio = Decimal::fromDouble(static_cast<double>(position) / trackLength);
    const Decimal fraction = isVertical || !isLeftToRightDirection ? Decimal(1) - ratio : ratio;
    StepRange stepRange(input->createStepRange(RejectAny));
    Decimal value = stepRange.clampValue(stepRange.valueFromProportion(fraction));

#if ENABLE(DATALIST_ELEMENT)
    const LayoutUnit snappingThreshold = renderer()->theme().sliderTickSnappingThreshold();
    if (snappingThreshold > 0) {
        Decimal closest = input->findClosestTickMarkValue(value);
        if (closest.isFinite()) {
            double closestFraction = stepRange.proportionFromValue(closest).toDouble();
            double closestRatio = isVertical || !isLeftToRightDirection ? 1.0 - closestFraction : closestFraction;
            LayoutUnit closestPosition = trackLength * closestRatio;
            if ((closestPosition - position).abs() <= snappingThreshold)
                value = closest;
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
    if (Frame* frame = document().frame()) {
        frame->eventHandler().setCapturingMouseEventsElement(this);
        m_inDragMode = true;
    }
}

void SliderThumbElement::stopDragging()
{
    if (!m_inDragMode)
        return;

    if (Frame* frame = document().frame())
        frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    m_inDragMode = false;
    if (renderer())
        renderer()->setNeedsLayout();

    RefPtr<HTMLInputElement> input = hostInput();
    if (input)
        input->dispatchFormControlChangeEvent();
}

#if !PLATFORM(IOS)
void SliderThumbElement::defaultEventHandler(Event* event)
{
    if (!event->isMouseEvent()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // FIXME: Should handle this readonly/disabled check in more general way.
    // Missing this kind of check is likely to occur elsewhere if adding it in each shadow element.
    HTMLInputElement* input = hostInput();
    if (!input || input->isDisabledOrReadOnly()) {
        stopDragging();
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent* mouseEvent = toMouseEvent(event);
    bool isLeftButton = mouseEvent->button() == LeftButton;
    const AtomicString& eventType = event->type();

    // We intentionally do not call event->setDefaultHandled() here because
    // MediaControlTimelineElement::defaultEventHandler() wants to handle these
    // mouse events.
    if (eventType == eventNames().mousedownEvent && isLeftButton) {
        startDragging();
        return;
    } else if (eventType == eventNames().mouseupEvent && isLeftButton) {
        stopDragging();
        return;
    } else if (eventType == eventNames().mousemoveEvent) {
        if (m_inDragMode)
            setPositionFromPoint(mouseEvent->absoluteLocation());
        return;
    }

    HTMLDivElement::defaultEventHandler(event);
}
#endif

#if !PLATFORM(IOS)
bool SliderThumbElement::willRespondToMouseMoveEvents()
{
    const HTMLInputElement* input = hostInput();
    if (input && !input->isDisabledOrReadOnly() && m_inDragMode)
        return true;

    return HTMLDivElement::willRespondToMouseMoveEvents();
}

bool SliderThumbElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = hostInput();
    if (input && !input->isDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}
#endif // !PLATFORM(IOS)

void SliderThumbElement::willDetachRenderers()
{
    if (m_inDragMode) {
        if (Frame* frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    }
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)
    unregisterForTouchEvents();
#endif
}

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)
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

static Touch* findTouchWithIdentifier(TouchList* list, unsigned identifier)
{
    unsigned length = list->length();
    for (unsigned i = 0; i < length; ++i) {
        Touch* touch = list->item(i);
        if (touch->identifier() == identifier)
            return touch;
    }
    return nullptr;
}

void SliderThumbElement::handleTouchStart(TouchEvent* touchEvent)
{
    TouchList* targetTouches = touchEvent->targetTouches();
    if (targetTouches->length() != 1)
        return;

    // Ignore the touch if it is not really inside the thumb.
    Touch* touch = targetTouches->item(0);
    IntRect boundingBox = renderer()->absoluteBoundingBoxRect();
    if (!boundingBox.contains(touch->pageX(), touch->pageY()))
        return;

    setExclusiveTouchIdentifier(touch->identifier());

    startDragging();
    touchEvent->setDefaultHandled();
}

void SliderThumbElement::handleTouchMove(TouchEvent* touchEvent)
{
    unsigned identifier = exclusiveTouchIdentifier();
    if (identifier == NoIdentifier)
        return;

    Touch* touch = findTouchWithIdentifier(touchEvent->targetTouches(), identifier);
    if (!touch)
        return;

    if (m_inDragMode)
        setPositionFromPoint(IntPoint(touch->pageX(), touch->pageY()));
    touchEvent->setDefaultHandled();
}

void SliderThumbElement::handleTouchEndAndCancel(TouchEvent* touchEvent)
{
    unsigned identifier = exclusiveTouchIdentifier();
    if (identifier == NoIdentifier)
        return;

    // If our exclusive touch still exists, it was not the touch
    // that ended, so we should not stop dragging.
    Touch* exclusiveTouch = findTouchWithIdentifier(touchEvent->targetTouches(), identifier);
    if (exclusiveTouch)
        return;

    clearExclusiveTouchIdentifier();

    stopDragging();
}

void SliderThumbElement::didAttachRenderers()
{
    if (shouldAcceptTouchEvents())
        registerForTouchEvents();
}

void SliderThumbElement::handleTouchEvent(TouchEvent* touchEvent)
{
    HTMLInputElement* input = hostInput();
    ASSERT(input);
    if (input->isReadOnly() || input->isDisabledFormControl()) {
        clearExclusiveTouchIdentifier();
        stopDragging();
        touchEvent->setDefaultHandled();
        HTMLDivElement::defaultEventHandler(touchEvent);
        return;
    }

    const AtomicString& eventType = touchEvent->type();
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

    document().addTouchEventListener(this);
    m_isRegisteredAsTouchEventListener = true;
}

void SliderThumbElement::unregisterForTouchEvents()
{
    if (!m_isRegisteredAsTouchEventListener)
        return;

    clearExclusiveTouchIdentifier();
    stopDragging();

    document().removeTouchEventListener(this);
    m_isRegisteredAsTouchEventListener = false;
}

void SliderThumbElement::disabledAttributeChanged()
{
    if (shouldAcceptTouchEvents())
        registerForTouchEvents();
    else
        unregisterForTouchEvents();
}
#endif // ENABLE(TOUCH_EVENTS) && PLATFORM(IOS)

HTMLInputElement* SliderThumbElement::hostInput() const
{
    // Only HTMLInputElement creates SliderThumbElement instances as its shadow nodes.
    // So, shadowHost() must be an HTMLInputElement.
    Element* host = shadowHost();
    return host ? host->toInputElement() : 0;
}

static const AtomicString& sliderThumbShadowPseudoId()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, sliderThumb, ("-webkit-slider-thumb", AtomicString::ConstructFromLiteral));
    return sliderThumb;
}

static const AtomicString& mediaSliderThumbShadowPseudoId()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, mediaSliderThumb, ("-webkit-media-slider-thumb", AtomicString::ConstructFromLiteral));
    return mediaSliderThumb;
}

const AtomicString& SliderThumbElement::shadowPseudoId() const
{
    HTMLInputElement* input = hostInput();
    if (!input)
        return sliderThumbShadowPseudoId();
    if (!input->renderer())
        return emptyAtom;

    const RenderStyle& sliderStyle = input->renderer()->style();
    switch (sliderStyle.appearance()) {
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaFullScreenVolumeSliderPart:
    case MediaFullScreenVolumeSliderThumbPart:
        return mediaSliderThumbShadowPseudoId();
    default:
        return sliderThumbShadowPseudoId();
    }
}

PassRefPtr<Element> SliderThumbElement::cloneElementWithoutAttributesAndChildren()
{
    return create(document());
}

// --------------------------------

inline SliderContainerElement::SliderContainerElement(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document)
{
}

PassRefPtr<SliderContainerElement> SliderContainerElement::create(Document& document)
{
    return adoptRef(new SliderContainerElement(document));
}

RenderPtr<RenderElement> SliderContainerElement::createElementRenderer(PassRef<RenderStyle> style)
{
    return createRenderer<RenderSliderContainer>(*this, WTF::move(style));
}

const AtomicString& SliderContainerElement::shadowPseudoId() const
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, mediaSliderContainer, ("-webkit-media-slider-container", AtomicString::ConstructFromLiteral));
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, sliderContainer, ("-webkit-slider-container", AtomicString::ConstructFromLiteral));

    HTMLInputElement* input = shadowHost()->toInputElement();
    if (!input)
        return sliderContainer;
    if (!input->renderer())
        return emptyAtom;

    const RenderStyle& sliderStyle = input->renderer()->style();
    switch (sliderStyle.appearance()) {
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaVolumeSliderPart:
    case MediaVolumeSliderThumbPart:
    case MediaFullScreenVolumeSliderPart:
    case MediaFullScreenVolumeSliderThumbPart:
        return mediaSliderContainer;
    default:
        return sliderContainer;
    }
}

}
