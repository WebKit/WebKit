/*
 * Copyright (C) 2008, 2009, 2015 Apple Inc. All Rights Reserved.
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

#include "RenderPtr.h"
#include "RenderStyleConstants.h"
#include "Scrollbar.h"
#include <wtf/HashMap.h>

namespace WebCore {

class Frame;
class Element;
class RenderBox;
class RenderScrollbarPart;
class RenderStyle;

class RenderScrollbar final : public Scrollbar {
public:
    friend class Scrollbar;
    static Ref<Scrollbar> createCustomScrollbar(ScrollableArea&, ScrollbarOrientation, Element*, Frame* owningFrame = nullptr);
    virtual ~RenderScrollbar();

    RenderBox* owningRenderer() const;

    void paintPart(GraphicsContext&, ScrollbarPart, const IntRect&);

    IntRect buttonRect(ScrollbarPart) const;
    IntRect trackRect(int startLength, int endLength) const;
    IntRect trackPieceRectWithMargins(ScrollbarPart, const IntRect&) const;

    int minimumThumbLength() const;

    float opacity() const;
    
    bool isHiddenByStyle() const final;

    std::unique_ptr<RenderStyle> getScrollbarPseudoStyle(ScrollbarPart, PseudoId) const;

private:
    RenderScrollbar(ScrollableArea&, ScrollbarOrientation, Element*, Frame*);

    bool isOverlayScrollbar() const override { return false; }

    void setParent(ScrollView*) override;
    void setEnabled(bool) override;

    void paint(GraphicsContext&, const IntRect& damageRect, Widget::SecurityOriginPaintPolicy, EventRegionContext*) override;

    void setHoveredPart(ScrollbarPart) override;
    void setPressedPart(ScrollbarPart) override;

    void styleChanged() override;

    void updateScrollbarParts();

    void updateScrollbarPart(ScrollbarPart);

    // This Scrollbar(Widget) may outlive the DOM which created it (during tear down),
    // so we keep a reference to the Element which caused this custom scrollbar creation.
    // This will not create a reference cycle as the Widget tree is owned by our containing
    // FrameView which this Element pointer can in no way keep alive. See webkit bug 80610.
    RefPtr<Element> m_ownerElement;

    WeakPtr<Frame> m_owningFrame;
    HashMap<unsigned, RenderPtr<RenderScrollbarPart>> m_parts;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RenderScrollbar)
    static bool isType(const WebCore::Scrollbar& scrollbar) { return scrollbar.isCustomScrollbar(); }
SPECIALIZE_TYPE_TRAITS_END()
