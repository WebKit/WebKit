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
#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvasItem.h>
#include <kcanvas/KCanvasMatrix.h>

#include "svgattrs.h"
#include "SVGAngleImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGDocumentImpl.h"
#include "SVGTransformImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGTransformListImpl.h"
#include "SVGAnimatedTransformListImpl.h"
#include "SVGAnimateTransformElementImpl.h"

#include <kdebug.h>

#include <cmath>

using namespace KSVG;
using namespace std;

SVGAnimateTransformElementImpl::SVGAnimateTransformElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix)
: SVGAnimationElementImpl(doc, id, prefix)
{
    m_type = SVG_TRANSFORM_UNKNOWN;
    
    m_toTransform = 0;
    m_fromTransform = 0;
    m_initialTransform = 0;

    m_currentItem = -1;

    m_lastMatrix = 0;
    m_transformMatrix = 0;

    m_rotateSpecialCase = false;
    m_toRotateSpecialCase = false;
    m_fromRotateSpecialCase = false;
}

SVGAnimateTransformElementImpl::~SVGAnimateTransformElementImpl()
{
    if(m_toTransform)
        m_toTransform->deref();
    if(m_fromTransform)
        m_fromTransform->deref();
    if(m_initialTransform)
        m_initialTransform->deref();
    
    if(m_lastMatrix)    
        m_lastMatrix->deref();
    if(m_transformMatrix)
        m_transformMatrix->deref();
}

void SVGAnimateTransformElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    KDOM::DOMString value(attr->value());
    switch(id)
    {
        case ATTR_TYPE:
        {
            if(value == "translate")
                m_type = SVG_TRANSFORM_TRANSLATE;
            else if(value == "scale")
                m_type = SVG_TRANSFORM_SCALE;
            else if(value == "rotate")
                m_type = SVG_TRANSFORM_ROTATE;
            else if(value == "skewX")
                m_type = SVG_TRANSFORM_SKEWX;
            else if(value == "skewY")
                m_type = SVG_TRANSFORM_SKEWY;

            break;
        }                
        default:
            SVGAnimationElementImpl::parseAttribute(attr);
    };
}

