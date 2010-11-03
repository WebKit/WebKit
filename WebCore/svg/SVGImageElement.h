/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGImageElement_h
#define SVGImageElement_h

#if ENABLE(SVG)
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPropertyMacros.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGImageLoader.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"
#include "SVGPreserveAspectRatio.h"

namespace WebCore {

    class SVGLength;

    class SVGImageElement : public SVGStyledTransformableElement,
                            public SVGTests,
                            public SVGLangSpace,
                            public SVGExternalResourcesRequired,
                            public SVGURIReference {
    public:
        static PassRefPtr<SVGImageElement> create(const QualifiedName&, Document*);

    private:
        SVGImageElement(const QualifiedName&, Document*);
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(Attribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);

        virtual void attach();
        virtual void insertedIntoDocument();

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
 
        virtual const QualifiedName& imageSourceAttributeName() const;       
        virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

        virtual bool haveLoadedRequiredResources();

        virtual bool selfHasRelativeLengths() const;
        virtual void willMoveToNewOwnerDocument();

        DECLARE_ANIMATED_PROPERTY_NEW(SVGImageElement, SVGNames::xAttr, SVGLength, X, x)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGImageElement, SVGNames::yAttr, SVGLength, Y, y)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGImageElement, SVGNames::widthAttr, SVGLength, Width, width)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGImageElement, SVGNames::heightAttr, SVGLength, Height, height)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGImageElement, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio)

        // SVGURIReference
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGImageElement, XLinkNames::hrefAttr, String, Href, href)

        // SVGExternalResourcesRequired
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGImageElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)

        SVGImageLoader m_imageLoader;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
