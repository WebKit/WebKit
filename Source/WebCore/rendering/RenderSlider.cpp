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
#include "CSSStyleSelector.h"
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
#include "SliderThumbElement.h"
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

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderBlock(element)
{
}

RenderSlider::~RenderSlider()
{
}

int RenderSlider::baselinePosition(FontBaseline, bool /*firstLine*/, LineDirectionMode, LinePositionMode) const
{
    // FIXME: Patch this function for writing-mode.
    return height() + marginTop();
}

void RenderSlider::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(style()->width().value());
    else
        m_maxPreferredLogicalWidth = defaultTrackLength * style()->effectiveZoom();

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPreferredLogicalWidth = 0;
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false); 
}

IntRect RenderSlider::thumbRect()
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    if (!thumbElement)
        return IntRect();

    IntRect thumbRect;
    RenderBox* thumb = toRenderBox(thumbElement->renderer());

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

    SliderThumbElement* thumbElement = sliderThumbElement();
    RenderBox* thumb = thumbElement ? toRenderBox(thumbElement->renderer()) : 0;

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
    updateLayerTransform();

    if (thumb) {
        if (oldSize != size())
            thumb->setChildNeedsLayout(true, false);

        LayoutStateMaintainer statePusher(view(), this, size(), style()->isFlippedBlocksWritingMode());

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

SliderThumbElement* RenderSlider::sliderThumbElement() const
{
    return toSliderThumbElement(static_cast<Element*>(node())->shadowRoot());
}

bool RenderSlider::mouseEventIsInThumb(MouseEvent* evt)
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    if (!thumbElement || !thumbElement->renderer())
        return false;

#if ENABLE(VIDEO)
    if (style()->appearance() == MediaSliderPart || style()->appearance() == MediaVolumeSliderPart) {
        MediaControlInputElement* sliderThumb = static_cast<MediaControlInputElement*>(thumbElement->renderer()->node());
        return sliderThumb->hitTest(evt->absoluteLocation());
    }
#endif

    FloatPoint localPoint = thumbElement->renderBox()->absoluteToLocal(evt->absoluteLocation(), false, true);
    IntRect thumbBounds = thumbElement->renderBox()->borderBoxRect();
    return thumbBounds.contains(roundedIntPoint(localPoint));
}

FloatPoint RenderSlider::mouseEventOffsetToThumb(MouseEvent* evt)
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    ASSERT(thumbElement && thumbElement->renderer());
    FloatPoint localPoint = thumbElement->renderBox()->absoluteToLocal(evt->absoluteLocation(), false, true);
    IntRect thumbBounds = thumbElement->renderBox()->borderBoxRect();
    FloatPoint offset;
    offset.setX(thumbBounds.x() + thumbBounds.width() / 2 - localPoint.x());
    offset.setY(thumbBounds.y() + thumbBounds.height() / 2 - localPoint.y());
    return offset;
}

void RenderSlider::setValueForPosition(int position)
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    if (!thumbElement || !thumbElement->renderer())
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
    SliderThumbElement* thumbElement = sliderThumbElement();
    if (!thumbElement || !thumbElement->renderer())
        return 0;

    int position;
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        position = p.y() - thumbElement->renderBox()->height() / 2;
    else
        position = p.x() - thumbElement->renderBox()->width() / 2;
    
    return max(0, min(position, trackSize()));
}

int RenderSlider::currentPosition()
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    ASSERT(thumbElement && thumbElement->renderer());

    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        return toRenderBox(thumbElement->renderer())->y() - contentBoxRect().y();
    return toRenderBox(thumbElement->renderer())->x() - contentBoxRect().x();
}

int RenderSlider::trackSize()
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    ASSERT(thumbElement && thumbElement->renderer());

    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        return contentHeight() - thumbElement->renderBox()->height();
    return contentWidth() - thumbElement->renderBox()->width();
}

void RenderSlider::forwardEvent(Event* event)
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    if (!thumbElement)
        return;

    if (event->isMouseEvent()) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        if (event->type() == eventNames().mousedownEvent && mouseEvent->button() == LeftButton) {
            if (!mouseEventIsInThumb(mouseEvent)) {
                IntPoint eventOffset = roundedIntPoint(absoluteToLocal(mouseEvent->absoluteLocation(), false, true));
                setValueForPosition(positionForOffset(eventOffset));
            }
        }
    }

    thumbElement->defaultEventHandler(event);
}

bool RenderSlider::inDragMode() const
{
    SliderThumbElement* thumbElement = sliderThumbElement();
    return thumbElement && thumbElement->inDragMode();
}

} // namespace WebCore
