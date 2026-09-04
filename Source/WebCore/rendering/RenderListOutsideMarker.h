/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "RenderBox.h"

namespace WebCore {

class CSSRegisteredCounterStyle;
class RenderBlockFlow;
class RenderListItem;
class StyleRuleCounterStyle;

enum class ListMarkerIncludeSuffix : bool { No, Yes };

struct ListMarkerTextContent {
    String textWithSuffix;
    uint32_t textWithoutSuffixLength { 0 };
    bool isEmpty() const
    {
        return textWithSuffix.isEmpty();
    }

    StringView textWithoutSuffix() const LIFETIME_BOUND
    {
        return StringView { textWithSuffix }.left(textWithoutSuffixLength);
    }
};

// Used to render the list item's marker.
// The RenderListOutsideMarker always has to be a child of a RenderListItem.
class RenderListOutsideMarker final : public RenderBox {
    WTF_MAKE_TZONE_ALLOCATED(RenderListOutsideMarker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderListOutsideMarker);
public:
    RenderListOutsideMarker(RenderListItem&, Style::ComputedStyle&&);
    virtual ~RenderListOutsideMarker();

    using IncludeSuffix = ListMarkerIncludeSuffix;
    String textContent(IncludeSuffix includeSuffix = IncludeSuffix::Yes) const
    {
        return includeSuffix == IncludeSuffix::Yes ? m_textContent.textWithSuffix : m_textContent.textWithoutSuffix().toString();
    }

    bool isDisclosureMarker() const;
    bool synthesizesGlyph() const;

    void updateInlineMarginsAndContent();

    bool isImage() const final;

    // True when the ::marker's `content` property generates the marker box contents
    // (css-lists-3 §3.3). In that case the contents live in an anonymous inline-block
    // child (contentContainer()) that this marker lays out and paints itself.
    bool hasContentProperty() const;
    bool needsContentContainer() const;

    RenderBlockFlow* contentContainer() const;

    LayoutUnit lineLogicalOffsetForListItem() const { return m_lineLogicalOffsetForListItem; }
    RenderListItem* NODELETE listItem() const;

    std::pair<float, float> layoutBounds() const { return m_layoutBounds; }

    struct ExcludedPosition {
        SingleThreadWeakPtr<RenderBlockFlow> firstFormattedLineRoot;
        FloatPoint topLeft;
        float lineStartInset { 0 };
    };
    void setExcludedPosition(ExcludedPosition);
    std::optional<ExcludedPosition> excludedPosition() const { return m_excludedPosition; }

    void invalidateExcludedMarkerContainer();

    bool shouldCollapseAnonymousBlockParent() const { return m_shouldCollapseAnonymousBlockParent; }
    void setShouldCollapseAnonymousBlockParent(bool value)
    {
        if (value) {
            ASSERT(parent());
            ASSERT(parent()->isAnonymousBlock());
        }
        m_shouldCollapseAnonymousBlockParent = value;
    }

private:
    void willBeDestroyed() final;
    ASCIILiteral renderName() const final { return "RenderListMarker"_s; }
    void computeIntrinsicLogicalWidthContributions() final;
    bool canHaveChildren() const final { return needsContentContainer(); }
    bool canHaveGeneratedChildren() const final { return true; }
    void paint(PaintInfo&, const LayoutPoint&) final;
    void layout() final;
    void imageChanged(WrappedImagePtr, const IntRect*) final;
    LayoutRect NODELETE selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent) final;
    bool canBeSelectionLeaf() const final { return true; }
    void styleWillChange(Style::Difference, const Style::ComputedStyle& newStyle) final;
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) final;
    Node* nodeForHitTest() const final;
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override { ASSERT_NOT_REACHED(); return { }; }
    std::pair<float, float> layoutBoundForTextContent(String) const;

    void element() const = delete;

    void updateInlineMargins();
    void updateContent();
    void updateContentContainerText();
    RenderBox* parentBox(RenderBox&);
    void layoutContentContainer(RenderBlockFlow&);

    FloatRect relativeMarkerRect();
    LayoutRect NODELETE localSelectionRect();

    RefPtr<CSSRegisteredCounterStyle> counterStyle() const;
    bool textNeedsBidiResolution() const;

private:
    ListMarkerTextContent m_textContent;
    RefPtr<Style::Image> m_image;

    SingleThreadWeakPtr<RenderListItem> m_listItem;
    LayoutUnit m_lineOffsetForListItem;
    LayoutUnit m_lineLogicalOffsetForListItem;
    std::pair<float, float> m_layoutBounds;
    std::optional<ExcludedPosition> m_excludedPosition;
    bool m_shouldCollapseAnonymousBlockParent { false };
};

// The room an image marker keeps between itself and the content it labels.
constexpr int listMarkerImagePadding = 7;
ListMarkerTextContent listMarkerTextContent(const Style::ComputedStyle& markerStyle, RenderListItem&);
bool listMarkerSynthesizesGlyph(const Style::ComputedStyle& markerStyle);
bool listMarkerIsDisclosure(const Style::ComputedStyle& markerStyle, Document&);
bool listMarkerIsDisclosure(const RenderElement*);
void setListMarkerInlineMargins(Style::ComputedStyle& markerStyle, WritingMode listItemWritingMode, LayoutUnit marginStart, LayoutUnit marginEnd);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderListOutsideMarker, isRenderListOutsideMarker())
