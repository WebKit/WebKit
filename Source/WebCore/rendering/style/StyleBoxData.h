/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleMaximumSize.h>
#include <WebCore/StyleMinimumSize.h>
#include <WebCore/StylePreferredSize.h>
#include <WebCore/StyleVerticalAlign.h>
#include <WebCore/StyleZIndex.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleBoxData);
class StyleBoxData : public RefCounted<StyleBoxData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleBoxData, StyleBoxData);
public:
    static Ref<StyleBoxData> create() { return adoptRef(*new StyleBoxData); }
    Ref<StyleBoxData> copy() const;

    bool operator==(const StyleBoxData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleBoxData&) const;
#endif

    const Style::PreferredSize& width() const { return m_width; }
    const Style::PreferredSize& height() const { return m_height; }

    const Style::MinimumSize& minWidth() const { return m_minWidth; }
    const Style::MinimumSize& minHeight() const { return m_minHeight; }

    const Style::MaximumSize& maxWidth() const { return m_maxWidth; }
    const Style::MaximumSize& maxHeight() const { return m_maxHeight; }
    
    const Style::VerticalAlign& verticalAlign() const { return m_verticalAlign; }

    Style::ZIndex specifiedZIndex() const { return { static_cast<bool>(m_hasAutoSpecifiedZIndex), m_specifiedZIndexValue }; }
    Style::ZIndex usedZIndex() const { return { static_cast<bool>(m_hasAutoUsedZIndex), m_usedZIndexValue }; }

    BoxSizing boxSizing() const { return static_cast<BoxSizing>(m_boxSizing); }
    BoxDecorationBreak boxDecorationBreak() const { return static_cast<BoxDecorationBreak>(m_boxDecorationBreak); }

private:
    friend class RenderStyle;

    StyleBoxData();
    StyleBoxData(const StyleBoxData&);

    Style::PreferredSize m_width;
    Style::PreferredSize m_height;

    Style::MinimumSize m_minWidth;
    Style::MaximumSize m_maxWidth;

    Style::MinimumSize m_minHeight;
    Style::MaximumSize m_maxHeight;

    Style::VerticalAlign m_verticalAlign;

    PREFERRED_TYPE(bool) uint8_t m_hasAutoSpecifiedZIndex : 1;
    PREFERRED_TYPE(bool) uint8_t m_hasAutoUsedZIndex : 1;
    PREFERRED_TYPE(BoxSizing) uint8_t m_boxSizing : 1;
    PREFERRED_TYPE(BoxDecorationBreak) uint8_t m_boxDecorationBreak : 1;

    Style::ZIndex::Value m_specifiedZIndexValue;
    Style::ZIndex::Value m_usedZIndexValue;
};

} // namespace WebCore
