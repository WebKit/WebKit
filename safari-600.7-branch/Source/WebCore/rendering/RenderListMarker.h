/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderListMarker_h
#define RenderListMarker_h

#include "RenderBox.h"

namespace WebCore {

class RenderListItem;

String listMarkerText(EListStyleType, int value);

// Used to render the list item's marker.
// The RenderListMarker always has to be a child of a RenderListItem.
class RenderListMarker final : public RenderBox {
public:
    RenderListMarker(RenderListItem&, PassRef<RenderStyle>);
    virtual ~RenderListMarker();

    const String& text() const { return m_text; }
    String suffix() const;

    bool isInside() const;

    void updateMarginsAndContent();

private:
    void element() const = delete;

    virtual const char* renderName() const override { return "RenderListMarker"; }
    virtual void computePreferredLogicalWidths() override;

    virtual bool isListMarker() const override { return true; }
    virtual bool canHaveChildren() const override { return false; }

    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    virtual void layout() override;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override;

    virtual std::unique_ptr<InlineElementBox> createInlineBox() override;

    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    virtual bool isImage() const override;
    bool isText() const { return !isImage(); }

    virtual void setSelectionState(SelectionState) override;
    virtual LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent = true) override;
    virtual bool canBeSelectionLeaf() const override { return true; }

    void updateMargins();
    void updateContent();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    IntRect getRelativeMarkerRect();
    LayoutRect localSelectionRect();

    String m_text;
    RefPtr<StyleImage> m_image;
    RenderListItem& m_listItem;
};

RENDER_OBJECT_TYPE_CASTS(RenderListMarker, isListMarker())

} // namespace WebCore

#endif // RenderListMarker_h
