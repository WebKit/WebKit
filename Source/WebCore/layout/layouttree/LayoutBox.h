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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutUnits.h"
#include "RenderStyle.h"
#include <wtf/CheckedPtr.h>
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {

class ContainerBox;
class BoxGeometry;
class InitialContainingBlock;
class LayoutState;
class TreeBuilder;

class Box : public CanMakeCheckedPtr {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    enum class ElementType {
        Document,
        Body,
        TableWrapperBox, // The table generates a principal block container box called the table wrapper box that contains the table box and any caption boxes.
        TableBox, // The table box is a block-level box that contains the table's internal table boxes.
        Image,
        IFrame,
        IntegrationBlockContainer,
        IntegrationInlineBlock, // Integration sets up inline-block boxes as replaced boxes.
        GenericElement
    };

    struct ElementAttributes {
        ElementType elementType;
    };

    enum BaseTypeFlag : uint8_t {
        BoxFlag                    = 1 << 0,
        InlineTextBoxFlag          = 1 << 1,
        LineBreakBoxFlag           = 1 << 2,
        ReplacedBoxFlag            = 1 << 3,
        InitialContainingBlockFlag = 1 << 4,
        ContainerBoxFlag           = 1 << 5
    };

    virtual ~Box();

    bool establishesFormattingContext() const;
    bool establishesBlockFormattingContext() const;
    bool establishesInlineFormattingContext() const;
    bool establishesTableFormattingContext() const;
    bool establishesFlexFormattingContext() const;
    bool establishesIndependentFormattingContext() const;

    bool isInFlow() const { return !isFloatingOrOutOfFlowPositioned(); }
    bool isPositioned() const { return isInFlowPositioned() || isOutOfFlowPositioned(); }
    bool isInFlowPositioned() const { return isRelativelyPositioned() || isStickyPositioned(); }
    bool isOutOfFlowPositioned() const { return isAbsolutelyPositioned(); }
    bool isRelativelyPositioned() const;
    bool isStickyPositioned() const;
    bool isAbsolutelyPositioned() const;
    bool isFixedPositioned() const;
    bool isFloatingPositioned() const;
    bool isLeftFloatingPositioned() const;
    bool isRightFloatingPositioned() const;
    bool hasFloatClear() const;
    bool isFloatAvoider() const;

    bool isFloatingOrOutOfFlowPositioned() const { return isFloatingPositioned() || isOutOfFlowPositioned(); }

    const ContainerBox& containingBlock() const;
    const ContainerBox& formattingContextRoot() const;
    const InitialContainingBlock& initialContainingBlock() const;

    bool isContainingBlockForInFlow() const;
    bool isContainingBlockForFixedPosition() const;
    bool isContainingBlockForOutOfFlowPosition() const;

    bool isInFormattingContextOf(const ContainerBox&) const;

    bool isAnonymous() const { return m_isAnonymous; }

    // Block level elements generate block level boxes.
    bool isBlockLevelBox() const;
    // A block-level box that is also a block container.
    bool isBlockBox() const;
    // A block-level box is also a block container box unless it is a table box or the principal box of a replaced element.
    bool isBlockContainer() const;
    bool isInlineLevelBox() const;
    bool isInlineBox() const;
    bool isAtomicInlineLevelBox() const;
    bool isInlineBlockBox() const;
    bool isInlineTableBox() const;
    bool isInitialContainingBlock() const { return baseTypeFlags().contains(InitialContainingBlockFlag); }
    bool isLayoutContainmentBox() const;
    bool isSizeContainmentBox() const;

    bool isDocumentBox() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Document; }
    bool isBodyBox() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Body; }
    bool isTableWrapperBox() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::TableWrapperBox; }
    bool isTableBox() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::TableBox; }
    bool isTableCaption() const { return style().display() == DisplayType::TableCaption; }
    bool isTableHeader() const { return style().display() == DisplayType::TableHeaderGroup; }
    bool isTableBody() const { return style().display() == DisplayType::TableRowGroup; }
    bool isTableFooter() const { return style().display() == DisplayType::TableFooterGroup; }
    bool isTableRow() const { return style().display() == DisplayType::TableRow; }
    bool isTableColumnGroup() const { return style().display() == DisplayType::TableColumnGroup; }
    bool isTableColumn() const { return style().display() == DisplayType::TableColumn; }
    bool isTableCell() const { return style().display() == DisplayType::TableCell; }
    bool isInternalTableBox() const;
    bool isFlexBox() const { return style().display() == DisplayType::Flex; }
    bool isFlexItem() const;
    bool isIFrame() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::IFrame; }
    bool isImage() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Image; }
    bool isInternalRubyBox() const { return false; }
    bool isIntegrationBlockContainer() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::IntegrationBlockContainer; }
    bool isIntegrationRoot() const { return isIntegrationBlockContainer() && !m_parent; }
    bool isIntegrationInlineBlock() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::IntegrationInlineBlock; }

    const ContainerBox& parent() const { return *m_parent; }
    const Box* nextSibling() const { return m_nextSibling.get(); }
    const Box* nextInFlowSibling() const;
    const Box* nextInFlowOrFloatingSibling() const;
    const Box* previousSibling() const { return m_previousSibling.get(); }
    const Box* previousInFlowSibling() const;
    const Box* previousInFlowOrFloatingSibling() const;
    bool isDescendantOf(const ContainerBox&) const;

    // FIXME: This is currently needed for style updates.
    Box* nextSibling() { return m_nextSibling.get(); }

    bool isContainerBox() const { return baseTypeFlags().contains(ContainerBoxFlag); }
    bool isInlineTextBox() const { return baseTypeFlags().contains(InlineTextBoxFlag); }
    bool isLineBreakBox() const { return baseTypeFlags().contains(LineBreakBoxFlag); }
    bool isReplacedBox() const { return baseTypeFlags().contains(ReplacedBoxFlag); }

    bool isPaddingApplicable() const;
    bool isOverflowVisible() const;

    void updateStyle(const RenderStyle& newStyle, std::unique_ptr<RenderStyle>&& newFirstLineStyle);
    const RenderStyle& style() const { return m_style; }
    const RenderStyle& firstLineStyle() const { return hasRareData() && rareData().firstLineStyle ? *rareData().firstLineStyle : m_style; }

    // FIXME: Find a better place for random DOM things.
    void setRowSpan(size_t);
    size_t rowSpan() const;

    void setColumnSpan(size_t);
    size_t columnSpan() const;

    void setColumnWidth(LayoutUnit);
    std::optional<LayoutUnit> columnWidth() const;

    void setIsAnonymous() { m_isAnonymous = true; }

    bool canCacheForLayoutState(const LayoutState&) const;
    BoxGeometry* cachedGeometryForLayoutState(const LayoutState&) const;
    void setCachedGeometryForLayoutState(LayoutState&, std::unique_ptr<BoxGeometry>) const;

