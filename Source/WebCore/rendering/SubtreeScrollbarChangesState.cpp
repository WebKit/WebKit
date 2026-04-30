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

#include "LayoutScope.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderBlock.h"
#include "RenderObjectInlines.h"
#include <wtf/Scope.h>

namespace WebCore {

SubtreeScrollbarChangesStateScope::SubtreeScrollbarChangesStateScope(LocalFrameViewLayoutContext& layoutContext, RenderBlock& subtreeRoot)
    : m_layoutContext(layoutContext)
{
    layoutContext.setSubtreeScrollbarChangesState(SubtreeScrollbarChangesState { subtreeRoot, { } });
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
        m_renderersWithScrollbarChangesHandledByAncestor = WTF::move(subtreeScrollbarChangesState->renderersWithScrollbarChange);
        layoutContext->setSubtreeScrollbarChangesState(SubtreeScrollbarChangesState { subtreeScrollbarChangesState->subtreeRoot, { } });
    }
}

SubtreeScrollbarChangesHandler::~SubtreeScrollbarChangesHandler()
{
    CheckedRef layoutContext = m_rendererHandlingScrollbarChanges->layoutContext();

    auto& subtreeScrollbarChangesState = layoutContext->subtreeScrollbarChangesState();
    bool isSubtreeRootHandlingScrollbarChanges = subtreeScrollbarChangesState->subtreeRoot.ptr() == m_rendererHandlingScrollbarChanges.ptr();

    auto descendantsWithScrollbarChange = WTF::move(subtreeScrollbarChangesState->renderersWithScrollbarChange);
    auto restoreRenderersWithScrollbarChanges = makeScopeExit([&]() {
        subtreeScrollbarChangesState->renderersWithScrollbarChange = WTF::move(m_renderersWithScrollbarChangesHandledByAncestor);
        ASSERT(descendantsWithScrollbarChange.isEmpty());
    });

    if (descendantsWithScrollbarChange.isEmpty())
        return;

    if (!isSubtreeRootHandlingScrollbarChanges) {
        while (!descendantsWithScrollbarChange.isEmpty()) {
            CheckedPtr rendererWithScrollbarChange = descendantsWithScrollbarChange.takeFirst();
            auto scope = LayoutScope { *rendererWithScrollbarChange };
            rendererWithScrollbarChange->setNeedsLayout(MarkingBehavior::MarkOnlyThis);
            rendererWithScrollbarChange->layoutBlock(RelayoutChildren::Yes);
        }
        return;
    }

    auto& subtreeRoot = subtreeScrollbarChangesState->subtreeRoot;
    while (!descendantsWithScrollbarChange.isEmpty()) {
        CheckedPtr rendererWithScrollbarChange = descendantsWithScrollbarChange.takeFirst();
        rendererWithScrollbarChange->setNeedsPreferredWidthsUpdate(MarkingBehavior::MarkContainingBlockChain, subtreeRoot.ptr());
    }

    auto scope = LayoutScope { subtreeRoot };
    subtreeRoot->setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    subtreeRoot->layoutBlock(RelayoutChildren::Yes);
}

} // namespace WebCore
