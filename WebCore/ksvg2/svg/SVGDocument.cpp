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
#include "SVGDocument.h"

#include "EventNames.h"
#include "ExceptionCode.h"
#include "SVGDOMImplementation.h"
#include "SVGElement.h"
#include "SVGSVGElement.h"
#include "SVGZoomEvent.h"

namespace WebCore {

SVGDocument::SVGDocument(SVGDOMImplementation *i, FrameView *view) : Document(i, view)
{
}

SVGDocument::~SVGDocument()
{
}

SVGSVGElement *SVGDocument::rootElement() const
{
    Element *elem = documentElement();
    if(elem && elem->hasTagName(SVGNames::svgTag))
        return static_cast<SVGSVGElement *>(elem);

    return 0;
}

void SVGDocument::dispatchZoomEvent(float prevScale, float newScale)
{
    // dispatch zoom event
    ExceptionCode ec;
    RefPtr<SVGZoomEvent> event = static_pointer_cast<SVGZoomEvent>(createEvent("SVGZoomEvents", ec));
    event->initEvent(EventNames::zoomEvent, true, false);
    event->setPreviousScale(prevScale);
    event->setNewScale(newScale);
    rootElement()->dispatchEvent(event.get(), ec);
}

void SVGDocument::dispatchScrollEvent()
{
    // dispatch zoom event
    ExceptionCode ec;
    RefPtr<Event> event = createEvent("SVGEvents", ec);
    event->initEvent(EventNames::scrollEvent, true, false);
    rootElement()->dispatchEvent(event.get(), ec);
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT
