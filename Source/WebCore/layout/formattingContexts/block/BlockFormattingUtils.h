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

#include "FormattingConstraints.h"
#include <WebCore/LayoutUnit.h>
#include <WebCore/WritingMode.h>
#include <optional>

namespace WebCore {

class RenderBox;

namespace Style {
class ComputedStyle;
struct PreferredSize;
}

namespace Layout {

class BoxGeometry;

class BlockFormattingUtils {
public:
    struct InlineSizeAndMargins {
        LayoutUnit borderBoxInlineSize;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
    };
    // CSS 2.2 §10.3.3 (block-level, non-replaced elements in normal flow) with min/max-width (§10.4). Auto margins resolve the
    // way RenderBox::computeInlineDirectionMargins resolves them.
    // The min-content, max-content and fit-content inline sizes resolve against the inline sizes of the content (css-sizing-3 §3.2).
    static InlineSizeAndMargins inlineSizeAndMargins(const RenderBox&, LayoutUnit borderAndPaddingInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<IntrinsicWidthConstraints> contentInlineSizes);
    // Its inline size, min inline size or max inline size is min-content, max-content or fit-content.
    static bool hasIntrinsicInlineSize(const RenderBox&, WritingMode);
    // CSS 2.2 §10.6.3 (block-level, non-replaced elements in normal flow) with min/max-height (§10.7).
    static LayoutUnit borderBoxBlockSize(const RenderBox&, LayoutUnit borderAndPaddingBlockSize, LayoutUnit intrinsicContentBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, WritingMode);
    // The border box block size a preferred aspect ratio gives an auto block size, from the inline size (css-sizing-4 §5.1, like
    // RenderBox::blockSizeFromAspectRatio). None without a ratio or with a block size.
    static std::optional<LayoutUnit> aspectRatioBorderBoxBlockSize(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, WritingMode);
    // Its block-size, min-block-size or max-block-size is a percentage of the containing block's block size.
    static bool hasPercentageBlockSize(const RenderBox&, WritingMode);
    // The min-content, max-content and fit-content block sizes of a block container are its automatic block size (css-sizing-3 §2.1, §3.2), and a
    // percentage block size behaves as auto when the block size of the containing block is not definite (CSS 2.2 §10.5).
    static bool blockSizeBehavesAsAuto(const Style::PreferredSize& blockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize);
    static std::optional<LayoutUnit> definiteContentBoxBlockSize(const RenderBox&, const BoxGeometry&, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, WritingMode);
};

}
}
