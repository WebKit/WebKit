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

#pragma once

#include "LayoutElementBox.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Layout {

class BlockFormattingState;
class FlexFormattingState;
class FormattingContext;
class FormattingState;
class BoxGeometry;
class InlineFormattingState;
class TableFormattingState;

class LayoutState : public CanMakeWeakPtr<LayoutState> {
    WTF_MAKE_NONCOPYABLE(LayoutState);
    WTF_MAKE_ISO_ALLOCATED(LayoutState);
public:
    enum class FormattingContextIntegrationType {
        Inline,
        Flex
    };
    LayoutState(const Document&, const ElementBox& rootContainer, std::optional<FormattingContextIntegrationType> = std::nullopt);
    ~LayoutState();

    void updateQuirksMode(const Document&);

    InlineFormattingState& ensureInlineFormattingState(const ElementBox& formattingContextRoot);
    BlockFormattingState& ensureBlockFormattingState(const ElementBox& formattingContextRoot);
    TableFormattingState& ensureTableFormattingState(const ElementBox& formattingContextRoot);
    FlexFormattingState& ensureFlexFormattingState(const ElementBox& formattingContextRoot);

    InlineFormattingState& formattingStateForInlineFormattingContext(const ElementBox& inlineFormattingContextRoot) const;
    BlockFormattingState& formattingStateForBlockFormattingContext(const ElementBox& blockFormattingContextRoot) const;
    TableFormattingState& formattingStateForTableFormattingContext(const ElementBox& tableFormattingContextRoot) const;
    FlexFormattingState& formattingStateForFlexFormattingContext(const ElementBox& flexFormattingContextRoot) const;

    FormattingState& formattingStateForFormattingContext(const ElementBox& formattingRoot) const;

    void destroyBlockFormattingState(const ElementBox& formattingContextRoot);
    void destroyInlineFormattingState(const ElementBox& formattingContextRoot);

    bool hasFormattingState(const ElementBox& formattingRoot) const;
    bool hasInlineFormattingState(const ElementBox& formattingRoot) const { return m_inlineFormattingStates.contains(&formattingRoot); }

#ifndef NDEBUG
    void registerFormattingContext(const FormattingContext&);
    void deregisterFormattingContext(const FormattingContext& formattingContext) { m_formattingContextList.remove(&formattingContext); }
#endif

    BoxGeometry& geometryForRootBox();
    BoxGeometry& ensureGeometryForBox(const Box&);
    const BoxGeometry& geometryForBox(const Box&) const;

    bool hasBoxGeometry(const Box&) const;

    enum class QuirksMode { No, Limited, Yes };
    bool inQuirksMode() const { return m_quirksMode == QuirksMode::Yes; }
    bool inLimitedQuirksMode() const { return m_quirksMode == QuirksMode::Limited; }
    bool inStandardsMode() const { return m_quirksMode == QuirksMode::No; }

    const ElementBox& root() const { return m_rootContainer; }

    // LFC integration only. Full LFC has proper ICB access.
    bool isInlineFormattingContextIntegration() const { return m_formattingContextIntegrationType && *m_formattingContextIntegrationType == FormattingContextIntegrationType::Inline; }
    bool isFlexFormattingContextIntegration() const { return m_formattingContextIntegrationType && *m_formattingContextIntegrationType == FormattingContextIntegrationType::Flex; }

    void setViewportSize(const LayoutSize&);
    LayoutSize viewportSize() const;
    bool shouldIgnoreTrailingLetterSpacing() const;
    bool shouldNotSynthesizeInlineBlockBaseline() const;

private:
    void setQuirksMode(QuirksMode quirksMode) { m_quirksMode = quirksMode; }
    BoxGeometry& ensureGeometryForBoxSlow(const Box&);

    HashMap<const ElementBox*, std::unique_ptr<InlineFormattingState>> m_inlineFormattingStates;
    HashMap<const ElementBox*, std::unique_ptr<BlockFormattingState>> m_blockFormattingStates;
    HashMap<const ElementBox*, std::unique_ptr<TableFormattingState>> m_tableFormattingStates;
    HashMap<const ElementBox*, std::unique_ptr<FlexFormattingState>> m_flexFormattingStates;

    std::unique_ptr<InlineFormattingState> m_rootInlineFormattingStateForIntegration;
    std::unique_ptr<FlexFormattingState> m_rootFlexFormattingStateForIntegration;

#ifndef NDEBUG
    HashSet<const FormattingContext*> m_formattingContextList;
#endif
    HashMap<const Box*, std::unique_ptr<BoxGeometry>> m_layoutBoxToBoxGeometry;
    QuirksMode m_quirksMode { QuirksMode::No };

    CheckedRef<const ElementBox> m_rootContainer;

    // LFC integration only.
    std::optional<FormattingContextIntegrationType> m_formattingContextIntegrationType;
    LayoutSize m_viewportSize;
};

inline bool LayoutState::hasBoxGeometry(const Box& layoutBox) const
{
    if (layoutBox.cachedGeometryForLayoutState(*this))
        return true;
    return m_layoutBoxToBoxGeometry.contains(&layoutBox);
}

inline BoxGeometry& LayoutState::ensureGeometryForBox(const Box& layoutBox)
{
    if (auto* boxGeometry = layoutBox.cachedGeometryForLayoutState(*this))
        return *boxGeometry;
    return ensureGeometryForBoxSlow(layoutBox);
}

inline const BoxGeometry& LayoutState::geometryForBox(const Box& layoutBox) const
{
    if (auto* boxGeometry = layoutBox.cachedGeometryForLayoutState(*this))
        return *boxGeometry;
    ASSERT(m_layoutBoxToBoxGeometry.contains(&layoutBox));
    return *m_layoutBoxToBoxGeometry.get(&layoutBox);
}

#ifndef NDEBUG
inline void LayoutState::registerFormattingContext(const FormattingContext& formattingContext)
{
    // Multiple formatting contexts of the same root within a layout frame indicates defective layout logic.
    ASSERT(!m_formattingContextList.contains(&formattingContext));
    m_formattingContextList.add(&formattingContext);
}
#endif

// These Layout::Box function are here to allow inlining.
inline bool Box::canCacheForLayoutState(const LayoutState& layoutState) const
{
    return !m_cachedLayoutState || m_cachedLayoutState.get() == &layoutState;
}

inline BoxGeometry* Box::cachedGeometryForLayoutState(const LayoutState& layoutState) const
{
    if (m_cachedLayoutState.get() != &layoutState)
        return nullptr;
    return m_cachedGeometryForLayoutState.get();
}

}
}
