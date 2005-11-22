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
#include <qimage.h>

#include <kdom/core/AttrImpl.h>
#include <kdom/cache/KDOMLoader.h>
#include <kdom/cache/KDOMCachedObject.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGFEImageElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasImage.h>
#include "KCanvasRenderingStyle.h"
#include <kcanvas/device/KRenderingDevice.h>
#include <kcanvas/device/KRenderingFillPainter.h>
#include <kcanvas/device/KRenderingPaintServerImage.h>

using namespace KSVG;

SVGFEImageElementImpl::SVGFEImageElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGFilterPrimitiveStandardAttributesImpl(tagName, doc), SVGURIReferenceImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
    m_filterEffect = 0;
    m_preserveAspectRatio = 0;
    m_cachedImage = 0;
}

SVGFEImageElementImpl::~SVGFEImageElementImpl()
{
    if(m_preserveAspectRatio)
        m_preserveAspectRatio->deref();
}

SVGAnimatedPreserveAspectRatioImpl *SVGFEImageElementImpl::preserveAspectRatio() const
{
    return lazy_create<SVGAnimatedPreserveAspectRatioImpl>(m_preserveAspectRatio, this);
}

void SVGFEImageElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
    if (attr->name() == SVGNames::preserveAspectRatioAttr)
        preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(value.impl());
    else
    {
        if(SVGURIReferenceImpl::parseMappedAttribute(attr)) {
            if (m_cachedImage)
                m_cachedImage->deref(this);
            m_cachedImage = ownerDocument()->docLoader()->requestImage(href()->baseVal());
            if(m_cachedImage)
                m_cachedImage->ref(this);
            return;
        }
        if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;

        SVGFilterPrimitiveStandardAttributesImpl::parseMappedAttribute(attr);
    }
}

void SVGFEImageElementImpl::notifyFinished(KDOM::CachedObject *finishedObj)
{
    if(finishedObj == m_cachedImage)
    {
        KCanvasImage *imageBuffer = static_cast<KCanvasImage *>(canvas()->renderingDevice()->createResource(RS_IMAGE));
        imageBuffer->init(m_cachedImage->pixmap());
        //filterEffect()->setImageBuffer(imageBuffer);

        m_cachedImage->deref(this);
        m_cachedImage = 0;
    }
}

KCanvasFilterEffect *SVGFEImageElementImpl::filterEffect() const
{
    if (!m_filterEffect) {
        m_filterEffect = static_cast<KCanvasFEImage *>(canvas()->renderingDevice()->createFilterEffect(FE_IMAGE));
        setStandardAttributes(m_filterEffect);
    }
    return m_filterEffect;
}

// vim:ts=4:noet
