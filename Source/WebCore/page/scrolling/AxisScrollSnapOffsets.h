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

#ifndef AxisScrollSnapOffsets_h
#define AxisScrollSnapOffsets_h

#if ENABLE(CSS_SCROLL_SNAP)

#include "ScrollTypes.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTMLElement;
class RenderBox;
class RenderStyle;
class ScrollableArea;

void updateSnapOffsetsForScrollableArea(ScrollableArea&, HTMLElement& scrollingElement, const RenderBox& scrollingElementBox, const RenderStyle& scrollingElementStyle);

// closestSnapOffset is a templated function that takes in a Vector representing snap offsets as LayoutTypes (e.g. LayoutUnit or float) and
// as well as a VelocityType indicating the velocity (e.g. float, CGFloat, etc.) This function is templated because the UI process will now
// use pixel snapped floats to represent snap offsets rather than LayoutUnits.
template <typename LayoutType, typename VelocityType>
LayoutType closestSnapOffset(const Vector<LayoutType>& snapOffsets, LayoutType scrollDestination, VelocityType velocity)
{
    ASSERT(snapOffsets.size());
    if (scrollDestination <= snapOffsets.first())
        return snapOffsets.first();

    if (scrollDestination >= snapOffsets.last())
        return snapOffsets.last();

    size_t lowerIndex = 0;
    size_t upperIndex = snapOffsets.size() - 1;
    while (lowerIndex < upperIndex - 1) {
        size_t middleIndex = (lowerIndex + upperIndex) / 2;
        if (scrollDestination < snapOffsets[middleIndex])
            upperIndex = middleIndex;
        else if (scrollDestination > snapOffsets[middleIndex])
            lowerIndex = middleIndex;
        else {
            upperIndex = middleIndex;
            lowerIndex = middleIndex;
            break;
        }
    }
    LayoutType lowerSnapPosition = snapOffsets[lowerIndex];
    LayoutType upperSnapPosition = snapOffsets[upperIndex];
    // Nonzero velocity indicates a flick gesture. Even if another snap point is closer, snap to the one in the direction of the flick gesture.
    if (velocity)
        return velocity < 0 ? lowerSnapPosition : upperSnapPosition;

    bool isCloserToLowerSnapPosition = scrollDestination - lowerSnapPosition <= upperSnapPosition - scrollDestination;
    return isCloserToLowerSnapPosition ? lowerSnapPosition : upperSnapPosition;
}

} // namespace WebCore

#endif // CSS_SCROLL_SNAP

#endif // AxisScrollSnapOffsets_h
