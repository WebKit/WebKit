/*
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

#include "config.h"
#include "TextBoxTrimmer.h"

#include "InlineIteratorLineBox.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderMultiColumnFlow.h"
#include "RenderView.h"

namespace WebCore {

static TextBoxTrim textBoxTrim(const RenderBlockFlow& textBoxTrimRoot)
{
    if (auto* multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(textBoxTrimRoot))
        return multiColumnFlow->multiColumnBlockFlow()->style().textBoxTrim();
    return textBoxTrimRoot.style().textBoxTrim();
}

static bool shouldIgnoreAsFirstLastFormattedLineContainer(const RenderBlockFlow& container)
{
    if (container.style().display() == DisplayType::RubyAnnotation || container.createsNewFormattingContext())
        return true;
    // Empty continuation pre/post blocks should be ignored as they are implementation detail.
    if (container.isAnonymousBlock()) {
        if (auto firstLineBox = InlineIterator::firstLineBoxFor(container))
            return !firstLineBox->firstLeafBox();
        return true;
    }
    return false;
}

static inline RenderBlockFlow* firstFormattedLineRoot(const RenderBlockFlow& enclosingBlockContainer)
{
    for (auto* child = enclosingBlockContainer.firstChild(); child; child = child->nextSibling()) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*child);
        if (!blockContainer || blockContainer->createsNewFormattingContext())
            continue;
        if (blockContainer->hasLines())
            return blockContainer.get();
        if (auto* descendantRoot = firstFormattedLineRoot(*blockContainer))
            return descendantRoot;
        if (!shouldIgnoreAsFirstLastFormattedLineContainer(*blockContainer))
            return nullptr;
    }
    return nullptr;
}

static RenderBlockFlow* lastFormattedLineRoot(const RenderBlockFlow& enclosingBlockContainer)
{
    for (auto* child = enclosingBlockContainer.lastChild(); child; child = child->previousSibling()) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*child);
        if (!blockContainer || blockContainer->createsNewFormattingContext())
            continue;
        if (blockContainer->hasLines())
            return blockContainer.get();
        if (auto* descendantRoot = lastFormattedLineRoot(*blockContainer))
            return descendantRoot;
        if (!shouldIgnoreAsFirstLastFormattedLineContainer(*blockContainer))
            return nullptr;
    }
    return nullptr;
}

TextBoxTrimmer::TextBoxTrimmer(const RenderBlockFlow& blockContainer)
    : m_blockContainer(blockContainer)
{
    if (m_blockContainer->view().frameView().layoutContext().layoutState())
        adjustTextBoxTrimStatusBeforeLayout({ });
}

TextBoxTrimmer::TextBoxTrimmer(const RenderBlockFlow& blockContainer, const RenderBlockFlow& lastFormattedLineRoot)
    : m_blockContainer(blockContainer)
{
    if (m_blockContainer->view().frameView().layoutContext().layoutState())
        adjustTextBoxTrimStatusBeforeLayout(&lastFormattedLineRoot);
}

TextBoxTrimmer::~TextBoxTrimmer()
{
    if (m_blockContainer->view().frameView().layoutContext().layoutState())
        adjustTextBoxTrimStatusAfterLayout();
}

RenderBlockFlow* TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd(const RenderBlockFlow& blockContainer)
{
    auto textBoxTrimValue = textBoxTrim(blockContainer);
    auto hasTextBoxTrimEnd = textBoxTrimValue == TextBoxTrim::TrimEnd || textBoxTrimValue == TextBoxTrim::TrimBoth;
    if (!hasTextBoxTrimEnd)
        return { };
    // If the last block container has border/padding end, trimming should not happen.
    if (auto* candidateForLastBlockContainer = lastFormattedLineRoot(blockContainer); candidateForLastBlockContainer && !candidateForLastBlockContainer->borderAndPaddingEnd())
        return candidateForLastBlockContainer;
    return { };
}

void TextBoxTrimmer::adjustTextBoxTrimStatusBeforeLayout(const RenderBlockFlow* lastFormattedLineRoot)
{
    auto textBoxTrimValue = textBoxTrim(*m_blockContainer);
    if (textBoxTrimValue == TextBoxTrim::None)
        return handlePropagatedTextBoxTrimBeforeLayout();

    // This block container starts setting up trimming for its subtree.
    // 1. Let's save the current trimming status (and restore after layout)
    // 2. Figure out which side(s) of the content is going to get trimmed.
    auto& layoutState = *m_blockContainer->view().frameView().layoutContext().layoutState();
    m_previousTextBoxTrimStatus = layoutState.textBoxTrim();
    m_shouldRestoreTextBoxTrimStatus = true;

    auto shouldTrimFirstFormattedLine = textBoxTrimValue == TextBoxTrim::TrimStart || textBoxTrimValue == TextBoxTrim::TrimBoth;
    if (textBoxTrimValue == TextBoxTrim::TrimEnd || textBoxTrimValue == TextBoxTrim::TrimBoth) {
        if (!lastFormattedLineRoot && m_blockContainer->childrenInline()) {
            // Trimming is explicitly set on this inline formatting context. Let's assume last line part of this block container.
            lastFormattedLineRoot = m_blockContainer.get();
        } else if (lastFormattedLineRoot) {
            // This is the dedicated "last line" layout on the last formatting context, where we should not trim the first line
            // unless this IFC includes it too.
            if (shouldTrimFirstFormattedLine)
                shouldTrimFirstFormattedLine = firstFormattedLineRoot(*m_blockContainer) == lastFormattedLineRoot;
        }
    }
    layoutState.setTextBoxTrim(RenderLayoutState::TextBoxTrim { shouldTrimFirstFormattedLine, m_blockContainer->style().textBoxEdge(), lastFormattedLineRoot });
}

void TextBoxTrimmer::adjustTextBoxTrimStatusAfterLayout()
{
    auto& layoutState = *m_blockContainer->view().frameView().layoutContext().layoutState();
    if (m_shouldRestoreTextBoxTrimStatus)
        return layoutState.setTextBoxTrim(m_previousTextBoxTrimStatus);

    if (layoutState.hasTextBoxTrimStart()) {
        // Only the first formatted line is trimmed.
        if (!shouldIgnoreAsFirstLastFormattedLineContainer(*m_blockContainer))
            layoutState.removeTextBoxTrimStart();
    }
}

void TextBoxTrimmer::handlePropagatedTextBoxTrimBeforeLayout()
{
    auto& layoutState = *m_blockContainer->view().frameView().layoutContext().layoutState();
    // This is when the block container does not have text-box-trim set.
    // 1. trimming does not get propagated into formatting contexts e.g inside inline-block.
    // 2. border and padding (start) prevent trim start.
    if (m_blockContainer->createsNewFormattingContext()) {
        m_previousTextBoxTrimStatus = layoutState.textBoxTrim();
        m_shouldRestoreTextBoxTrimStatus = true;
        // Run layout on this subtree with no text-box-trim.
        layoutState.setTextBoxTrim({ });
        return;
    }

    if (layoutState.hasTextBoxTrimStart() && m_blockContainer->borderAndPaddingStart()) {
        // We've got this far with no start trimming and now border/padding prevent trimming.
        layoutState.removeTextBoxTrimStart();
    }
}

} // namespace WebCore
