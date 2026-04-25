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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleWebKitMarqueeIncrement.h>
#include <WebCore/StyleWebKitMarqueeRepetition.h>
#include <WebCore/StyleWebKitMarqueeSpeed.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MarqueeData);
class MarqueeData : public RefCounted<MarqueeData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(MarqueeData, MarqueeData);
public:
    static Ref<MarqueeData> create() { return adoptRef(*new MarqueeData); }
    Ref<MarqueeData> copy() const;

    bool operator==(const MarqueeData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const MarqueeData&) const;
#endif

    WebkitMarqueeIncrement marqueeIncrement;
    WebkitMarqueeSpeed marqueeSpeed;
    WebkitMarqueeRepetition marqueeRepetition;

    PREFERRED_TYPE(MarqueeBehavior) unsigned marqueeBehavior : 2;
    PREFERRED_TYPE(MarqueeDirection) unsigned marqueeDirection : 3;

private:
    MarqueeData();
    MarqueeData(const MarqueeData&);
};

} // namespace Style
} // namespace WebCore
