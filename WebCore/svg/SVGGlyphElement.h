/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>
   Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#ifndef SVGGlyphElement_h
#define SVGGlyphElement_h

#if ENABLE(SVG_FONTS)
#include "SVGStyledElement.h"

#include "Path.h"

namespace WebCore {
    
    class SVGGlyphElement : public SVGStyledElement {
    public:
        SVGGlyphElement(const QualifiedName&, Document*);

        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
    };

    // Describe a SVG <glyph> element
    struct SVGGlyphIdentifier {
        enum Orientation { Vertical, Horizontal, Both };
        enum ArabicForm { Initial, Medial, Terminal, Isolated };

        SVGGlyphIdentifier()
            : orientation(Both)
            , arabicForm(Initial)
            , horizontalAdvanceX(0.0f)
            , verticalOriginX(0.0f)
            , verticalOriginY(0.0f)
            , verticalAdvanceY(0.0f)
        {
        }

        // 'orientation' property;
        Orientation orientation;

        // 'arabic-form' property
        ArabicForm arabicForm;

        // 'horiz-adv-x' property
        float horizontalAdvanceX;

        // 'vert-origin-x' property
        float verticalOriginX;

        // 'vert-origin-y' property
        float verticalOriginY;

        // 'vert-adv-y' property
        float verticalAdvanceY;

        // 'd' property
        Path pathData;

        // 'lang' property
        Vector<String> languages;
    };

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif

// vim:ts=4:noet
