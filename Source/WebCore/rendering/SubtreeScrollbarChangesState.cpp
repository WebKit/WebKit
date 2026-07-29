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
#include "SubtreeScrollbarChangesState.h"

#include "HTMLTextAreaElement.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderBlock.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include <wtf/Scope.h>

namespace WebCore {

bool SubtreeScrollbarChangesState::isEligibleForScrollbarHandlingByAncestor(const RenderBlock& renderer)
{
    // Textareas have some behavior related to how scrollbars are incorporated into their sizing that needs some investigating.
    return !is<HTMLTextAreaElement>(renderer.element());
}

EnumSet<LogicalBoxAxis> SubtreeScrollbarChangesState::sizesAffectedForSubtreeRootFromScrollbarChanges(const RenderBlock& rendererWithScrollbarChanges, EnumSet<ScrollbarOrientation> orientationsForChangedScrollbars) const
{
    auto subtreeRootWritingMode = subtreeRoot->writingMode();
    auto translateToSubtreeRootAxis = [&](BoxAxis perpendicularAxis) {
        auto axis = mapAxisPhysicalToLogical(subtreeRootWritingMode, perpendicularAxis);
        // FIXME:
        //   <div class="outer" style="writing-mode: vertical-lr">
        //     <div class="parent" style="writing-mode: horizontal-tb; overflow: auto">
        //       <div class="content"></div>
        //     </div>
        //   </div>
        //
        // .outer is the subtree root (orthogonal to body, hence shrink-to-fit). When
        // .content overflows, .parent gains a vertical scrollbar. In .outer's frame
        // (vertical-lr) that scrollbar consumes block-axis space, so without this
        // remap we'd classify the change as Block and discard it. But in .parent's
        // frame (horizontal-tb) the same scrollbar consumes inline-axis space, so
        // .parent's preferred inline-size grows. The only thing that propagates that
        // growth up to .outer is the brute-force subtreeRoot->layoutBlock(RelayoutChildren::Yes)
        // at the end of the handler, which reruns orthogonal-flow sizing. We have to do
        // this remapping in order to maintain functionality but this should really be
        // treated as a change to the content's block axis size for the subtree root.
        if (axis == LogicalBoxAxis::Block && subtreeRootWritingMode.isOrthogonal(rendererWithScrollbarChanges.writingMode()))
            axis = LogicalBoxAxis::Inline;
        return axis;
    };

    EnumSet<LogicalBoxAxis> sizesAffectedFromScrollbarChanges;
    if (orientationsForChangedScrollbars.contains(ScrollbarOrientation::Vertical))
        sizesAffectedFromScrollbarChanges.add(translateToSubtreeRootAxis(BoxAxis::Horizontal));
    if (orientationsForChangedScrollbars.contains(ScrollbarOrientation::Horizontal))
        sizesAffectedFromScrollbarChanges.add(translateToSubtreeRootAxis(BoxAxis::Vertical));
    return sizesAffectedFromScrollbarChanges & sizesAffectedForSubtreeRoot;
}

void SubtreeScrollbarChangesState::addRendererWithScrollbarChange(RenderBlock& renderer, EnumSet<LogicalBoxAxis> sizesAffectedFromScrollbarChanges)
{
    ASSERT(sizesAffectedForSubtreeRoot.containsAll(sizesAffectedFromScrollbarChanges));
    auto currentEntryIndex = rendererScrollbarChanges.findIf([&](auto& rendererScrollbarChange) {
        return rendererScrollbarChange.renderer.ptr() == &renderer;
    });
    if (currentEntryIndex == notFound) {
        rendererScrollbarChanges.append({ renderer, sizesAffectedFromScrollbarChanges });
        return;
    }
    rendererScrollbarChanges[currentEntryIndex].sizesAffectedFromScrollbarChanges.add(sizesAffectedFromScrollbarChanges);
}

SubtreeScrollbarChangesStateScope::SubtreeScrollbarChangesStateScope(LocalFrameViewLayoutContext& layoutContext, RenderBlock& subtreeRoot, EnumSet<LogicalBoxAxis> sizesAffectedForSubtreeRoot)
    : m_layoutContext(layoutContext)
{
    layoutContext.setSubtreeScrollbarChangesState(SubtreeScrollbarChangesState { subtreeRoot, sizesAffectedForSubtreeRoot, { } });
}

SubtreeScrollbarChangesStateScope::~SubtreeScrollbarChangesStateScope()
{
    m_layoutContext->setSubtreeScrollbarChangesState({ });
}

SubtreeScrollbarChangesHandler::SubtreeScrollbarChangesHandler(RenderBlock& rendererHandlingScrollbarChanges)
    : m_rendererHandlingScrollbarChanges(rendererHandlingScrollbarChanges)
{
    CheckedRef layoutContext = rendererHandlingScrollbarChanges.layoutContext();

    auto& subtreeScrollbarChangesState = layoutContext->subtreeScrollbarChangesState();
    ASSERT(subtreeScrollbarChangesState);
    if (!subtreeScrollbarChangesState)
        return;

    bool isSubtreeRootHandlingScrollbarChanges = subtreeScrollbarChangesState->subtreeRoot.ptr() == &rendererHandlingScrollbarChanges;
    if (!isSubtreeRootHandlingScrollbarChanges) {
        m_rendererScrollbarChangesHandledByAncestor = WTF::move(subtreeScrollbarChangesState->rendererScrollbarChanges);
        layoutContext->setSubtreeScrollbarChangesState(SubtreeScrollbarChangesState { subtreeScrollbarChangesState->subtreeRoot, subtreeScrollbarChangesState->sizesAffectedForSubtreeRoot, { } });
    }
}

SubtreeScrollbarChangesHandler::~SubtreeScrollbarChangesHandler()
{
    CheckedRef layoutContext = m_rendererHandlingScrollbarChanges->layoutContext();

    auto& subtreeScrollbarChangesState = layoutContext->subtreeScrollbarChangesState();
    bool isSubtreeRootHandlingScrollbarChanges = subtreeScrollbarChangesState->subtreeRoot.ptr() == m_rendererHandlingScrollbarChanges.ptr();

    auto descendantsWithScrollbarChange = WTF::move(subtreeScrollbarChangesState->rendererScrollbarChanges);
    auto restoreRenderersWithScrollbarChanges = makeScopeExit([&]() {
        subtreeScrollbarChangesState->rendererScrollbarChanges = WTF::move(m_rendererScrollbarChangesHandledByAncestor);
        ASSERT(descendantsWithScrollbarChange.isEmpty());
    });

    if (descendantsWithScrollbarChange.isEmpty())
        return;

#if ASSERT_ENABLED
    for (auto& rendererScrollbarChange : descendantsWithScrollbarChange)
        ASSERT(subtreeScrollbarChangesState->isEligibleForScrollbarHandlingByAncestor(rendererScrollbarChange.renderer.get()));
#endif

    if (!isSubtreeRootHandlingScrollbarChanges) {
        for (auto& rendererScrollbarChange : descendantsWithScrollbarChange)
            RenderBlock::relayoutRenderBlockForScrollbarChange(rendererScrollbarChange.renderer.get());
        descendantsWithScrollbarChange.clear();
        return;
    }

    auto& subtreeRoot = subtreeScrollbarChangesState->subtreeRoot;
    for (auto& rendererScrollbarChange : descendantsWithScrollbarChange) {
        CheckedRef renderer = rendererScrollbarChange.renderer;
        ASSERT(renderer->isDescendantOf(subtreeRoot.ptr()));
        if (rendererScrollbarChange.sizesAffectedFromScrollbarChanges.contains(LogicalBoxAxis::Block))
            renderer->setNeedsLayout();
        if (rendererScrollbarChange.sizesAffectedFromScrollbarChanges.contains(LogicalBoxAxis::Inline))
            renderer->invalidateContentLogicalWidths(MarkingBehavior::MarkContainingBlockChain, protect(subtreeRoot->containingBlock()));
    }
    descendantsWithScrollbarChange.clear();

    subtreeRoot->setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    subtreeRoot->layoutBlock(RelayoutChildren::Yes);
}

} // namespace WebCore
