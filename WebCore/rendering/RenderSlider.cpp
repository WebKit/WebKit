/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ShadowElement.h"
#include "StepRange.h"
#include <wtf/MathExtras.h>

using std::min;

namespace WebCore {

static const int defaultTrackLength = 129;

// Returns a value between 0 and 1.
static double sliderPosition(HTMLInputElement* element)
{
    StepRange range(element);
    return range.proportionFromValue(range.valueFromElement(element));
}

class SliderThumbElement : public ShadowBlockElement {
public:
    static PassRefPtr<SliderThumbElement> create(HTMLElement* shadowParent);

    bool inDragMode() const { return m_inDragMode; }

    virtual void defaultEventHandler(Event*);
    virtual void detach();

private:        
    SliderThumbElement(HTMLElement* shadowParent);

    FloatPoint m_offsetToThumb;
    bool m_inDragMode;
};

inline SliderThumbElement::SliderThumbElement(HTMLElement* shadowParent)
    : ShadowBlockElement(shadowParent)
    , m_inDragMode(false)
{
}

inline PassRefPtr<SliderThumbElement> SliderThumbElement::create(HTMLElement* shadowParent)
{
    return adoptRef(new SliderThumbElement(shadowParent));
}

void SliderThumbElement::defaultEventHandler(Event* event)
{
    if (!event->isMouseEvent()) {
        ShadowBlockElement::defaultEventHandler(event);
        return;
    }

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    bool isLeftButton = mouseEvent->button() == LeftButton;
    const AtomicString& eventType = event->type();

    if (eventType == eventNames().mousedownEvent && isLeftButton) {
        if (document()->frame() && renderer()) {
            RenderSlider* slider = toRenderSlider(renderer()->parent());
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
                document()->frame()->eventHandler()->setCapturingMouseEventsNode(shadowParent());
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
            RenderSlider* slider = toRenderSlider(renderer()->parent());
            if (slider) {
                FloatPoint curPoint = slider->absoluteToLocal(mouseEvent->absoluteLocation(), false, true);
                IntPoint eventOffset(curPoint.x() + m_offsetToThumb.x(), curPoint.y() + m_offsetToThumb.y());
                slider->setValueForPosition(slider->positionForOffset(eventOffset));
                event->setDefaultHandled();
                return;
            }
        }
    }

    ShadowBlockElement::defaultEventHandler(event);
}

void SliderThumbElement::detach()
{
    if (m_inDragMode) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);      
    }
    ShadowBlockElement::detach();
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

    int toAdd = borderAndPaddingWidth();
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
    else if (parentStyle->appearance() == MediaVolumeSliderPart)
        style->setAppearance(MediaVolumeSliderThumbPart);

    return style.release();
}

IntRect RenderSlider::thumbRect()
{
    if (!m_thumb)
        return IntRect();

    IntRect thumbRect;
    RenderBox* thumb = toRenderBox(m_thumb->renderer());

    thumbRect.setWidth(thumb->style()->width().calcMinValue(contentWidth()));
    thumbRect.setHeight(thumb->style()->height().calcMinValue(contentHeight()));

    double fraction = sliderPosition(static_cast<HTMLInputElement*>(node()));
    IntRect contentRect = contentBoxRect();
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart) {
        thumbRect.setX(contentRect.x() + (contentRect.width() - thumbRect.width()) / 2);
        thumbRect.setY(contentRect.y() + static_cast<int>(nextafter((contentRect.height() - thumbRect.height()) + 1, 0) * (1 - fraction)));
    } else {
        thumbRect.setX(contentRect.x() + static_cast<int>(nextafter((contentRect.width() - thumbRect.width()) + 1, 0) * fraction));
        thumbRect.setY(contentRect.y() + (contentRect.height() - thumbRect.height()) / 2);
    }

    return thumbRect;
}

void RenderSlider::layout()
{
    ASSERT(needsLayout());

    RenderBox* thumb = m_thumb ? toRenderBox(m_thumb->renderer()) : 0;

    IntSize baseSize(borderAndPaddingWidth(), borderAndPaddingHeight());

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
    computeLogicalWidth();
    computeLogicalHeight();

    if (thumb) {
        if (oldSize != size())
            thumb->setChildNeedsLayout(true, false);

        LayoutStateMaintainer statePusher(view(), this, size());

        IntRect oldThumbRect = thumb->frameRect();

        thumb->layoutIfNeeded();

        IntRect rect = thumbRect();
        thumb->setFrameRect(rect);
        if (thumb->checkForRepaintDuringLayout())
            thumb->repaintDuringLayoutIfMoved(oldThumbRect);

        statePusher.pop();
        addOverflowFromChild(thumb);
    }

    repainter.repaintAfterLayout();    

    setNeedsLayout(false);
}

void RenderSlider::updateFromElement()
{
    // Layout will take care of the thumb's size and position.
    if (!m_thumb) {
        m_thumb = SliderThumbElement::create(static_cast<HTMLElement*>(node()));
        RefPtr<RenderStyle> thumbStyle = createThumbStyle(style());
        m_thumb->setRenderer(m_thumb->createRenderer(renderArena(), thumbStyle.get()));
        m_thumb->renderer()->setStyle(thumbStyle.release());
        m_thumb->setAttached();
        m_thumb->setInDocument();
        addChild(m_thumb->renderer());
    }
    setNeedsLayout(true);
}

bool RenderSlider::mouseEventIsInThumb(MouseEvent* evt)
{
    if (!m_thumb || !m_thumb->renderer())
        return false;

#if ENABLE(VIDEO)
    if (style()->appearance() == MediaSliderPart || style()->appearance() == MediaVolumeSliderPart) {
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
    ASSERT(m_thumb && m_thumb->renderer());
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
    StepRange range(element);
    double fraction = static_cast<double>(position) / trackSize();
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        fraction = 1 - fraction;
    double value = range.clampValue(range.valueFromProportion(fraction));
    element->setValueFromRenderer(serializeForNumberType(value));

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
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        position = p.y() - m_thumb->renderBox()->height() / 2;
    else
        position = p.x() - m_thumb->renderBox()->width() / 2;
    
    return max(0, min(position, trackSize()));
}

int RenderSlider::currentPosition()
{
    ASSERT(m_thumb);
    ASSERT(m_thumb->renderer());

    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        return toRenderBox(m_thumb->renderer())->y() - contentBoxRect().y();
    return toRenderBox(m_thumb->renderer())->x() - contentBoxRect().x();
}

int RenderSlider::trackSize()
{
    ASSERT(m_thumb);
    ASSERT(m_thumb->renderer());

    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
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
