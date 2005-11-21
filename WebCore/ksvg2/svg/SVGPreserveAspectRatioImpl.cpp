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
#include <qstring.h>
#include <qstringlist.h>

#include <kdom/core/DOMStringImpl.h>

#include "ksvg.h"
#include "SVGMatrixImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGPreserveAspectRatioImpl.h"

using namespace KSVG;

SVGPreserveAspectRatioImpl::SVGPreserveAspectRatioImpl(const SVGStyledElementImpl *context) : KDOM::Shared<SVGPreserveAspectRatioImpl>()
{
    m_context = context;
    m_meetOrSlice = SVG_MEETORSLICE_MEET;
    m_align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
}

SVGPreserveAspectRatioImpl::~SVGPreserveAspectRatioImpl()
{
}

void SVGPreserveAspectRatioImpl::setAlign(unsigned short align)
{
    m_align = align;
}

unsigned short SVGPreserveAspectRatioImpl::align() const
{
    return m_align;
}

void SVGPreserveAspectRatioImpl::setMeetOrSlice(unsigned short meetOrSlice)
{
    m_meetOrSlice = meetOrSlice;
}

unsigned short SVGPreserveAspectRatioImpl::meetOrSlice() const
{
    return m_meetOrSlice;
}

void SVGPreserveAspectRatioImpl::parsePreserveAspectRatio(KDOM::DOMStringImpl *strImpl)
{
    // Spec: set the defaults
    setAlign(SVG_PRESERVEASPECTRATIO_NONE);
    setMeetOrSlice(SVG_MEETORSLICE_MEET);
    
    KDOM::DOMString s(strImpl);
    QString str = s.qstring();
    QStringList params = QStringList::split(' ', str.simplifyWhiteSpace());

    if(params[0].compare(QString::fromLatin1("none")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_NONE;
    else if(params[0].compare(QString::fromLatin1("xMinYMin")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMINYMIN;
    else if(params[0].compare(QString::fromLatin1("xMidYMin")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMIDYMIN;
    else if(params[0].compare(QString::fromLatin1("xMaxYMin")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMAXYMIN;
    else if(params[0].compare(QString::fromLatin1("xMinYMid")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMINYMID;
    else if(params[0].compare(QString::fromLatin1("xMidYMid")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
    else if(params[0].compare(QString::fromLatin1("xMaxYMid")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMAXYMID;
    else if(params[0].compare(QString::fromLatin1("xMinYMax")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMINYMAX;
    else if(params[0].compare(QString::fromLatin1("xMidYMax")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMIDYMAX;
    else if(params[0].compare(QString::fromLatin1("xMaxYMax")) == 0)
        m_align = SVG_PRESERVEASPECTRATIO_XMAXYMAX;

    if(m_align != SVG_PRESERVEASPECTRATIO_NONE)
    {
        if((params.count() > 1) && params[1].compare(QString::fromLatin1("slice")) == 0)
            m_meetOrSlice = SVG_MEETORSLICE_SLICE;
        else
            m_meetOrSlice = SVG_MEETORSLICE_MEET;
    }

    if(m_context)
        m_context->notifyAttributeChange();
}

SVGMatrixImpl *SVGPreserveAspectRatioImpl::getCTM(float logicX, float logicY, float logicWidth, float logicHeight,
                                                  float /*physX*/, float /*physY*/, float physWidth, float physHeight)
{
    SVGMatrixImpl *temp = SVGSVGElementImpl::createSVGMatrix();

    if(align() == SVG_PRESERVEASPECTRATIO_UNKNOWN)
        return temp;

    float vpar = logicWidth / logicHeight;
    float svgar = physWidth / physHeight;

    if(align() == SVG_PRESERVEASPECTRATIO_NONE)
    {
        temp->scaleNonUniform(physWidth / logicWidth, physHeight / logicHeight);
        temp->translate(-logicX, -logicY);
    }
    else if(vpar < svgar && (meetOrSlice() == SVG_MEETORSLICE_MEET) || vpar >= svgar && (meetOrSlice() == SVG_MEETORSLICE_SLICE))
    {
        temp->scale(physHeight / logicHeight);

        if(align() == SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVG_PRESERVEASPECTRATIO_XMINYMAX)
            temp->translate(-logicX, -logicY);
        else if(align() == SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVG_PRESERVEASPECTRATIO_XMIDYMAX)
            temp->translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp->translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    }
    else
    {
        temp->scale(physWidth / logicWidth);

        if(align() == SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVG_PRESERVEASPECTRATIO_XMAXYMIN)
            temp->translate(-logicX, -logicY);
        else if(align() == SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVG_PRESERVEASPECTRATIO_XMAXYMID)
            temp->translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp->translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

// vim:ts=4:noet
