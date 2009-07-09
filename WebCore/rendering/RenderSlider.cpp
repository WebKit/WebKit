/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderSlider.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include <wtf/MathExtras.h>

using std::min;

namespace WebCore {

using namespace HTMLNames;

static const int defaultTrackLength = 129;

// FIXME: The SliderRange class and functions are entirely based on the DOM,
// and could be put with HTMLInputElement (possibly with a new name) instead of here.
struct SliderRange {
    bool isIntegral;
    double minimum;
    double maximum;

    explicit SliderRange(HTMLInputElement*);
    double clampValue(double value);

    // Map value into 0-1 range
    double proportionFromValue(double value)
    {
        if (minimum == maximum)
            return 0;

        return (value - minimum) / (maximum - minimum);
    }
    
    // Map from 0-1 range to value
    double valueFromProportion(double proportion)
    {
        return minimum + proportion * (maximum - minimum);
    }
    
    double valueFromElement(HTMLInputElement*, bool* wasClamped = 0);
};

SliderRange::SliderRange(HTMLInputElement* element)
{
    // FIXME: What's the right way to handle an integral range with non-integral minimum and maximum?
    // Currently values are guaranteed to be integral but could be outside the range in that case.

    isIntegral = !equalIgnoringCase(element->getAttribute(precisionAttr), "float");

    // FIXME: This treats maximum strings that can't be parsed as 0, but perhaps 100 would be more appropriate.
    const AtomicString& maxString = element->getAttribute(maxAttr);
    maximum = maxString.isNull() ? 100.0 : maxString.toDouble();

    // If the maximum is smaller, use it as the minimum.
    minimum = min(element->getAttribute(minAttr).toDouble(), maximum);
}

double SliderRange::clampValue(double value)
{
    double clampedValue = max(minimum, min(value, maximum));
    return isIntegral ? round(clampedValue) : clampedValue;
}

double SliderRange::valueFromElement(HTMLInputElement* element, bool* wasClamped)
{
    String valueString = element->value();
    double oldValue = valueString.isNull() ? (minimum + maximum) / 2 : valueString.toDouble();
    double newValue = clampValue(oldValue);

    if (wasClamped)
        *wasClamped = valueString.isNull() || newValue != oldValue;

    return newValue;
}

// Returns a value between 0 and 1.
// As with SliderRange, this could be on HTMLInputElement instead of here.
static double sliderPosition(HTMLInputElement* element)
{
    SliderRange range(element);
    return range.proportionFromValue(range.valueFromElement(element));
}

class SliderThumbElement : public HTMLDivElement {
public:
    SliderThumbElement(Document*, Node* shadowParent);
    
    bool inDragMode() const { return m_inDragMode; }

    virtual void defaultEventHandler(Event*);
    virtual void detach();

private:        
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_shadowParent; }

    FloatPoint m_offsetToThumb;
    Node* m_shadowParent;
    bool m_inDragMode;
};

SliderThumbElement::SliderThumbElement(Document* document, Node* shadowParent)
    : HTMLDivElement(divTag, document)
    , m_shadowParent(shadowParent)
    , m_inDragMode(false)
{
}

void SliderThumbElement::defaultEventHandler(Event* event)
{
    if (!event->isMouseEvent()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    bool isLeftButton = mouseEvent->button() == LeftButton;
    const AtomicString& eventType = event->type();

    if (eventType == eventNames().mousedownEvent && isLeftButton) {
        if (document()->frame() && renderer()) {
            RenderSlider* slider = static_cast<RenderSlider*>(renderer()->parent());
            if (slider) {
                if (slider->mouseEventIsInThumb(mouseEvent)) {
                    // We selected the thumb, we want the cursor to always stay at
                    // the same position relative to the thumb.
                    m_offsetToThumb = slider->mouseEventOffsetToThumb(mouseEvent);
                } else {
                    // We are outside the thumb, move the thumb to the point were
                    // we clicked. We'll be exactly at the center of the thumb.
                    m_offsetToThumb.setX(0);
                    m_offsetToThumb.setY(0);
                }

                m_inDragMode = true;
                document()->frame()->eventHandler()->setCapturingMouseEventsNode(m_shadowParent);
                event->setDefaultHandled();
                return;
            }
        }
    } else if (eventType == eventNames().mouseupEvent && isLeftButton) {
        if (m_inDragMode) {
            if (Frame* frame = document()->frame())
                frame->eventHandler()->setCapturingMouseEventsNode(0);      
            m_inDragMode = false;
            event->setDefaultHandled();
            return;
        }
    } else if (eventType == eventNames().mousemoveEvent) {
        if (m_inDragMode && renderer() && renderer()->parent()) {
            RenderSlider* slider = static_cast<RenderSlider*>(renderer()->parent());
            if (slider) {
                FloatPoint curPoint = slider->absoluteToLocal(mouseEvent->absoluteLocation(), false, true);
                IntPoint eventOffset(curPoint.x() + m_offsetToThumb.x(), curPoint.y() + m_offsetToThumb.y());
                slider->setValueForPosition(slider->positionForOffset(eventOffset));
                event->setDefaultHandled();
                return;
            }
        }
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

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderBlock(element)
{
}

RenderSlider::~RenderSlider()
{
    if (m_thumb)
        m_thumb->detach();
}

int RenderSlider::baselinePosition(bool, bool) const
{
    return height() + marginTop();
}

void RenderSlider::calcPrefWidths()
{
    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else
        m_maxPrefWidth = defaultTrackLength * style()->effectiveZoom();

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false); 
}

void RenderSlider::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_thumb)
        m_thumb->renderer()->setStyle(createThumbStyle(style()));

    setReplaced(isInline());
}

