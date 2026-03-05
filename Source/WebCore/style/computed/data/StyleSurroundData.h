/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include <WebCore/BorderData.h>
#include <WebCore/StyleInset.h>
#include <WebCore/StyleMargin.h>
#include <WebCore/StylePadding.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SurroundData);
class SurroundData : public RefCounted<SurroundData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(SurroundData, SurroundData);
public:
    static Ref<SurroundData> create() { return adoptRef(*new SurroundData); }
    Ref<SurroundData> copy() const;

    bool operator==(const SurroundData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const SurroundData&) const;
#endif

    // Here instead of in BorderData to pack up against the refcount.
    bool hasExplicitlySetBorderBottomLeftRadius : 1;
    bool hasExplicitlySetBorderBottomRightRadius : 1;
    bool hasExplicitlySetBorderTopLeftRadius : 1;
    bool hasExplicitlySetBorderTopRightRadius : 1;

    bool hasExplicitlySetPaddingBottom : 1;
    bool hasExplicitlySetPaddingLeft : 1;
    bool hasExplicitlySetPaddingRight : 1;
    bool hasExplicitlySetPaddingTop : 1;

    InsetBox inset;
    MarginBox margin;
    PaddingBox padding;
    BorderData border;

private:
    SurroundData();
    SurroundData(const SurroundData&);    
};

} // namespace Style
} // namespace WebCore
