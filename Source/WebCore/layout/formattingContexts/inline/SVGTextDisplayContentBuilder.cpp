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
#include "SVGTextDisplayContentBuilder.h"

#include "FontCascadeInlines.h"
#include "InlineDisplayBoxInlines.h"
#include "LayoutElementBox.h"
#include "LayoutInlineTextBox.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

namespace {

struct SVGTextRun {
    enum class TrailingWhitespace : uint8_t { None, Collapsible, Collapsed, Preserved };

    CheckedRef<const Box> layoutBox;
    size_t parentIndex { 0 };
    size_t start { 0 };
    size_t length { 0 };
    TrailingWhitespace trailingWhitespace { TrailingWhitespace::None };
    bool isWordSeparator { false };
    bool hasContent { false };

    bool LEGACY_NODELETE isText() const { return is<InlineTextBox>(layoutBox.get()); }
    bool LEGACY_NODELETE hasCollapsibleTrailingWhitespace() const { return trailingWhitespace == TrailingWhitespace::Collapsible || trailingWhitespace == TrailingWhitespace::Collapsed; }
};

}

static bool hasInlineDirectionMarginBorderOrPadding(const Style::ComputedStyle& style)
{
    // Percentages and calc() may resolve to zero but are treated as non-zero. Auto margins are zero on inline boxes.
    auto isZeroMargin = [](auto& margin) {
        return margin.isAuto() || margin.isKnownZero();
    };
    if (!isZeroMargin(style.marginStart()) || !isZeroMargin(style.marginEnd()))
        return true;
    if (!style.paddingStart().isKnownZero() || !style.paddingEnd().isKnownZero())
        return true;
    auto borderWidths = Style::effectiveBorderWidths(style);
    return !borderWidths.start(style.writingMode()).isZero() || !borderWidths.end(style.writingMode()).isZero();
}

