/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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

#include <WebCore/RenderElement.h>

namespace WebCore {

class RenderBlock;
class RenderText;

namespace LayoutIntegration {

#if ENABLE(TREE_DEBUGGING)
class InlineContent;
#endif

// The box tree is the render tree, and the boxes get their data from their renderers. The root state is set and cleared
// by the root's own LineLayout or GridLayout. This clears what a formatting context stored on its boxes.
class BoxTreeUpdater {
public:
    NODELETE BoxTreeUpdater(RenderBlock&);
    NODELETE ~BoxTreeUpdater();

    // Clears what the formatting context stored on its boxes. Text content that waited for the inline items goes in.
    void tearDown();

    // Partial removal in an inline formatting context.
    void remove(RenderObject& child);

    // The renderer is content of a formatting context whose root has a live LineLayout or GridLayout. Constant time, except
    // for content of inline boxes, which walks up to the block container.
    static bool isInLiveFormattingContext(const RenderObject&);
    // The same, or the renderer is such a root.
    static bool isLaidOutByIntegration(const RenderObject&);

    // The text content of the box and the flags derived from it.
    static void updateContent(RenderText&);
    // Simplified measuring also depends on the style.
    static void updateSimplifiedMeasuring(RenderText&);
    // So does the text-combine-upright font. Returns whether the text became combined or stopped being combined.
    static bool updateCombinedText(RenderText&);

    // From the renderer type, which is set up before the layout box.
    static Layout::Box::ElementAttributes elementAttributes(const RenderObject&);
    // Whether the box may have a first-line style. Changes with the style, and for anonymous blocks, with the parent.
    static void updateFirstLineStyleState(RenderElement&);

private:
    Layout::ElementBox& rootLayoutBox();

    RenderBlock& m_rootRenderer;
};

inline bool BoxTreeUpdater::isInLiveFormattingContext(const RenderObject& renderer)
{
    SUPPRESS_UNCHECKED_LOCAL auto* parent = renderer.parent();
    if (!parent)
        return false;
    // These renderers have a parent but are not in its child list.
    auto isOutsideChildList = [&] {
        return renderer.isRenderReplica() || renderer.isRenderScrollbarPart() || Layout::isExcludedFromLayoutBoxNavigation(renderer);
    };
    if (!parent->childrenInline()) [[likely]]
        return parent->layoutBox().isGridIntegrationRoot() && !isOutsideChildList();
    if (isOutsideChildList()) [[unlikely]]
        return false;
    while (parent->isInlineBox()) {
        parent = parent->parent();
        if (!parent)
            return false;
    }
    return parent->layoutBox().isInlineIntegrationRoot();
}

inline bool BoxTreeUpdater::isLaidOutByIntegration(const RenderObject& renderer)
{
    return renderer.layoutBox().isIntegrationRoot() || isInLiveFormattingContext(renderer);
}

#if ENABLE(TREE_DEBUGGING)
void showInlineContent(TextStream&, const InlineContent&, size_t depth, bool isDamaged = false);
#endif
}
}

