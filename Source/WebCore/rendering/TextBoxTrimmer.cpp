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

#include "InlineIteratorBox.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorLineBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderMultiColumnFlow.h"
#include "RenderObjectDocument.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {

static TextBoxTrim textBoxTrim(const RenderBlockFlow& textBoxTrimRoot)
{
    if (auto* multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(textBoxTrimRoot))
        return multiColumnFlow->multiColumnBlockFlow()->style().textBoxTrim();
    return textBoxTrimRoot.style().textBoxTrim();
}

static void removeTextBoxTrimStart(LocalFrameViewLayoutContext& layoutContext)
{
    auto textBoxTrim = layoutContext.textBoxTrim();
    if (!textBoxTrim || !textBoxTrim->trimFirstFormattedLine) {
        ASSERT_NOT_REACHED();
        return;
    }
    layoutContext.setTextBoxTrim(LocalFrameViewLayoutContext::TextBoxTrim { false, textBoxTrim->lastFormattedLineRoot });
}

static bool shouldIgnoreAsFirstLastFormattedLineContainer(const RenderBlockFlow& container)
{
    if (container.style().display() == DisplayType::RubyAnnotation || container.createsNewFormattingContext())
        return true;
    // Empty continuation pre/post blocks should be ignored as they are implementation detail.
    if (container.isAnonymousBlock()) {
        if (auto firstLineBox = InlineIterator::firstLineBoxFor(container))
            return !firstLineBox->lineLeftmostLeafBox();
        return true;
    }
    return false;
}

static inline RenderBlockFlow* firstFormattedLineRoot(const RenderBlockFlow& enclosingBlockContainer)
{
    for (auto* child = enclosingBlockContainer.firstChild(); child; child = child->nextSibling()) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*child);
        if (!blockContainer || blockContainer->createsNewFormattingContext() || blockContainer->isFirstLetter())
            continue;
        if (blockContainer->hasLines())
            return blockContainer.unsafeGet();
        if (auto* descendantRoot = firstFormattedLineRoot(*blockContainer))
            return descendantRoot;
        if (!shouldIgnoreAsFirstLastFormattedLineContainer(*blockContainer))
            return nullptr;
    }
    return nullptr;
}

static RenderBlockFlow* lastFormattedLineRoot(const RenderBlockFlow& enclosingBlockContainer)
{
    if (enclosingBlockContainer.hasLines()) {
        // With blocks-in-inline, the last formatted line may be a block sitting on the last line.
        auto firstBoxOnLastFormattedLineWithContent = [&]() -> InlineIterator::LeafBoxIterator {
            for (auto lineBox = InlineIterator::lastLineBoxFor(enclosingBlockContainer); lineBox; --lineBox) {
                if (auto box = lineBox->logicalLeftmostLeafBox())
                    return box;
            }
            return { };
        };
        if (auto box = firstBoxOnLastFormattedLineWithContent(); box && box->isBlockLevelBox()) {
            ASSERT(box->renderer().settings().blocksInInlineLayoutEnabled());
            ASSERT(is<RenderBlockFlow>(box->renderer()));
            if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(box->renderer())) {
                if (auto* candidate = lastFormattedLineRoot(*blockFlow))
                    return candidate;
            }
        }
    }

    for (auto* child = enclosingBlockContainer.lastChild(); child; child = child->previousSibling()) {
        CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(*child);
        if (!blockContainer || blockContainer->createsNewFormattingContext() || blockContainer->isFirstLetter())
            continue;
        if (blockContainer->hasLines()) {
            if (auto* candidate = lastFormattedLineRoot(*blockContainer))
                return candidate;
            return blockContainer.unsafeGet();
        }
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
    adjustTextBoxTrimStatusBeforeLayout({ });
}

TextBoxTrimmer::TextBoxTrimmer(const RenderBlockFlow& blockContainer, const RenderBlockFlow& lastFormattedLineRoot)
    : m_blockContainer(blockContainer)
{
    adjustTextBoxTrimStatusBeforeLayout(&lastFormattedLineRoot);
}

TextBoxTrimmer::~TextBoxTrimmer()
{
    adjustTextBoxTrimStatusAfterLayout();
}

RenderBlockFlow* TextBoxTrimmer::lastInlineFormattingContextRootForTrimEnd(const RenderBlockFlow& blockContainer)
{
    auto textBoxTrimValue = textBoxTrim(blockContainer);
    auto hasTextBoxTrimEnd = textBoxTrimValue == TextBoxTrim::TrimEnd || textBoxTrimValue == TextBoxTrim::TrimBoth;
    if (!hasTextBoxTrimEnd)
        return { };
    auto* candidateForLastBlockContainer = lastFormattedLineRoot(blockContainer);
    if (!candidateForLastBlockContainer || candidateForLastBlockContainer == &blockContainer)
        return { };
    // If the nested (last) block container has border/padding end, trimming should not happen.
    return !candidateForLastBlockContainer->borderAndPaddingEnd() ? candidateForLastBlockContainer : nullptr;
}

