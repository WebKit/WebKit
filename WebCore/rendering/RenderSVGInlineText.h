/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *           (C) 2008 Rob Buis <buis@kde.org>
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

#ifndef RenderSVGInlineText_h
#define RenderSVGInlineText_h

#if ENABLE(SVG)

#include "RenderText.h"

namespace WebCore {
class RenderSVGInlineText : public RenderText {
public:
    RenderSVGInlineText(Node*, PassRefPtr<StringImpl>);
    virtual const char* renderName() const { return "RenderSVGInlineText"; }
        
    virtual void styleDidChange(StyleDifference, const RenderStyle*);

    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty);
    virtual void absoluteQuads(Vector<FloatQuad>&);

    virtual bool requiresLayer() const { return false; }
    virtual IntRect selectionRectForRepaint(RenderBoxModelObject* repaintContainer, bool clipToVisibleContent = true);
    virtual bool isSVGText() const { return true; }

    virtual IntRect localCaretRect(InlineBox*, int caretOffset, int* extraWidthToEndOfLine = 0);
    virtual VisiblePosition positionForPoint(const IntPoint&);

    virtual void destroy();

private:
    virtual InlineTextBox* createTextBox();
    IntRect computeRepaintRectForRange(RenderBoxModelObject* repaintContainer, int startPos, int endPos);
    FloatQuad computeRepaintQuadForRange(RenderBoxModelObject* repaintContainer, int startPos, int endPos);
};

}

#endif // ENABLE(SVG)

#endif // !RenderSVGInlineText_h
