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
#include "RelayoutScopeForScrollbarChange.h"

#include "LayoutScope.h"
#include "RenderBlock.h"
#include "RenderFlexibleBox.h"
#include "RenderObjectInlines.h"

namespace WebCore {

RelayoutScopeForScrollbarChange::RelayoutScopeForScrollbarChange(RenderBlock& renderBlock, InOverflowRelayout inOverflowRelayout)
    : m_renderBlock(renderBlock)
    , m_inOverflowRelayout(inOverflowRelayout)
    , m_scrollbarUpdateScope(renderBlock.updateScrollInfoAfterLayout())
{
}

RelayoutScopeForScrollbarChange::~RelayoutScopeForScrollbarChange()
{
    if (!m_scrollbarUpdateScope)
        return;

    auto& autoScrollbarChanges = m_scrollbarUpdateScope->autoScrollbarChanges();
    if (autoScrollbarChanges.isEmpty())
        return;

    // Scrollbars with auto behavior may need to lay out again if scrollbars got added or removed.
    m_renderBlock->repaint();

    if (m_renderBlock->style().overflowX() == Overflow::Auto || m_renderBlock->style().overflowY() == Overflow::Auto) {
        if (m_inOverflowRelayout == InOverflowRelayout::No) {
            if (auto& subtreeScrollbarChangesState = m_renderBlock->layoutContext().subtreeScrollbarChangesState(); subtreeScrollbarChangesState && subtreeScrollbarChangesState->isEligibleForScrollbarHandlingByAncestor(m_renderBlock.get())) {
                ASSERT(m_renderBlock.ptr() != subtreeScrollbarChangesState->subtreeRoot.ptr());
                if (auto sizesAffectedFromScrollbarChanges = subtreeScrollbarChangesState->sizesAffectedForSubtreeRootFromScrollbarChanges(m_renderBlock, autoScrollbarChanges)) {
                    subtreeScrollbarChangesState->addRendererWithScrollbarChange(m_renderBlock, sizesAffectedFromScrollbarChanges);
                    return;
                }
            }
            m_renderBlock->scrollbarsChanged(autoScrollbarChanges.contains(ScrollbarOrientation::Horizontal), autoScrollbarChanges.contains(ScrollbarOrientation::Vertical));
            RenderBlock::relayoutRenderBlockForScrollbarChange(m_renderBlock.get());
        }
    }

    // FIXME: This does not belong here.
    CheckedPtr parent = m_renderBlock->parent();
    if (CheckedPtr parentFlexibleBox = dynamicDowncast<RenderFlexibleBox>(parent); parentFlexibleBox)
        parentFlexibleBox->invalidateBlockAxisSizeForFlexItem(m_renderBlock.get());
}

} // namespace WebCore
