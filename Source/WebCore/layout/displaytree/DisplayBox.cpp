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

#include "config.h"
#include "DisplayBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box(const RenderStyle& style)
    : m_style(style)
{
}

Box::~Box()
{
}

Box::Style::Style(const RenderStyle& style)
    : boxSizing(style.boxSizing())
{

}

Box::Rect Box::marginBox() const
{
    ASSERT(m_hasValidMargin);
    auto marginBox = borderBox();

    marginBox.shiftLeftTo(marginBox.left() + m_margin.left);
    marginBox.shiftBottomTo(marginBox.top() + m_margin.top);
    marginBox.shiftRightTo(marginBox.right() - m_margin.right);
    marginBox.shiftBottomTo(marginBox.bottom() - m_margin.bottom);

    return marginBox;
}

Box::Rect Box::borderBox() const
{
    if (m_style.boxSizing == BoxSizing::BorderBox)
        return Box::Rect( { }, size());

    // Width is content box.
    ASSERT(m_hasValidBorder);
    ASSERT(m_hasValidPadding);
    auto borderBoxSize = size();
    borderBoxSize.expand(borderLeft() + paddingLeft() + paddingRight() + borderRight(), borderTop() + paddingTop() + paddingBottom() + borderBottom());
    return Box::Rect( { }, borderBoxSize);
}

Box::Rect Box::paddingBox() const
{
    ASSERT(m_hasValidBorder);
    auto paddingBox = borderBox();

    paddingBox.shiftLeftTo(paddingBox.left() + m_border.left);
    paddingBox.shiftTopTo(paddingBox.top() + m_border.top);
    paddingBox.shiftRightTo(paddingBox.left() - m_border.right);
    paddingBox.shiftBottomTo(paddingBox.bottom() - m_border.bottom);

    return paddingBox;
}

Box::Rect Box::contentBox() const
{
    if (m_style.boxSizing == BoxSizing::ContentBox)
        return Box::Rect(LayoutPoint(0, 0), size());

    // Width is border box.
    ASSERT(m_hasValidPadding);
    auto contentBox = paddingBox();

    contentBox.shiftLeftTo(contentBox.left() + m_padding.left);
    contentBox.shiftTopTo(contentBox.top() + m_padding.top);
    contentBox.shiftBottomTo(contentBox.bottom() - m_padding.bottom);
    contentBox.shiftRightTo(contentBox.right() - m_padding.right);

    return contentBox;
}

}
}

#endif
