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
struct ListMarkerTextContent;

// Used to render the list item's marker.
// The RenderListOutsideMarker always has to be a child of a RenderListItem.
class RenderListOutsideMarker final : public RenderBox {
    WTF_MAKE_TZONE_ALLOCATED(RenderListOutsideMarker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderListOutsideMarker);
public:
    RenderListOutsideMarker(RenderListItem&, UniqueRef<Style::ComputedStyle>&&);
    virtual ~RenderListOutsideMarker();

    void updateContent();

    bool isImage() const final;

    NODELETE RenderBlockFlow* contentContainer() const;

    NODELETE RenderListItem* listItem() const;

    std::pair<float, float> layoutBounds() const { return m_layoutBounds; }

private:
    void willBeDestroyed() final;
    ASCIILiteral renderName() const final { return "RenderListMarker"_s; }
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidthContributions() final;
    bool canHaveChildren() const final { return true; }
    bool canHaveGeneratedChildren() const final { return true; }
    void paint(PaintInfo&, const LayoutPoint&) final;
    void layout() final;
    void imageChanged(WrappedImagePtr, const IntRect*) final;
    void forEachStyleImage(NOESCAPE const ScopedLambda<void(Style::Image&)>&) override final;
    NODELETE LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent) final;
    bool canBeSelectionLeaf() const final { return true; }
    void styleWillChange(Style::Difference, const Style::ComputedStyle& newStyle) final;
    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) final;
    Node* nodeForHitTest() const final;
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override { ASSERT_NOT_REACHED(); return { }; }
    std::pair<float, float> layoutBoundForTextContent(String) const;

    void element() const = delete;

    std::pair<LayoutUnit, LayoutUnit> inlineMargins() const;
    LayoutSize markerImageSize(const Style::Image&) const;
    void updateContentContainerText(const ListMarkerTextContent&);
    void setContentContainerImageSize(LayoutSize);
    void layoutContentContainer(RenderBlockFlow&);

private:
    SingleThreadWeakPtr<RenderListItem> m_listItem;
    std::pair<float, float> m_layoutBounds;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderListOutsideMarker, isRenderListOutsideMarker())
