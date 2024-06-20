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

class InlineBoxIterator;

class InlineBox : public Box {
public:
    InlineBox(PathVariant&&);

    const RenderBoxModelObject& renderer() const { return downcast<RenderBoxModelObject>(Box::renderer()); }

    std::pair<bool, bool> hasClosedLeftAndRightEdge() const;

    // FIXME: Remove. For intermediate porting steps only.
    const LegacyInlineFlowBox* legacyInlineBox() const { return downcast<LegacyInlineFlowBox>(Box::legacyInlineBox()); }

    InlineBoxIterator nextInlineBox() const;
    InlineBoxIterator previousInlineBox() const;
    InlineBoxIterator iterator() const;

    LeafBoxIterator firstLeafBox() const;
    LeafBoxIterator lastLeafBox() const;
    LeafBoxIterator endLeafBox() const;
};

class InlineBoxIterator : public BoxIterator {
public:
    InlineBoxIterator() = default;
    InlineBoxIterator(Box::PathVariant&&);
    InlineBoxIterator(const Box&);

    const InlineBox& operator*() const { return get(); }
    const InlineBox* operator->() const { return &get(); }

    InlineBoxIterator& traverseNextInlineBox();
    InlineBoxIterator& traversePreviousInlineBox();

private:
    const InlineBox& get() const { return downcast<InlineBox>(m_box); }
};

InlineBoxIterator firstInlineBoxFor(const RenderInline&);
InlineBoxIterator firstRootInlineBoxFor(const RenderBlockFlow&);

InlineBoxIterator inlineBoxFor(const LegacyInlineFlowBox&);
InlineBoxIterator inlineBoxFor(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&);
InlineBoxIterator inlineBoxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);

inline InlineBoxIterator InlineBox::iterator() const
{
    return { *this };
}

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::InlineBox)
static bool isType(const WebCore::InlineIterator::Box& box) { return box.isInlineBox(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::InlineBoxIterator)
static bool isType(const WebCore::InlineIterator::BoxIterator& box) { return !box || box->isInlineBox(); }
SPECIALIZE_TYPE_TRAITS_END()
