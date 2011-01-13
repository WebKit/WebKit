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

#include "Event.h"
#include "Frame.h"
#include "MouseEvent.h"
#include "RenderSlider.h"

namespace WebCore {

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
                document()->frame()->eventHandler()->setCapturingMouseEventsNode(shadowHost());
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

}

