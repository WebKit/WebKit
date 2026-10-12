/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "LayoutTreeBuilder.h"

#include "InlineDisplayContent.h"
#include "LayoutBoxGeometry.h"
#include "LayoutBoxInlines.h"
#include "LayoutChildIterator.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutState.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

#if ENABLE(TREE_DEBUGGING)
void showInlineTreeAndRuns(TextStream& stream, const LayoutState& layoutState, const ElementBox& inlineFormattingRoot, size_t depth)
{
    UNUSED_PARAM(layoutState);
    UNUSED_PARAM(inlineFormattingRoot);
    // FIXME: Populate inline display content.
    auto lines = InlineDisplay::Lines { };
    auto boxes = InlineDisplay::Boxes { };

    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        auto addSpacing = [&] {
            size_t printedCharacters = 0;
            stream << "-------- --";
            while (++printedCharacters <= depth * 2)
                stream << " ";

        };
        addSpacing();
        auto& line = lines[lineIndex];
        auto& lineBoxRect = line.lineBoxRect();
        stream << "line at (" << lineBoxRect.x() << "," << lineBoxRect.y() << ") size (" << lineBoxRect.width() << "x" << lineBoxRect.height() << ") baseline (" << line.baseline() << ") enclosing top (" << line.enclosingContentLogicalTop() << ") bottom (" << line.enclosingContentLogicalBottom() << ")";
        stream.nextLine();

        addSpacing();
        stream << "  Inline level boxes:";
        stream.nextLine();

        auto outputInlineLevelBox = [&](const auto& inlineLevelBox) {
            addSpacing();
            stream << "    ";
            auto rect = inlineLevelBox.visualRectIgnoringBlockDirection();
            auto& layoutBox = inlineLevelBox.layoutBox();
            if (layoutBox.isAtomicInlineBox())
                stream << "Atomic inline box";
            else if (layoutBox.isLineBreakBox())
                stream << "Line break box";
            else if (layoutBox.isInlineBox())
                stream << "Inline box";
            else
                stream << "Generic inline level box";
            stream
                << " at (" << rect.x() << "," << rect.y() << ")"
                << " size (" << rect.width() << "x" << rect.height() << ")";
            stream.nextLine();
        };
        for (auto& box : boxes) {
            if (box.lineIndex() != lineIndex)
                continue;
            if (!box.layoutBox().isInlineLevelBox())
                continue;
            outputInlineLevelBox(box);
        }

        addSpacing();
        stream << "  Runs:";
        stream.nextLine();
        for (auto& box : boxes) {
            if (box.lineIndex() != lineIndex)
                continue;
            addSpacing();
            stream << "    ";
            if (box.isTextOrSoftLineBreak())
                stream << "text box";
            else
                stream << "box box";
            stream << " at (" << box.left() << "," << box.top() << ") size " << box.width() << "x" << box.height();
            if (box.isTextOrSoftLineBreak())
                stream << " box(" << box.text().start() << ", " << box.text().end() << ")";
            stream.nextLine();
        }

    }
}

