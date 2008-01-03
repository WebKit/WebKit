/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool topLevel = true);
    virtual bool requiresLayer() { return false; }
    virtual IntRect selectionRect(bool clipToVisibleContent = true);
    virtual bool isSVGText() const { return true; }
    virtual InlineTextBox* createInlineTextBox();

    virtual IntRect caretRect(int offset, EAffinity, int* extraWidthToEndOfLine = 0);
    virtual VisiblePosition positionForCoordinates(int x, int y);

private:
    IntRect computeAbsoluteRectForRange(int startPos, int endPos);
};

}

#endif // ENABLE(SVG)

#endif // !RenderSVGInlineText_h
