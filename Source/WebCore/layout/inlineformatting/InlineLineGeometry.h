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
    InlineLineGeometry(const InlineRect& lineLogicalRect, const InlineRect& lineBoxLogicalRect, InlineLayoutUnit aligmentBaseline);

    InlineLayoutUnit logicalLeft() const { return logicalRect().left(); };
    InlineLayoutUnit logicalRight() const { return logicalRect().right(); };

    InlineLayoutUnit logicalTop() const { return logicalRect().top(); };
    InlineLayoutUnit logicalBottom() const { return logicalRect().bottom(); };

    InlineLayoutUnit logicalWidth() const { return logicalRect().width(); };
    InlineLayoutUnit logicalHeight() const { return logicalRect().height(); };

    const InlineRect& logicalRect() const { return m_logicalRect; }
    const InlineRect& lineBoxLogicalRect() const { return m_lineBoxLogicalRect; }

    InlineLayoutUnit baseline() const { return m_aligmentBaseline; }

    void moveVertically(InlineLayoutUnit offset) { m_logicalRect.moveVertically(offset); }

private:
    InlineRect m_logicalRect;
    InlineRect m_lineBoxLogicalRect;
    InlineLayoutUnit m_aligmentBaseline { 0 };
};

inline InlineLineGeometry::InlineLineGeometry(const InlineRect& lineLogicalRect, const InlineRect& lineBoxLogicalRect, InlineLayoutUnit aligmentBaseline)
    : m_logicalRect(lineLogicalRect)
    , m_lineBoxLogicalRect(lineBoxLogicalRect)
    , m_aligmentBaseline(aligmentBaseline)
{
}

}
}

#endif
