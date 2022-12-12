/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "FloatSize.h"
#include "ScrollTypes.h"

namespace WebCore {

WEBCORE_EXPORT FloatSize unitVectorForScrollDirection(ScrollDirection);

struct KeyboardScroll {
    FloatSize offset; // Points per increment.
    FloatSize maximumVelocity; // Points per second.
    FloatSize force;

    ScrollGranularity granularity { ScrollGranularity::Line };
    ScrollDirection direction { ScrollDirection::ScrollUp };

    bool operator==(const KeyboardScroll& other) const
    {
        return offset == other.offset
            && maximumVelocity == other.maximumVelocity
            && force == other.force
            && granularity == other.granularity
            && direction == other.direction;
    }
};

struct KeyboardScrollParameters {
    float springMass { 1 };
    float springStiffness { 109 };
    float springDamping { 20 };

    float maximumVelocityMultiplier { 25 };
    float timeToMaximumVelocity { 1 };

    float rubberBandForce { 5000 };

    static const KeyboardScrollParameters& parameters()
    {
        static const KeyboardScrollParameters parameters;
        return parameters;
    }
};

} // namespace WebCore
