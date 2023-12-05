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

class CSSCounterStyle;
class RenderListItem;
class StyleRuleCounterStyle;

// Used to render the list item's marker.
// The RenderListMarker always has to be a child of a RenderListItem.
class RenderListMarker final : public RenderBox {
    WTF_MAKE_ISO_ALLOCATED(RenderListMarker);
public:
    RenderListMarker(RenderListItem&, RenderStyle&&);
    virtual ~RenderListMarker();

    StringView textWithoutSuffix() const;
    StringView textWithSuffix() const { return m_textWithSuffix; }

    bool isInside() const;

    void updateMarginsAndContent();
    void addOverflowFromListMarker();

    bool isImage() const final;

    LayoutUnit lineLogicalOffsetForListItem() const { return m_lineLogicalOffsetForListItem; }
    const RenderListItem* listItem() const;

private:
    void willBeDestroyed() final;
    ASCIILiteral renderName() const final { return "RenderListMarker"_s; }
    void computePreferredLogicalWidths() final;
    bool canHaveChildren() const final { return false; }
    void paint(PaintInfo&, const LayoutPoint&) final;
    void layout() final;
    void imageChanged(WrappedImagePtr, const IntRect*) final;
    std::unique_ptr<LegacyInlineElementBox> createInlineBox() final;
    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode) const final;
    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode) const final;
    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent) final;
    bool canBeSelectionLeaf() const final { return true; }
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;

    void element() const = delete;

    void updateMargins();
    void updateContent();
    RenderBox* parentBox(RenderBox&);
    FloatRect relativeMarkerRect();
    LayoutRect localSelectionRect();

    struct TextRunWithUnderlyingString;
    TextRunWithUnderlyingString textRun() const;

    RefPtr<CSSCounterStyle> counterStyle() const;
    bool widthUsesMetricsOfPrimaryFont() const;

    String m_textWithSuffix;
    uint8_t m_textWithoutSuffixLength { 0 };
    bool m_textIsLeftToRightDirection { true };
    RefPtr<StyleImage> m_image;
    SingleThreadWeakPtr<RenderListItem> m_listItem;
    LayoutUnit m_lineOffsetForListItem;
    LayoutUnit m_lineLogicalOffsetForListItem;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderListMarker, isRenderListMarker())
