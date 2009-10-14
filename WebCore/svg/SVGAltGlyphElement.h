/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2008 Apple Computer, Inc.

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

#ifndef SVGAltGlyphElement_h
#define SVGAltGlyphElement_h

#if ENABLE(SVG_FONTS)
#include "AtomicString.h"
#include "SVGTextPositioningElement.h"
#include "SVGURIReference.h"

namespace WebCore {

    class SVGGlyphElement;

    class SVGAltGlyphElement : public SVGTextPositioningElement,
                               public SVGURIReference {
    public:
        SVGAltGlyphElement(const QualifiedName&, Document*);
        virtual ~SVGAltGlyphElement();
                
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        bool childShouldCreateRenderer(Node*) const;

        const AtomicString& glyphRef() const;
        void setGlyphRef(const AtomicString&, ExceptionCode&);
        const AtomicString& format() const;
        void setFormat(const AtomicString&, ExceptionCode&);
    
        SVGGlyphElement* glyphElement() const;

    private:    
        // SVGURIReference
        ANIMATED_PROPERTY_DECLARATIONS(SVGAltGlyphElement, SVGURIReferenceIdentifier, XLinkNames::hrefAttrString, String, Href, href)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
