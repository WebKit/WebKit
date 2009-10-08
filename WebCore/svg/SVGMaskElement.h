/*
    Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>

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

#ifndef SVGMaskElement_h
#define SVGMaskElement_h

#if ENABLE(SVG)
#include "SVGResourceMasker.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

    class SVGLength;

    class SVGMaskElement : public SVGStyledLocatableElement,
                           public SVGURIReference,
                           public SVGTests,
                           public SVGLangSpace,
                           public SVGExternalResourcesRequired {
    public:
        SVGMaskElement(const QualifiedName&, Document*);
        virtual ~SVGMaskElement();
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource();

        PassOwnPtr<ImageBuffer> drawMaskerContent(const FloatRect& targetRect, FloatRect& maskRect) const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGNames::maskTagString, SVGNames::maskUnitsAttrString, int, MaskUnits, maskUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGNames::maskTagString, SVGNames::maskContentUnitsAttrString, int, MaskContentUnits, maskContentUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGNames::maskTagString, SVGNames::xAttrString, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGNames::maskTagString, SVGNames::yAttrString, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGNames::maskTagString, SVGNames::widthAttrString, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGNames::maskTagString, SVGNames::heightAttrString, SVGLength, Height, height)

        // SVGURIReference
        ANIMATED_PROPERTY_DECLARATIONS(SVGMaskElement, SVGURIReferenceIdentifier, XLinkNames::hrefAttrString, String, Href, href)

        // SVGExternalResourcesRequired
        ANIMATED_PROPERTY_DECLARATIONS(SVGExternalResourcesRequired, SVGExternalResourcesRequiredIdentifier,
                                       SVGNames::externalResourcesRequiredAttrString, bool,
                                       ExternalResourcesRequired, externalResourcesRequired)

        RefPtr<SVGResourceMasker> m_masker;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
