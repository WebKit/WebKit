/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderTableRow_h
#define RenderTableRow_h

#include "RenderTableSection.h"

namespace WebCore {

static const unsigned unsetRowIndex = 0x7FFFFFFF;
static const unsigned maxRowIndex = 0x7FFFFFFE; // 2,147,483,646

class RenderTableRow final : public RenderBox {
public:
    RenderTableRow(Element&, PassRef<RenderStyle>);
    RenderTableRow(Document&, PassRef<RenderStyle>);

    RenderTableRow* nextRow() const;
    RenderTableRow* previousRow() const;

    RenderTableCell* firstCell() const;
    RenderTableCell* lastCell() const;

    RenderTableSection* section() const { return toRenderTableSection(parent()); }
    RenderTable* table() const { return toRenderTable(parent()->parent()); }

    void paintOutlineForRowIfNeeded(PaintInfo&, const LayoutPoint&);

    static RenderTableRow* createAnonymousWithParentRenderer(const RenderObject*);
    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const override
    {
        return createAnonymousWithParentRenderer(parent);
    }

    void setRowIndex(unsigned rowIndex)
    {
        if (UNLIKELY(rowIndex > maxRowIndex))
            CRASH();

        m_rowIndex = rowIndex;
    }

    bool rowIndexWasSet() const { return m_rowIndex != unsetRowIndex; }
    unsigned rowIndex() const
    {
        ASSERT(rowIndexWasSet());
        return m_rowIndex;
    }

    const BorderValue& borderAdjoiningTableStart() const
    {
        if (section()->hasSameDirectionAs(table()))
            return style().borderStart();

        return style().borderEnd();
    }

    const BorderValue& borderAdjoiningTableEnd() const
    {
        if (section()->hasSameDirectionAs(table()))
            return style().borderEnd();

        return style().borderStart();
    }

    const BorderValue& borderAdjoiningStartCell(const RenderTableCell*) const;
    const BorderValue& borderAdjoiningEndCell(const RenderTableCell*) const;

    virtual void addChild(RenderObject* child, RenderObject* beforeChild = 0) override;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

private:
    virtual const char* renderName() const override { return (isAnonymous() || isPseudoElement()) ? "RenderTableRow (anonymous)" : "RenderTableRow"; }

    virtual bool isTableRow() const override { return true; }

    virtual bool canHaveChildren() const override { return true; }
    virtual void willBeRemovedFromTree() override;

    virtual void layout() override;
    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;

    virtual bool requiresLayer() const override { return hasOverflowClip() || hasTransform() || hasHiddenBackface() || hasClipPath() || createsGroup(); }

    virtual void paint(PaintInfo&, const LayoutPoint&) override;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void firstChild() const = delete;
    void lastChild() const = delete;
    void nextSibling() const = delete;
    void previousSibling() const = delete;

    unsigned m_rowIndex : 31;
};

RENDER_OBJECT_TYPE_CASTS(RenderTableRow, isTableRow())

inline RenderTableRow* RenderTableSection::firstRow() const
{
    return toRenderTableRow(RenderBox::firstChild());
}

inline RenderTableRow* RenderTableSection::lastRow() const
{
    return toRenderTableRow(RenderBox::lastChild());
}

inline RenderTableRow* RenderTableRow::nextRow() const
{
    return toRenderTableRow(RenderBox::nextSibling());
}

inline RenderTableRow* RenderTableRow::previousRow() const
{
    return toRenderTableRow(RenderBox::previousSibling());
}

} // namespace WebCore

#endif // RenderTableRow_h
