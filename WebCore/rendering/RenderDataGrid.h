/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderDataGrid_h
#define RenderDataGrid_h

#if ENABLE(DATAGRID)

#include "HTMLDataGridElement.h"
#include "RenderBlock.h"
#include "ScrollbarClient.h"
#include "StyleImage.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderDataGrid : public RenderBlock, private ScrollbarClient {
public:
    RenderDataGrid(Element*);
    ~RenderDataGrid();
    
    virtual const char* renderName() const { return "RenderDataGrid"; }
    virtual bool canHaveChildren() const { return false; }
    virtual void calcPrefWidths();
    virtual void layout();
    virtual void paintObject(PaintInfo&, int tx, int ty);

    void columnsChanged();

private:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    virtual bool requiresForcedStyleRecalcPropagation() const { return true; }

    RenderStyle* columnStyle(DataGridColumn*);
    RenderStyle* headerStyle(DataGridColumn*);
    void recalcStyleForColumns();
    void recalcStyleForColumn(DataGridColumn*);

    void layoutColumns();
    void paintColumnHeaders(PaintInfo&, int tx, int ty);
    void paintColumnHeader(DataGridColumn*, PaintInfo&, int tx, int ty);

    HTMLDataGridElement* gridElement() const { return static_cast<HTMLDataGridElement*>(node()); }

    // ScrollbarClient interface.
    virtual int scrollSize(ScrollbarOrientation orientation) const;
    virtual void setScrollOffsetFromAnimation(const IntPoint&);
    virtual void valueChanged(Scrollbar*);
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
    virtual bool isActive() const;
    virtual bool scrollbarCornerPresent() const { return false; } // We don't support resize on data grids yet.  If we did this would have to change.
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const;

    RefPtr<Scrollbar> m_vBar;
};

}

#endif

#endif // RenderDataGrid_h
