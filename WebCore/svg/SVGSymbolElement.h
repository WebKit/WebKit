/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGSymbolElement_h
#define SVGSymbolElement_h

#if ENABLE(SVG)
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"

namespace WebCore {

    class SVGSymbolElement : public SVGStyledElement,
                             public SVGLangSpace,
                             public SVGExternalResourcesRequired,
                             public SVGFitToViewBox {
    public:
        SVGSymbolElement(const QualifiedName&, Document*);
        virtual ~SVGSymbolElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual bool shouldAttachChild(Element*) const { return false; }
    
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }

    private:
        // SVGExternalResourcesRequired
        ANIMATED_PROPERTY_DECLARATIONS(SVGSymbolElement, SVGExternalResourcesRequiredIdentifier,
                                       SVGNames::externalResourcesRequiredAttrString, bool,
                                       ExternalResourcesRequired, externalResourcesRequired)
 
        // SVGFitToViewBox
        ANIMATED_PROPERTY_DECLARATIONS(SVGSymbolElement, SVGFitToViewBoxIdentifier, SVGNames::viewBoxAttrString, FloatRect, ViewBox, viewBox)
        ANIMATED_PROPERTY_DECLARATIONS(SVGSymbolElement, SVGFitToViewBoxIdentifier, SVGNames::preserveAspectRatioAttrString, SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio) 
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
