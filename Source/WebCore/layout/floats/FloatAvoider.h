/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutPoint.h"
#include "LayoutUnits.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {

class FloatAvoider {
    WTF_MAKE_ISO_ALLOCATED(FloatAvoider);
public:
    FloatAvoider(LayoutPoint absoluteTopLeft, LayoutUnit borderBoxWidth, const Edges& margin, HorizontalEdges containingBlockAbsoluteContentBox, bool isFloatingPositioned, bool isLeftAligned);
    virtual ~FloatAvoider() = default;

    void setHorizontalPosition(LayoutUnit);
    void setVerticalPosition(LayoutUnit);
    void resetHorizontalPosition() { m_absoluteTopLeft.setX(initialHorizontalPosition()); }

    bool overflowsContainingBlock() const;

    LayoutUnit top() const;
    LayoutUnit left() const;
    LayoutUnit right() const;

    bool isLeftAligned() const { return m_isLeftAligned; }

private:
    LayoutUnit borderBoxWidth() const { return m_borderBoxWidth; }
    LayoutUnit initialHorizontalPosition() const;

    LayoutUnit marginBefore() const { return m_margin.vertical.top; }
    LayoutUnit marginAfter() const { return m_margin.vertical.bottom; }
    LayoutUnit marginStart() const { return m_margin.horizontal.left; }
    LayoutUnit marginEnd() const { return m_margin.horizontal.right; }
    LayoutUnit marginBoxWidth() const { return marginStart() + borderBoxWidth() + marginEnd(); }

    bool isFloatingBox() const { return m_isFloatingPositioned; }

private:
    // These coordinate values are relative to the formatting root's border box.
    LayoutPoint m_absoluteTopLeft;
    // Note that float avoider should work with no height value.
    LayoutUnit m_borderBoxWidth;
    Edges m_margin;
    HorizontalEdges m_containingBlockAbsoluteContentBox;
    bool m_isFloatingPositioned { true };
    bool m_isLeftAligned { true };
};

inline LayoutUnit FloatAvoider::top() const
{
    auto top = m_absoluteTopLeft.y();
    if (isFloatingBox())
        top -= marginBefore();
    return top;
}

inline LayoutUnit FloatAvoider::left() const
{
    auto left = m_absoluteTopLeft.x();
    if (isFloatingBox())
        left -= marginStart();
    return left;
}

inline LayoutUnit FloatAvoider::right() const
{
    auto right = left() + borderBoxWidth();
    if (isFloatingBox())
        right += marginEnd();
    return right;
}

}
}
