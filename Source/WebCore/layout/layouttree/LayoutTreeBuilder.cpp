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

#include "LayoutBlockContainer.h"
#include "LayoutChildIterator.h"
#include "LayoutContainer.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "RenderBlock.h"
#include "RenderChildIterator.h"
#include "RenderElement.h"
#include "RenderInline.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

std::unique_ptr<Container> TreeBuilder::createLayoutTree(const RenderView& renderView)
{
    std::unique_ptr<Container> initialContainingBlock(new BlockContainer(RenderStyle::clone(renderView.style())));
    TreeBuilder::createSubTree(renderView, *initialContainingBlock);
    return initialContainingBlock;
}

void TreeBuilder::createSubTree(const RenderElement& rootRenderer, Container& rootContainer)
{
    // Skip RenderText (and some others) for now.
    for (auto& child : childrenOfType<RenderElement>(rootRenderer)) {
        Box* box = nullptr;
        if (is<RenderBlock>(child)) {
            box = new BlockContainer(RenderStyle::clone(child.style()));
            createSubTree(child, downcast<Container>(*box));
        } else if (is<RenderInline>(child)) {
            box = new InlineContainer(RenderStyle::clone(child.style()));
            createSubTree(child, downcast<Container>(*box));
        } else
            ASSERT_NOT_REACHED();

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
    }
}

#if ENABLE(TREE_DEBUGGING)
static void outputLayoutBox(TextStream& stream, const Box& layoutBox, unsigned depth)
{
    unsigned printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";

    if (is<InlineContainer>(layoutBox))
        stream << "inline container";
    else if (is<InlineBox>(layoutBox))
        stream << "inline box";
    else if (is<BlockContainer>(layoutBox)) {
        if (!layoutBox.parent())
            stream << "initial ";
        stream << "block container";
    } else
        stream << "box";
    stream << " at [0 0] size [0 0]";
    stream << " object [" << &layoutBox << "]";

    stream.nextLine();
}

static void outputLayoutTree(TextStream& stream, const Container& rootContainer, unsigned depth)
{
    for (auto& child : childrenOfType<Box>(rootContainer)) {
        outputLayoutBox(stream, child, depth);
        if (is<Container>(child))
            outputLayoutTree(stream, downcast<Container>(child), depth + 1);
    }
}

void TreeBuilder::showLayoutTree(const Container& layoutBox)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputLayoutBox(stream, layoutBox, 0);
    outputLayoutTree(stream, layoutBox, 1);
    WTFLogAlways("%s", stream.release().utf8().data());
}

void printLayoutTreeForLiveDocuments()
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView())
            continue;
        if (document->frame() && document->frame()->isMainFrame())
            fprintf(stderr, "----------------------main frame--------------------------\n");
        fprintf(stderr, "%s\n", document->url().string().utf8().data());
        Layout::TreeBuilder::showLayoutTree(*TreeBuilder::createLayoutTree(*document->renderView()));
    }
}
#endif

}
}

#endif