PassRefPtr<RenderStyle> RenderSlider::createThumbStyle(const RenderStyle* parentStyle)
{
    RefPtr<RenderStyle> style;
    RenderStyle* pseudoStyle = getCachedPseudoStyle(SLIDER_THUMB);
    if (pseudoStyle)
        // We may be sharing style with another slider, but we must not share the thumb style.
        style = RenderStyle::clone(pseudoStyle);
    else
        style = RenderStyle::create();

    if (parentStyle)
        style->inheritFrom(parentStyle);

    style->setDisplay(BLOCK);

    if (parentStyle->appearance() == SliderVerticalPart)
        style->setAppearance(SliderThumbVerticalPart);
    else if (parentStyle->appearance() == SliderHorizontalPart)
        style->setAppearance(SliderThumbHorizontalPart);
    else if (parentStyle->appearance() == MediaSliderPart)
        style->setAppearance(MediaSliderThumbPart);

    return style.release();
}

void RenderSlider::layout()
{
    ASSERT(needsLayout());

    RenderBox* thumb = m_thumb ? toRenderBox(m_thumb->renderer()) : 0;

    IntSize baseSize(borderLeft() + paddingLeft() + paddingRight() + borderRight(),
        borderTop() + paddingTop() + paddingBottom() + borderBottom());

    if (thumb) {
        // Allow the theme to set the size of the thumb.
        if (thumb->style()->hasAppearance()) {
            // FIXME: This should pass the style, not the renderer, to the theme.
            theme()->adjustSliderThumbSize(thumb);
        }

        baseSize.expand(thumb->style()->width().calcMinValue(0), thumb->style()->height().calcMinValue(0));
    }

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    IntSize oldSize = size();

    setSize(baseSize);
    calcWidth();
    calcHeight();

    IntRect overflowRect(IntPoint(), size());

    if (thumb) {
        if (oldSize != size())
            thumb->setChildNeedsLayout(true, false);

        LayoutStateMaintainer statePusher(view(), this, size());

        IntRect oldThumbRect = thumb->frameRect();

        thumb->layoutIfNeeded();

        IntRect thumbRect;

        thumbRect.setWidth(thumb->style()->width().calcMinValue(contentWidth()));
        thumbRect.setHeight(thumb->style()->height().calcMinValue(contentHeight()));

        double fraction = sliderPosition(static_cast<HTMLInputElement*>(node()));
        IntRect contentRect = contentBoxRect();
        if (style()->appearance() == SliderVerticalPart) {
            thumbRect.setX(contentRect.x() + (contentRect.width() - thumbRect.width()) / 2);
            thumbRect.setY(contentRect.y() + static_cast<int>(nextafter((contentRect.height() - thumbRect.height()) + 1, 0) * (1 - fraction)));
        } else {
            thumbRect.setX(contentRect.x() + static_cast<int>(nextafter((contentRect.width() - thumbRect.width()) + 1, 0) * fraction));
            thumbRect.setY(contentRect.y() + (contentRect.height() - thumbRect.height()) / 2);
        }

        thumb->setFrameRect(thumbRect);

        if (thumb->checkForRepaintDuringLayout())
            thumb->repaintDuringLayoutIfMoved(oldThumbRect);

        statePusher.pop();

        IntRect thumbOverflowRect = thumb->overflowRect();
        thumbOverflowRect.move(thumb->x(), thumb->y());
        overflowRect.unite(thumbOverflowRect);
    }

    // FIXME: m_overflowWidth and m_overflowHeight should be renamed
    // m_overflowRight and m_overflowBottom.
    m_overflowLeft = overflowRect.x();
    m_overflowTop = overflowRect.y();
    m_overflowWidth = overflowRect.right();
    m_overflowHeight = overflowRect.bottom();

    repainter.repaintAfterLayout();    

    setNeedsLayout(false);
}

