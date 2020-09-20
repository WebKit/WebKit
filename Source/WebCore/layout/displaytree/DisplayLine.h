/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "FloatRect.h"

namespace WebCore {
namespace Display {

class Line {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Line(const FloatRect&, const FloatRect& scrollableOverflow, const FloatRect& inkOverflow, float baseline, float horizontalAlignmentOffset);

    const FloatRect& rect() const { return m_rect; }
    const FloatRect& scrollableOverflow() const { return m_scrollableOverflow; }
    const FloatRect& inkOverflow() const { return m_inkOverflow; }
    float baseline() const { return m_baseline; }
    float horizontalAlignmentOffset() const { return m_horizontalAlignmentOffset; }


private:
    FloatRect m_rect;
    FloatRect m_scrollableOverflow;
    FloatRect m_inkOverflow;
    float m_baseline { 0 };
    float m_horizontalAlignmentOffset;
};

inline Line::Line(const FloatRect& rect, const FloatRect& scrollableOverflow, const FloatRect& inkOverflow, float baseline, float horizontalAlignmentOffset)
    : m_rect(rect)
    , m_scrollableOverflow(scrollableOverflow)
    , m_inkOverflow(inkOverflow)
    , m_baseline(baseline)
    , m_horizontalAlignmentOffset(horizontalAlignmentOffset)
{
}

}
}

#endif
