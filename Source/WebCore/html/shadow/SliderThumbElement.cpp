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
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "MouseEvent.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderSlider.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "StepRange.h"
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

inline static double sliderPosition(HTMLInputElement* element)
{
    StepRange range(element);
    return range.proportionFromValue(range.valueFromElement(element));
}

inline static bool hasVerticalAppearance(HTMLInputElement* input)
{
    ASSERT(input->renderer());
    RenderStyle* sliderStyle = input->renderer()->style();
    return sliderStyle->appearance() == SliderVerticalPart || sliderStyle->appearance() == MediaVolumeSliderPart;
}

SliderThumbElement* sliderThumbElementOf(Node* node)
{
    ASSERT(node);
    ShadowRoot* shadow = node->toInputElement()->shadowRoot();
    ASSERT(shadow);
    Node* thumb = shadow->firstChild()->firstChild()->firstChild();
    ASSERT(thumb);
    return toSliderThumbElement(thumb);
}

// --------------------------------

RenderSliderThumb::RenderSliderThumb(Node* node)
    : RenderBlock(node)
{
}

void RenderSliderThumb::updateAppearance(RenderStyle* parentStyle)
{
    if (parentStyle->appearance() == SliderVerticalPart)
        style()->setAppearance(SliderThumbVerticalPart);
    else if (parentStyle->appearance() == SliderHorizontalPart)
        style()->setAppearance(SliderThumbHorizontalPart);
    else if (parentStyle->appearance() == MediaSliderPart)
        style()->setAppearance(MediaSliderThumbPart);
    else if (parentStyle->appearance() == MediaVolumeSliderPart)
        style()->setAppearance(MediaVolumeSliderThumbPart);
    if (style()->hasAppearance())
        theme()->adjustSliderThumbSize(style());
}

bool RenderSliderThumb::isSliderThumb() const
{
    return true;
}

void RenderSliderThumb::layout()
{
    // Do not cast node() to SliderThumbElement. This renderer is used for
    // TrackLimitElement too.
    HTMLInputElement* input = node()->shadowAncestorNode()->toInputElement();
    bool isVertical = style()->appearance() == SliderThumbVerticalPart || style()->appearance() == MediaVolumeSliderThumbPart;

    double fraction = sliderPosition(input) * 100;
    if (isVertical)
        style()->setTop(Length(100 - fraction, Percent));
    else if (style()->isLeftToRightDirection())
        style()->setLeft(Length(fraction, Percent));
    else
        style()->setRight(Length(fraction, Percent));

    RenderBlock::layout();
}

// --------------------------------

// FIXME: Find a way to cascade appearance and adjust heights, and get rid of this class.
// http://webkit.org/b/62535
class RenderSliderContainer : public RenderDeprecatedFlexibleBox {
public:
    RenderSliderContainer(Node* node)
        : RenderDeprecatedFlexibleBox(node) { }

private:
    virtual void layout();
};

void RenderSliderContainer::layout()
{
    HTMLInputElement* input = node()->shadowAncestorNode()->toInputElement();
    bool isVertical = hasVerticalAppearance(input);
    style()->setBoxOrient(isVertical ? VERTICAL : HORIZONTAL);
    // Sets the concrete height if the height of the <input> is not fixed or a
    // percentage value because a percentage height value of this box won't be
    // based on the <input> height in such case.
    Length inputHeight = input->renderer()->style()->height();
    RenderObject* trackRenderer = node()->firstChild()->renderer();
    if (!isVertical && input->renderer()->isSlider() && !inputHeight.isFixed() && !inputHeight.isPercent()) {
        RenderObject* thumbRenderer = input->shadowRoot()->firstChild()->firstChild()->firstChild()->renderer();
        if (thumbRenderer) {
            style()->setHeight(thumbRenderer->style()->height());
            if (trackRenderer)
                trackRenderer->style()->setHeight(thumbRenderer->style()->height());
        }
    } else {
        style()->setHeight(Length(100, Percent));
        if (trackRenderer)
            trackRenderer->style()->setHeight(Length());
    }

    RenderDeprecatedFlexibleBox::layout();

    // Percentage 'top' for the thumb doesn't work if the parent style has no
    // concrete height.
    Node* track = node()->firstChild();
    if (track && track->renderer()->isBox()) {
        RenderBox* trackBox = track->renderBox();
        trackBox->style()->setHeight(Length(trackBox->height() - trackBox->borderAndPaddingHeight(), Fixed));
    }
}

// --------------------------------

void SliderThumbElement::setPositionFromValue()
{
    // Since the code to calculate position is in the RenderSliderThumb layout
    // path, we don't actually update the value here. Instead, we poke at the
    // renderer directly to trigger layout.
    if (renderer())
        renderer()->setNeedsLayout(true);
}

RenderObject* SliderThumbElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSliderThumb(this);
}

bool SliderThumbElement::isEnabledFormControl() const
{
    return hostInput()->isEnabledFormControl();
}

bool SliderThumbElement::isReadOnlyFormControl() const
{
    return hostInput()->isReadOnlyFormControl();
}

Node* SliderThumbElement::focusDelegate()
{
    return hostInput();
}

