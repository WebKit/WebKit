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

#include <qregexp.h>
#include <qstringlist.h>

#include <kdom/impl/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasItem.h>

#include "svgattrs.h"
#include "SVGHelper.h"
#include "SVGMatrixImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGAnimatedTransformListImpl.h"

using namespace KSVG;

SVGTransformableImpl::SVGTransformableImpl() : SVGLocatableImpl()
{
    m_transform = 0;
    m_localMatrix = 0;
}

SVGTransformableImpl::~SVGTransformableImpl()
{
    if(m_transform)
        m_transform->deref();
    if(m_localMatrix)
        m_localMatrix->deref();
}

SVGAnimatedTransformListImpl *SVGTransformableImpl::transform() const
{
    const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
    return lazy_create<SVGAnimatedTransformListImpl>(m_transform, context);
}

SVGMatrixImpl *SVGTransformableImpl::localMatrix() const
{
    return lazy_create<SVGMatrixImpl>(m_localMatrix);
}

SVGMatrixImpl *SVGTransformableImpl::getCTM() const
{
    SVGMatrixImpl *ctm = SVGLocatableImpl::getCTM();

    if(m_localMatrix)
        ctm->multiply(m_localMatrix);

    return ctm;
}

SVGMatrixImpl *SVGTransformableImpl::getScreenCTM() const
{
    SVGMatrixImpl *ctm = SVGLocatableImpl::getScreenCTM();

    if(m_localMatrix)
        ctm->multiply(m_localMatrix);

    return ctm;
}

void SVGTransformableImpl::updateLocalTransform(SVGTransformListImpl *localTransforms)
{
    // Update cached local matrix
    SVGTransformImpl *localTransform = localTransforms->concatenate();
    if(localTransform)
    {
        localTransform->ref();
        
        KDOM_SAFE_SET(m_localMatrix, localTransform->matrix());

        localTransform->deref();
    }
}

void SVGTransformableImpl::updateSubtreeMatrices(KDOM::NodeImpl *node)
{
    if(!node)
        return;

    // First update local matrix...
    SVGStyledElementImpl *styled = dynamic_cast<SVGStyledElementImpl *>(node);
    SVGTransformableImpl *transform = dynamic_cast<SVGTransformableImpl *>(node);

    if(styled && transform && transform->localMatrix())
    {
        QWMatrix useMatrix = transform->localMatrix()->qmatrix();
    
        KDOM::ElementImpl *parentElement = dynamic_cast<KDOM::ElementImpl *>(node->parentNode());
        if(parentElement && parentElement->id() == ID_G)
        {
            SVGMatrixImpl *ctm = transform->getCTM();
            ctm->ref();
                
            useMatrix *= ctm->qmatrix();

            ctm->deref();
        }

        styled->updateCTM(useMatrix);
    }

    // ... then update the whole subtree, if possible.
    for(KDOM::NodeImpl *n = node->firstChild(); n != 0; n = n->nextSibling())
        updateSubtreeMatrices(n);
}

bool SVGTransformableImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    if(id == ATTR_TRANSFORM)
    {
        SVGTransformListImpl *localTransforms = transform()->baseVal();
        
        localTransforms->clear();
        SVGTransformableImpl::parseTransformAttribute(localTransforms, value);

        // Update cached local matrix
        updateLocalTransform(localTransforms);

        SVGStyledElementImpl *styledElement = dynamic_cast<SVGStyledElementImpl *>(this);
        if(!styledElement)
            return false;
            
        updateSubtreeMatrices(styledElement);
        return true;
    }

    return false;
}

void SVGTransformableImpl::parseTransformAttribute(SVGTransformListImpl *list, const KDOM::DOMString &transform)
{
    // Split string for handling 1 transform statement at a time
    QStringList subtransforms = QStringList::split(')', transform.string());
    QStringList::ConstIterator it = subtransforms.begin();
    QStringList::ConstIterator end = subtransforms.end();
    for(; it != end; ++it)
    {
        QStringList subtransform = QStringList::split('(', (*it));

        subtransform[0] = subtransform[0].stripWhiteSpace().lower();
        subtransform[1] = subtransform[1].simplifyWhiteSpace();
        QRegExp reg(QString::fromLatin1("([-]?\\d*\\.?\\d+(?:e[-]?\\d+)?)"));

        int pos = 0;
        QStringList params;
        
        while(pos >= 0)
        {
            pos = reg.search(subtransform[1], pos);
            if(pos != -1)
            {
                params += reg.cap(1);
                pos += reg.matchedLength();
            }
        }

        if(subtransform[0].startsWith(QString::fromLatin1(";")) ||
           subtransform[0].startsWith(QString::fromLatin1(",")))
        {
            subtransform[0] = subtransform[0].right(subtransform[0].length() - 1);
        }

        SVGTransformImpl *t = new SVGTransformImpl();

        if(subtransform[0] == QString::fromLatin1("rotate"))
        {
            if(params.count() == 3)
                t->setRotate(params[0].toDouble(),
                             params[1].toDouble(),
                              params[2].toDouble());
            else
                t->setRotate(params[0].toDouble(), 0, 0);
        }
        else if(subtransform[0] == QString::fromLatin1("translate"))
        {
            if(params.count() == 2)
                t->setTranslate(params[0].toDouble(), params[1].toDouble());
            else // Spec: if only one param given, assume 2nd param to be 0
                t->setTranslate(params[0].toDouble(), 0);
        }
        else if(subtransform[0] == QString::fromLatin1("scale"))
        {
            if(params.count() == 2)
                t->setScale(params[0].toDouble(), params[1].toDouble());
            else // Spec: if only one param given, assume uniform scaling
                t->setScale(params[0].toDouble(), params[0].toDouble());
        }
        else if(subtransform[0] == QString::fromLatin1("skewx"))
            t->setSkewX(params[0].toDouble());
        else if(subtransform[0] == QString::fromLatin1("skewy"))
            t->setSkewY(params[0].toDouble());
        else if(subtransform[0] == QString::fromLatin1("matrix"))
        {
            if(params.count() >= 6)
            {
                SVGMatrixImpl *ret = new SVGMatrixImpl(params[0].toDouble(),
                                                       params[1].toDouble(),
                                                       params[2].toDouble(),
                                                       params[3].toDouble(),
                                                       params[4].toDouble(),
                                                       params[5].toDouble());
                t->setMatrix(ret);
            }
        }

        list->appendItem(t);
    }
}

// vim:ts=4:noet
