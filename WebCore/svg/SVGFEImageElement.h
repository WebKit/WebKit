/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGFEImageElement_h
#define SVGFEImageElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "ImageBuffer.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFEImage.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGLangSpace.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGFEImageElement : public SVGFilterPrimitiveStandardAttributes,
                          public SVGURIReference,
                          public SVGLangSpace,
                          public SVGExternalResourcesRequired,
                          public CachedResourceClient {
public:
    static PassRefPtr<SVGFEImageElement> create(const QualifiedName&, Document*);

    virtual ~SVGFEImageElement();

private:
    SVGFEImageElement(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    virtual void notifyFinished(CachedResource*);

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*);

    void requestImageResource();

    DECLARE_ANIMATED_PROPERTY_NEW(SVGFEImageElement, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio)

    // SVGURIReference
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFEImageElement, XLinkNames::hrefAttr, String, Href, href)

    // SVGExternalResourcesRequired
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFEImageElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)

    CachedResourceHandle<CachedImage> m_cachedImage;
    OwnPtr<ImageBuffer> m_targetImage;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