void SVGAnimateTransformElementImpl::handleTimerEvent(double timePercentage)
{
    // Start condition.
    if(!m_connected)
    {    
        if(m_initialTransform)
            m_initialTransform->deref();
    
        m_initialTransform = 0;
        
        // Save initial transform... (needed for fill="remove" or additve="sum")
        SVGTransformableImpl *transform = dynamic_cast<SVGTransformableImpl *>(targetElement());
        SVGTransformListImpl *transformList = (transform ? transform->transform()->baseVal() : 0);
        if(transformList)
        {
            transformList->ref();
            
            for(unsigned long i = 0; i < transformList->numberOfItems(); i++)
            {
                SVGTransformImpl *value = transformList->getItem(i);
                if(!value)
                    continue;
                    
                if(value->type() == m_type)
                {
                    value->ref();
                    m_initialTransform = value;
                    break;
                }
            }

            transformList->deref();
        }
                
        // Animation mode handling
        switch(detectAnimationMode())
        {
            case TO_ANIMATION:
            case FROM_TO_ANIMATION:
            {
                if(m_toTransform)
                    m_toTransform->deref();
        
                m_toTransform = parseTransformValue(m_to);
                m_toRotateSpecialCase = m_rotateSpecialCase;

                if(m_fromTransform)
                    m_fromTransform->deref();

                if(!m_from.isEmpty()) // from-to animation
                {
                    m_fromTransform = parseTransformValue(m_from);
                    m_fromRotateSpecialCase = m_rotateSpecialCase;
                }
                else // to animation
                {
                    m_fromTransform = m_initialTransform;
                    m_fromTransform->ref();

                    m_fromRotateSpecialCase = false;
                }

                if(!m_fromTransform)
                {
                    m_fromTransform = new SVGTransformImpl();
                    m_fromTransform->ref();
                }

                break;
            }
            case BY_ANIMATION:
            case FROM_BY_ANIMATION:
            {
                if(m_toTransform)
                    m_toTransform->deref();

                m_toTransform = parseTransformValue(m_by);
                m_toRotateSpecialCase = m_rotateSpecialCase;
            
                if(m_fromTransform)
                    m_fromTransform->deref();
    
                if(!m_from.isEmpty()) // from-by animation
                {
                    m_fromTransform = parseTransformValue(m_from);
                    m_fromRotateSpecialCase = m_rotateSpecialCase;
                }
                else // by animation
                {
                    m_fromTransform = m_initialTransform;
                    m_fromRotateSpecialCase = false;
                }

                if(!m_fromTransform)
                {
                    m_fromTransform = new SVGTransformImpl();
                    m_fromTransform->ref();
                }

                SVGMatrixImpl *byMatrix = m_toTransform->matrix();
                SVGMatrixImpl *fromMatrix = m_fromTransform->matrix();

                byMatrix->multiply(fromMatrix);

                break;
            }
            case VALUES_ANIMATION:
                break;
            default:
            {
                kdError() << k_funcinfo << " Unable to detect animation mode! Aborting creation!" << endl;
                return;
            }
        }
        
        SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
        if(document)
        {
            document->timeScheduler()->connectIntervalTimer(this);
            m_connected = true;
        }

        return;
    }

    // Calculations...
    if(timePercentage >= 1.0)
        timePercentage = 1.0;
    
    QWMatrix qToMatrix, qFromMatrix;
    double useTimePercentage = timePercentage;

    if(m_values)
    {
        int itemByPercentage = calculateCurrentValueItem(timePercentage);

        if(itemByPercentage == -1)
            return;
        
        if(m_currentItem != itemByPercentage) // Item changed...
        {
            // Extract current 'from' / 'to' values
            KDOM::DOMString value1 = KDOM::DOMString(m_values->getItem(itemByPercentage));
            KDOM::DOMString value2 = KDOM::DOMString(m_values->getItem(itemByPercentage + 1));

            // Calculate new from/to transform values...
            if(!value1.isEmpty() && !value2.isEmpty())
            {
                bool apply = false;
                if(m_toTransform && m_fromTransform)
                {    
                    qToMatrix = m_toTransform->matrix()->qmatrix();
                    qFromMatrix = m_fromTransform->matrix()->qmatrix();
                    
                    apply = true;
                    useTimePercentage = 1.0;
                }

                if(m_toTransform)
                    m_toTransform->deref();
        
                m_toTransform = parseTransformValue(value2.string());
                m_toRotateSpecialCase = m_rotateSpecialCase;

                if(m_fromTransform)
                    m_fromTransform->deref();
    
                m_fromTransform = parseTransformValue(value1.string());
                m_fromRotateSpecialCase = m_rotateSpecialCase;

                m_currentItem = itemByPercentage;

                if(!apply)
                    return;
            }
        }
        else if(m_toTransform && m_fromTransform)
            useTimePercentage = calculateRelativeTimePercentage(timePercentage, m_currentItem);
    }

    if(m_toTransform && m_toTransform->matrix() && qToMatrix.isIdentity())
        qToMatrix = m_toTransform->matrix()->qmatrix();

    if(m_fromTransform && m_fromTransform->matrix() && qFromMatrix.isIdentity())
        qFromMatrix = m_fromTransform->matrix()->qmatrix();

    if(!m_transformMatrix)
    {
        m_transformMatrix = new SVGMatrixImpl();
        m_transformMatrix->ref();
    }
    else
    {
        m_transformMatrix->reset();

        if(isAccumulated() && repeations() != 0.0 && m_lastMatrix)
            m_transformMatrix->multiply(m_lastMatrix);
    }
    
    switch(m_type)
    {
        case SVG_TRANSFORM_TRANSLATE:
        {
            double dx = ((qToMatrix.dx() - qFromMatrix.dx()) * useTimePercentage) + qFromMatrix.dx();
            double dy = ((qToMatrix.dy() - qFromMatrix.dy()) * useTimePercentage) + qFromMatrix.dy();

            m_transformMatrix->translate(dx, dy);
            break;
        }
        case SVG_TRANSFORM_SCALE:
        {
            double sx = ((qToMatrix.m11() - qFromMatrix.m11()) * useTimePercentage) + qFromMatrix.m11();
            double sy = ((qToMatrix.m22() - qFromMatrix.m22()) * useTimePercentage) + qFromMatrix.m22();

            m_transformMatrix->scaleNonUniform(sx, sy);
            break;
        }
        case SVG_TRANSFORM_ROTATE:
        {
            double toAngle, toCx, toCy, fromAngle, fromCx, fromCy;
            calculateRotationFromMatrix(qToMatrix, toAngle, toCx, toCy);
            calculateRotationFromMatrix(qFromMatrix, fromAngle, fromCx, fromCy);

            if(m_toRotateSpecialCase)
            {
                if(qRound(toAngle) == 1)
                    toAngle = 0.0;
                else
                    toAngle = 360.0;
            }

            if(m_fromRotateSpecialCase)
            {
                if(qRound(fromAngle) == 1)
                    fromAngle = 0.0;
                else
                    fromAngle = 360.0;
            }
                    
            double angle = ((toAngle - fromAngle) * useTimePercentage) + fromAngle;
            double cx = (toCx - fromCx) * useTimePercentage + fromCx;
            double cy = (toCy - fromCy) * useTimePercentage + fromCy;

            m_transformMatrix->translate(cx, cy);
            m_transformMatrix->rotate(angle);
            m_transformMatrix->translate(-cx, -cy);
            break;
        }
        case SVG_TRANSFORM_SKEWX:
        {
            double sx = (SVGAngleImpl::todeg(atan(qToMatrix.m21()) - atan(qFromMatrix.m21())) *
                         useTimePercentage) + SVGAngleImpl::todeg(atan(qFromMatrix.m21()));

            m_transformMatrix->skewX(sx);
            break;
        }
        case SVG_TRANSFORM_SKEWY:
        {
            double sy = (SVGAngleImpl::todeg(atan(qToMatrix.m12()) - atan(qFromMatrix.m12())) *
                         useTimePercentage) + SVGAngleImpl::todeg(atan(qFromMatrix.m12()));

            m_transformMatrix->skewY(sy);
            break;
        }
        default:
            break;
    }

    // End condition.
    if(timePercentage == 1.0)
    {
        if((m_repeatCount > 0 && m_repeations < m_repeatCount - 1) || isIndefinite(m_repeatCount))
        {
            if(m_lastMatrix)
                m_lastMatrix->deref();

            m_lastMatrix = new SVGMatrixImpl();
            m_lastMatrix->ref();
    
            if(m_transformMatrix)
                m_lastMatrix->copy(m_transformMatrix);

            m_repeations++;
            return;
        }

        SVGDocumentImpl *document = static_cast<SVGDocumentImpl *>(ownerDocument());
        if(document)
        {
            document->timeScheduler()->disconnectIntervalTimer(this);
            m_connected = false;
        }

        // Reset...
        m_currentItem = -1;

        if(m_toTransform)
            m_toTransform->deref();

        m_toTransform = 0;

        if(m_fromTransform)
            m_fromTransform->deref();

        m_fromTransform = 0;

        if(m_initialTransform)
            m_initialTransform->deref();
        
        m_initialTransform = 0;

        if(!isFrozen())
        {
            if(m_transformMatrix)
                m_transformMatrix->deref();

            SVGMatrixImpl *initial = initialMatrix();
            if(initial)
                m_transformMatrix = initial;
            else
            {
                m_transformMatrix = new SVGMatrixImpl();
                m_transformMatrix->ref();
            }
        }
    }
}

