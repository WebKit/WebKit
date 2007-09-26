/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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

#include "AffineTransform.h"
#include "Path.h"
#include "SVGElement.h"
#include "SVGLength.h"
#include "SVGResource.h"
#include "SVGStylable.h"

namespace WebCore {

    class CSSStyleDeclaration;
    class RenderPath;

    class SVGStyledElement : public SVGElement {
    public:
        SVGStyledElement(const QualifiedName&, Document*);
        virtual ~SVGStyledElement();
        
        virtual bool isStyled() const { return true; }
        virtual bool supportsMarkers() const { return false; }

        // 'SVGStylable' functions
        // These need to be implemented.
        virtual bool rendererIsNeeded(RenderStyle*);
        virtual Path toPathData() const { return Path(); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource() { return 0; }
        
        virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
        virtual void parseMappedAttribute(MappedAttribute*);

        virtual void notifyAttributeChange() const;
        void notifyResourceParentIfExistant() const;

        virtual void attributeChanged(Attribute*, bool preserveDecls = false);

    protected:
        friend class RenderPath;
        void rebuildRenderer() const;

        virtual bool hasRelativeValues() const { return false; }
        
        static int cssPropertyIdForSVGAttributeName(const QualifiedName&);

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGStyledElement, String, String, ClassName, className)

        void updateElementInstance(SVGDocumentExtensions*) const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGStyledElement

// vim:ts=4:noet
