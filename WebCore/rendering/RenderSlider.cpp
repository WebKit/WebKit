/**
 *
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc.
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
#include "RenderTheme.h"
#include <wtf/MathExtras.h>

using std::min;

namespace WebCore {

using namespace HTMLNames;

const int defaultTrackLength = 129;

class HTMLSliderThumbElement : public HTMLDivElement {
public:
    HTMLSliderThumbElement(Document*, Node* shadowParent = 0);
        
    virtual void defaultEventHandler(Event*);
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_shadowParent; }
    
    bool inDragMode() const { return m_inDragMode; }
private:
    Node* m_shadowParent;
    FloatPoint m_initialClickPoint;       // initial click point in RenderSlider-local coordinates
    int m_initialPosition;
    bool m_inDragMode;
};

HTMLSliderThumbElement::HTMLSliderThumbElement(Document* doc, Node* shadowParent)
    : HTMLDivElement(divTag, doc)
    , m_shadowParent(shadowParent)
    , m_initialClickPoint(IntPoint())
    , m_initialPosition(0)
    , m_inDragMode(false)
{
}

void HTMLSliderThumbElement::defaultEventHandler(Event* event)
{
    const AtomicString& eventType = event->type();
    if (eventType == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        RenderSlider* slider;
        if (document()->frame() && renderer() && renderer()->parent() &&
                (slider = static_cast<RenderSlider*>(renderer()->parent())) &&
                slider->mouseEventIsInThumb(mouseEvent)) {
            // Cache the initial point where the mouse down occurred, in slider coordinates
            m_initialClickPoint = slider->absoluteToLocal(FloatPoint(mouseEvent->pageX(), mouseEvent->pageY()), false, true);
            // Cache the initial position of the thumb.
            m_initialPosition = slider->currentPosition();
            m_inDragMode = true;
            
            document()->frame()->eventHandler()->setCapturingMouseEventsNode(m_shadowParent);
            
            event->setDefaultHandled();
            return;
        }
    } else if (eventType == eventNames().mouseupEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        if (m_inDragMode) {
            if (Frame* frame = document()->frame())
                frame->eventHandler()->setCapturingMouseEventsNode(0);      
            m_inDragMode = false;
            event->setDefaultHandled();
            return;
        }
    } else if (eventType == eventNames().mousemoveEvent && event->isMouseEvent()) {
        if (m_inDragMode && renderer() && renderer()->parent()) {
            // Move the slider
            MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
            RenderSlider* slider = static_cast<RenderSlider*>(renderer()->parent());
            FloatPoint curPoint = slider->absoluteToLocal(FloatPoint(mouseEvent->pageX(), mouseEvent->pageY()), false, true);
            int newPosition = slider->positionForOffset(
                IntPoint(m_initialPosition + curPoint.x() - m_initialClickPoint.x()
                        + (renderer()->width() / 2), 
                    m_initialPosition + curPoint.y() - m_initialClickPoint.y()
                        + (renderer()->height() / 2)));
            if (slider->currentPosition() != newPosition) {
                slider->setCurrentPosition(newPosition);
                slider->valueChanged();
            }
            event->setDefaultHandled();
            return;
        }
    }

    HTMLDivElement::defaultEventHandler(event);
}

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderBlock(element)
    , m_thumb(0)
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

void RenderSlider::styleDidChange(RenderStyle::Diff diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    
    if (m_thumb)
        m_thumb->renderer()->setStyle(createThumbStyle(style(), m_thumb->renderer()->style()));
        
    setReplaced(isInline());
}

PassRefPtr<RenderStyle> RenderSlider::createThumbStyle(const RenderStyle* parentStyle, const RenderStyle* oldStyle)
{
    RefPtr<RenderStyle> style;
    RenderStyle* pseudoStyle = getCachedPseudoStyle(RenderStyle::SLIDER_THUMB);
    if (pseudoStyle)
        // We may be sharing style with another slider, but we must not share the thumb style.
        style = RenderStyle::clone(pseudoStyle);
    else
        style = RenderStyle::create();

    if (parentStyle)
        style->inheritFrom(parentStyle);

    style->setDisplay(BLOCK);
    style->setPosition(RelativePosition);
    if (oldStyle) {
        style->setLeft(oldStyle->left());
        style->setTop(oldStyle->top());
    }

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
    bool relayoutChildren = false;
    
    if (m_thumb && m_thumb->renderer()) {
            
        int oldWidth = m_width;
        calcWidth();
        int oldHeight = m_height;
        calcHeight();
        
        if (oldWidth != m_width || oldHeight != m_height)
            relayoutChildren = true;  

        // Allow the theme to set the size of the thumb
        if (m_thumb->renderer()->style()->hasAppearance())
            theme()->adjustSliderThumbSize(m_thumb->renderer());

        if (style()->appearance() == SliderVerticalPart) {
            // FIXME: Handle percentage widths correctly. See http://bugs.webkit.org/show_bug.cgi?id=12104
            m_thumb->renderer()->style()->setLeft(Length(contentWidth() / 2 - m_thumb->renderer()->style()->width().value() / 2, Fixed));
        } else {
            // FIXME: Handle percentage heights correctly. See http://bugs.webkit.org/show_bug.cgi?id=12104
            m_thumb->renderer()->style()->setTop(Length(contentHeight() / 2 - m_thumb->renderer()->style()->height().value() / 2, Fixed));
        }

        if (relayoutChildren)
            setPositionFromValue(true);
    }

    RenderBlock::layoutBlock(relayoutChildren);
}

void RenderSlider::updateFromElement()
{
    if (!m_thumb) {
        m_thumb = new HTMLSliderThumbElement(document(), node());
        RefPtr<RenderStyle> thumbStyle = createThumbStyle(style());
        m_thumb->setRenderer(m_thumb->createRenderer(renderArena(), thumbStyle.get()));
        m_thumb->renderer()->setStyle(thumbStyle.release());
        m_thumb->setAttached();
        m_thumb->setInDocument(true);
        addChild(m_thumb->renderer());
    }
    setPositionFromValue();
    setNeedsLayout(true, false);
}

bool RenderSlider::mouseEventIsInThumb(MouseEvent* evt)
{
    if (!m_thumb || !m_thumb->renderer())
        return false;

#if ENABLE(VIDEO)
    if (style()->appearance() == MediaSliderPart) {
        MediaControlInputElement *sliderThumb = static_cast<MediaControlInputElement*>(m_thumb->renderer()->node());
        IntPoint absPoint(evt->pageX(), evt->pageY());
        return sliderThumb->hitTest(absPoint);
    } else 
#endif
    {
        FloatPoint localPoint = m_thumb->renderer()->absoluteToLocal(FloatPoint(evt->pageX(), evt->pageY()), false, true);
        IntRect thumbBounds = m_thumb->renderer()->borderBox();
        return thumbBounds.contains(roundedIntPoint(localPoint));
    }
}

void RenderSlider::setValueForPosition(int position)
{
    if (!m_thumb || !m_thumb->renderer())
        return;
    
    const AtomicString& minStr = static_cast<HTMLInputElement*>(node())->getAttribute(minAttr);
    const AtomicString& maxStr = static_cast<HTMLInputElement*>(node())->getAttribute(maxAttr);
    const AtomicString& precision = static_cast<HTMLInputElement*>(node())->getAttribute(precisionAttr);
    
    double minVal = minStr.isNull() ? 0.0 : minStr.toDouble();
    double maxVal = maxStr.isNull() ? 100.0 : maxStr.toDouble();
    minVal = min(minVal, maxVal); // Make sure the range is sane.
    
    // Calculate the new value based on the position
    double factor = (double)position / (double)trackSize();
    if (style()->appearance() == SliderVerticalPart)
        factor = 1.0 - factor;
    double val = minVal + factor * (maxVal - minVal);
            
    val = max(minVal, min(val, maxVal)); // Make sure val is within min/max.

    // Force integer value if not float.
    if (!equalIgnoringCase(precision, "float"))
        val = lround(val);

    static_cast<HTMLInputElement*>(node())->setValueFromRenderer(String::number(val));
    
    if (position != currentPosition()) {
        setCurrentPosition(position);
        static_cast<HTMLInputElement*>(node())->onChange();
    }
}

double RenderSlider::setPositionFromValue(bool inLayout)
{
    if (!m_thumb || !m_thumb->renderer())
        return 0;
    
    if (!inLayout)
        document()->updateLayout();
        
    String value = static_cast<HTMLInputElement*>(node())->value();
    const AtomicString& minStr = static_cast<HTMLInputElement*>(node())->getAttribute(minAttr);
    const AtomicString& maxStr = static_cast<HTMLInputElement*>(node())->getAttribute(maxAttr);
    const AtomicString& precision = static_cast<HTMLInputElement*>(node())->getAttribute(precisionAttr);
    
    double minVal = minStr.isNull() ? 0.0 : minStr.toDouble();
    double maxVal = maxStr.isNull() ? 100.0 : maxStr.toDouble();
    minVal = min(minVal, maxVal); // Make sure the range is sane.
    
    double oldVal = value.isNull() ? (maxVal + minVal)/2.0 : value.toDouble();
    double val = max(minVal, min(oldVal, maxVal)); // Make sure val is within min/max.
        
    // Force integer value if not float.
    if (!equalIgnoringCase(precision, "float"))
        val = lround(val);

    // Calculate the new position based on the value
    double factor = (val - minVal) / (maxVal - minVal);
    if (style()->appearance() == SliderVerticalPart)
        factor = 1.0 - factor;

    setCurrentPosition((int)(factor * trackSize()));
    
    if (value.isNull() || val != oldVal)
        static_cast<HTMLInputElement*>(node())->setValueFromRenderer(String::number(val));
    
    return val;
}

int RenderSlider::positionForOffset(const IntPoint& p)
{
    if (!m_thumb || !m_thumb->renderer())
        return 0;
   
    int position;
    if (style()->appearance() == SliderVerticalPart)
        position = p.y() - m_thumb->renderer()->height() / 2;
    else
        position = p.x() - m_thumb->renderer()->width() / 2;
    
    return max(0, min(position, trackSize()));
}

void RenderSlider::valueChanged()
{
    setValueForPosition(currentPosition());
    static_cast<HTMLInputElement*>(node())->onChange();
}

int RenderSlider::currentPosition()
{
    if (!m_thumb || !m_thumb->renderer())
        return 0;

    if (style()->appearance() == SliderVerticalPart)
        return m_thumb->renderer()->style()->top().value();
    return m_thumb->renderer()->style()->left().value();
}

void RenderSlider::setCurrentPosition(int pos)
{
    if (!m_thumb || !m_thumb->renderer())
        return;

    if (style()->appearance() == SliderVerticalPart)
        m_thumb->renderer()->style()->setTop(Length(pos, Fixed));
    else
        m_thumb->renderer()->style()->setLeft(Length(pos, Fixed));

    m_thumb->renderer()->layer()->updateLayerPosition();
    repaint();
    m_thumb->renderer()->repaint();
}

int RenderSlider::trackSize()
{
    if (!m_thumb || !m_thumb->renderer())
        return 0;

    if (style()->appearance() == SliderVerticalPart)
        return contentHeight() - m_thumb->renderer()->height();
    return contentWidth() - m_thumb->renderer()->width();
}

void RenderSlider::forwardEvent(Event* evt)
{
    m_thumb->defaultEventHandler(evt);
}

bool RenderSlider::inDragMode() const
{
    return m_thumb->inDragMode();
}

} // namespace WebCore
