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

#include "Event.h"
#include "EventListenerImpl.h"
#include "RegisteredEventListener.h"

using namespace KDOM;

RegisteredEventListener::RegisteredEventListener(const DOMString &type, EventListenerImpl *listener, bool useCapture)
{
	m_type = type;
	m_listener = listener;
	m_useCapture = useCapture;
}

RegisteredEventListener::~RegisteredEventListener()
{
}

bool RegisteredEventListener::operator==(const RegisteredEventListener &other) const
{
	return type() == other.type() &&
		   useCapture() == other.useCapture() &&
		   listener() == other.listener();
}

bool RegisteredEventListener::operator!=(const RegisteredEventListener &other) const
{
	return !operator==(other);
}

DOMString RegisteredEventListener::type() const
{
	return m_type;
}

bool RegisteredEventListener::useCapture() const
{
	return m_useCapture;
}

EventListenerImpl *RegisteredEventListener::listener() const
{
	return m_listener;
}

// vim:ts=4:noet
