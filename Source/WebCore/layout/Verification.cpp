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
#include "LayoutContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "RenderBox.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

static void outputMismatchingBoxInformationIfNeeded(TextStream& stream, const LayoutContext& context, const RenderBox& renderer, const Box& layoutBox)
{
    bool firstMismatchingRect = true;
    auto outputRect = [&] (const String& prefix, const LayoutRect& rendererRect, const LayoutRect& layoutRect) {
        if (firstMismatchingRect) {
            stream << (renderer.element() ? renderer.element()->nodeName().utf8().data() : "") << " " << renderer.renderName() << "(" << &renderer << ") layoutBox(" << &layoutBox << ")";
            stream.nextLine();
            firstMismatchingRect = false;
        }

        stream  << prefix.utf8().data() << "\trenderer->(" << rendererRect.x() << "," << rendererRect.y() << ") (" << rendererRect.width() << "x" << rendererRect.height() << ")"
            << "\tlayout->(" << layoutRect.x() << "," << layoutRect.y() << ") (" << layoutRect.width() << "x" << layoutRect.height() << ")"; 
        stream.nextLine();
    };

    auto* displayBox = context.displayBoxForLayoutBox(layoutBox);
    ASSERT(displayBox);

    if (renderer.marginBoxRect() != displayBox->marginBox())
        outputRect("frameBox", renderer.frameRect(), displayBox->rect());

    if (renderer.marginBoxRect() != displayBox->marginBox())
        outputRect("marginBox", renderer.marginBoxRect(), displayBox->marginBox());

    if (renderer.borderBoxRect() != displayBox->borderBox())
        outputRect("borderBox", renderer.borderBoxRect(), displayBox->borderBox());

    if (renderer.paddingBoxRect() != displayBox->paddingBox())
        outputRect("paddingBox", renderer.paddingBoxRect(), displayBox->paddingBox());

    if (renderer.marginBoxRect() != displayBox->contentBox())
        outputRect("contentBox", renderer.contentBoxRect(), displayBox->contentBox());

    stream.nextLine();
}

static void verifyAndOutputSubtree(TextStream& stream, const LayoutContext& context, const RenderBox& renderer, const Box& layoutBox)
{
    outputMismatchingBoxInformationIfNeeded(stream, context, renderer, layoutBox);

    if (!is<Container>(layoutBox))
        return;

    auto& container = downcast<Container>(layoutBox);
    auto* childBox = container.firstChild();
    auto* childRenderer = renderer.firstChild();

    while (childRenderer) {
        if (!is<RenderBox>(*childRenderer)) {
            childRenderer = childRenderer->nextSibling();
            continue;
        }

        verifyAndOutputSubtree(stream, context, downcast<RenderBox>(*childRenderer), *childBox);
        childBox = childBox->nextSibling();
        childRenderer = childRenderer->nextSibling();
    }
}

void LayoutContext::verifyAndOutputMismatchingLayoutTree(const RenderView& renderView) const
{
    TextStream stream;
#if ENABLE(TREE_DEBUGGING)
    showRenderTree(&renderView);
#endif
    verifyAndOutputSubtree(stream, *this, renderView, *m_root.get());
    WTFLogAlways("%s", stream.release().utf8().data());
}

}
}

#endif
