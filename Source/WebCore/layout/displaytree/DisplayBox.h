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

#include "LayoutPoint.h"
#include "LayoutRect.h"
#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {
class LayoutContext;
class BlockFormattingContext;
}

namespace Display {

class Box {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    friend class Layout::LayoutContext;
    friend class Layout::BlockFormattingContext;

    ~Box();

    LayoutRect rect() const { return m_rect; }

    LayoutUnit top() const { return m_rect.y(); }
    LayoutUnit left() const { return m_rect.x(); }
    LayoutUnit bottom() const { return m_rect.maxY(); }
    LayoutUnit right() const { return m_rect.maxX(); }

    LayoutPoint topLeft() const { return m_rect.location(); }
    LayoutPoint bottomRight() const { return m_rect.location(); }

    LayoutSize size() const { return m_rect.size(); }
    LayoutUnit width() const { return m_rect.width(); }
    LayoutUnit height() const { return m_rect.height(); }

    LayoutUnit marginTop() const { return m_marginTop; }
    LayoutUnit marginLeft() const { return m_marginLeft; }
    LayoutUnit marginBottom() const { return m_marginBottom; }
    LayoutUnit marginRight() const { return m_marginRight; }

    LayoutRect marginBox() const;
    LayoutRect borderBox() const;
    LayoutRect paddingBox() const;
    LayoutRect contentBox() const;

private:
    Box();

    void setRect(const LayoutRect& rect) { m_rect = rect; }
    void setTopLeft(const LayoutPoint& topLeft) { m_rect.setLocation(topLeft); }
    void setTop(LayoutUnit top) { m_rect.setY(top); }
    void setLeft(LayoutUnit left) { m_rect.setX(left); }
    void setSize(const LayoutSize& size) { m_rect.setSize(size); }
    void setWidth(LayoutUnit width) { m_rect.setWidth(width); }
    void setHeight(LayoutUnit height) { m_rect.setHeight(height); }

    void setMarginTop(LayoutUnit marginTop) { m_marginTop = marginTop; }
    void setMarginLeft(LayoutUnit marginLeft) { m_marginLeft = marginLeft; }
    void setMarginBottom(LayoutUnit marginBottom) { m_marginBottom = marginBottom; }
    void setMarginRight(LayoutUnit marginRight) { m_marginRight = marginRight; }

    void setBorderTop(LayoutUnit borderTop) { m_borderTop = borderTop; }
    void setBorderLeft(LayoutUnit borderLeft) { m_borderLeft = borderLeft; }
    void setBorderBottom(LayoutUnit borderBottom) { m_borderBottom = borderBottom; }
    void setBorderRight(LayoutUnit borderRight) { m_borderRight = borderRight; }

    void setPaddingTop(LayoutUnit paddingTop) { m_paddingTop = paddingTop; }
    void setPaddingLeft(LayoutUnit paddingLeft) { m_paddingLeft = paddingLeft; }
    void setPaddingBottom(LayoutUnit paddingBottom) { m_paddingBottom = paddingBottom; }
    void setPaddingRight(LayoutUnit paddingRight) { m_paddingRight = paddingRight; }

    LayoutRect m_rect;
    LayoutUnit m_marginTop;
    LayoutUnit m_marginLeft;
    LayoutUnit m_marginBottom;
    LayoutUnit m_marginRight;

    LayoutUnit m_borderTop;
    LayoutUnit m_borderLeft;
    LayoutUnit m_borderBottom;
    LayoutUnit m_borderRight;

    LayoutUnit m_paddingTop;
    LayoutUnit m_paddingLeft;
    LayoutUnit m_paddingBottom;
    LayoutUnit m_paddingRight;
};

}
}
#endif