SVGTransformImpl *SVGAnimateTransformElementImpl::parseTransformValue(const QString &data) const
{
    SVGTransformImpl *ret = 0;
    QString parse = data.stripWhiteSpace();
    if(parse.isEmpty())
        return ret;
    
    int commaPos = parse.find(','); // In case two values are passed...

    switch(m_type)
    {
        case SVG_TRANSFORM_TRANSLATE:
        {
            ret = new SVGTransformImpl();
            ret->ref();

            double tx = 0.0, ty = 0.0;
            if(commaPos != - 1)
            {
                tx = parse.mid(0, commaPos).toDouble();
                ty = parse.mid(commaPos + 1).toDouble();
            }
            else 
                tx = parse.toDouble();

            ret->setTranslate(tx, ty);
            break;    
        }
        case SVG_TRANSFORM_SCALE:
        {
            ret = new SVGTransformImpl();
            ret->ref();

            double sx = 1.0, sy = 1.0;
            if(commaPos != - 1)
            {
                sx = parse.mid(0, commaPos).toDouble();
                sy = parse.mid(commaPos + 1).toDouble();
            }
            else
            {
                sx = parse.toDouble();
                sy = sx;
            }

            ret->setScale(sx, sy);
            break;
        }
        case SVG_TRANSFORM_ROTATE:
        {
            ret = new SVGTransformImpl();
            ret->ref();

            double angle = 0, cx = 0, cy = 0;
            if(commaPos != - 1)
            {
                angle = parse.mid(0, commaPos).toDouble(); // TODO: probably needs it's own 'angle' parser
    
                int commaPosTwo = parse.find(',', commaPos + 1); // In case three values are passed...
                if(commaPosTwo != -1)
                {
                    cx = parse.mid(commaPos + 1, commaPosTwo - commaPos - 1).toDouble();
                    cy = parse.mid(commaPosTwo + 1).toDouble();
                }
            }
            else 
                angle = parse.toDouble();

            // Ok now here's a hack to make it possible to calculate cx/cy values, if angle = 0 
            // or angle=360 -> for angle=0 our matrix is m11: 1 m12: 0 m21: 0 m22: 1 dx: 0 dy: 0
            // As you can see there is no way to retrieve the cx/cy values for these angles.
            // -> set 'm_rotateSpecialCase' to true, and save angle = 1/359 -> this way we can calculate
            //    the cx/cy values, while keeping this uber-optimized way of handling <animateTransform>!
            m_rotateSpecialCase = false;
            
            if(angle == 0.0)
            {
                angle = angle + 1.0;
                m_rotateSpecialCase = true;
            }
            else if(angle == 360.0)
            {
                angle = angle - 1.0;
                m_rotateSpecialCase = true;
            }
            
            ret->setRotate(angle, cx, cy);
            break;    
        }
        case SVG_TRANSFORM_SKEWX:
        case SVG_TRANSFORM_SKEWY:
        {
            ret = new SVGTransformImpl();
            ret->ref();
            
            double angle = parse.toDouble(); // TODO: probably needs it's own 'angle' parser
            
            if(m_type == SVG_TRANSFORM_SKEWX)
                ret->setSkewX(angle);
            else
                ret->setSkewY(angle);

            break;
        }
        default:
            break;
    }

    if(ret)
        ret->ref();
        
    return ret;
}

