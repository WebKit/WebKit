/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "InlineIteratorTextBox.h"
#include "RenderSVGInlineText.h"

namespace WebCore {

class RenderSVGText;
struct SVGTextFragment;

namespace InlineIterator {

class SVGTextBox : public TextBox {
public:
    SVGTextBox(const LayoutIntegration::InlineContent&, size_t boxIndex);

    FloatRect calculateBoundariesIncludingSVGTransform() const;
    static FloatRect calculateBoundariesIncludingSVGTransform(const RenderSVGInlineText&, std::span<const SVGTextFragment>);
    LayoutRect localSelectionRect(unsigned start, unsigned end) const;
    const Vector<SVGTextFragment>& LEGACY_NODELETE textFragments() const;

    const RenderSVGInlineText& renderer() const { return downcast<RenderSVGInlineText>(TextBox::renderer()); }
};

class SVGTextBoxIterator : public BasicBoxIterator<SVGTextBoxIterator, SVGTextBox, BoxFilter::SameLayoutBox> {
public:
    using BasicBoxIterator::BasicBoxIterator;
};

using SVGTextBoxRange = BoxRange<SVGTextBoxIterator>;

SVGTextBoxIterator firstSVGTextBoxFor(const RenderSVGInlineText&);

SVGTextBoxRange svgTextBoxesFor(const RenderSVGInlineText&);
SVGTextBoxIterator svgTextBoxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);

BoxRange<BoxIterator> boxesFor(const RenderSVGText&);
BoxIterator lastBoxFor(const RenderSVGText&);

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::SVGTextBox)
static bool isType(const WebCore::InlineIterator::Box& box) { return box.isSVGText(); }
SPECIALIZE_TYPE_TRAITS_END()