void RenderSlider::updateFromElement()
{
    HTMLInputElement* element = static_cast<HTMLInputElement*>(node());

    // Send the value back to the element if the range changes it.
    SliderRange range(element);
    bool clamped;
    double value = range.valueFromElement(element, &clamped);
    if (clamped)
        element->setValueFromRenderer(String::number(value));

    // Layout will take care of the thumb's size and position.
    if (!m_thumb) {
        m_thumb = new SliderThumbElement(document(), node());
        RefPtr<RenderStyle> thumbStyle = createThumbStyle(style());
        m_thumb->setRenderer(m_thumb->createRenderer(renderArena(), thumbStyle.get()));
        m_thumb->renderer()->setStyle(thumbStyle.release());
        m_thumb->setAttached();
        m_thumb->setInDocument(true);
        addChild(m_thumb->renderer());
    }
    setNeedsLayout(true);
}

bool RenderSlider::mouseEventIsInThumb(MouseEvent* evt)
{
    if (!m_thumb || !m_thumb->renderer())
        return false;

#if ENABLE(VIDEO)
    if (style()->appearance() == MediaSliderPart) {
        MediaControlInputElement *sliderThumb = static_cast<MediaControlInputElement*>(m_thumb->renderer()->node());
        return sliderThumb->hitTest(evt->absoluteLocation());
    }
#endif

    FloatPoint localPoint = m_thumb->renderBox()->absoluteToLocal(evt->absoluteLocation(), false, true);
    IntRect thumbBounds = m_thumb->renderBox()->borderBoxRect();
    return thumbBounds.contains(roundedIntPoint(localPoint));
}

FloatPoint RenderSlider::mouseEventOffsetToThumb(MouseEvent* evt)
{
    ASSERT (m_thumb && m_thumb->renderer());
    FloatPoint localPoint = m_thumb->renderBox()->absoluteToLocal(evt->absoluteLocation(), false, true);
    IntRect thumbBounds = m_thumb->renderBox()->borderBoxRect();
    FloatPoint offset;
    offset.setX(thumbBounds.x() + thumbBounds.width() / 2 - localPoint.x());
    offset.setY(thumbBounds.y() + thumbBounds.height() / 2 - localPoint.y());
    return offset;
}

void RenderSlider::setValueForPosition(int position)
{
    if (!m_thumb || !m_thumb->renderer())
        return;

    HTMLInputElement* element = static_cast<HTMLInputElement*>(node());

    // Calculate the new value based on the position, and send it to the element.
    SliderRange range(element);
    double fraction = static_cast<double>(position) / trackSize();
    if (style()->appearance() == SliderVerticalPart)
        fraction = 1 - fraction;
    double value = range.clampValue(range.valueFromProportion(fraction));
    element->setValueFromRenderer(String::number(value));

    // Also update the position if appropriate.
    if (position != currentPosition()) {
        setNeedsLayout(true);

        // FIXME: It seems like this could send extra change events if the same value is set
        // multiple times with no layout in between.
        element->dispatchFormControlChangeEvent();
    }
}

int RenderSlider::positionForOffset(const IntPoint& p)
{
    if (!m_thumb || !m_thumb->renderer())
        return 0;

    int position;
    if (style()->appearance() == SliderVerticalPart)
        position = p.y() - m_thumb->renderBox()->height() / 2;
    else
        position = p.x() - m_thumb->renderBox()->width() / 2;
    
    return max(0, min(position, trackSize()));
}

int RenderSlider::currentPosition()
{
    ASSERT(m_thumb);
    ASSERT(m_thumb->renderer());

    if (style()->appearance() == SliderVerticalPart)
        return toRenderBox(m_thumb->renderer())->y() - contentBoxRect().y();
    return toRenderBox(m_thumb->renderer())->x() - contentBoxRect().x();
}

int RenderSlider::trackSize()
{
    ASSERT(m_thumb);
    ASSERT(m_thumb->renderer());

    if (style()->appearance() == SliderVerticalPart)
        return contentHeight() - m_thumb->renderBox()->height();
    return contentWidth() - m_thumb->renderBox()->width();
}

void RenderSlider::forwardEvent(Event* event)
{
    if (event->isMouseEvent()) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        if (event->type() == eventNames().mousedownEvent && mouseEvent->button() == LeftButton) {
            if (!mouseEventIsInThumb(mouseEvent)) {
                IntPoint eventOffset = roundedIntPoint(absoluteToLocal(mouseEvent->absoluteLocation(), false, true));
                setValueForPosition(positionForOffset(eventOffset));
            }
        }
    }

    m_thumb->defaultEventHandler(event);
}

bool RenderSlider::inDragMode() const
{
    return m_thumb && m_thumb->inDragMode();
}

} // namespace WebCore
