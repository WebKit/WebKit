/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>
				  
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

#include <kdom/Shared.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGRect.h"
#include "SVGPoint.h"
#include "SVGZoomEvent.h"
#include "SVGZoomEventImpl.h"

#include <ksvg2/data/EventsConstants.h>
#include "SVGZoomEvent.lut.h"
using namespace KSVG;

/*
@begin SVGZoomEvent::s_hashTable 7
 zoomRectScreen		SVGZoomEventConstants::ZoomRectScreen		DontDelete|ReadOnly
 previousScale		SVGZoomEventConstants::PreviousScale		DontDelete|ReadOnly
 previousTranslate	SVGZoomEventConstants::PreviousTranslate	DontDelete|ReadOnly
 newScale			SVGZoomEventConstants::NewScale				DontDelete|ReadOnly
 newTranslate		SVGZoomEventConstants::NewTranslate			DontDelete|ReadOnly
@end
*/

Value SVGZoomEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGZoomEventConstants::ZoomRectScreen:
			return KDOM::safe_cache<SVGRect>(exec, zoomRectScreen());
		case SVGZoomEventConstants::PreviousTranslate:
			return KDOM::safe_cache<SVGPoint>(exec, previousTranslate());
		case SVGZoomEventConstants::PreviousScale:
			return Number(previousScale());
		case SVGZoomEventConstants::NewTranslate:
			return KDOM::safe_cache<SVGPoint>(exec, newTranslate());
		case SVGZoomEventConstants::NewScale:
			return Number(newScale());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGZoomEventImpl *>(d))

SVGZoomEvent SVGZoomEvent::null;

SVGZoomEvent::SVGZoomEvent() : KDOM::UIEvent()
{
}

SVGZoomEvent::SVGZoomEvent(SVGZoomEventImpl *i) : KDOM::UIEvent(i)
{
}

SVGZoomEvent::SVGZoomEvent(const SVGZoomEvent &other) : KDOM::UIEvent()
{
	(*this) = other;
}

SVGZoomEvent::SVGZoomEvent(const KDOM::Event &other) : KDOM::UIEvent()
{
	(*this) = other;
}

SVGZoomEvent::~SVGZoomEvent()
{
}

SVGZoomEvent &SVGZoomEvent::operator=(const SVGZoomEvent &other)
{
	KDOM::Event::operator=(other);
	return *this;
}

KDOM_EVENT_DERIVED_ASSIGN_OP(SVGZoomEvent, Event, KDOM::TypeLastEvent)

SVGRect SVGZoomEvent::zoomRectScreen() const
{
	if(!d)
		return SVGRect::null;

	return SVGRect(impl->zoomRectScreen());
}

float SVGZoomEvent::previousScale() const
{
	if(!d)
		return 1.;

	return impl->previousScale();
}

SVGPoint SVGZoomEvent::previousTranslate() const
{
	if(!d)
		return SVGPoint::null;

	return SVGPoint(impl->previousTranslate());
}

float SVGZoomEvent::newScale() const
{
	if(!d)
		return 1.;

	return impl->newScale();
}

SVGPoint SVGZoomEvent::newTranslate() const
{
	if(!d)
		return SVGPoint::null;

	return SVGPoint(impl->newTranslate());
}

// vim:ts=4:noet
