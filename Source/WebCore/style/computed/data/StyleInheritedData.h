/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleColor.h>
#include <WebCore/StyleLineHeight.h>
#include <WebCore/StyleWebKitBorderSpacing.h>
#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

class FontData;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InheritedData);
class InheritedData : public RefCounted<InheritedData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(InheritedData, InheritedData);
public:
    static Ref<InheritedData> create() { return adoptRef(*new InheritedData); }
    Ref<InheritedData> copy() const;
    ~InheritedData();

    bool operator==(const InheritedData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const InheritedData&) const;
#endif

    bool fastPathInheritedEqual(const InheritedData&) const;
    bool nonFastPathInheritedEqual(const InheritedData&) const;
    void NODELETE fastPathInheritFrom(const InheritedData&);

    WebkitBorderSpacing borderHorizontalSpacing;
    WebkitBorderSpacing borderVerticalSpacing;

    LineHeight lineHeight;
#if ENABLE(TEXT_AUTOSIZING)
    LineHeight specifiedLineHeight;
#endif

    DataRef<FontData> fontData;
    WebCore::Color color;
    WebCore::Color visitedLinkColor;

private:
    InheritedData();
    InheritedData(const InheritedData&);
    void operator=(const InheritedData&) = delete;
};

} // namespace Style
} // namespace WebCore
