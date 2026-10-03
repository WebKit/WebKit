/**
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "RenderLayoutState.h"
#include <WebCore/LocalFrameView.h>
#include <WebCore/LocalFrameViewInlines.h>
#include <WebCore/RenderView.h>
#include <WebCore/StyleDisplay.h>
#include <WebCore/StyleMaximumLines.h>
#include <wtf/CheckedPtr.h>

namespace WebCore {

class LineClampUpdater {
public:
    LineClampUpdater(const RenderBlock& blockContainer);
    ~LineClampUpdater();

    bool isLineClampRoot() const { return m_isLineClampRoot; }
    void resetLineClamp();

private:
    const CheckedRef<const RenderBlock> m_blockContainer;
    bool m_isLineClampRoot { false };
    std::optional<RenderLayoutState::LineClamp> m_previousLineClamp { };
    std::optional<RenderLayoutState::LegacyLineClamp> m_skippedLegacyLineClampToRestore { };
};

inline LineClampUpdater::LineClampUpdater(const RenderBlock& blockContainer)
    : m_blockContainer(blockContainer)
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;

    m_previousLineClamp = layoutState->lineClamp();
    auto maximumLinesForBlockContainer = m_blockContainer->style().maxLines().tryValue();
    if (blockContainer.isFieldset() || (layoutState->legacyLineClamp() && blockContainer.isNonReplacedAtomicInlineLevelBox()) || blockContainer.isFloatingOrOutOfFlowPositioned()) {
        // Legacy line clamp does not cross into the interior of an atomic inline-level box.
        layoutState->setLineClamp({ });

        m_skippedLegacyLineClampToRestore = layoutState->legacyLineClamp();
        layoutState->setLegacyLineClamp({ });
        // The box may still clamp its own content.
        if (!maximumLinesForBlockContainer)
            return;
    }

    if (maximumLinesForBlockContainer) {
        // Ignore top level legacy line clamp for now.
        if (m_blockContainer->style().overflowContinue() == OverflowContinue::WebkitLegacy)
            return;
        // New, top level line clamp.
        m_isLineClampRoot = true;
        layoutState->setLineClamp(RenderLayoutState::LineClamp { static_cast<size_t>(maximumLinesForBlockContainer->value), m_blockContainer->style().overflowContinue() == OverflowContinue::Discard });
        return;
    }

    if (m_previousLineClamp) {
        // Propagated line clamp.
        if (blockContainer.establishesIndependentFormattingContext() || blockContainer.style().display() == Style::DisplayType::RubyText) {
            // Contents of descendants that establish independent formatting contexts are skipped over while counting line boxes,
            // and a ruby annotation belongs to the line of its base: it is clamped with that line, not line by line on its own.
            layoutState->setLineClamp({ });
            return;
        }
        auto effectiveShouldDiscard = m_previousLineClamp->shouldDiscardOverflow  || m_blockContainer->style().overflowContinue() == OverflowContinue::Discard;
        layoutState->setLineClamp(RenderLayoutState::LineClamp { m_previousLineClamp->maximumLines, effectiveShouldDiscard });
        return;
    }
}

inline LineClampUpdater::~LineClampUpdater()
{
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;

    if (m_skippedLegacyLineClampToRestore)
        layoutState->setLegacyLineClamp(m_skippedLegacyLineClampToRestore);

    if (!m_previousLineClamp) {
        layoutState->setLineClamp({ });
        return;
    }

    auto lineClamp = layoutState->lineClamp();
    if (!lineClamp || m_blockContainer->establishesIndependentFormattingContext()) {
        // "Only line boxes in the same block formatting context are counted: the contents of descendants
        // that establish independent formatting contexts are skipped over while counting line boxes."
        // https://drafts.csswg.org/css-overflow-4/#max-lines
        layoutState->setLineClamp(m_previousLineClamp);
        return;
    }

    size_t lineCount = m_isLineClampRoot ? 0 : m_previousLineClamp->maximumLines - std::min(m_previousLineClamp->maximumLines, lineClamp->maximumLines);
    if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(m_blockContainer.get()); blockFlow && blockFlow->childrenInline())
        lineCount = blockFlow->lineCount();
    layoutState->setLineClamp(RenderLayoutState::LineClamp { m_previousLineClamp->maximumLines - std::min(m_previousLineClamp->maximumLines, lineCount), m_previousLineClamp->shouldDiscardOverflow });
}

inline void LineClampUpdater::resetLineClamp()
{
    ASSERT(m_isLineClampRoot);
    auto* layoutState = m_blockContainer->view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return;
    layoutState->setLineClamp({ });
}

}