protected:
    Box(std::optional<ElementAttributes>, RenderStyle&&, std::unique_ptr<RenderStyle>&& firstLineStyle, OptionSet<BaseTypeFlag>);

private:
    friend class ContainerBox;

    void setParent(ContainerBox*);
    void setNextSibling(Box*);
    void setPreviousSibling(Box*);

    class BoxRareData {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        BoxRareData() = default;

        CellSpan tableCellSpan;
        std::optional<LayoutUnit> columnWidth;
        std::unique_ptr<RenderStyle> firstLineStyle;
    };

    bool hasRareData() const { return m_hasRareData; }
    void setHasRareData(bool hasRareData) { m_hasRareData = hasRareData; }
    const BoxRareData& rareData() const;
    BoxRareData& ensureRareData();
    void removeRareData();
    
    OptionSet<BaseTypeFlag> baseTypeFlags() const { return OptionSet<BaseTypeFlag>::fromRaw(m_baseTypeFlags); }

    typedef HashMap<const Box*, std::unique_ptr<BoxRareData>> RareDataMap;

    static RareDataMap& rareDataMap();

    RenderStyle m_style;
    std::optional<ElementAttributes> m_elementAttributes;

    CheckedPtr<ContainerBox> m_parent;
    CheckedPtr<Box> m_previousSibling;
    CheckedPtr<Box> m_nextSibling;
    
    // First LayoutState gets a direct cache.
    mutable WeakPtr<LayoutState> m_cachedLayoutState;
    mutable std::unique_ptr<BoxGeometry> m_cachedGeometryForLayoutState;

    unsigned m_baseTypeFlags : 6; // OptionSet<BaseTypeFlag>
    bool m_hasRareData : 1;
    bool m_isAnonymous : 1;
};

inline bool Box::isContainingBlockForInFlow() const
{
    return isBlockContainer() || establishesFormattingContext();
}

inline bool Box::isContainingBlockForFixedPosition() const
{
    return isInitialContainingBlock() || isLayoutContainmentBox() || style().hasTransform();
}

inline bool Box::isContainingBlockForOutOfFlowPosition() const
{
    return isInitialContainingBlock() || isPositioned() || isLayoutContainmentBox() || style().hasTransform();
}

}
}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::Box& box) { return box.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
