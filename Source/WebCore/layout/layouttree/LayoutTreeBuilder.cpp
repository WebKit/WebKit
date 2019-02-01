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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "InlineFormattingState.h"
#include "LayoutBlockContainer.h"
#include "LayoutBox.h"
#include "LayoutChildIterator.h"
#include "LayoutContainer.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "LayoutState.h"
#include "RenderBlock.h"
#include "RenderChildIterator.h"
#include "RenderElement.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

std::unique_ptr<Container> TreeBuilder::createLayoutTree(const RenderView& renderView)
{
    std::unique_ptr<Container> initialContainingBlock(new BlockContainer(WTF::nullopt, RenderStyle::clone(renderView.style())));
    TreeBuilder::createSubTree(renderView, *initialContainingBlock);
    return initialContainingBlock;
}

void TreeBuilder::createSubTree(const RenderElement& rootRenderer, Container& rootContainer)
{
    auto elementAttributes = [] (const RenderElement& renderer) -> Optional<Box::ElementAttributes> {
        if (renderer.isDocumentElementRenderer())
            return Box::ElementAttributes { Box::ElementType::Document };
        if (auto* element = renderer.element()) {
            if (element->hasTagName(HTMLNames::bodyTag))
                return Box::ElementAttributes { Box::ElementType::Body };
            if (element->hasTagName(HTMLNames::colTag))
                return Box::ElementAttributes { Box::ElementType::TableColumn };
            if (element->hasTagName(HTMLNames::trTag))
                return Box::ElementAttributes { Box::ElementType::TableRow };
            if (element->hasTagName(HTMLNames::colgroupTag))
                return Box::ElementAttributes { Box::ElementType::TableColumnGroup };
            if (element->hasTagName(HTMLNames::tbodyTag))
                return Box::ElementAttributes { Box::ElementType::TableRowGroup };
            if (element->hasTagName(HTMLNames::theadTag))
                return Box::ElementAttributes { Box::ElementType::TableHeaderGroup };
            if (element->hasTagName(HTMLNames::tfootTag))
                return Box::ElementAttributes { Box::ElementType::TableFooterGroup };
            if (element->hasTagName(HTMLNames::tfootTag))
                return Box::ElementAttributes { Box::ElementType::TableFooterGroup };
            if (element->hasTagName(HTMLNames::imgTag))
                return Box::ElementAttributes { Box::ElementType::Image };
            if (element->hasTagName(HTMLNames::iframeTag))
                return Box::ElementAttributes { Box::ElementType::IFrame };
            return Box::ElementAttributes { Box::ElementType::GenericElement };
        }
        return WTF::nullopt;
    };

    for (auto& child : childrenOfType<RenderObject>(rootRenderer)) {
        std::unique_ptr<Box> box;

        if (is<RenderText>(child)) {
            box = std::make_unique<InlineBox>(Optional<Box::ElementAttributes>(), RenderStyle::createAnonymousStyleWithDisplay(rootRenderer.style(), DisplayType::Inline));
            downcast<InlineBox>(*box).setTextContent(downcast<RenderText>(child).originalText());
        } else if (is<RenderReplaced>(child)) {
            auto& renderer = downcast<RenderReplaced>(child);
            auto display = renderer.style().display();
            if (display == DisplayType::Block)
                box = std::make_unique<Box>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
            else
                box = std::make_unique<InlineBox>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
            // FIXME: We don't yet support all replaced elements.
            if (!renderer.intrinsicSize().isEmpty() && box->replaced())
                box->replaced()->setIntrinsicSize(renderer.intrinsicSize());
            if (is<RenderImage>(renderer)) {
                auto& imageRenderer = downcast<RenderImage>(renderer);
                if (imageRenderer.imageResource().errorOccurred())
                    box->replaced()->setIntrinsicRatio(1);
            }
        } else if (is<RenderElement>(child)) {
            auto& renderer = downcast<RenderElement>(child);
            auto display = renderer.style().display();
            if (display == DisplayType::Block)
                box = std::make_unique<BlockContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
            else if (display == DisplayType::Inline)
                box = std::make_unique<InlineContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
            else if (display == DisplayType::InlineBlock)
                box = std::make_unique<InlineContainer>(elementAttributes(renderer), RenderStyle::clone(renderer.style()));
            else {
                ASSERT_NOT_IMPLEMENTED_YET();
                continue;
            }
        } else {
            ASSERT_NOT_IMPLEMENTED_YET();
            continue;
        }

        if (!rootContainer.hasChild()) {
            rootContainer.setFirstChild(*box);
            rootContainer.setLastChild(*box);
        } else {
            auto* lastChild = const_cast<Box*>(rootContainer.lastChild());
            box->setPreviousSibling(*lastChild);
            lastChild->setNextSibling(*box);
            rootContainer.setLastChild(*box);
        }

        box->setParent(rootContainer);

        if (box->isOutOfFlowPositioned()) {
            // Not efficient, but this is temporary anyway.
            // Collect the out-of-flow descendants at the formatting root level (as opposed to at the containing block level, though they might be the same).
            const_cast<Container&>(box->formattingContextRoot()).addOutOfFlowDescendant(*box);
        }
        if (is<Container>(*box))
            createSubTree(downcast<RenderElement>(child), downcast<Container>(*box));
        // Temporary
        box.release();
    }
}

