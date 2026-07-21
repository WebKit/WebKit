/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationCoverage.h"

#include "Document.h"
#include "InlineWalker.h"
#include "RenderBlockFlow.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderListMarker.h"
#include "RenderObjectInlines.h"
#include "RenderSVGBlock.h"
#include "RenderSVGForeignObject.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"

namespace WebCore {
namespace LayoutIntegration {

bool canUseForLineLayout(const RenderBlockFlow& rootContainer)
{
    if (is<RenderSVGBlock>(rootContainer) && !rootContainer.isRenderOrLegacyRenderSVGForeignObject())
        return rootContainer.document().settings().useIFCForSVGText();
    return true;
}

bool canUseForIntrinsicWidthComputation(const RenderBlockFlow& blockContainer)
{
    for (auto walker = InlineWalker(blockContainer); !walker.atEnd(); walker.advance()) {
        CheckedRef renderer = *walker.current();
        if (!renderer->isInFlow())
            return false;

        auto isFullySupportedInFlowRenderer = isAnyOf<RenderText, RenderLineBreak, RenderInline, RenderListMarker>(renderer);
        if (isFullySupportedInFlowRenderer)
            continue;

        if (CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(renderer.get()); renderBlock && renderBlock->isAtomicInlineLevelBox() && !renderBlock->firstChild()) {
            if (renderBlock->style().usedAppearance() != StyleAppearance::None || (renderBlock->element() && renderBlock->element()->firstChild())) {
                // FIXME: Various widgets with or without appearance.
                // Dynamic content change (e.g. adding/removing select options) needs to dirty inlineContentCache.
                return false;
            }
            continue;
        }

        CheckedRef unsupportedRenderElement = downcast<RenderElement>(renderer.get());
        if (!unsupportedRenderElement->writingMode().isHorizontal() || !unsupportedRenderElement->style().logicalWidth().isFixed())
            return false;

        auto isNonSupportedFixedWidthContent = [&] {
            // FIXME: Implement this image special in line builder.
            auto allowImagesToBreak = !blockContainer.document().inQuirksMode() || !blockContainer.isRenderTableCell();
            if (!allowImagesToBreak)
                return true;
            // FIXME: See RenderReplaced::computeIntrinsicLogicalWidthContributions where m_minContentLogicalWidthContribution is set to 0.
            auto isReplacedWithSpecialIntrinsicWidth = [&] {
                if (auto* renderReplaced = dynamicDowncast<RenderReplaced>(unsupportedRenderElement.get()))
                    return renderReplaced->style().logicalMaxWidth().isPercentOrCalculated();
                return false;
            };
            return isReplacedWithSpecialIntrinsicWidth();
        };
        if (isNonSupportedFixedWidthContent())
            return false;
    }
    return true;
}

}
}
