/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef RenderSVGTSpan_h
#define RenderSVGTSpan_h

#ifdef SVG_SUPPORT
#include "RenderSVGInline.h"

namespace WebCore {
class RenderSVGTSpan : public RenderSVGInline {
public:
    RenderSVGTSpan(Node*);
    virtual const char* renderName() const { return "RenderSVGTSpan"; }
    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty);
};
}

#endif // SVG_SUPPORT
#endif // !RenderSVGTSpan_h
