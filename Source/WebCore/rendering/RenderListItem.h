/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "RenderBlockFlow.h"

namespace WebCore {

class HTMLOListElement;
class RenderImage;

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

class RenderListItem final : public RenderBlockFlow {
    WTF_MAKE_TZONE_ALLOCATED(RenderListItem);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderListItem);
public:
    RenderListItem(Element&, UniqueRef<Style::ComputedStyle>&&);
    virtual ~RenderListItem();

    int value() const;
    void updateValue();

    WEBCORE_EXPORT String markerText(ListMarkerIncludeSuffix = ListMarkerIncludeSuffix::Yes) const;
    static String markerText(const RenderObject& marker, ListMarkerIncludeSuffix = ListMarkerIncludeSuffix::Yes);

    Vector<CheckedRef<RenderListItem>> updateListMarkerNumbers();

    static void updateItemValuesForOrderedList(const HTMLOListElement&);
    static unsigned itemCountForOrderedList(const HTMLOListElement&);
    static int startForReversedOrderedList(const HTMLOListElement&);

    UniqueRef<Style::ComputedStyle> computeMarkerStyle() const;

    RenderBoxModelObject* markerRenderer() const { return m_marker.get(); }
    void setMarkerRenderer(RenderBoxModelObject& marker)
    {
        m_marker = marker;
        m_outsideMarkerPosition = { };
    }
    // The outside marker. We place it against our first formatted line after layout.
    NODELETE RenderBlockFlow* markerBox() const;
    static RenderListItem* listItemForMarker(const RenderObject&);

    // Fills in what the marker shows, in whichever shape it was built.
    void updateMarkerContent();
    // The image an image marker shows.
    RenderImage* markerImage() const;

    bool isInReversedOrderedList() const;

    struct FirstFormattedLineCandidate {
        CheckedPtr<RenderBlock> parent;
        CheckedPtr<RenderBlock> fallbackParent;
    };
    static FirstFormattedLineCandidate firstFormattedLineRootFor(RenderBlock& blockContainer, const RenderBoxModelObject& marker);
    static Vector<CheckedPtr<RenderListItem>> listItemsWithOutsideMarkerFor(const RenderBlockFlow& inlineRoot, const Vector<SingleThreadWeakPtr<RenderListItem>>&);

    // Where the line our outside marker is aligned with put it.
    struct OutsideMarkerPosition {
        SingleThreadWeakPtr<RenderBlockFlow> firstFormattedLineRoot;
        FloatPoint topLeft;
        bool isLineStartConstrainedByFloat { false };
    };
    void setOutsideMarkerPosition(OutsideMarkerPosition&&);
    const std::optional<OutsideMarkerPosition>& outsideMarkerPosition() const LIFETIME_BOUND { return m_outsideMarkerPosition; }
    // The layout bounds of our outside marker on its line, computed when the marker is laid out.
    std::pair<float, float> markerLayoutBounds() const { return m_markerLayoutBounds; }

    // What layoutBlock does for the outside marker of a list item whose content the block formatting context lays out: the marker is laid
    // out before the content, its line of its own in quirks mode goes above the block children, and it goes against its first formatted
    // line once the content and the boxes in it have their geometry.
    void layoutOutsideMarker(RelayoutChildren);
    bool markerNeedsOwnLine() const;
    // The line of its own the marker gets above the block children in quirks mode, none otherwise.
    LayoutUnit markerOwnLineBlockSize() const { return markerNeedsOwnLine() ? lineHeight() : 0_lu; }
    void placeOutsideMarkerAfterLayout();

private:
    ASCIILiteral renderName() const final { return "RenderListItem"_s; }

    void paint(PaintInfo&, const LayoutPoint&) final;
    void paintObject(PaintInfo&, const LayoutPoint&) final;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;
    bool hasLineIfEmpty() const final;

    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    void layoutExcludedChildren(RelayoutChildren) final;

    void placeOutsideMarker(RenderBlockFlow&);
    void invalidateOutsideMarkerContainer();
    LayoutSize markerImageSize(const Style::Image&) const;

    void willBeDestroyed() final;
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) final;
    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) final;
    void forEachStyleImage(NOESCAPE const ScopedLambda<void(Style::Image&)>&) final;

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const final;

    void updateValueAndMarkerContent();
    void updateValueNow() const;
    void usedCounterDirectivesChanged();

    SingleThreadWeakPtr<RenderBoxModelObject> m_marker;
    std::optional<OutsideMarkerPosition> m_outsideMarkerPosition;
    std::pair<float, float> m_markerLayoutBounds;
    mutable std::optional<int> m_value;
};

NODELETE bool isHTMLListElement(const Node&);

// The room an image marker keeps between itself and the content it labels.
constexpr int listMarkerImagePadding = 7;
ListMarkerTextContent listMarkerTextContent(const Style::ComputedStyle& markerStyle, RenderListItem&);
NODELETE bool listMarkerHasContent(const Style::ComputedStyle& markerStyle, Document&);
bool listMarkerSynthesizesGlyph(const Style::ComputedStyle& markerStyle, Document&);
RefPtr<Style::Image> listMarkerImage(const Style::ComputedStyle& markerStyle);
bool listMarkerIsDisclosure(const Style::ComputedStyle& markerStyle, Document&);
bool listMarkerIsDisclosure(const RenderElement*);
void setListMarkerInlineMargins(Style::ComputedStyle& markerStyle, WritingMode listItemWritingMode, LayoutUnit marginStart, LayoutUnit marginEnd);

// How far an outside marker's start edge is from the start edge of the line it goes on, in its list item's inline direction. It is negative: the marker hangs off the start of the line.
LayoutUnit outsideMarkerMarginStart(const RenderBlockFlow&);
// The offset from the block-start edge of an outside marker's primary font box (its image, for an image marker) to the marker's own block-start edge, so that box can be placed instead of the marker. A marker with `content` is placed by its own box, so this is 0.
LayoutUnit outsideMarkerContentLogicalTop(const RenderBlockFlow&);

inline int RenderListItem::value() const
{
    if (!m_value)
        updateValueNow();
    return m_value.value();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderListItem, isRenderListItem())
