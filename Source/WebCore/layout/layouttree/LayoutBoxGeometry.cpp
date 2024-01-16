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
#include "LayoutBoxGeometry.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(BoxGeometry);

BoxGeometry::BoxGeometry(const BoxGeometry& other)
    : m_topLeft(other.m_topLeft)
    , m_contentBoxWidth(other.m_contentBoxWidth)
    , m_contentBoxHeight(other.m_contentBoxHeight)
    , m_margin(other.m_margin)
    , m_border(other.m_border)
    , m_padding(other.m_padding)
    , m_verticalSpaceForScrollbar(other.m_verticalSpaceForScrollbar)
    , m_horizontalSpaceForScrollbar(other.m_horizontalSpaceForScrollbar)
#if ASSERT_ENABLED
    , m_hasValidTop(other.m_hasValidTop)
    , m_hasValidLeft(other.m_hasValidLeft)
    , m_hasValidHorizontalMargin(other.m_hasValidHorizontalMargin)
    , m_hasValidVerticalMargin(other.m_hasValidVerticalMargin)
    , m_hasValidBorder(other.m_hasValidBorder)
    , m_hasValidPadding(other.m_hasValidPadding)
    , m_hasValidContentBoxHeight(other.m_hasValidContentBoxHeight)
    , m_hasValidContentBoxWidth(other.m_hasValidContentBoxWidth)
    , m_hasPrecomputedMarginBefore(other.m_hasPrecomputedMarginBefore)
#endif
{
}

BoxGeometry::~BoxGeometry()
{
}

Rect BoxGeometry::marginBox() const
{
    auto borderBox = this->borderBox();

    Rect marginBox;
    marginBox.setTop(borderBox.top() - marginBefore());
    marginBox.setLeft(borderBox.left() - marginStart());
    marginBox.setHeight(borderBox.height() + marginBefore() + marginAfter());
    marginBox.setWidth(borderBox.width() + marginStart() + marginEnd());
    return marginBox;
}

Rect BoxGeometry::borderBox() const
{
    Rect borderBox;
    borderBox.setTopLeft({ });
    borderBox.setSize({ borderBoxWidth(), borderBoxHeight() });
    return borderBox;
}

Rect BoxGeometry::paddingBox() const
{
    auto borderBox = this->borderBox();

    Rect paddingBox;
    paddingBox.setTop(borderBox.top() + borderBefore());
    paddingBox.setLeft(borderBox.left() + borderStart());
    paddingBox.setHeight(borderBox.bottom() - verticalSpaceForScrollbar() - borderAfter() - borderBefore());
    paddingBox.setWidth(borderBox.width() - borderEnd() - horizontalSpaceForScrollbar() - borderStart());
    return paddingBox;
}

Rect BoxGeometry::contentBox() const
{
    Rect contentBox;
    contentBox.setTop(contentBoxTop());
    contentBox.setLeft(contentBoxLeft());
    contentBox.setWidth(contentBoxWidth());
    contentBox.setHeight(contentBoxHeight());
    return contentBox;
}

}
}

