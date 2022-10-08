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
#include "FlexFormattingState.h"
#include "FloatingState.h"
#include "InlineFormattingState.h"
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

LayoutState::LayoutState(const Document& document, const ElementBox& rootContainer, std::optional<FormattingContextIntegrationType> formattingContextIntegrationType)
    : m_rootContainer(rootContainer)
    , m_formattingContextIntegrationType(formattingContextIntegrationType)
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
    return m_blockFormattingStates.contains(&formattingContextRoot)
        || m_inlineFormattingStates.contains(&formattingContextRoot)
        || m_tableFormattingStates.contains(&formattingContextRoot)
        || m_flexFormattingStates.contains(&formattingContextRoot);
}

FormattingState& LayoutState::formattingStateForFormattingContext(const ElementBox& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());

    if (isFlexFormattingContextIntegration()) {
        ASSERT(&formattingContextRoot == m_rootContainer.ptr());
        return *m_rootFlexFormattingStateForIntegration;
    }

    if (formattingContextRoot.establishesInlineFormattingContext())
        return formattingStateForInlineFormattingContext(formattingContextRoot);

    if (formattingContextRoot.establishesBlockFormattingContext())
        return formattingStateForBlockFormattingContext(formattingContextRoot);

    if (formattingContextRoot.establishesTableFormattingContext())
        return formattingStateForTableFormattingContext(formattingContextRoot);

    if (formattingContextRoot.establishesFlexFormattingContext())
        return formattingStateForFlexFormattingContext(formattingContextRoot);

    CRASH();
}

InlineFormattingState& LayoutState::formattingStateForInlineFormattingContext(const ElementBox& inlineFormattingContextRoot) const
{
    ASSERT(inlineFormattingContextRoot.establishesInlineFormattingContext());

    return *m_inlineFormattingStates.get(&inlineFormattingContextRoot);
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

FlexFormattingState& LayoutState::formattingStateForFlexFormattingContext(const ElementBox& flexFormattingContextRoot) const
{
    ASSERT(flexFormattingContextRoot.establishesFlexFormattingContext());

    if (isFlexFormattingContextIntegration()) {
        ASSERT(&flexFormattingContextRoot == m_rootContainer.ptr());
        return *m_rootFlexFormattingStateForIntegration;
    }

    return *m_flexFormattingStates.get(&flexFormattingContextRoot);
}

InlineFormattingState& LayoutState::ensureInlineFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesInlineFormattingContext());

    auto create = [&] {
        // If the block container box that initiates this inline formatting context also establishes a block context, the floats outside of the formatting root
        // should not interfere with the content inside.
        // <div style="float: left"></div><div style="overflow: hidden"> <- is a non-intrusive float, because overflow: hidden triggers new block formatting context.</div>
        if (formattingContextRoot.establishesBlockFormattingContext() || formattingContextRoot.isInlineIntegrationRoot())
            return makeUnique<InlineFormattingState>(FloatingState::create(*this, formattingContextRoot), *this);

        // Otherwise, the formatting context inherits the floats from the parent formatting context.
        // Find the formatting state in which this formatting root lives, not the one it creates and use its floating state.
        auto parentFormattingState = [&] () -> FormattingState& {
            for (auto& containingBlock : containingBlockChain(formattingContextRoot)) {
                if (containingBlock.establishesBlockFormattingContext())
                    return formattingStateForFormattingContext(containingBlock);
            }
            ASSERT_NOT_REACHED();
            return formattingStateForFormattingContext(FormattingContext::initialContainingBlock(formattingContextRoot));
        };
        return makeUnique<InlineFormattingState>(parentFormattingState().floatingState(), *this);
    };

    return *m_inlineFormattingStates.ensure(&formattingContextRoot, create).iterator->value;
}

BlockFormattingState& LayoutState::ensureBlockFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesBlockFormattingContext());

    auto create = [&] {
        return makeUnique<BlockFormattingState>(FloatingState::create(*this, formattingContextRoot), *this);
    };

    return *m_blockFormattingStates.ensure(&formattingContextRoot, create).iterator->value;
}

TableFormattingState& LayoutState::ensureTableFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesTableFormattingContext());

    auto create = [&] {
        // Table formatting context always establishes a new floating state -and it stays empty.
        return makeUnique<TableFormattingState>(FloatingState::create(*this, formattingContextRoot), *this, formattingContextRoot);
    };

    return *m_tableFormattingStates.ensure(&formattingContextRoot, create).iterator->value;
}

FlexFormattingState& LayoutState::ensureFlexFormattingState(const ElementBox& formattingContextRoot)
{
    ASSERT(formattingContextRoot.establishesFlexFormattingContext());

    auto create = [&] {
        // Flex formatting context always establishes a new floating state -and it stays empty.
        return makeUnique<FlexFormattingState>(FloatingState::create(*this, formattingContextRoot), *this);
    };

    if (isFlexFormattingContextIntegration()) {
        if (!m_rootFlexFormattingStateForIntegration) {
            ASSERT(&formattingContextRoot == m_rootContainer.ptr());
            m_rootFlexFormattingStateForIntegration = create();
        }
        return *m_rootFlexFormattingStateForIntegration;
    }

    return *m_flexFormattingStates.ensure(&formattingContextRoot, create).iterator->value;
}

void LayoutState::destroyInlineFormattingState(const ElementBox& formattingContextRoot)
{
    m_inlineFormattingStates.remove(&formattingContextRoot);
}

void LayoutState::setViewportSize(const LayoutSize& viewportSize)
{
    ASSERT(isInlineFormattingContextIntegration());
    m_viewportSize = viewportSize;
}

LayoutSize LayoutState::viewportSize() const
{
    ASSERT(isInlineFormattingContextIntegration());
    return m_viewportSize;
}

bool LayoutState::shouldIgnoreTrailingLetterSpacing() const
{
    return isInlineFormattingContextIntegration();
}

bool LayoutState::shouldNotSynthesizeInlineBlockBaseline() const
{
    return isInlineFormattingContextIntegration();
}

}
}

