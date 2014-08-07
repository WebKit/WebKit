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

StyleScrollSnapPoints::StyleScrollSnapPoints()
    : repeatOffsetX(StyleScrollSnapPoints::defaultRepeatOffset())
    , repeatOffsetY(StyleScrollSnapPoints::defaultRepeatOffset())
    , destinationX(StyleScrollSnapPoints::defaultDestinationOffset())
    , destinationY(StyleScrollSnapPoints::defaultDestinationOffset())
    , hasRepeatX(true)
    , hasRepeatY(true)
    , usesElementsX(false)
    , usesElementsY(false)
{
}

inline StyleScrollSnapPoints::StyleScrollSnapPoints(const StyleScrollSnapPoints& o)
    : repeatOffsetX(o.repeatOffsetX)
    , repeatOffsetY(o.repeatOffsetY)
    , destinationX(o.destinationX)
    , destinationY(o.destinationY)
    , hasRepeatX(o.hasRepeatX)
    , hasRepeatY(o.hasRepeatY)
    , usesElementsX(o.usesElementsX)
    , usesElementsY(o.usesElementsY)
    , offsetsX(o.offsetsX)
    , offsetsY(o.offsetsY)
    , coordinates(o.coordinates)
{
}

PassRef<StyleScrollSnapPoints> StyleScrollSnapPoints::copy() const
{
    return adoptRef(*new StyleScrollSnapPoints(*this));
}

bool StyleScrollSnapPoints::operator==(const StyleScrollSnapPoints& o) const
{
    return (offsetsX == o.offsetsX
        && offsetsY == o.offsetsY
        && repeatOffsetX == o.repeatOffsetX
        && hasRepeatX == o.hasRepeatX
        && hasRepeatY == o.hasRepeatY
        && destinationX == o.destinationX
        && destinationY == o.destinationY
        && coordinates == o.coordinates
        && usesElementsX == o.usesElementsX
        && usesElementsY == o.usesElementsY);
}
    
} // namespace WebCore

#endif /* ENABLE(CSS_SCROLL_SNAP) */
