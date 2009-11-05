/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGStyledElement_h
#define SVGStyledElement_h

#if ENABLE(SVG)
#include "HTMLNames.h"
#include "SVGElement.h"
#include "SVGStylable.h"

namespace WebCore {

    extern char SVGStyledElementIdentifier[];
    class SVGResource;

    class SVGStyledElement : public SVGElement,
                             public SVGStylable {
    public:
        SVGStyledElement(const QualifiedName&, Document*);
        virtual ~SVGStyledElement();
        
        virtual bool isStyled() const { return true; }
        virtual bool supportsMarkers() const { return false; }

        virtual PassRefPtr<CSSValue> getPresentationAttribute(const String& name);
        virtual CSSStyleDeclaration* style() { return StyledElement::style(); }

        bool isKnownAttribute(const QualifiedName&);

        virtual bool rendererIsNeeded(RenderStyle*);
        virtual SVGResource* canvasResource() { return 0; }
        
        virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
        virtual void parseMappedAttribute(MappedAttribute*);

        virtual void svgAttributeChanged(const QualifiedName&);

        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        // Centralized place to force a manual style resolution. Hacky but needed for now.
        PassRefPtr<RenderStyle> resolveStyle(RenderStyle* parentStyle);

        void invalidateResourcesInAncestorChain() const;        
        virtual void detach();
                                 
        void setInstanceUpdatesBlocked(bool);
        
    protected:
        virtual bool hasRelativeValues() const { return true; }
        
        static int cssPropertyIdForSVGAttributeName(const QualifiedName&);

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGStyledElement, SVGStyledElementIdentifier, HTMLNames::classAttrString, String, ClassName, className)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGStyledElement
