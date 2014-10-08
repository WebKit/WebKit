/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollLatchingState_h
#define ScrollLatchingState_h

#include <wtf/RefPtr.h>

namespace WebCore {

class ContainerNode;
class Element;
class Frame;

class ScrollLatchingState {
public:
    ScrollLatchingState();
    virtual ~ScrollLatchingState();

    void clear();

    Element* wheelEventElement() { return m_wheelEventElement.get(); }
    void setWheelEventElement(PassRefPtr<Element>);
    Frame* frame() { return m_frame; }
    void setFrame(Frame* frame) { m_frame = frame; }

    bool widgetIsLatched() const { return m_widgetIsLatched; }
    void setWidgetIsLatched(bool isOverWidget);

    Element* previousWheelScrolledElement() { return m_previousWheelScrolledElement.get(); }
    void setPreviousWheelScrolledElement(PassRefPtr<Element>);
    
    ContainerNode* scrollableContainer() { return m_scrollableContainer.get(); }
    void setScrollableContainer(PassRefPtr<ContainerNode>);
    bool startedGestureAtScrollLimit() const { return m_startedGestureAtScrollLimit; }
    void setStartedGestureAtScrollLimit(bool startedAtLimit) { m_startedGestureAtScrollLimit = startedAtLimit; }

private:
    RefPtr<Element> m_wheelEventElement;
    RefPtr<Element> m_previousWheelScrolledElement;
    RefPtr<ContainerNode> m_scrollableContainer;

    Frame* m_frame;
    
    bool m_widgetIsLatched;
    bool m_startedGestureAtScrollLimit;
};
    
}

#endif
