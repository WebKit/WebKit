/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGTextFragment_h
#define SVGTextFragment_h

#if ENABLE(SVG)
#include "AffineTransform.h"

namespace WebCore {

// A SVGTextFragment describes a text fragment of a RenderSVGInlineText which can be rendered at once.
struct SVGTextFragment {
    SVGTextFragment()
        : positionListOffset(0)
        , length(0)
        , x(0)
        , y(0)
        , width(0)
        , height(0)
    {
    }

    // The first rendered character starts at RenderSVGInlineText::characters() + positionListOffset.
    unsigned positionListOffset;
    unsigned length;

    float x;
    float y;
    float width;
    float height;

    // Includes rotation/glyph-orientation-(horizontal|vertical) transforms, lengthAdjust="spacingAndGlyphs" (for textPath only),
    // as well as orientation related shifts (see SVGTextLayoutEngine, which builds this transformation).
    AffineTransform transform;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
