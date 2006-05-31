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
#include "SVGSetElement.h"
#include "KSVGTimeScheduler.h"
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
    if (!m_connected) {    
        ownerSVGElement()->timeScheduler()->connectIntervalTimer(this);
        m_connected = true;
        return;
    }

    // Calculations...
    if (timePercentage >= 1.0)
        timePercentage = 1.0;

    // Commit change now...
    if (m_savedTo.isEmpty()) {
        String attr(targetAttribute());
        m_savedTo = attr.deprecatedString();
        setTargetAttribute(String(m_to).impl());
    }

    // End condition.
    if (timePercentage == 1.0) {
        ownerSVGElement()->timeScheduler()->disconnectIntervalTimer(this);
        m_connected = false;

        if (!isFrozen())
            setTargetAttribute(String(m_savedTo).impl());

        m_savedTo = DeprecatedString();
    }
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

