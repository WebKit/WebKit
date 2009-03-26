/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StyleRareInheritedData.h"

#include "RenderStyle.h"
#include "StyleImage.h"

namespace WebCore {

StyleInheritedData::StyleInheritedData()
    : indent(RenderStyle::initialTextIndent())
    , line_height(RenderStyle::initialLineHeight())
    , list_style_image(RenderStyle::initialListStyleImage())
    , color(RenderStyle::initialColor())
    , m_effectiveZoom(RenderStyle::initialZoom())
    , horizontal_border_spacing(RenderStyle::initialHorizontalBorderSpacing())
    , vertical_border_spacing(RenderStyle::initialVerticalBorderSpacing())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , page_break_inside(RenderStyle::initialPageBreak())
{
}

StyleInheritedData::~StyleInheritedData()
{
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : RefCounted<StyleInheritedData>()
    , indent(o.indent)
    , line_height(o.line_height)
    , list_style_image(o.list_style_image)
    , cursorData(o.cursorData)
    , font(o.font)
    , color(o.color)
    , m_effectiveZoom(o.m_effectiveZoom)
    , horizontal_border_spacing(o.horizontal_border_spacing)
    , vertical_border_spacing(o.vertical_border_spacing)
    , widows(o.widows)
    , orphans(o.orphans)
    , page_break_inside(o.page_break_inside)
{
}

static bool cursorDataEquivalent(const CursorList* c1, const CursorList* c2)
{
    if (c1 == c2)
        return true;
    if ((!c1 && c2) || (c1 && !c2))
        return false;
    return (*c1 == *c2);
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return
        indent == o.indent &&
        line_height == o.line_height &&
        StyleImage::imagesEquivalent(list_style_image.get(), o.list_style_image.get()) &&
        cursorDataEquivalent(cursorData.get(), o.cursorData.get()) &&
        font == o.font &&
        color == o.color &&
        m_effectiveZoom == o.m_effectiveZoom &&
        horizontal_border_spacing == o.horizontal_border_spacing &&
        vertical_border_spacing == o.vertical_border_spacing &&
        widows == o.widows &&
        orphans == o.orphans &&
        page_break_inside == o.page_break_inside;
}

} // namespace WebCore