void SVGAnimateTransformElementImpl::calculateRotationFromMatrix(const QWMatrix &matrix, double &angle, double &cx, double &cy) const
{
    double cosa = matrix.m11();
    double sina = -matrix.m21();

    if(cosa != 1.0)
    {
        // Calculate the angle via magic ;)
        double temp = SVGAngleImpl::todeg(asin(sina));
        angle = SVGAngleImpl::todeg(acos(cosa));
        if(temp < 0)
            angle = 360.0 - angle;
        
        double res = (1 - cosa) + ((sina * sina) / (1 - cosa));
        
        cx = (matrix.dx() - ((sina * matrix.dy()) / (1 - cosa))) / res;
        cy = (matrix.dy() + ((sina * matrix.dx()) / (1 - cosa))) / res;

        return;
    }

    cx = 0.0;
    cy = 0.0;
    angle = 0.0;
}

SVGMatrixImpl *SVGAnimateTransformElementImpl::initialMatrix() const
{
    SVGTransformableImpl *transform = dynamic_cast<SVGTransformableImpl *>(targetElement());
    SVGTransformListImpl *transformList = (transform ? transform->transform()->baseVal() : 0);
    if(!transformList)
        return 0;
    
    SVGTransformImpl *result = transformList->concatenate();
    if(!result)
        return 0;

    result->ref();

    SVGMatrixImpl *ret = result->matrix();
    ret->ref();

    result->deref();
    return ret;
}

SVGMatrixImpl *SVGAnimateTransformElementImpl::transformMatrix() const
{
    return m_transformMatrix;
}

// vim:ts=4:noet
