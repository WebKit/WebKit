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

#include "config.h"
#include "BlockItemsBuilder.h"

#include "RenderBlockFlow.h"

namespace WebCore {
namespace Layout {

BlockItemsBuilder::BlockItemsBuilder(const RenderBlockFlow& root)
    : m_root(root)
{
}

static bool contentTakesPartInBlockFormattingContext(const RenderBox& box)
{
    // A block container's block-level content takes part in its parent's block formatting context unless the block
    // container establishes a new one (CSS 2.2 §9.4.1).
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return blockContainer && !blockContainer->childrenInline() && !blockContainer->createsNewFormattingContext();
}

static bool establishesIndependentFormattingContext(const RenderBox& box)
{
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return blockContainer && blockContainer->createsNewFormattingContext();
}

static bool hasInlineContent(const RenderBox& box)
{
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return blockContainer && blockContainer->childrenInline() && blockContainer->firstChild() && !blockContainer->createsNewFormattingContext();
}

BlockItemList BlockItemsBuilder::build()
{
    auto blockItems = BlockItemList { };
    CheckedPtr<const RenderBox> box = root().firstInFlowChildBox();
    while (box) {
        // FIXME: Floats and out-of-flow boxes are kept out by coverage.
        auto isIndependentFormattingContextRoot = establishesIndependentFormattingContext(*box);
        if (isIndependentFormattingContextRoot)
            blockItems.append({ *box, BlockItem::Type::IndependentFormattingContextRoot });
        else {
            ASSERT(contentTakesPartInBlockFormattingContext(*box) || hasInlineContent(*box) || !box->firstChild());
            blockItems.append({ *box, BlockItem::Type::BlockContainerStart });
            // FIXME: Block-level boxes inside the inline content split it into more ranges (block-in-inline is kept out by coverage).
            if (hasInlineContent(*box))
                blockItems.append({ *box, BlockItem::Type::InlineContent });
            else if (CheckedPtr firstChild = box->firstInFlowChildBox(); firstChild && contentTakesPartInBlockFormattingContext(*box)) {
                box = WTF::move(firstChild);
                continue;
            }
        }
        // Close this box (unless it is a single item) and every ancestor whose last in-flow child it is.
        for (auto closesBox = !isIndependentFormattingContextRoot; box; closesBox = true) {
            if (closesBox)
                blockItems.append({ *box, BlockItem::Type::BlockContainerEnd });
            if (CheckedPtr nextSibling = box->nextInFlowSiblingBox()) {
                box = WTF::move(nextSibling);
                break;
            }
            CheckedPtr<const RenderBox> parent = box->parentBox();
            box = parent.get() == &root() ? nullptr : WTF::move(parent);
        }
    }
    return blockItems;
}

}
}
