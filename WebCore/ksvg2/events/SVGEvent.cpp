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

#include <kdom/Shared.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGEvent.h"
#include "SVGEventImpl.h"

#include <ksvg2/data/EventsConstants.h>
#include "SVGEvent.lut.h"
using namespace KSVG;

/*
@begin SVGEvent::s_hashTable 3
 dummy	SVGEventConstants::Dummy	DontDelete|ReadOnly
@end
*/

ValueImp *SVGEvent::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(KDOM::DOMException)
	return Undefined();
}

SVGEvent SVGEvent::null;

SVGEvent::SVGEvent() : KDOM::Event()
{
}

SVGEvent::SVGEvent(SVGEventImpl *i) : KDOM::Event(i)
{
}

SVGEvent::SVGEvent(const SVGEvent &other) : KDOM::Event()
{
	(*this) = other;
}

SVGEvent::SVGEvent(const KDOM::Event &other) : KDOM::Event()
{
	(*this) = other;
}

SVGEvent::~SVGEvent()
{
}

SVGEvent &SVGEvent::operator=(const SVGEvent &other)
{
	KDOM::Event::operator=(other);
	return *this;
}

KDOM_EVENT_DERIVED_ASSIGN_OP(SVGEvent, Event, KDOM::TypeLastEvent)

// vim:ts=4:noet