void SliderThumbElement::dragFrom(const LayoutPoint& point)
{
    setPositionFromPoint(point);
    startDragging();
}

void SliderThumbElement::setPositionFromPoint(const LayoutPoint& point)
{
    HTMLInputElement* input = hostInput();

    if (!input->renderer() || !renderer())
        return;

    LayoutPoint offset = roundedLayoutPoint(input->renderer()->absoluteToLocal(point, false, true));
    bool isVertical = hasVerticalAppearance(input);
    LayoutUnit trackSize;
    LayoutUnit position;
    LayoutUnit currentPosition;
    // We need to calculate currentPosition from absolute points becaue the
    // renderer for this node is usually on a layer and renderBox()->x() and
    // y() are unusable.
    // FIXME: This should probably respect transforms.
    LayoutPoint absoluteThumbOrigin = renderBox()->absoluteBoundingBoxRectIgnoringTransforms().location();
    LayoutPoint absoluteSliderContentOrigin = roundedLayoutPoint(input->renderer()->localToAbsolute());
    if (isVertical) {
        trackSize = input->renderBox()->contentHeight() - renderBox()->height();
        position = offset.y() - renderBox()->height() / 2;
        currentPosition = absoluteThumbOrigin.y() - absoluteSliderContentOrigin.y();
    } else {
        trackSize = input->renderBox()->contentWidth() - renderBox()->width();
        position = offset.x() - renderBox()->width() / 2;
        currentPosition = absoluteThumbOrigin.x() - absoluteSliderContentOrigin.x();
    }
    position = max<LayoutUnit>(0, min(position, trackSize));
    if (position == currentPosition)
        return;

    StepRange range(input);
    double fraction = static_cast<double>(position) / trackSize;
    if (isVertical || !renderBox()->style()->isLeftToRightDirection())
        fraction = 1 - fraction;
    double value = range.clampValue(range.valueFromProportion(fraction));

    // FIXME: This is no longer being set from renderer. Consider updating the method name.
    input->setValueFromRenderer(serializeForNumberType(value));
    renderer()->setNeedsLayout(true);
    input->dispatchFormControlChangeEvent();
}

void SliderThumbElement::startDragging()
{
    if (Frame* frame = document()->frame()) {
        frame->eventHandler()->setCapturingMouseEventsNode(this);
        m_inDragMode = true;
    }
}

void SliderThumbElement::stopDragging()
{
    if (!m_inDragMode)
        return;

    if (Frame* frame = document()->frame())
        frame->eventHandler()->setCapturingMouseEventsNode(0);
    m_inDragMode = false;
    if (renderer())
        renderer()->setNeedsLayout(true);
}

void SliderThumbElement::defaultEventHandler(Event* event)
{
    if (!event->isMouseEvent()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // FIXME: Should handle this readonly/disabled check in more general way.
    // Missing this kind of check is likely to occur elsewhere if adding it in each shadow element.
    HTMLInputElement* input = hostInput();
    if (!input || input->isReadOnlyFormControl() || !input->isEnabledFormControl()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
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

void SliderThumbElement::detach()
{
    if (m_inDragMode) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
    }
    HTMLDivElement::detach();
}

HTMLInputElement* SliderThumbElement::hostInput() const
{
    // Only HTMLInputElement creates SliderThumbElement instances as its shadow nodes.
    // So, shadowAncestorNode() must be an HTMLInputElement.
    return shadowAncestorNode()->toInputElement();
}

const AtomicString& SliderThumbElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, sliderThumb, ("-webkit-slider-thumb"));
    return sliderThumb;
}

// --------------------------------

inline TrackLimiterElement::TrackLimiterElement(Document* document)
    : HTMLDivElement(HTMLNames::divTag, document)
{
}

PassRefPtr<TrackLimiterElement> TrackLimiterElement::create(Document* document)
{
    RefPtr<TrackLimiterElement> element = adoptRef(new TrackLimiterElement(document));

    CSSMutableStyleDeclaration* style = element->ensureInlineStyleDecl();
    style->setProperty(CSSPropertyVisibility, CSSValueHidden);
    style->setProperty(CSSPropertyPosition, CSSValueStatic);

    return element.release();
}

RenderObject* TrackLimiterElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSliderThumb(this);
}

const AtomicString& TrackLimiterElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, sliderThumb, ("-webkit-slider-thumb"));
    return sliderThumb;
}

TrackLimiterElement* trackLimiterElementOf(Node* node)
{
    ASSERT(node);
    ShadowRoot* shadow = node->toInputElement()->shadowRoot();
    ASSERT(shadow);
    Node* limiter = shadow->firstChild()->lastChild();
    ASSERT(limiter);
    return static_cast<TrackLimiterElement*>(limiter);
}

// --------------------------------

inline SliderContainerElement::SliderContainerElement(Document* document)
    : HTMLDivElement(HTMLNames::divTag, document)
{
}

PassRefPtr<SliderContainerElement> SliderContainerElement::create(Document* document)
{
    return adoptRef(new SliderContainerElement(document));
}

RenderObject* SliderContainerElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSliderContainer(this);
}

const AtomicString& SliderContainerElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, sliderThumb, ("-webkit-slider-container"));
    return sliderThumb;
}

}