#if ENABLE(TREE_DEBUGGING)
static void outputInlineRuns(TextStream& stream, const LayoutState& layoutState, const Container& inlineFormattingRoot, unsigned depth)
{
    auto& inlineFormattingState = layoutState.establishedFormattingState(inlineFormattingRoot);
    ASSERT(is<InlineFormattingState>(inlineFormattingState));
    auto& inlineRuns = downcast<InlineFormattingState>(inlineFormattingState).inlineRuns();

    for (auto& inlineRun : inlineRuns) {
        unsigned printedCharacters = 0;
        while (++printedCharacters <= depth * 2)
            stream << " ";
        stream << "run";
        if (inlineRun.textContext())
            stream << "(" << inlineRun.textContext()->start() << ", " << inlineRun.textContext()->start() + inlineRun.textContext()->length() << ") ";
        else
            stream << "(x, x) ";
        stream << "at [" << inlineRun.logicalLeft() << ", " << inlineRun.logicalTop() << "] size [" << inlineRun.logicalWidth() << " " << inlineRun.logicalHeight() << "]";
        stream.nextLine();
    }
}

static void outputLayoutBox(TextStream& stream, const Box& layoutBox, const Display::Box* displayBox, unsigned depth)
{
    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (is<InlineContainer>(layoutBox))
        stream << "inline container";
    else if (is<InlineBox>(layoutBox)) {
        if (layoutBox.replaced())
            stream << "inline replaced box";
        else if (downcast<InlineBox>(layoutBox).hasTextContent()) {
            auto textContent = downcast<InlineBox>(layoutBox).textContent();
            stream << "inline text [\"" << textContent.utf8().data() << "\"]";
        } else
            stream << "inline box";
    } else if (is<BlockContainer>(layoutBox)) {
        if (!layoutBox.parent())
            stream << "initial ";
        stream << "block container";
    } else if (layoutBox.isBlockLevelBox())
        stream << "block box";
    else
        stream << "box";
    // FIXME: Inline text runs don't create display boxes yet.
    if (displayBox) {
        // FIXME: display box is not completely set yet.
        if ((is<InlineBox>(layoutBox) || is<InlineContainer>(layoutBox)) && !layoutBox.isFloatingPositioned())
            stream << " at [" << "." << " " << "." << "] size [" << displayBox->width() << " " << displayBox->height() << "]";
        else
            stream << " at [" << displayBox->left() << " " << displayBox->top() << "] size [" << displayBox->width() << " " << displayBox->height() << "]";
    }
    stream << " [" << &layoutBox << "]";

    stream.nextLine();
}

static void outputLayoutTree(const LayoutState* layoutState, TextStream& stream, const Container& rootContainer, unsigned depth)
{
    for (auto& child : childrenOfType<Box>(rootContainer)) {
        Display::Box* displayBox = nullptr;
        // Not all boxes generate display boxes.
        if (layoutState && layoutState->hasDisplayBox(child))
            displayBox = &layoutState->displayBoxForLayoutBox(child);

        outputLayoutBox(stream, child, displayBox, depth);
        if (layoutState && child.establishesInlineFormattingContext())
            outputInlineRuns(stream, *layoutState, downcast<Container>(child), depth + 1);

        if (is<Container>(child))
            outputLayoutTree(layoutState, stream, downcast<Container>(child), depth + 1);
    }
}

void showLayoutTree(const Box& layoutBox, const LayoutState* layoutState)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    auto& initialContainingBlock = layoutBox.initialContainingBlock();
    outputLayoutBox(stream, initialContainingBlock, layoutState ? &layoutState->displayBoxForLayoutBox(initialContainingBlock) : nullptr, 0);
    outputLayoutTree(layoutState, stream, initialContainingBlock, 1);
    WTFLogAlways("%s", stream.release().utf8().data());
}

void showLayoutTree(const Box& layoutBox)
{
    showLayoutTree(layoutBox, nullptr);
}

void printLayoutTreeForLiveDocuments()
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isMainFrame())
            fprintf(stderr, "----------------------main frame--------------------------\n");
        fprintf(stderr, "%s\n", document->url().string().utf8().data());
        // FIXME: Need to find a way to output geometry without layout context.
        // Layout::TreeBuilder::showLayoutTree(*TreeBuilder::createLayoutTree(*document->renderView()));
    }
}
#endif

}
}

#endif