// Produces the same boxes as InlineFormattingContext for single line, non-wrapping, left-to-right content, including
// the whitespace collapsing and trimming of Line::appendText and Line::handleTrailingTrimmableContent.
// Returns nothing for content that needs the full inline layout.
std::optional<InlineDisplay::Content> buildSVGTextDisplayContent(const ElementBox& root)
{
    auto isSupportedElementBox = [](const Box& box) {
        CheckedRef style = box.style();
        if (style->writingMode().isBidiRTL() || style->unicodeBidi() != UnicodeBidi::Normal)
            return false;
        if (!style->whiteSpaceTrim().isNone() || !style->textAutospace().isNoAutospace())
            return false;
        // ::first-line applies to <text> and <tspan> with block-level display.
        return &box.firstLineStyle() == style.ptr();
    };
    auto isSupportedInlineBox = [&](const ElementBox& inlineBox) {
        // SVG text content elements only create inline boxes (RenderSVGInline) and text.
        ASSERT(inlineBox.isInlineBox() && !inlineBox.isLineBreakBox());
        // Float on a tspan.
        if (!inlineBox.isInFlow())
            return false;
        // These make the inline box contentful (see Line::Run::isContentfulOrHasDecoration).
        if (hasInlineDirectionMarginBorderOrPadding(protect(inlineBox.style())))
            return false;
        return isSupportedElementBox(inlineBox);
    };

    if (!isSupportedElementBox(root))
        return { };

    using TrailingWhitespace = SVGTextRun::TrailingWhitespace;
    Vector<SVGTextRun, 8> runs;
    runs.append({ root });

    std::optional<size_t> lastTextRunIndex;

    auto appendText = [&](const InlineTextBox& inlineTextBox, size_t parentIndex) -> bool {
        // Text has the first line style of its parent, which is checked before. SVG text nodes are never combined.
        ASSERT(&inlineTextBox.firstLineStyle() == &inlineTextBox.style() && !inlineTextBox.isCombined());
        if (inlineTextBox.hasStrongDirectionalityContent())
            return false;

        CheckedRef style = inlineTextBox.style();
        auto shouldPreserveWhitespace = [&] -> std::optional<bool> {
            switch (style->whiteSpaceCollapse()) {
            case WhiteSpaceCollapse::Collapse:
                return false;
            case WhiteSpaceCollapse::Preserve:
                return true;
            default:
                return { };
            }
        }();
        if (!shouldPreserveWhitespace || style->nbspMode() != NBSPMode::Normal)
            return false;
        auto hasWordSpacing = !!style->fontCascade().wordSpacing();

        // The run of this text box that the next item may extend.
        std::optional<size_t> currentRunIndex;
        auto appendItem = [&](size_t start, size_t length, TrailingWhitespace trailingWhitespace) {
            auto isWhitespace = trailingWhitespace != TrailingWhitespace::None;
            auto canExtendCurrentRun = currentRunIndex && runs[*currentRunIndex].trailingWhitespace != TrailingWhitespace::Collapsed && !(hasWordSpacing && isWhitespace);
            if (canExtendCurrentRun) {
                auto& run = runs[*currentRunIndex];
                run.length += length;
                run.trailingWhitespace = trailingWhitespace;
            } else {
                runs.append({ inlineTextBox, parentIndex, start, length, trailingWhitespace, isWhitespace });
                currentRunIndex = runs.size() - 1;
            }
            lastTextRunIndex = currentRunIndex;
        };

        auto& content = inlineTextBox.content();
        auto contentLength = content.length();
        // RenderSVGInlineText applies the SVG whitespace rules on creation but not on text or text-transform changes.
        auto isCollapsibleWhitespace = [&](auto character) {
            return character == space || character == tabCharacter || character == newlineCharacter;
        };
        auto isUnsupportedCharacter = [&](auto character) {
            return character == lineSeparator || character == paragraphSeparator || character == zeroWidthSpace;
        };

        size_t position = 0;
        while (position < contentLength) {
            auto end = position;
            if (*shouldPreserveWhitespace) {
                if (content[position] == space) {
                    while (end < contentLength && content[end] == space)
                        ++end;
                    appendItem(position, end - position, TrailingWhitespace::Preserved);
                    position = end;
                    continue;
                }
                // Preserved tabs have position dependent widths and preserved newlines are forced line breaks.
                for (; end < contentLength && content[end] != space; ++end) {
                    if (content[end] == tabCharacter || content[end] == newlineCharacter || isUnsupportedCharacter(content[end]))
                        return false;
                }
                appendItem(position, end - position, TrailingWhitespace::None);
                position = end;
                continue;
            }

            if (isCollapsibleWhitespace(content[position])) {
                while (end < contentLength && isCollapsibleWhitespace(content[end]))
                    ++end;
                auto collapsesCompletely = !lastTextRunIndex || runs[*lastTextRunIndex].hasCollapsibleTrailingWhitespace();
                if (!collapsesCompletely)
                    appendItem(position, 1, end - position == 1 ? TrailingWhitespace::Collapsible : TrailingWhitespace::Collapsed);
                position = end;
                continue;
            }
            for (; end < contentLength && !isCollapsibleWhitespace(content[end]); ++end) {
                if (isUnsupportedCharacter(content[end]))
                    return false;
            }
            appendItem(position, end - position, TrailingWhitespace::None);
            position = end;
        }
        return true;
    };

    Vector<size_t, 4> inlineBoxRunIndexStack;
    auto parentIndex = [&] {
        return inlineBoxRunIndexStack.isEmpty() ? 0 : inlineBoxRunIndexStack.last();
    };
    for (CheckedPtr layoutBox = root.firstChild(); layoutBox;) {
        if (CheckedPtr inlineTextBox = dynamicDowncast<InlineTextBox>(*layoutBox)) {
            if (!appendText(*inlineTextBox, parentIndex()))
                return { };
        } else {
            CheckedRef inlineBox = downcast<ElementBox>(*layoutBox);
            if (!isSupportedInlineBox(inlineBox.get()))
                return { };
            runs.append({ inlineBox.get() });
            if (CheckedPtr firstChild = inlineBox->firstChild()) {
                inlineBoxRunIndexStack.append(runs.size() - 1);
                layoutBox = firstChild;
                continue;
            }
        }
        while (!layoutBox->nextSibling()) {
            layoutBox = &layoutBox->parent();
            if (layoutBox.get() == &root)
                break;
            inlineBoxRunIndexStack.removeLast();
        }
        layoutBox = layoutBox.get() == &root ? nullptr : layoutBox->nextSibling();
    }

    if (lastTextRunIndex && runs[*lastTextRunIndex].hasCollapsibleTrailingWhitespace())
        --runs[*lastTextRunIndex].length;

    auto hasContentfulText = false;
    for (auto& run : runs) {
        if (!run.isText() || !run.length)
            continue;
        runs[run.parentIndex].hasContent = true;
        hasContentfulText = true;
    }

    auto content = InlineDisplay::Content { };
    auto& boxes = content.boxes;
    boxes.reserveInitialCapacity(runs.size());

    using PositionWithinInlineLevelBox = InlineDisplay::Box::PositionWithinInlineLevelBox;
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& run = runs[index];
        if (!index) {
            boxes.append({ 0, InlineDisplay::Box::Type::RootInlineBox, root, UBIDI_DEFAULT_LTR, { }, { }, hasContentfulText, { }, { }, run.hasContent, false });
            boxes.last().setIsFirstForLayoutBox();
            boxes.last().setIsLastForLayoutBox();
            continue;
        }
        if (!run.isText()) {
            boxes.append({ 0, InlineDisplay::Box::Type::NonRootInlineBox, run.layoutBox.get(), UBIDI_DEFAULT_LTR, { }, { }, hasContentfulText, { }, { }, run.hasContent, false, { PositionWithinInlineLevelBox::First, PositionWithinInlineLevelBox::Last } });
            continue;
        }
        if (!run.length)
            continue;
        CheckedRef inlineTextBox = downcast<InlineTextBox>(run.layoutBox.get());
        auto type = run.isWordSeparator ? InlineDisplay::Box::Type::WordSeparator : InlineDisplay::Box::Type::Text;
        boxes.append({ 0, type, inlineTextBox.get(), UBIDI_DEFAULT_LTR, { }, { }, hasContentfulText, { }, InlineDisplay::Box::Text { run.start, run.length, inlineTextBox->content() }, true, false });
    }

    for (size_t index = 1; index < boxes.size(); ++index) {
        auto& box = boxes[index];
        if (!box.isText())
            continue;
        if (&boxes[index - 1].layoutBox() != &box.layoutBox())
            box.setIsFirstForLayoutBox();
        if (index + 1 == boxes.size() || &boxes[index + 1].layoutBox() != &box.layoutBox())
            box.setIsLastForLayoutBox();
    }

    CheckedRef rootStyle = root.style();
    auto hasInflowBox = boxes.size() > 1;
    auto baseline = rootStyle->metricsOfPrimaryFont().ascent(FontBaseline::Alphabetic);
    auto line = InlineDisplay::Line { hasInflowBox, hasContentfulText, false, { }, { }, { }, { }, baseline, FontBaseline::Alphabetic, 0, 0, 0, true, rootStyle->writingMode().isHorizontal(), false };
    line.setFirstBoxIndex(0);
    line.setBoxCount(boxes.size());
    content.lines.append(WTF::move(line));

    return content;
}

}
}