void TextBoxTrimmer::adjustTextBoxTrimStatusBeforeLayout(const RenderBlockFlow* lastFormattedLineRoot)
{
    auto textBoxTrimValue = textBoxTrim(*m_blockContainer);
    if (textBoxTrimValue == TextBoxTrim::None)
        return handleTextBoxTrimNoneBeforeLayout();

    auto& layoutContext = m_blockContainer->view().frameView().layoutContext();
    // This block container starts setting up trimming for its subtree.
    // 1. Let's save the current trimming status, merge (and restore after layout).
    // 2. Figure out which side(s) of the content is going to get trimmed.
    m_previousTextBoxTrimStatus = layoutContext.textBoxTrim();
    m_shouldRestoreTextBoxTrimStatus = true;

    auto shouldTrimFirstFormattedLineStart = (textBoxTrimValue == TextBoxTrim::TrimStart || textBoxTrimValue == TextBoxTrim::TrimBoth) || (m_previousTextBoxTrimStatus && m_previousTextBoxTrimStatus->trimFirstFormattedLine);
    auto shouldTrimmingLastFormattedLineEnd = textBoxTrimValue == TextBoxTrim::TrimEnd || textBoxTrimValue == TextBoxTrim::TrimBoth;

    if (shouldTrimmingLastFormattedLineEnd) {
        if (!lastFormattedLineRoot && m_blockContainer->childrenInline()) {
            // Last line end trimming is explicitly set on this inline formatting context. Let's assume last line is part of this block container.
            lastFormattedLineRoot = m_blockContainer.get();
        } else if (lastFormattedLineRoot) {
            // This is the dedicated "last line" layout on the last inline formatting context, where we should not trim the first line
            // unless this IFC includes it too.
            if (shouldTrimFirstFormattedLineStart)
                shouldTrimFirstFormattedLineStart = firstFormattedLineRoot(*m_blockContainer) == lastFormattedLineRoot;
        }
    }
    if (!lastFormattedLineRoot && m_previousTextBoxTrimStatus)
        lastFormattedLineRoot =  m_previousTextBoxTrimStatus->lastFormattedLineRoot.get();

    layoutContext.setTextBoxTrim(LocalFrameViewLayoutContext::TextBoxTrim { shouldTrimFirstFormattedLineStart, lastFormattedLineRoot });
}

void TextBoxTrimmer::adjustTextBoxTrimStatusAfterLayout()
{
    auto& layoutContext = m_blockContainer->view().frameView().layoutContext();
    if (m_shouldRestoreTextBoxTrimStatus)
        return layoutContext.setTextBoxTrim(m_previousTextBoxTrimStatus);

    if (auto textBoxTrim = layoutContext.textBoxTrim(); textBoxTrim && textBoxTrim->trimFirstFormattedLine) {
        // Only the first formatted line is trimmed.
        if (!shouldIgnoreAsFirstLastFormattedLineContainer(*m_blockContainer))
            removeTextBoxTrimStart(layoutContext);
    }
}

void TextBoxTrimmer::handleTextBoxTrimNoneBeforeLayout()
{
    auto& layoutContext = m_blockContainer->view().frameView().layoutContext();
    // This is when the block container does not have text-box-trim set.
    // 1. trimming from ancestors does not get propagated into formatting contexts e.g inside inline-block.
    // 2. border and padding (start) prevent trim start.
    if (m_blockContainer->createsNewFormattingContext()) {
        m_previousTextBoxTrimStatus = layoutContext.textBoxTrim();
        m_shouldRestoreTextBoxTrimStatus = true;
        // Run layout on this subtree with no text-box-trim.
        layoutContext.setTextBoxTrim({ });
        return;
    }

    auto hasTextBoxTrimStart = layoutContext.textBoxTrim() && layoutContext.textBoxTrim()->trimFirstFormattedLine;
    if (hasTextBoxTrimStart && m_blockContainer->borderAndPaddingStart()) {
        // We've got this far with no start trimming and now border/padding prevent trimming.
        removeTextBoxTrimStart(layoutContext);
    }
}

TextBoxTrimStartDisabler::TextBoxTrimStartDisabler(const RenderBox& renderBox)
    : m_renderBox(renderBox)
{
    auto& layoutContext = m_renderBox->view().frameView().layoutContext();
    m_previousTextBoxTrimStatus = layoutContext.textBoxTrim();
    if (m_previousTextBoxTrimStatus)
        layoutContext.setTextBoxTrim(LocalFrameViewLayoutContext::TextBoxTrim { false, m_previousTextBoxTrimStatus->lastFormattedLineRoot });
}

TextBoxTrimStartDisabler::~TextBoxTrimStartDisabler()
{
    if (!m_renderBox) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_renderBox->view().frameView().layoutContext().setTextBoxTrim(m_previousTextBoxTrimStatus);
}

} // namespace WebCore
