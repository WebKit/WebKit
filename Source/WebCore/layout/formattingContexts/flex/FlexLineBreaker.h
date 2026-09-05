/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutUnit.h>
#include <span>
#include <wtf/Vector.h>

namespace WebCore {

// greedyLineBreaks fills each line until the next item would not fit, per
// https://drafts.csswg.org/css-flexbox-1/#algo-line-break.
WEBCORE_EXPORT Vector<size_t> greedyLineBreaks(std::span<const LayoutUnit> itemMainAxisSizes, LayoutUnit mainAxisAvailableSpace, LayoutUnit gapBetweenItems);

// balancedLineBreaks instead minimizes the sum of squared free space across all lines, per
// https://drafts.csswg.org/css-flexbox-2/#algo-balance. The result has at least minimumLineCount
// lines, unless there are fewer items than that.
WEBCORE_EXPORT Vector<size_t> balancedLineBreaks(std::span<const LayoutUnit> itemMainAxisSizes, LayoutUnit mainAxisAvailableSpace, LayoutUnit gapBetweenItems, size_t minimumLineCount);

} // namespace WebCore
