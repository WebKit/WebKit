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

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box(EBoxSizing boxSizing)
    : m_boxSizing(boxSizing)
{
}

Box::~Box()
{
}

LayoutRect Box::marginBox() const
{
    ASSERT(m_hasValidMargin);
    auto marginBox = borderBox();

    marginBox.shiftXEdgeTo(marginBox.x() + m_marginLeft);
    marginBox.shiftYEdgeTo(marginBox.y() + m_marginTop);
    marginBox.shiftMaxXEdgeTo(marginBox.maxX() - m_marginRight);
    marginBox.shiftMaxYEdgeTo(marginBox.maxY() - m_marginBottom);

    return marginBox;
}

LayoutRect Box::borderBox() const
{
    if (m_boxSizing == BORDER_BOX)
        return LayoutRect( { }, size());

    // Width is content box.
    ASSERT(m_hasValidBorder);
    ASSERT(m_hasValidPadding);
    auto borderBoxSize = size();
    borderBoxSize.expand(borderLeft() + paddingLeft() + paddingRight() + borderRight() , borderTop() + paddingTop() + paddingBottom() + borderBottom());
    return LayoutRect( { }, borderBoxSize);
}

LayoutRect Box::paddingBox() const
{
    ASSERT(m_hasValidBorder);
    auto paddingBox = borderBox();

    paddingBox.shiftXEdgeTo(paddingBox.x() + m_borderLeft);
    paddingBox.shiftYEdgeTo(paddingBox.y() + m_borderTop);
    paddingBox.shiftMaxXEdgeTo(paddingBox.maxX() - m_borderRight);
    paddingBox.shiftMaxYEdgeTo(paddingBox.maxY() - m_borderBottom);

    return paddingBox;
}

LayoutRect Box::contentBox() const
{
    if (m_boxSizing == CONTENT_BOX)
        return LayoutRect(LayoutPoint(0, 0), size());

    // Width is border box.
    ASSERT(m_hasValidPadding);
    auto contentBox = paddingBox();

    contentBox.shiftXEdgeTo(contentBox.x() + m_paddingLeft);
    contentBox.shiftYEdgeTo(contentBox.y() + m_paddingTop);
    contentBox.shiftMaxXEdgeTo(contentBox.maxX() - m_paddingRight);
    contentBox.shiftMaxYEdgeTo(contentBox.maxY() - m_paddingBottom);

    return contentBox;
}

}
}

#endif
