/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineItem.h"

namespace WebCore {
namespace Layout {

class TextUtil {
public:
    static LayoutUnit width(const Box&, unsigned from, unsigned to, LayoutUnit contentLogicalLeft);
    static Optional<unsigned> hyphenPositionBefore(const InlineItem&, unsigned from, unsigned length);
    static bool isTrimmableContent(const InlineItem&);
    struct SplitData {
        unsigned start { 0 };
        unsigned length { 0 };
        LayoutUnit logicalWidth;
    };
    static SplitData split(const Box&, unsigned startPosition, unsigned length, LayoutUnit textWidth, LayoutUnit availableWidth, LayoutUnit contentLogicalLeft);

private:
    static LayoutUnit fixedPitchWidth(String, const RenderStyle&, unsigned from, unsigned to, LayoutUnit contentLogicalLeft);
};

}
}
#endif
