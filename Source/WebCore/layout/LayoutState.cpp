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
#include "LayoutState.h"

#include "BlockFormattingState.h"
#include "InlineContentCache.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContainingBlockChainIterator.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "RenderBox.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(LayoutState);

LayoutState::LayoutState(const Document& document, const ElementBox& rootContainer)
    : m_rootContainer(rootContainer)
{
    // It makes absolutely no sense to construct a dedicated layout state for a non-formatting context root (layout would be a no-op).
    ASSERT(root().establishesFormattingContext());

    updateQuirksMode(document);
}

LayoutState::~LayoutState() = default;

void LayoutState::updateQuirksMode(const Document& document)
{
    auto quirksMode = [&] {
        if (document.inLimitedQuirksMode())
            return LayoutState::QuirksMode::Limited;
        if (document.inQuirksMode())
            return LayoutState::QuirksMode::Yes;
        return LayoutState::QuirksMode::No;
    };
    setQuirksMode(quirksMode());
}

BoxGeometry& LayoutState::geometryForRootBox()
{
    return ensureGeometryForBox(root());
}

BoxGeometry& LayoutState::ensureGeometryForBoxSlow(const Box& layoutBox)
{
    if (layoutBox.canCacheForLayoutState(*this)) {
        ASSERT(!layoutBox.cachedGeometryForLayoutState(*this));
        auto newBox = makeUnique<BoxGeometry>();
        auto& newBoxPtr = *newBox;
        layoutBox.setCachedGeometryForLayoutState(*this, WTFMove(newBox));
        return newBoxPtr;
    }

    return *m_layoutBoxToBoxGeometry.ensure(&layoutBox, [] {
        return makeUnique<BoxGeometry>();
    }).iterator->value;
}

bool LayoutState::hasFormattingState(const ElementBox& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    return m_blockFormattingStates.contains(&formattingContextRoot) || m_tableFormattingStates.contains(&formattingContextRoot);
}

FormattingState& LayoutState::formattingStateForFormattingContext(const ElementBox& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());

    if (formattingContextRoot.establishesBlockFormattingContext())
        return formattingStateForBlockFormattingContext(formattingContextRoot);

    if (formattingContextRoot.establishesTableFormattingContext())
        return formattingStateForTableFormattingContext(formattingContextRoot);

    CRASH();
}

BlockFormattingState& LayoutState::formattingStateForBlockFormattingContext(const ElementBox& blockFormattingContextRoot) const
{
    ASSERT(blockFormattingContextRoot.establishesBlockFormattingContext());
    return *m_blockFormattingStates.get(&blockFormattingContextRoot);
}

TableFormattingState& LayoutState::formattingStateForTableFormattingContext(const ElementBox& tableFormattingContextRoot) const
{
    ASSERT(tableFormattingContextRoot.establishesTableFormattingContext());
    return *m_tableFormattingStates.get(&tableFormattingContextRoot);
}

InlineContentCache& LayoutState::inlineContentCache(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesInlineFormattingContext());
    return *m_inlineContentCaches.ensure(&formattingContextRoot, [&] { return makeUnique<InlineContentCache>(); }).iterator->value;
}

BlockFormattingState& LayoutState::ensureBlockFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesBlockFormattingContext());
    return *m_blockFormattingStates.ensure(&formattingContextRoot, [&] { return makeUnique<BlockFormattingState>(*this, formattingContextRoot); }).iterator->value;
}

TableFormattingState& LayoutState::ensureTableFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesTableFormattingContext());
    return *m_tableFormattingStates.ensure(&formattingContextRoot, [&] { return makeUnique<TableFormattingState>(*this, formattingContextRoot); }).iterator->value;
}

void LayoutState::destroyBlockFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesBlockFormattingContext());
    m_blockFormattingStates.remove(&formattingContextRoot);
}

void LayoutState::destroyInlineContentCache(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesInlineFormattingContext());
    m_inlineContentCaches.remove(&formattingContextRoot);
}

}
}

