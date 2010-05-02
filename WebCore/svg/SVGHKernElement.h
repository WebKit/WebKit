/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>
   Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
   Copyright (C) 2008 Apple, Inc

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

#ifndef SVGHKernElement_h
#define SVGHKernElement_h

#if ENABLE(SVG_FONTS)
#include "SVGParserUtilities.h"
#include "SVGStyledElement.h"

#include <limits>

namespace WebCore {

    class AtomicString;
    class SVGFontData;

    // Describe an SVG <hkern> element
    struct SVGHorizontalKerningPair {
        UnicodeRanges unicodeRange1;
        HashSet<String> unicodeName1;
        HashSet<String> glyphName1;
        UnicodeRanges unicodeRange2;
        HashSet<String> unicodeName2;
        HashSet<String> glyphName2;
        float kerning;
        
        SVGHorizontalKerningPair()
            : kerning(0)
        {
        }
    };

    class SVGHKernElement : public SVGElement {
    public:
        SVGHKernElement(const QualifiedName&, Document*);
        virtual ~SVGHKernElement();

        virtual void insertedIntoDocument();
        virtual void removedFromDocument();

        virtual bool rendererIsNeeded(RenderStyle*) { return false; }

        SVGHorizontalKerningPair buildHorizontalKerningPair() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif
