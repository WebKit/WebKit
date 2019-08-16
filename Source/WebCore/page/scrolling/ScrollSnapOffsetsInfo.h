/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>

namespace WebCore {

template <typename T>
struct ScrollOffsetRange {
    T start;
    T end;
};

template <typename T>
struct ScrollSnapOffsetsInfo {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Vector<T> horizontalSnapOffsets;
    Vector<T> verticalSnapOffsets;

    // Snap offset ranges represent non-empty ranges of scroll offsets in which scrolling may rest after scroll snapping.
    // These are used in two cases: (1) for proximity scroll snapping, where portions of areas between adjacent snap offsets
    // may emit snap offset ranges, and (2) in the case where the snap area is larger than the snap port, in which case areas
    // where the snap port fits within the snap area are considered to be valid snap positions.
    Vector<ScrollOffsetRange<T>> horizontalSnapOffsetRanges;
    Vector<ScrollOffsetRange<T>> verticalSnapOffsetRanges;
};

}; // namespace WebCore
