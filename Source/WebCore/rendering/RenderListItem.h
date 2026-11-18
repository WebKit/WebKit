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
#include "RenderListOutsideMarker.h"

namespace WebCore {

class HTMLOListElement;

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

    Vector<CheckedRef<RenderListItem>> updateListMarkerNumbers();

    static void updateItemValuesForOrderedList(const HTMLOListElement&);
    static unsigned itemCountForOrderedList(const HTMLOListElement&);
    static int startForReversedOrderedList(const HTMLOListElement&);

    UniqueRef<Style::ComputedStyle> computeMarkerStyle() const;

    RenderBoxModelObject* markerRenderer() const { return m_marker.get(); }
    void setMarkerRenderer(RenderBoxModelObject& marker)
    {
        m_marker = marker;
        m_excludedMarkerPosition = { };
    }
    NODELETE RenderListOutsideMarker* markerBox() const;

    // Fills in what the marker shows, in whichever shape it was built.
    void updateMarkerContent();

    bool isInReversedOrderedList() const;

    struct FirstFormattedLineCandidate {
        CheckedPtr<RenderBlock> parent;
        CheckedPtr<RenderBlock> fallbackParent;
    };
    static FirstFormattedLineCandidate firstFormattedLineRootFor(RenderBlock& blockContainer, const RenderBoxModelObject& marker);
    static Vector<CheckedPtr<RenderListItem>> listItemsWithExcludedMarkerFor(const RenderBlockFlow& inlineRoot, const Vector<SingleThreadWeakPtr<RenderListItem>>&);

    // Where the line our excluded marker is aligned with put it.
    struct ExcludedMarkerPosition {
        SingleThreadWeakPtr<RenderBlockFlow> firstFormattedLineRoot;
        FloatPoint topLeft;
        bool isLineStartConstrainedByFloat { false };
    };
    void setExcludedMarkerPosition(ExcludedMarkerPosition&&);
    const std::optional<ExcludedMarkerPosition>& excludedMarkerPosition() const LIFETIME_BOUND { return m_excludedMarkerPosition; }

private:
    ASCIILiteral renderName() const final { return "RenderListItem"_s; }

    void paint(PaintInfo&, const LayoutPoint&) final;
    void paintObject(PaintInfo&, const LayoutPoint&) final;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;
    bool hasLineIfEmpty() const final;

    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;
    void layoutExcludedChildren(RelayoutChildren) final;

    void placeExcludedMarker(RenderListOutsideMarker&);
    NODELETE RenderListOutsideMarker* excludedMarker() const;
    void invalidateExcludedMarkerContainer();

    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) final;

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const final;

    void updateValueAndMarkerContent();
    void updateValueNow() const;
    void usedCounterDirectivesChanged();

    SingleThreadWeakPtr<RenderBoxModelObject> m_marker;
    std::optional<ExcludedMarkerPosition> m_excludedMarkerPosition;
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

inline int RenderListItem::value() const
{
    if (!m_value)
        updateValueNow();
    return m_value.value();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderListItem, isRenderListItem())
