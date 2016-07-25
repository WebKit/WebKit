/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "StyleScrollSnapPoints.h"

#if ENABLE(CSS_SCROLL_SNAP)

namespace WebCore {

ScrollSnapPoints::ScrollSnapPoints()
    : repeatOffset(0, Fixed)
    , hasRepeat(false)
    , usesElements(false)
{
}

bool operator==(const ScrollSnapPoints& a, const ScrollSnapPoints& b)
{
    return a.repeatOffset == b.repeatOffset
        && a.hasRepeat == b.hasRepeat
        && a.usesElements == b.usesElements
        && a.offsets == b.offsets;
}

LengthSize defaultScrollSnapDestination()
{
    return LengthSize(Length(0, Fixed), Length(0, Fixed));
}

StyleScrollSnapPoints::StyleScrollSnapPoints()
    : destination(defaultScrollSnapDestination())
{
}

inline StyleScrollSnapPoints::StyleScrollSnapPoints(const StyleScrollSnapPoints& other)
    : RefCounted()
    , xPoints(other.xPoints ? std::make_unique<ScrollSnapPoints>(*other.xPoints) : nullptr)
    , yPoints(other.yPoints ? std::make_unique<ScrollSnapPoints>(*other.yPoints) : nullptr)
    , destination(other.destination)
    , coordinates(other.coordinates)
{
}

Ref<StyleScrollSnapPoints> StyleScrollSnapPoints::copy() const
{
    return adoptRef(*new StyleScrollSnapPoints(*this));
}

bool operator==(const StyleScrollSnapPoints& a, const StyleScrollSnapPoints& b)
{
    return a.xPoints == b.xPoints
        && a.yPoints == b.yPoints
        && a.destination == b.destination
        && a.coordinates == b.coordinates;
}

} // namespace WebCore

#endif /* ENABLE(CSS_SCROLL_SNAP) */
