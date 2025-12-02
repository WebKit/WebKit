/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include <WebCore/BorderValue.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleColumnCount.h>
#include <WebCore/StyleColumnWidth.h>
#include <WebCore/StyleLineWidth.h>
#include <wtf/RefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

// CSS3 Multi Column Layout

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMultiColData);
class StyleMultiColData : public RefCounted<StyleMultiColData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleMultiColData, StyleMultiColData);
public:
    static Ref<StyleMultiColData> create() { return adoptRef(*new StyleMultiColData); }
    Ref<StyleMultiColData> copy() const;
    
    bool operator==(const StyleMultiColData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleMultiColData&) const;
#endif

    Style::LineWidth columnRuleWidth() const;

    Style::ColumnWidth columnWidth { CSS::Keyword::Auto { } };
    Style::ColumnCount columnCount { CSS::Keyword::Auto { } };
    BorderValue columnRule;
    Style::Color visitedLinkColumnRuleColor;

    PREFERRED_TYPE(ColumnFill) unsigned columnFill : 1;
    PREFERRED_TYPE(ColumnSpan) unsigned columnSpan : 1;
    PREFERRED_TYPE(ColumnAxis) unsigned columnAxis : 2;
    PREFERRED_TYPE(ColumnProgression) unsigned columnProgression : 2;

private:
    StyleMultiColData();
    StyleMultiColData(const StyleMultiColData&);
};

} // namespace WebCore
