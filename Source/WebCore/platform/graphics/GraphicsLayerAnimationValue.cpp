/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#include "GraphicsLayerAnimationValue.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FloatAnimationValue);
WTF_MAKE_TZONE_ALLOCATED_IMPL(TransformAnimationValue);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FilterAnimationValue);

int validateFilterOperations(const KeyframeValueList<FilterAnimationValue>& keyframes)
{
    ASSERT(animatedPropertyIsFilterOrRelated(keyframes.property()));

    if (keyframes.size() < 2)
        return -1;

    // Empty filters match anything, so find the first non-empty entry as the reference
    size_t firstIndex = 0;
    for (; firstIndex < keyframes.size(); ++firstIndex) {
        if (!keyframes[firstIndex].value().isEmpty())
            break;
    }

    if (firstIndex >= keyframes.size())
        return -1;

    const FilterOperations& firstValue = keyframes[firstIndex].value();

    for (size_t i = firstIndex + 1; i < keyframes.size(); ++i) {
        const FilterOperations& value = keyframes[i].value();

        // An empty filter list matches anything.
        if (value.isEmpty())
            continue;

        if (!firstValue.operationsMatch(value))
            return -1;
    }

    return firstIndex;
}

} // namespace WebCore