static void outputLayoutBox(TextStream& stream, const Box& layoutBox, const BoxGeometry* boxGeometry, unsigned depth)
{
    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (layoutBox.isFloatingPositioned())
        stream << "[float] ";

    if (is<InitialContainingBlock>(layoutBox))
        stream << "Initial containing block";
    else if (layoutBox.isDocumentBox())
        stream << "HTML";
    else if (layoutBox.isBodyBox())
        stream << "BODY";
    else if (layoutBox.isTableWrapperBox())
        stream << "TABLE wrapper box";
    else if (layoutBox.isTableBox())
        stream << "TABLE";
    else if (layoutBox.isTableCaption())
        stream << "CAPTION";
    else if (layoutBox.isTableHeader())
        stream << "THEAD";
    else if (layoutBox.isTableBody())
        stream << "TBODY";
    else if (layoutBox.isTableFooter())
        stream << "TFOOT";
    else if (layoutBox.isTableColumnGroup())
        stream << "COL GROUP";
    else if (layoutBox.isTableColumn())
        stream << "COL";
    else if (layoutBox.isTableCell())
        stream << "TD";
    else if (layoutBox.isTableRow())
        stream << "TR";
    else if (layoutBox.isFlexBox())
        stream << "Flex box";
    else if (layoutBox.isFlexItem())
        stream << "Flex item";
    else if (layoutBox.isInlineLevelBox()) {
        if (layoutBox.isAnonymous())
            stream << "anonymous inline box";
        else if (layoutBox.isInlineBlockBox())
            stream << "inline-block box";
        else if (layoutBox.isLineBreakBox())
            stream << (layoutBox.isWordBreakOpportunity() ? "word break opportunity" : "line break");
        else if (layoutBox.isAtomicInlineBox())
            stream << "atomic inline box";
        else if (layoutBox.isReplacedBox())
            stream << "replaced inline box";
        else if (layoutBox.isInlineBox())
            stream << "inline box";
        else
            stream << "other inline level box";
    } else if (layoutBox.isBlockLevelBox())
        stream << "block box";
    else
        stream << "unknown box";

    if (boxGeometry) {
        auto borderBox = BoxGeometry::borderBoxRect(*boxGeometry);
        stream << " at (" << borderBox.left() << "," << borderBox.top() << ") size " << borderBox.width() << "x" << borderBox.height();
    }
    stream << " (" << &layoutBox << ")";
    if (auto* inlineTextBox = dynamicDowncast<InlineTextBox>(layoutBox)) {
        auto textContent = inlineTextBox->content();
        stream << " length->(" << textContent.length() << ")";

        textContent = makeStringByReplacingAll(textContent, '\\', "\\\\"_s);
        textContent = makeStringByReplacingAll(textContent, '\n', "\\n"_s);

        const size_t maxPrintedLength = 80;
        if (textContent.length() > maxPrintedLength) {
            auto substring = StringView(textContent).left(maxPrintedLength);
            stream << " \"" << substring << "\"...";
        } else
            stream << " \"" << textContent << "\"";
    }
    stream.nextLine();
}

static void outputLayoutTree(const LayoutState* layoutState, TextStream& stream, const ElementBox& rootContainer, unsigned depth)
{
    for (auto& child : childrenOfType<Box>(rootContainer)) {
        if (layoutState) {
            // Not all boxes generate display boxes.
            if (layoutState->hasBoxGeometry(child))
                outputLayoutBox(stream, child, &layoutState->geometryForBox(child), depth);
            else
                outputLayoutBox(stream, child, nullptr, depth);
            if (child.establishesInlineFormattingContext())
                showInlineTreeAndRuns(stream, *layoutState, downcast<ElementBox>(child), depth + 1);
        } else
            outputLayoutBox(stream, child, nullptr, depth);

        if (auto* elementBox = dynamicDowncast<ElementBox>(child))
            outputLayoutTree(layoutState, stream, *elementBox, depth + 1);
    }
}

String layoutTreeAsText(const InitialContainingBlock& initialContainingBlock, const LayoutState* layoutState)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    outputLayoutBox(stream, initialContainingBlock, layoutState ? &layoutState->geometryForBox(initialContainingBlock) : nullptr, 0);
    outputLayoutTree(layoutState, stream, initialContainingBlock, 1);
    
    return stream.release();
}

void showLayoutTree(const InitialContainingBlock& initialContainingBlock, const LayoutState* layoutState)
{
    auto treeAsText = layoutTreeAsText(initialContainingBlock, layoutState);
    SAFE_WTFLOGALWAYS("%s", treeAsText.utf8());
}

void showLayoutTree(const InitialContainingBlock& initialContainingBlock)
{
    showLayoutTree(initialContainingBlock, nullptr);
}
#endif

}
}
