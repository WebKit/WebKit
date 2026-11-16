/*
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/LayoutBox.h>

namespace WebCore {

class RenderElement;

namespace Style {
class ComputedStyle;
}

namespace Layout {

class ElementBox : public Box {
public:
    // The render tree children, without excluded list markers. RenderView's box is the only child of the initial containing block.
    // Defined in RenderElement.h.
    inline const Box* firstChild() const;
    NODELETE const Box* firstInFlowChild() const;
    NODELETE const Box* firstInFlowOrFloatingChild() const;
    NODELETE const Box* firstOutOfFlowChild() const;
    inline const Box* lastChild() const;
    NODELETE const Box* lastInFlowChild() const;
    NODELETE const Box* lastInFlowOrFloatingChild() const;
    NODELETE const Box* lastOutOfFlowChild() const;

    bool hasChild() const { return firstChild(); }
    bool hasInFlowChild() const { return firstInFlowChild(); }
    bool hasInFlowOrFloatingChild() const { return firstInFlowOrFloatingChild(); }
    NODELETE bool hasOutOfFlowChild() const;

    // From the renderer.
    bool isListMarkerImage() const;
    // FIXME: This is temporary until after list marker content is accessible by IFC (webkit.org/b/294342)
    std::pair<float, float> layoutBoundsForListMarker() const;

    // Null only for the initial containing block. Defined in RenderElement.h.
    inline RenderElement* rendererForIntegration() const;

protected:
    ElementBox(Style::ComputedStyle&&, ElementAttributes, EnumSet<BaseTypeFlag> = { });
    ~ElementBox();

private:
    NODELETE const Box& initialContainingBlockChild() const;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ElementBox, isElementBox())

// For the inline functions that read the renderer. RenderElement.h includes this header first.
#include <WebCore/RenderElement.h>
