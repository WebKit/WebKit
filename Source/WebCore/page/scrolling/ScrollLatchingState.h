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

#pragma once

#include <wtf/WeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class ContainerNode;
class Element;
class Frame;

class ScrollLatchingState final {
public:
    ScrollLatchingState();
    ~ScrollLatchingState();

    void clear();

    Element* wheelEventElement() const { return m_wheelEventElement.get(); }
    void setWheelEventElement(Element*);

    Frame* frame() const { return m_frame; }
    void setFrame(Frame* frame) { m_frame = frame; }

    bool widgetIsLatched() const { return m_widgetIsLatched; }
    void setWidgetIsLatched(bool isOverWidget);

    Element* previousWheelScrolledElement() const { return m_previousWheelScrolledElement.get(); }
    void setPreviousWheelScrolledElement(Element*);
    
    ContainerNode* scrollableContainer() const { return m_scrollableContainer.get(); }
    void setScrollableContainer(ContainerNode*);

private:
    WeakPtr<Element> m_wheelEventElement;
    WeakPtr<Element> m_previousWheelScrolledElement;
    WeakPtr<ContainerNode> m_scrollableContainer;

    Frame* m_frame { nullptr };

    bool m_widgetIsLatched { false };
};

WTF::TextStream& operator<<(WTF::TextStream&, const ScrollLatchingState&);

} // namespace WebCore
