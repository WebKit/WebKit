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
#include <kdebug.h>
#include <qregexp.h>
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/RenderPath.h>

#include "SVGHelper.h"
#include "SVGMatrixImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGStyledTransformableElementImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGAnimatedTransformListImpl.h"

using namespace KSVG;

SVGStyledTransformableElementImpl::SVGStyledTransformableElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGStyledLocatableElementImpl(tagName, doc), SVGTransformableImpl()
{
}

SVGStyledTransformableElementImpl::~SVGStyledTransformableElementImpl()
{
}

SVGAnimatedTransformListImpl *SVGStyledTransformableElementImpl::transform() const
{
    return lazy_create<SVGAnimatedTransformListImpl>(m_transform, this);
}

SVGMatrixImpl *SVGStyledTransformableElementImpl::localMatrix() const
{
    return lazy_create<SVGMatrixImpl>(m_localMatrix);
}

SVGMatrixImpl *SVGStyledTransformableElementImpl::getCTM() const
{
    SVGMatrixImpl *ctm = SVGLocatableImpl::getCTM(this);

    if(m_localMatrix)
        ctm->multiply(m_localMatrix.get());

    return ctm;
}

SVGMatrixImpl *SVGStyledTransformableElementImpl::getScreenCTM() const
{
    SVGMatrixImpl *ctm = SVGLocatableImpl::getScreenCTM(this);

    if(m_localMatrix)
        ctm->multiply(m_localMatrix.get());

    return ctm;
}

void SVGStyledTransformableElementImpl::updateLocalTransform(SVGTransformListImpl *localTransforms)
{
    // Update cached local matrix
    RefPtr<SVGTransformImpl> localTransform = localTransforms->concatenate();
    if(localTransform) {
        m_localMatrix = localTransform->matrix();
        if (renderer()) {
            renderer()->setLocalTransform(m_localMatrix->qmatrix());
            renderer()->setNeedsLayout(true);
        }
    }
}

void SVGStyledTransformableElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    if (attr->name() == SVGNames::transformAttr) {
        SVGTransformListImpl *localTransforms = transform()->baseVal();
        localTransforms->clear();
        
        SVGTransformableImpl::parseTransformAttribute(localTransforms, attr->value());
        updateLocalTransform(localTransforms);
    } else
        SVGStyledLocatableElementImpl::parseMappedAttribute(attr);
}

SVGElementImpl *SVGStyledTransformableElementImpl::nearestViewportElement() const
{
    return SVGTransformableImpl::nearestViewportElement(this);
}

SVGElementImpl *SVGStyledTransformableElementImpl::farthestViewportElement() const
{
    return SVGTransformableImpl::farthestViewportElement(this);
}

SVGRectImpl *SVGStyledTransformableElementImpl::getBBox() const
{
    return SVGTransformableImpl::getBBox(this);
}

SVGMatrixImpl *SVGStyledTransformableElementImpl::getTransformToElement(SVGElementImpl *) const
{
    return 0;
}

void SVGStyledTransformableElementImpl::attach()
{
    SVGStyledElementImpl::attach();

    if (renderer() && m_localMatrix)
        renderer()->setLocalTransform(m_localMatrix->qmatrix());
}


// vim:ts=4:noet
#endif // SVG_SUPPORT

