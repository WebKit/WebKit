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

#ifndef KDOM_DocumentEvent_H
#define KDOM_DocumentEvent_H

namespace KDOM
{
	class Event;

	class DocumentEventImpl;
	class DocumentEvent
	{
	public:
		DocumentEvent();
		explicit DocumentEvent(DocumentEventImpl *i);
		DocumentEvent(const DocumentEvent &other);
		virtual ~DocumentEvent();

		// Operators
		DocumentEvent &operator=(const DocumentEvent &other);
		DocumentEvent &operator=(DocumentEventImpl *other);

		// 'DocumentEvent' functions
		Event createEvent(const DOMString &eventType);

		// Internal
		KDOM_INTERNAL(DocumentEvent)

	public: // We don't really belong to our own "impl" class;
			// our d-ptr is shared with the 'Document' class
		DocumentEventImpl *d;

	public: // EcmaScript section
		KDOM_BASECLASS_GET

		KJS::Value getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(DocumentEventProto)
KDOM_IMPLEMENT_PROTOFUNC(DocumentEventProtoFunc, DocumentEvent)

#endif

// vim:ts=4:noet
