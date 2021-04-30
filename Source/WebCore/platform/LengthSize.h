/*
    Copyright (C) 2006-2017 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include "Length.h"

namespace WebCore {

struct BlendingContext;

struct LengthSize {
    Length width;
    Length height;

    bool isEmpty() const { return width.isZero() || height.isZero(); }
    bool isZero() const { return width.isZero() && height.isZero(); }
};

ALWAYS_INLINE bool operator==(const LengthSize& a, const LengthSize& b)
{
    return a.width == b.width && a.height == b.height;
}

inline LengthSize blend(const LengthSize& from, const LengthSize& to, const BlendingContext& context)
{
    return { blend(from.width, to.width, context), blend(from.height, to.height, context) };
}

WTF::TextStream& operator<<(WTF::TextStream&, const LengthSize&);

} // namespace WebCore
