/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "InlineIteratorBox.h"

namespace WebCore {

namespace InlineIterator {

class InlineBox : public Box {
public:
    InlineBox(const LayoutIntegration::InlineContent&, size_t boxIndex);

    const RenderBoxModelObject& renderer() const { return downcast<RenderBoxModelObject>(Box::renderer()); }

    RectEdges<bool> LEGACY_NODELETE closedEdges() const;

    InlineBoxIterator LEGACY_NODELETE nextInlineBoxLineRightward() const;
    InlineBoxIterator LEGACY_NODELETE nextInlineBoxLineLeftward() const;
    InlineBoxIterator iterator() const;

    LeafBoxIterator firstLeafBox() const;
    LeafBoxIterator lastLeafBox() const;
    inline bool isSplit() const;

    BoxRange<BoxIterator> descendants() const;
    LeafBoxRange leafDescendants() const;

private:
    size_t endOfDescendantsIndex() const;
};

// Fragments of one inline box, in display order.
class InlineBoxIterator : public BasicBoxIterator<InlineBoxIterator, InlineBox, BoxFilter::SameLayoutBox> {
public:
    using BasicBoxIterator::BasicBoxIterator;

    InlineBoxIterator& traverseInlineBoxLineRightward() { return ++*this; }
    InlineBoxIterator& traverseInlineBoxLineLeftward() { return --*this; }
};

using InlineBoxRange = BoxRange<InlineBoxIterator>;

InlineBoxIterator lineLeftmostInlineBoxFor(const RenderBoxModelObject&);
InlineBoxRange inlineBoxesFor(const RenderBoxModelObject&);
InlineBoxIterator firstRootInlineBoxFor(const RenderBlockFlow&);
InlineBoxRange rootInlineBoxesFor(const RenderBlockFlow&);

InlineBox inlineBoxFor(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&);
InlineBox inlineBoxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);

inline bool InlineBox::isSplit() const
{
    return nextInlineBoxLineRightward() || nextInlineBoxLineLeftward();
}

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::InlineBox)
static bool isType(const WebCore::InlineIterator::Box& box) { return box.isInlineBox(); }
SPECIALIZE_TYPE_TRAITS_END()

