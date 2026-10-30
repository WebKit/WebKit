/*
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutUnits.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleComputedStyle.h>
#include <wtf/CheckedPtr.h>
#include <wtf/EnumSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LayoutShape;
class RenderObject;
class UsedStyle;

namespace Layout {

class ElementBox;
class BoxGeometry;
class InitialContainingBlock;
class LayoutState;
class TreeBuilder;

// Boxes are base classes of the renderers. The initial containing block is the only box that is allocated on its own.
class Box {
public:
    enum class NodeType : uint8_t {
        Text,
        GenericElement,
        InlineBox,
        ReplacedElement,
        DocumentElement,
        Body,
        TableWrapperBox, // The table generates a principal block container box called the table wrapper box that contains the table box and any caption boxes.
        TableBox, // The table box is a block-level box that contains the table's internal table boxes.
        Image,
        IFrame,
        LineBreak,
        WordBreakOpportunity,
        ListMarker,
        ImplicitFlexBox // These boxes are implicit flex boxes with no flex display type and they should probably turned into proper flex boxes.
    };

    enum class IsAnonymous : bool { No, Yes };

    struct ElementAttributes {
        NodeType nodeType;
        IsAnonymous isAnonymous;
    };

    enum BaseTypeFlag : uint8_t {
        InlineTextBoxFlag,
        ElementBoxFlag,
        InitialContainingBlockFlag,
    };

    // Counts CheckedPtrs to the box. RenderElement and RenderText add it to the RenderObject counter.
    ALWAYS_INLINE uint32_t LEGACY_NODELETE checkedPtrCount() const { return m_checkedPtrCounter.checkedPtrCount(); }
    ALWAYS_INLINE uint32_t checkedPtrCountWithoutThreadCheck() const { return m_checkedPtrCounter.checkedPtrCountWithoutThreadCheck(); }
    ALWAYS_INLINE void LEGACY_NODELETE incrementCheckedPtrCount() const { m_checkedPtrCounter.incrementCheckedPtrCount(); }
    SUPPRESS_NODELETE ALWAYS_INLINE void LEGACY_NODELETE decrementCheckedPtrCount() const { m_checkedPtrCounter.decrementCheckedPtrCount(); }
    ALWAYS_INLINE void setDidBeginCheckedPtrDeletion() { m_checkedPtrCounter.setDidBeginCheckedPtrDeletion(); }

    bool establishesFormattingContext() const;
    bool establishesBlockFormattingContext() const;
    bool LEGACY_NODELETE establishesInlineFormattingContext() const;
    bool LEGACY_NODELETE establishesTableFormattingContext() const;
    bool LEGACY_NODELETE establishesFlexFormattingContext() const;
    bool LEGACY_NODELETE establishesGridFormattingContext() const;
    bool establishesIndependentFormattingContext() const;

    bool isInFlow() const { return !isFloatingOrOutOfFlowPositioned(); }
    bool isPositioned() const { return isInFlowPositioned() || isOutOfFlowPositioned(); }
    bool isInFlowPositioned() const { return isRelativelyPositioned() || isStickyPositioned(); }
    bool isOutOfFlowPositioned() const { return isAbsolutelyPositioned(); }
    bool LEGACY_NODELETE isRelativelyPositioned() const;
    bool LEGACY_NODELETE isStickyPositioned() const;
    bool LEGACY_NODELETE isAbsolutelyPositioned() const;
    bool LEGACY_NODELETE isFixedPositioned() const;
    bool LEGACY_NODELETE isFloatingPositioned() const;
    bool LEGACY_NODELETE hasFloatClear() const;
    bool isFloatAvoider() const;

    bool isFloatingOrOutOfFlowPositioned() const { return isFloatingPositioned() || isOutOfFlowPositioned(); }

    bool isContainingBlockForInFlow() const;
    inline bool isContainingBlockForFixedPosition() const;
    inline bool isContainingBlockForOutOfFlowPosition() const;

    bool isAnonymous() const { return m_isAnonymous; }

    // Block level elements generate block level boxes.
    bool LEGACY_NODELETE isBlockLevelBox() const;
    // A block-level box that is also a block container.
    bool LEGACY_NODELETE isBlockBox() const;
    // A block-level box is also a block container box unless it is a table box or the principal box of a replaced element.
    bool LEGACY_NODELETE isBlockContainer() const;
    bool LEGACY_NODELETE isInlineLevelBox() const;
    bool LEGACY_NODELETE isInlineBox() const;
    bool LEGACY_NODELETE isAtomicInlineBox() const;
    bool LEGACY_NODELETE isInlineBlockBox() const;
    bool LEGACY_NODELETE isInlineTableBox() const;
    bool isInitialContainingBlock() const { return baseTypeFlags().contains(InitialContainingBlockFlag); }
    bool isLayoutContainmentBox() const;
    bool isSizeContainmentBox() const;
    bool LEGACY_NODELETE isInternalRubyBox() const;
    bool LEGACY_NODELETE isRubyAnnotationBox() const;
    bool LEGACY_NODELETE isInterlinearRubyAnnotationBox() const;

    bool isDocumentBox() const { return m_nodeType == NodeType::DocumentElement; }
    bool isTableWrapperBox() const { return m_nodeType == NodeType::TableWrapperBox; }
    bool isTableBox() const { return m_nodeType == NodeType::TableBox; }
    bool isBodyBox() const { return m_nodeType == NodeType::Body; }
    bool isLineBreakBox() const { return m_nodeType == NodeType::LineBreak || m_nodeType == NodeType::WordBreakOpportunity; }
    bool isIFrame() const { return m_nodeType == NodeType::IFrame; }
    bool isImage() const { return m_nodeType == NodeType::Image; }
    bool isListMarkerBox() const { return m_nodeType == NodeType::ListMarker; }
    bool isReplacedBox() const { return m_nodeType == NodeType::ReplacedElement || m_nodeType == NodeType::Image || m_nodeType == NodeType::ListMarker; }
    bool isWordBreakOpportunity() const { return m_nodeType == NodeType::WordBreakOpportunity; }
    bool LEGACY_NODELETE isInternalTableBox() const;
    inline bool LEGACY_NODELETE isRuby() const;
    bool LEGACY_NODELETE isRubyBase() const;
    inline bool isRubyInlineBox() const;
    inline bool isTableCaption() const;
    inline bool isTableHeader() const;
    inline bool isTableBody() const;
    inline bool isTableFooter() const;
    inline bool isTableRow() const;
    inline bool isTableColumnGroup() const;
    inline bool isTableColumn() const;
    inline bool isTableCell() const;
    inline bool isFlexBox() const;
    bool LEGACY_NODELETE isFlexItem() const;
    inline bool isGridFormattingContext() const;
    inline bool isGridBox() const;
    inline bool isGridLanesBox() const;
    bool LEGACY_NODELETE isGridItem() const;
    inline bool isListItem() const;

    // Set while the box is the root of a live LineLayout or GridLayout.
    bool isInlineIntegrationRoot() const { return m_isInlineIntegrationRoot; }
    bool isGridIntegrationRoot() const { return m_isGridIntegrationRoot; }
    bool isIntegrationRoot() const { return m_isInlineIntegrationRoot || m_isGridIntegrationRoot; }
    bool isAnonymousTextIndentCandidateForIntegration() const { return m_isAnonymousTextIndentCandidateForIntegration; }

    // Navigation follows the render tree, skipping excluded list markers. The initial containing block is the parent of
    // RenderView and of renderers without a parent. parent() is defined in RenderElement.h, the siblings in RenderText.h.
    inline const ElementBox& parent() const;
    inline const Box* nextSibling() const;
    const Box* LEGACY_NODELETE nextInFlowSibling() const;
    const Box* LEGACY_NODELETE nextInFlowOrFloatingSibling() const;
    const Box* LEGACY_NODELETE nextOutOfFlowSibling() const;
    inline const Box* previousSibling() const;
    const Box* LEGACY_NODELETE previousInFlowSibling() const;
    const Box* LEGACY_NODELETE previousInFlowOrFloatingSibling() const;
    const Box* LEGACY_NODELETE previousOutOfFlowSibling() const;
    bool LEGACY_NODELETE isDescendantOf(const Box&) const;
    bool isDescendantOfWithinFormattingContext(const Box&) const;
    bool isInFormattingContextEstablishedBy(const ElementBox& formattingContextRoot) const;

    bool isElementBox() const { return baseTypeFlags().contains(ElementBoxFlag); }
    bool isInlineTextBox() const { return baseTypeFlags().contains(InlineTextBoxFlag); }

    bool LEGACY_NODELETE isPaddingApplicable() const;
    bool LEGACY_NODELETE isOverflowVisible() const;

    void setFirstLineStyle(std::unique_ptr<Style::ComputedStyle>&&);
    // Always inlined so that RenderElement::style() costs the same in debug builds.
    ALWAYS_INLINE const Style::ComputedStyle& style() const LIFETIME_BOUND { return m_style; }
    // Text uses the first-line style of its parent when the parent has one.
    const Style::ComputedStyle& LEGACY_NODELETE firstLineStyle() const LIFETIME_BOUND
    {
        if (m_mayHaveFirstLineStyle) [[unlikely]]
            return firstLineStyleForText();
        return hasBoxRareData() && boxRareData().firstLineStyle ? *boxRareData().firstLineStyle : m_style;
    }
    WritingMode writingMode() const { return style().writingMode(); }

    void setIsInlineIntegrationRoot(bool value) { m_isInlineIntegrationRoot = value; }
    void setIsGridIntegrationRoot(bool value) { m_isGridIntegrationRoot = value; }
    void setIsAnonymousTextIndentCandidateForIntegration(bool value) { m_isAnonymousTextIndentCandidateForIntegration = value; }
    void setMayHaveFirstLineStyle(bool value) { m_mayHaveFirstLineStyle = value; }

    const LayoutShape* LEGACY_NODELETE shape() const;
    void setShape(RefPtr<const LayoutShape>);

    const ElementBox* LEGACY_NODELETE associatedRubyAnnotationBox() const;

    // Null only for the initial containing block. Defined in RenderText.h.
    inline RenderObject* rendererForIntegration() const;

    void setElementAttributes(ElementAttributes);

    UsedStyle LEGACY_NODELETE usedStyle() const;
    UsedStyle LEGACY_NODELETE firstLineUsedStyle() const;

    // Clears what the formatting context stored on the box, when the box leaves it or the formatting context goes away.
    void clearFormattingContextData();

protected:
    Box(Style::ComputedStyle&&, EnumSet<BaseTypeFlag>);
    ~Box();

    ALWAYS_INLINE Style::ComputedStyle& mutableStyle() LIFETIME_BOUND { return m_style; }

private:
    friend class ElementBox;
    friend class LayoutState;

    class BoxRareData {
        WTF_MAKE_TZONE_ALLOCATED(BoxRareData);
    public:
        BoxRareData() = default;

        std::unique_ptr<Style::ComputedStyle> firstLineStyle;
        RefPtr<const LayoutShape> shape;
    };

    // RenderView and renderers that are being detached.
    const ElementBox& LEGACY_NODELETE parentForRendererWithoutParent() const;

    // Named apart from the RenderObject rare data functions, which renderer code calls unqualified.
    bool hasBoxRareData() const { return m_hasRareData; }
    void setHasRareData(bool hasRareData) { m_hasRareData = hasRareData; }
    const BoxRareData& LEGACY_NODELETE boxRareData() const;
    const Style::ComputedStyle& LEGACY_NODELETE firstLineStyleForText() const LIFETIME_BOUND;
    Box::BoxRareData& boxRareData();
    BoxRareData& ensureBoxRareData();
    void removeBoxRareData();
    
    EnumSet<BaseTypeFlag> baseTypeFlags() const { return EnumSet<BaseTypeFlag>::fromRaw(m_baseTypeFlags); }

    typedef HashMap<const Box*, std::unique_ptr<BoxRareData>> BoxRareDataMap;

    static BoxRareDataMap& LEGACY_NODELETE boxRareDataMap();

    WTF::CanMakeCheckedPtrBase<WTF::SingleThreadIntegralWrapper<uint32_t>, uint32_t, bool, WTF::CheckedPtrDeleteCheckException::No> m_checkedPtrCounter;

    NodeType m_nodeType : 4;
    bool m_isAnonymous : 1;

    unsigned m_baseTypeFlags : 4; // EnumSet<BaseTypeFlag>
    bool m_hasRareData : 1 { false };
    bool m_isInlineIntegrationRoot : 1 { false };
    bool m_isGridIntegrationRoot : 1 { false };
    bool m_isAnonymousTextIndentCandidateForIntegration : 1 { false }; // Either first anonymous block box child or simple anonymous block container (e.g flex item).
    // Text only. Set when the parent may have a first-line style. Use of first-line styles is rare.
    bool m_mayHaveFirstLineStyle : 1 { false };

    Style::ComputedStyle m_style;

    // Primary LayoutState gets a direct cache.
#if ASSERT_ENABLED
    mutable WeakPtr<LayoutState> m_primaryLayoutState;
#endif
    mutable std::unique_ptr<BoxGeometry> m_cachedGeometryForPrimaryLayoutState;
};

inline bool Box::isContainingBlockForInFlow() const
{
    return isBlockContainer() || establishesFormattingContext();
}

}

}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::Box& box) { return box.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
