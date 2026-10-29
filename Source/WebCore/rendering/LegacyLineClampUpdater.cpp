/**
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
#include "LegacyLineClampUpdater.h"

#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LocalFrameView.h"
#include "LocalFrameViewInlines.h"
#include "RenderBlockFlow.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "StyleMaximumLines.h"

namespace WebCore {

LegacyLineClampUpdater::LegacyLineClampUpdater(RenderBlock& lineClampContainer)
    : m_lineClampContainer(lineClampContainer)
{
    auto& layoutState = *m_lineClampContainer->view().frameView().layoutContext().layoutState();
    m_ancestorLineClamp = layoutState.legacyLineClamp();

    auto lineCountForLineClamp = WTF::switchOn(m_lineClampContainer->style().maxLines(),
        [](const CSS::Keyword::Auto&) -> size_t {
            ASSERT_NOT_REACHED();
            return 1;
        },
        [](const Style::MaximumLines::Integer& integer) -> size_t {
            return integer.value;
        }
    );
    layoutState.setLegacyLineClamp(RenderLayoutState::LegacyLineClamp { lineCountForLineClamp, { }, { }, { } });
}

LegacyLineClampUpdater::~LegacyLineClampUpdater()
{
    m_lineClampContainer->view().frameView().layoutContext().layoutState()->setLegacyLineClamp(m_ancestorLineClamp);
}

static CheckedPtr<RenderBlockFlow> blockContainerForLastFormattedLine(RenderBlock& enclosingBlockContainer)
{
    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(enclosingBlockContainer); blockFlow && blockFlow->childrenInline()) {
        // The lines tell us where the last formatted line is: either it is one of this container's own lines, or it is inside the block level box sitting on the last line.
        auto blockLevelBoxOnLastFormattedLine = [&] -> CheckedPtr<RenderBlock> {
            for (auto& lineBox : InlineIterator::lineBoxesFor(*blockFlow) | std::views::reverse) {
                if (!lineBox.hasContentfulInFlowBox()) {
                    // Out-of-flow content could initiate a line with no inline content.
                    continue;
                }
                if (!lineBox.hasBlockContent()) {
                    // The last formatted line is one of this inline formatting context's own lines.
                    return { };
                }
                auto blockBox = lineBox.blockLevelBox();
                return blockBox ? dynamicDowncast<RenderBlock>(const_cast<RenderObject&>(blockBox->renderer())) : nullptr;
            }
            return { };
        };
        if (CheckedPtr blockOnLastFormattedLine = blockLevelBoxOnLastFormattedLine())
            return blockContainerForLastFormattedLine(*blockOnLastFormattedLine);
        return blockFlow->hasContentfulInlineLine() ? blockFlow : nullptr;
    }

    for (CheckedPtr child = enclosingBlockContainer.lastChild(); child; child = child->previousSibling()) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlock>(*child);
        if (!blockContainer)
            continue;
        if (CheckedPtr descendantRoot = blockContainerForLastFormattedLine(*blockContainer))
            return descendantRoot;
    }
    return { };
}

std::optional<LegacyLineClampUpdater::ClampedContent> LegacyLineClampUpdater::clampedContent()
{
    auto& layoutState = *m_lineClampContainer->view().frameView().layoutContext().layoutState();
    if (CheckedPtr lastRoot = blockContainerForLastFormattedLine(m_lineClampContainer.get())) {
        if (CheckedPtr inlineLayout = lastRoot->inlineLayout(); inlineLayout && inlineLayout->hasEllipsisInBlockDirectionOnLastFormattedLine()) {
            auto currentLineClamp = layoutState.legacyLineClamp();

            // Let line-clamp logic run but make sure no clamping happens (it's needed to make sure certain features are disabled like ellipsis in inline direction).
            layoutState.setLegacyLineClamp(RenderLayoutState::LegacyLineClamp { inlineLayout->lineCount() + 1, { }, { }, { } });
            lastRoot->setChildNeedsLayout(MarkingBehavior::OnlyThis);
            lastRoot->layoutIfNeeded();

            layoutState.setLegacyLineClamp(currentLineClamp);
        }
    }

    auto lineClamp = *layoutState.legacyLineClamp();
    if (!lineClamp.clampedContentLogicalHeight) {
        // We've managed to run line clamping but it came back with no clamped content (i.e. there are fewer lines than the line-clamp limit).
        return { };
    }
    return ClampedContent { *lineClamp.clampedContentLogicalHeight, lineClamp.clampedRenderer };
}

} // namespace WebCore
