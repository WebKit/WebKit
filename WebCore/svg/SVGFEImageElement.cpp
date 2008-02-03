/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEImageElement.h"

#include "Attr.h"
#include "CachedImage.h"
#include "DocLoader.h"
#include "Document.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEImageElement::SVGFEImageElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , SVGURIReference()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_preserveAspectRatio(new SVGPreserveAspectRatio())
    , m_cachedImage(0)
    , m_filterEffect(0)
{
}

SVGFEImageElement::~SVGFEImageElement()
{
    delete m_filterEffect;
    if (m_cachedImage)
        m_cachedImage->deref(this);
}

ANIMATED_PROPERTY_DEFINITIONS(SVGFEImageElement, SVGPreserveAspectRatio*, PreserveAspectRatio, preserveAspectRatio, PreserveAspectRatio, preserveAspectRatio, SVGNames::preserveAspectRatioAttr, m_preserveAspectRatio.get())

void SVGFEImageElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::preserveAspectRatioAttr) {
        const UChar* c = value.characters();
        const UChar* end = c + value.length();
        preserveAspectRatioBaseValue()->parsePreserveAspectRatio(c, end);
    } else {
        if (SVGURIReference::parseMappedAttribute(attr)) {
            if (!href().startsWith("#")) {
                // FIXME: this code needs to special-case url fragments and later look them up using getElementById instead of loading them here
                if (m_cachedImage)
                    m_cachedImage->deref(this);
                m_cachedImage = ownerDocument()->docLoader()->requestImage(href());
                if (m_cachedImage)
                    m_cachedImage->ref(this);
            }
            return;
        }
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;

        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
    }
}

void SVGFEImageElement::notifyFinished(CachedResource* finishedObj)
{
    if (finishedObj == m_cachedImage && m_filterEffect)
        m_filterEffect->setCachedImage(m_cachedImage);
}

SVGFEImage* SVGFEImageElement::filterEffect(SVGResourceFilter* filter) const
{
    if (!m_filterEffect)
        m_filterEffect = new SVGFEImage(filter);
    
    // The resource may already be loaded!
    if (m_cachedImage)
        m_filterEffect->setCachedImage(m_cachedImage);

    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
