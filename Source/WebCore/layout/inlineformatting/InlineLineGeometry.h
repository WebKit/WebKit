/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineRect.h"

namespace WebCore {
namespace Layout {

class InlineLineGeometry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct EnclosingTopAndBottom {
        // This values encloses the root inline box and any other inline level box's border box.
        InlineLayoutUnit top { 0 };
        InlineLayoutUnit bottom { 0 };
    };
    InlineLineGeometry(const InlineRect& lineBoxLogicalRect, EnclosingTopAndBottom, InlineLayoutUnit aligmentBaseline, InlineLayoutUnit contentLogicalLeftOffset, InlineLayoutUnit contentLogicalWidth);

    const InlineRect& lineBoxLogicalRect() const { return m_lineBoxLogicalRect; }

    EnclosingTopAndBottom enclosingTopAndBottom() const { return m_enclosingTopAndBottom; }

    InlineLayoutUnit baseline() const { return m_aligmentBaseline; }

    InlineLayoutUnit contentLogicalLeftOffset() const { return m_contentLogicalLeftOffset; }
    InlineLayoutUnit contentLogicalWidth() const { return m_contentLogicalWidth; }

    void moveVertically(InlineLayoutUnit offset) { m_lineBoxLogicalRect.moveVertically(offset); }

private:
    // This is line box geometry (see https://www.w3.org/TR/css-inline-3/#line-box).
    InlineRect m_lineBoxLogicalRect;
    // Enclosing top and bottom includes all inline level boxes (border box) vertically.
    // While the line box usually enclose them as well, its vertical geometry is based on
    // the layout bounds of the inline level boxes which may be different when line-height is present.
    EnclosingTopAndBottom m_enclosingTopAndBottom;
    InlineLayoutUnit m_aligmentBaseline { 0 };
    InlineLayoutUnit m_contentLogicalLeftOffset { 0 };
    InlineLayoutUnit m_contentLogicalWidth { 0 };
};

inline InlineLineGeometry::InlineLineGeometry(const InlineRect& lineBoxLogicalRect, EnclosingTopAndBottom enclosingTopAndBottom, InlineLayoutUnit aligmentBaseline, InlineLayoutUnit contentLogicalLeftOffset, InlineLayoutUnit contentLogicalWidth)
    : m_lineBoxLogicalRect(lineBoxLogicalRect)
    , m_enclosingTopAndBottom(enclosingTopAndBottom)
    , m_aligmentBaseline(aligmentBaseline)
    , m_contentLogicalLeftOffset(contentLogicalLeftOffset)
    , m_contentLogicalWidth(contentLogicalWidth)
{
}

}
}

#endif
