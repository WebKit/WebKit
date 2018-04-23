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

namespace Display {

class Box {
    WTF_MAKE_ISO_ALLOCATED(Box);
public:
    void setRect(const LayoutRect&);
    void setTopLeft(const LayoutPoint&);
    void setTop(LayoutUnit);
    void setLeft(LayoutUnit);
    void setSize(const LayoutSize&);
    void setWidth(LayoutUnit);
    void setHeight(LayoutUnit);

    LayoutRect rect() const;

    LayoutUnit top() const;
    LayoutUnit left() const;
    LayoutUnit bottom() const;
    LayoutUnit right() const;

    LayoutPoint topLeft() const;
    LayoutPoint bottomRight() const;

    LayoutSize size() const;
    LayoutUnit width() const;
    LayoutUnit height() const;

    void setMarginTop(LayoutUnit);
    void setMarginLeft(LayoutUnit);
    void setMarginBottom(LayoutUnit);
    void setMarginRight(LayoutUnit);

    LayoutUnit marginTop() const;
    LayoutUnit marginLeft() const;
    LayoutUnit marginBottom() const;
    LayoutUnit marginRight() const;

    LayoutRect marginBox() const;
    LayoutRect borderBox() const;
    LayoutRect paddingBox() const;
    LayoutRect contentBox() const;

    void setParent(Box&);
    void setNextSibling(Box&);
    void setPreviousSibling(Box&);
    void setFirstChild(Box&);
    void setLastChild(Box&);

    Box* parent() const;
    Box* nextSibling() const;
    Box* previousSibling() const;
    Box* firstChild() const;
    Box* lastChild() const;
};

}
}
#endif
