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

#pragma once

#include "HTMLDivElement.h"
#include "RenderBlockFlow.h"
#include <wtf/Forward.h>

namespace WebCore {

class HTMLInputElement;
class TouchEvent;

class SliderThumbElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(SliderThumbElement);
public:
    static Ref<SliderThumbElement> create(Document&);

    void setPositionFromValue();
    void dragFrom(const LayoutPoint&);
    RefPtr<HTMLInputElement> hostInput() const;
    void setPositionFromPoint(const LayoutPoint&);

#if ENABLE(IOS_TOUCH_EVENTS)
    void handleTouchEvent(TouchEvent&);
#endif

    void hostDisabledStateChanged();

private:
    SliderThumbElement(Document&);
    bool isSliderThumbElement() const final { return true; }

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&) final;
    bool isDisabledFormControl() const final;
    bool matchesReadWritePseudoClass() const final;

    void defaultEventHandler(Event&) final;
    bool willRespondToMouseMoveEvents() const final;
    bool willRespondToMouseClickEventsWithEditability(Editability) const final;

#if ENABLE(IOS_TOUCH_EVENTS)
    void didAttachRenderers() final;
#endif
    void willDetachRenderers() final;

    std::optional<Style::ResolvedStyle> resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle*) final;

    void startDragging();
    void stopDragging();

#if ENABLE(IOS_TOUCH_EVENTS)
    unsigned exclusiveTouchIdentifier() const;
    void setExclusiveTouchIdentifier(unsigned);
    void clearExclusiveTouchIdentifier();

    void handleTouchStart(TouchEvent&);
    void handleTouchMove(TouchEvent&);
    void handleTouchEndAndCancel(TouchEvent&);

    bool shouldAcceptTouchEvents();
    void registerForTouchEvents();
    void unregisterForTouchEvents();
#endif

    AtomString m_shadowPseudoId;
    bool m_inDragMode { false };

#if ENABLE(IOS_TOUCH_EVENTS)
    // FIXME: Currently it is safe to use 0, but this may need to change
    // if touch identifiers change in the future and can be 0.
    static const unsigned NoIdentifier = 0;
    unsigned m_exclusiveTouchIdentifier { NoIdentifier };
    bool m_isRegisteredAsTouchEventListener { false };
#endif
};

// --------------------------------

class SliderContainerElement final : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(SliderContainerElement);
public:
    static Ref<SliderContainerElement> create(Document&);

private:
    SliderContainerElement(Document&);
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool isSliderContainerElement() const final { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SliderThumbElement)
    static bool isType(const WebCore::Element& element) { return element.isSliderThumbElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Element>(node) && isType(downcast<WebCore::Element>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SliderContainerElement)
    static bool isType(const WebCore::Element& element) { return element.isSliderContainerElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Element>(node) && isType(downcast<WebCore::Element>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
