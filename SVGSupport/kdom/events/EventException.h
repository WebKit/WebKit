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

#ifndef KDOM_EventException_H
#define KDOM_EventException_H

#include <kdom/ecma/Ecma.h>

namespace KDOM
{
	class EventExceptionImpl;
	class EventException
	{
	public:
		EventException();
		explicit EventException(EventExceptionImpl *i);
		EventException(const EventException &other);
		virtual ~EventException();

		// Operators
		EventException &operator=(const EventException &other);
		bool operator==(const EventException &other) const;
		bool operator!=(const EventException &other) const;

		// 'EventException' functions
		unsigned short code() const;

		// Internal
		KDOM_INTERNAL_BASE(EventException)

	protected:
		EventExceptionImpl *d;

	public: // EcmaScript section
		KDOM_GET

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

#endif

// vim:ts=4:noet
