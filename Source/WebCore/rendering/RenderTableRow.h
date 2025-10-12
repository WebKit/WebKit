/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Google Inc. All rights reserved.
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
 */

#pragma once

#include "RenderTableSection.h"

namespace WebCore {

static const unsigned unsetRowIndex = 0x7FFFFFFF;
static const unsigned maxRowIndex = 0x7FFFFFFE; // 2,147,483,646

class RenderTableRow final : public RenderBox {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderTableRow);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderTableRow);
public:
    RenderTableRow(Element&, RenderStyle&&);
    RenderTableRow(Document&, RenderStyle&&);
    virtual ~RenderTableRow();

    RenderTableRow* nextRow() const;
    RenderTableRow* previousRow() const;

    RenderTableCell* firstCell() const;
    RenderTableCell* lastCell() const;

    RenderTable* table() const;

    void paintOutlineForRowIfNeeded(PaintInfo&, const LayoutPoint&);

    void setRowIndex(unsigned);
    bool rowIndexWasSet() const { return m_rowIndex != unsetRowIndex; }
    unsigned rowIndex() const;

    inline const BorderValue& borderAdjoiningTableStart() const;
    inline const BorderValue& borderAdjoiningTableEnd() const;
    const BorderValue& borderAdjoiningStartCell(const RenderTableCell&) const;
    const BorderValue& borderAdjoiningEndCell(const RenderTableCell&) const;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    RenderTableSection* section() const { return downcast<RenderTableSection>(parent()); }

    void didInsertTableCell(RenderTableCell& child, RenderObject* beforeChild);

    // Whether a row has opaque background depends on many factors, e.g. border spacing, border collapsing, missing cells, etc.
    // For simplicity, just conservatively assume all table rows are not opaque.
    bool foregroundIsKnownToBeOpaqueInRect(const LayoutRect&, unsigned) const override { return false; }
    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const override { return false; }

private:
    static RenderPtr<RenderTableRow> createTableRowWithStyle(Document&, const RenderStyle&);

    ASCIILiteral renderName() const override;
    bool canHaveChildren() const override { return true; }
    void willBeRemovedFromTree() override;
    void layout() override;

    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const override;
    RepaintRects rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds) const override;
    void computeIntrinsicLogicalWidths(LayoutUnit&, LayoutUnit&) const override { }

    bool requiresLayer() const final;
    void paint(PaintInfo&, const LayoutPoint&) override;
    void imageChanged(WrappedImagePtr, const IntRect* = 0) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void firstChild() const = delete;
    void lastChild() const = delete;
    void nextSibling() const = delete;
    void previousSibling() const = delete;

    unsigned m_rowIndex : 31;
};

inline void RenderTableRow::setRowIndex(unsigned rowIndex)
{
    if (rowIndex > maxRowIndex) [[unlikely]]
        CRASH();
    m_rowIndex = rowIndex;
}

inline unsigned RenderTableRow::rowIndex() const
{
    ASSERT(rowIndexWasSet());
    return m_rowIndex;
}

inline RenderTable* RenderTableRow::table() const
{
    RenderTableSection* section = this->section();
    if (!section)
        return nullptr;
    return downcast<RenderTable>(section->parent());
}

inline RenderTableRow* RenderTableRow::nextRow() const
{
    return downcast<RenderTableRow>(RenderBox::nextSibling());
}

inline RenderTableRow* RenderTableRow::previousRow() const
{
    return downcast<RenderTableRow>(RenderBox::previousSibling());
}

inline RenderTableRow* RenderTableSection::firstRow() const
{
    return downcast<RenderTableRow>(RenderBox::firstChild());
}

inline RenderTableRow* RenderTableSection::lastRow() const
{
    return downcast<RenderTableRow>(RenderBox::lastChild());
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderTableRow, isRenderTableRow())
