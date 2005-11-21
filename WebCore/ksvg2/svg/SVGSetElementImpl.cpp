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
#include "SVGSetElementImpl.h"
#include "SVGSVGElementImpl.h"
#include "KSVGTimeScheduler.h"

using namespace KSVG;

SVGSetElementImpl::SVGSetElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGAnimationElementImpl(tagName, doc)
{
}

SVGSetElementImpl::~SVGSetElementImpl()
{
}

void SVGSetElementImpl::handleTimerEvent(double timePercentage)
{
    // Start condition.
    if(!m_connected)
    {    
        SVGSVGElementImpl *ownerSVG = ownerSVGElement();
        if(ownerSVG)
        {
            ownerSVG->timeScheduler()->connectIntervalTimer(this);
            m_connected = true;
        }

        return;
    }

    // Calculations...
    if(timePercentage >= 1.0)
        timePercentage = 1.0;

    // Commit change now...
    if(m_savedTo.isEmpty())
    {
        KDOM::DOMString attr(targetAttribute());
        m_savedTo = attr.qstring();
        setTargetAttribute(KDOM::DOMString(m_to).impl());
    }

    // End condition.
    if(timePercentage == 1.0)
    {
        SVGSVGElementImpl *ownerSVG = ownerSVGElement();
        if(ownerSVG)
        {
            ownerSVG->timeScheduler()->disconnectIntervalTimer(this);
            m_connected = false;
        }

        if(!isFrozen())
            setTargetAttribute(KDOM::DOMString(m_savedTo).impl());

        m_savedTo = QString();
    }
}

// vim:ts=4:noet
