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

Box::Rect::Rect(LayoutUnit top, LayoutUnit left, LayoutUnit width, LayoutUnit height)
    : m_rect(left, top, width, height)
{
#if !ASSERT_DISABLED
    m_hasValidTop = true;
    m_hasValidLeft = true;
    m_hasValidWidth = true;
    m_hasValidHeight = true;
#endif
}

Box::Box(const RenderStyle& style)
    : m_style(style)
{
}

Box::Box(const Box& other)
    : m_style(other.m_style)
    , m_topLeft(other.m_topLeft)
    , m_contentWidth(other.m_contentWidth)
    , m_contentHeight(other.m_contentHeight)
    , m_horizontalMargin(other.m_horizontalMargin)
    , m_verticalMargin(other.m_verticalMargin)
    , m_horizontalNonComputedMargin(other.m_horizontalNonComputedMargin)
    , m_estimatedMarginTop(other.m_estimatedMarginTop)
    , m_border(other.m_border)
    , m_padding(other.m_padding)
#if !ASSERT_DISABLED
    , m_hasValidTop(other.m_hasValidTop)
    , m_hasValidLeft(other.m_hasValidLeft)
    , m_hasValidHorizontalMargin(other.m_hasValidHorizontalMargin)
    , m_hasValidVerticalMargin(other.m_hasValidVerticalMargin)
    , m_hasValidVerticalNonCollapsedMargin(other.m_hasValidVerticalNonCollapsedMargin)
    , m_hasValidHorizontalNonComputedMargin(other.m_hasValidHorizontalNonComputedMargin)
    , m_hasValidBorder(other.m_hasValidBorder)
    , m_hasValidPadding(other.m_hasValidPadding)
    , m_hasValidContentHeight(other.m_hasValidContentHeight)
    , m_hasValidContentWidth(other.m_hasValidContentWidth)
#endif
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
    auto borderBox = this->borderBox();

    Rect marginBox;
    marginBox.setTop(borderBox.top() - marginTop());
    marginBox.setLeft(borderBox.left() - marginLeft());
    marginBox.setHeight(borderBox.height() + marginTop() + marginBottom());
    marginBox.setWidth(borderBox.width() + marginLeft() + marginRight());
    return marginBox;
}

Box::Rect Box::nonCollapsedMarginBox() const
{
    auto borderBox = this->borderBox();

    Rect marginBox;
    marginBox.setTop(borderBox.top() - nonCollapsedMarginTop());
    marginBox.setLeft(borderBox.left() - marginLeft());
    marginBox.setHeight(borderBox.height() + nonCollapsedMarginTop() + nonCollapsedMarginBottom());
    marginBox.setWidth(borderBox.width() + marginLeft() + marginRight());
    return marginBox;
}

Box::Rect Box::borderBox() const
{
    Rect borderBox;
    borderBox.setTopLeft({ });
    borderBox.setSize({ width(), height() });
    return borderBox;
}

Box::Rect Box::paddingBox() const
{
    auto borderBox = this->borderBox();

    Rect paddingBox;
    paddingBox.setTop(borderBox.top() + borderTop());
    paddingBox.setLeft(borderBox.left() + borderLeft());
    paddingBox.setHeight(borderBox.bottom() - borderTop() - borderBottom());
    paddingBox.setWidth(borderBox.width() - borderLeft() - borderRight());
    return paddingBox;
}

Box::Rect Box::contentBox() const
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

#endif
