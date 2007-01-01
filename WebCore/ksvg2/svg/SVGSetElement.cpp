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
#ifdef SVG_SUPPORT
#include "SVGSetElement.h"
#include "TimeScheduler.h"
#include "Document.h"
#include "SVGDocumentExtensions.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGSetElement::SVGSetElement(const QualifiedName& tagName, Document *doc)
    : SVGAnimationElement(tagName, doc)
{
}

SVGSetElement::~SVGSetElement()
{
}

void SVGSetElement::handleTimerEvent(double timePercentage)
{
    // Start condition.
    if (!connectedToTimer()) {
        connectTimer();
        return;
    }
    
    // Commit change now...
    if (m_savedTo.isEmpty()) {
        m_savedTo = targetAttribute();
        setTargetAttribute(m_to);
    }

    // End condition.
    if (timePercentage == 1.0) {
        disconnectTimer();

        if (!isFrozen())
            setTargetAttribute(m_savedTo);

        m_savedTo = String();
    }
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

