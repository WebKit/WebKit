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

#include "LayoutReplaced.h"
#include "RenderStyle.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Layout {

class Container;
class TreeBuilder;

class Box : public CanMakeWeakPtr<Box> {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    enum class ElementType {
        Document,
        Body,
        TableCell,
        TableColumn,
        TableRow,
        TableColumnGroup,
        TableRowGroup,
        TableHeaderGroup,
        TableFooterGroup,
        Image,
        IFrame,
        GenericElement
    };

    struct ElementAttributes {
        ElementType elementType;
    };

    enum BaseTypeFlag {
        BoxFlag               = 1 << 0,
        ContainerFlag         = 1 << 1,
        BlockContainerFlag    = 1 << 2,
        InlineBoxFlag         = 1 << 3,
        InlineContainerFlag   = 1 << 4,
        LineBreakBoxFlag      = 1 << 5
    };
    typedef unsigned BaseTypeFlags;

    Box(Optional<ElementAttributes>, RenderStyle&&);
    virtual ~Box();

    bool establishesFormattingContext() const;
    bool establishesBlockFormattingContext() const;
    bool establishesBlockFormattingContextOnly() const;
    virtual bool establishesInlineFormattingContext() const { return false; }

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

    bool isFloatingOrOutOfFlowPositioned() const { return isFloatingPositioned() || isOutOfFlowPositioned(); }

    const Container* containingBlock() const;
    const Container& formattingContextRoot() const;
    const Container& initialContainingBlock() const;

    bool isDescendantOf(const Container&) const;

    bool isAnonymous() const { return !m_elementAttributes; }

    bool isBlockLevelBox() const;
    bool isInlineLevelBox() const;
    bool isInlineBlockBox() const;
    bool isBlockContainerBox() const;
    bool isInitialContainingBlock() const;

    bool isDocumentBox() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Document; }
    bool isBodyBox() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Body; }
    bool isTableCell() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::TableCell; }
    bool isReplaced() const { return isImage() || isIFrame(); }
    bool isIFrame() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::IFrame; }
    bool isImage() const { return m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Image; }

    const Container* parent() const { return m_parent; }
    const Box* nextSibling() const { return m_nextSibling; }
    const Box* nextInFlowSibling() const;
    const Box* nextInFlowOrFloatingSibling() const;
    const Box* previousSibling() const { return m_previousSibling; }
    const Box* previousInFlowSibling() const;
    const Box* previousInFlowOrFloatingSibling() const;

    bool isContainer() const { return m_baseTypeFlags & ContainerFlag; }
    bool isBlockContainer() const { return m_baseTypeFlags & BlockContainerFlag; }
    bool isInlineBox() const { return m_baseTypeFlags & InlineBoxFlag; }
    bool isInlineContainer() const { return m_baseTypeFlags & InlineContainerFlag; }
    bool isLineBreakBox() const { return m_baseTypeFlags & LineBreakBoxFlag; }

    bool isPaddingApplicable() const;
    bool isOverflowVisible() const;

    const RenderStyle& style() const { return m_style; }

    const Replaced* replaced() const { return m_replaced.get(); }
    // FIXME: Temporary until after intrinsic size change is tracked by Replaced.
    Replaced* replaced() { return m_replaced.get(); }

    void setParent(Container& parent) { m_parent = &parent; }
    void setNextSibling(Box& nextSibling) { m_nextSibling = &nextSibling; }
    void setPreviousSibling(Box& previousSibling) { m_previousSibling = &previousSibling; }

protected:
    Box(Optional<ElementAttributes>, RenderStyle&&, BaseTypeFlags);

private:
    RenderStyle m_style;
    Optional<ElementAttributes> m_elementAttributes;

    Container* m_parent { nullptr };
    Box* m_previousSibling { nullptr };
    Box* m_nextSibling { nullptr };

    std::unique_ptr<Replaced> m_replaced;

    unsigned m_baseTypeFlags : 5;
};

}
}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::Box& box) { return box.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
