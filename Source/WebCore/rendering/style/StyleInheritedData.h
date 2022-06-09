/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "Color.h"
#include "FontCascade.h"
#include "Length.h"

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleInheritedData);
class StyleInheritedData : public RefCounted<StyleInheritedData> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleInheritedData);
public:
    static Ref<StyleInheritedData> create() { return adoptRef(*new StyleInheritedData); }
    Ref<StyleInheritedData> copy() const;

    bool operator==(const StyleInheritedData&) const;
    bool operator!=(const StyleInheritedData& other) const { return !(*this == other); }

    float horizontalBorderSpacing;
    float verticalBorderSpacing;

    Length lineHeight;
#if ENABLE(TEXT_AUTOSIZING)
    Length specifiedLineHeight;
#endif

    FontCascade fontCascade;
    Color color;
    Color visitedLinkColor;

private:
    StyleInheritedData();
    StyleInheritedData(const StyleInheritedData&);
    void operator=(const StyleInheritedData&) = delete;
};

} // namespace WebCore
