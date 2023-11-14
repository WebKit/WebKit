/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "KeyframeInterpolation.h"

namespace WebCore {

const KeyframeInterpolation::KeyframeInterval KeyframeInterpolation::interpolationKeyframes(Property property, double iterationProgress, const Keyframe& defaultStartKeyframe, const Keyframe& defaultEndKeyframe) const
{
    // 1. If iteration progress is unresolved abort this procedure.
    // 2. Let target property be the longhand property for which the effect value is to be calculated.
    // 3. If animation type of the target property is not animatable abort this procedure since the effect cannot be applied.
    // 4. Define the neutral value for composition as a value which, when combined with an underlying value using the add composite operation,
    //    produces the underlying value.

    // 5. Let property-specific keyframes be the result of getting the set of computed keyframes for this keyframe effect.
    // 6. Remove any keyframes from property-specific keyframes that do not have a property value for target property.
    unsigned numberOfKeyframesWithZeroOffset = 0;
    unsigned numberOfKeyframesWithOneOffset = 0;

    Vector<const Keyframe*> propertySpecificKeyframes;

    for (size_t i = 0; i < numberOfKeyframes(); ++i) {
        auto& keyframe = keyframeAtIndex(i);
        auto offset = keyframe.offset();
        if (!keyframe.animatesProperty(property))
            continue;
        if (!offset)
            numberOfKeyframesWithZeroOffset++;
        if (offset == 1)
            numberOfKeyframesWithOneOffset++;
        propertySpecificKeyframes.append(&keyframe);
    }

    // 7. If property-specific keyframes is empty, return underlying value.
    if (propertySpecificKeyframes.isEmpty())
        return { };

    auto hasImplicitZeroKeyframe = !numberOfKeyframesWithZeroOffset;
    auto hasImplicitOneKeyframe = !numberOfKeyframesWithOneOffset;

    // 8. If there is no keyframe in property-specific keyframes with a computed keyframe offset of 0, create a new keyframe with a computed keyframe
    //    offset of 0, a property value set to the neutral value for composition, and a composite operation of add, and prepend it to the beginning of
    //    property-specific keyframes.
    if (hasImplicitZeroKeyframe) {
        propertySpecificKeyframes.insert(0, &defaultStartKeyframe);
        numberOfKeyframesWithZeroOffset = 1;
    }

    // 9. Similarly, if there is no keyframe in property-specific keyframes with a computed keyframe offset of 1, create a new keyframe with a computed
    //    keyframe offset of 1, a property value set to the neutral value for composition, and a composite operation of add, and append it to the end of
    //    property-specific keyframes.
    if (hasImplicitOneKeyframe) {
        propertySpecificKeyframes.append(&defaultEndKeyframe);
        numberOfKeyframesWithOneOffset = 1;
    }

    // 10. Let interval endpoints be an empty sequence of keyframes.
    Vector<const Keyframe*> intervalEndpoints;

    // 11. Populate interval endpoints by following the steps from the first matching condition from below:
    if (iterationProgress < 0 && numberOfKeyframesWithZeroOffset > 1) {
        // If iteration progress < 0 and there is more than one keyframe in property-specific keyframes with a computed keyframe offset of 0,
        // Add the first keyframe in property-specific keyframes to interval endpoints.
        intervalEndpoints.append(propertySpecificKeyframes.first());
    } else if (iterationProgress >= 1 && numberOfKeyframesWithOneOffset > 1) {
        // If iteration progress ≥ 1 and there is more than one keyframe in property-specific keyframes with a computed keyframe offset of 1,
        // Add the last keyframe in property-specific keyframes to interval endpoints.
        intervalEndpoints.append(propertySpecificKeyframes.last());
    } else {
        // Otherwise,
        // 1. Append to interval endpoints the last keyframe in property-specific keyframes whose computed keyframe offset is less than or equal
        //    to iteration progress and less than 1. If there is no such keyframe (because, for example, the iteration progress is negative),
        //    add the last keyframe whose computed keyframe offset is 0.
        // 2. Append to interval endpoints the next keyframe in property-specific keyframes after the one added in the previous step.
        size_t indexOfLastKeyframeWithZeroOffset = 0;
        int indexOfFirstKeyframeToAddToIntervalEndpoints = -1;
        for (size_t i = 0; i < propertySpecificKeyframes.size(); ++i) {
            auto offset = propertySpecificKeyframes[i]->offset();
            if (!offset)
                indexOfLastKeyframeWithZeroOffset = i;
            if (offset <= iterationProgress && offset < 1)
                indexOfFirstKeyframeToAddToIntervalEndpoints = i;
            else
                break;
        }

        if (indexOfFirstKeyframeToAddToIntervalEndpoints >= 0) {
            intervalEndpoints.append(propertySpecificKeyframes[indexOfFirstKeyframeToAddToIntervalEndpoints]);
            intervalEndpoints.append(propertySpecificKeyframes[indexOfFirstKeyframeToAddToIntervalEndpoints + 1]);
        } else {
            ASSERT(indexOfLastKeyframeWithZeroOffset < propertySpecificKeyframes.size() - 1);
            intervalEndpoints.append(propertySpecificKeyframes[indexOfLastKeyframeWithZeroOffset]);
            intervalEndpoints.append(propertySpecificKeyframes[indexOfLastKeyframeWithZeroOffset + 1]);
        }
    }

    return { intervalEndpoints, hasImplicitZeroKeyframe, hasImplicitOneKeyframe };
}

} // namespace WebCore
