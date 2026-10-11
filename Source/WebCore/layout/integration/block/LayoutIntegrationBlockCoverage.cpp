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
#include "LayoutIntegrationBlockCoverage.h"

#include "Document.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {
namespace LayoutIntegration {

bool canUseForBlockLayout(const RenderBlockFlow& rootContainer)
{
    if (!rootContainer.document().settings().blockFormattingContextIntegrationEnabled())
        return false;
    // Block formatting context integration does not cover any content yet.
    return false;
}

BlockLayoutCoverage computeBlockLayoutCoverage(const RenderView& renderView)
{
    // A block-level box is placed by the block formatting context of its block container, which the new block formatting context lays
    // out when the root of that block formatting context is covered.
    auto coverage = BlockLayoutCoverage { };
    auto renderersToVisit = Vector<std::pair<CheckedRef<const RenderElement>, bool>> { { renderView, false } };
    while (!renderersToVisit.isEmpty()) {
        auto [renderer, isCovered] = renderersToVisit.takeLast();
        for (CheckedRef child : childrenOfType<RenderElement>(renderer)) {
            if (is<RenderBlockFlow>(renderer) && is<RenderBox>(child) && !child->isInline()) {
                ++coverage.boxes;
                if (isCovered)
                    ++coverage.coveredBoxes;
            }
            CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(child);
            auto childrenAreCovered = blockContainer && blockContainer->createsNewFormattingContext() ? canUseForBlockLayout(*blockContainer) : isCovered;
            renderersToVisit.append({ child, childrenAreCovered });
        }
    }
    return coverage;
}

} // namespace LayoutIntegration
} // namespace WebCore
