/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "InlineIteratorTextBox.h"

#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBoxInlines.h"
#include "RenderCombineText.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleTabSize.h"

namespace WebCore {
namespace InlineIterator {

TextRun TextBox::textRun(TextRunMode mode) const
{
    CheckedRef style = displayBox().style();
    auto expansion = displayBox().expansion();
    auto logicalLeft = [&] {
        // Offset from the formatting context's content box, with the text-align offset removed.
        // text-align shifts the root inline box within the line (contentLogicalLeft), but tab stops
        // are measured from the containing block's content box edge regardless of alignment.
        //
        //   containing block's content box
        //   |                                                  |
        //   |               text-align: center                 |
        //   |               |<- content ->|                    |
        //   |               |X         X  |                    |
        //   |                                                  |
        //   logicalLeft = 0 (not the visual position)
        CheckedRef root = formattingContextRoot();
        auto positionRelativeToContentBox = [&] {
            if (style->writingMode().isBidiLTR())
                return visualRectIgnoringBlockDirection().x() - root->borderAndPaddingStart();
            return root->contentBoxWidth() - visualRectIgnoringBlockDirection().maxX();
        }();
        return positionRelativeToContentBox - line().contentLogicalLeft();
    };
    auto characterScanForCodePath = isText() && !renderer().canUseSimpleFontCodePath();
    auto textRun = TextRun { mode == TextRunMode::Editing ? originalText() : displayBox().text().renderedContent(), logicalLeft(), expansion.horizontalExpansion, expansion.behavior, direction(), style->rtlOrdering() == Order::Visual, characterScanForCodePath };
    textRun.setTabSize(!style->collapseWhiteSpace(), Style::toPlatform(style->tabSize(), style->usedZoomForLength()));
    return textRun;
}

TextBoxIterator TextBox::nextTextBox() const
{
    return TextBoxIterator(*this).traverseNextTextBox();
}

const FontCascade& TextBox::fontCascade() const
{
    if (CheckedPtr renderer = dynamicDowncast<RenderCombineText>(this->renderer()); renderer && renderer->isCombined())
        return renderer->textCombineFont();

    return style()->fontCascade();
}

TextBoxIterator::TextBoxIterator(const Box& box)
    : LeafBoxIterator(box)
{
}

TextBoxIterator& TextBoxIterator::traverseNextTextBox()
{
    m_box.traverseNextTextBox();
    return *this;
}

TextBoxIterator lineLeftmostTextBoxFor(const RenderText& text)
{
    if (CheckedPtr lineLayout = LayoutIntegration::LineLayout::containing(text))
        return lineLayout->textBoxesFor(text);

    // During teardown we may hit this codepath _after_ the display content is destroyed (e.g. calling repaint on RenderText).
    return { };
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, const InlineDisplay::Box& box)
{
    return textBoxFor(content, content.indexForBox(box));
}

TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent& content, size_t boxIndex)
{
    ASSERT(content.displayContent().boxes[boxIndex].isTextOrSoftLineBreak());
    return { Box { content, boxIndex } };
}

BoxRange<TextBoxIterator> textBoxesFor(const RenderText& text)
{
    return { lineLeftmostTextBoxFor(text) };
}

}
}
