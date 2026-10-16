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

namespace WebCore {

class RenderBlockFlow;
class RenderView;

namespace LayoutIntegration {

bool canUseForBlockLayout(const RenderBlockFlow&);
// Whether the block formatting context a block formatting context root is in lays out its content, instead of laying it out through the
// render tree.
bool canUseForNestedBlockLayout(const RenderBlockFlow&);
// Whether the new block formatting context lays out the root and an out-of-flow box whose containing block is the root or a box inside it.
bool canUseForBlockLayoutWithOutOfFlowBoxes(const RenderBlockFlow&);
bool canUseForIntrinsicInlineSizes(const RenderBlockFlow&);

struct BlockLayoutCoverage {
    unsigned coveredBoxes { 0 };
    unsigned boxes { 0 };

    bool operator==(const BlockLayoutCoverage&) const = default;
};
BlockLayoutCoverage computeBlockLayoutCoverage(const RenderView&);

} // namespace LayoutIntegration
} // namespace WebCore
