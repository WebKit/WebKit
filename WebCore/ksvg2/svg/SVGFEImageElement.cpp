/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT

#include "Attr.h"
#include <kdom/cache/KDOMLoader.h>
#include <kdom/cache/KDOMCachedObject.h>
#include "Document.h"
#include "DocLoader.h"

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGFEImageElement.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatio.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasImage.h>
#include "KCanvasRenderingStyle.h"
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>

using namespace WebCore;

SVGFEImageElement::SVGFEImageElement(const QualifiedName& tagName, Document *doc)
: SVGFilterPrimitiveStandardAttributes(tagName, doc), SVGURIReference(), SVGLangSpace(), SVGExternalResourcesRequired()
{
    m_filterEffect = 0;
    m_cachedImage = 0;
}

SVGFEImageElement::~SVGFEImageElement()
{
    delete m_filterEffect;
    if (m_cachedImage)
        m_cachedImage->deref(this);
}

SVGAnimatedPreserveAspectRatio *SVGFEImageElement::preserveAspectRatio() const
{
    return lazy_create<SVGAnimatedPreserveAspectRatio>(m_preserveAspectRatio, this);
}

void SVGFEImageElement::parseMappedAttribute(MappedAttribute *attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::preserveAspectRatioAttr)
        preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(value.impl());
    else
    {
        if (SVGURIReference::parseMappedAttribute(attr)) {
            if (m_cachedImage)
                m_cachedImage->deref(this);
            m_cachedImage = ownerDocument()->docLoader()->requestImage(href()->baseVal());
            if (m_cachedImage)
                m_cachedImage->ref(this);
            return;
        }
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;

        SVGFilterPrimitiveStandardAttributes::parseMappedAttribute(attr);
    }
}

void SVGFEImageElement::notifyFinished(CachedObject *finishedObj)
{
    if (finishedObj == m_cachedImage && filterEffect())
        filterEffect()->setCachedImage(m_cachedImage);
}

KCanvasFEImage *SVGFEImageElement::filterEffect() const
{
    if (!m_filterEffect)
        m_filterEffect = static_cast<KCanvasFEImage *>(renderingDevice()->createFilterEffect(FE_IMAGE));
    if (!m_filterEffect)
        return 0;
    setStandardAttributes(m_filterEffect);
    return m_filterEffect;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

