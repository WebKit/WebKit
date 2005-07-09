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

#ifndef KDOM_EventImpl_H
#define KDOM_EventImpl_H

#include <qdatetime.h>

#include <kdom/kdom.h>
#include <kdom/Shared.h>
#include <kdom/DOMString.h>

namespace KDOM
{
	typedef enum
	{
		TypeGenericEvent = 0,
		TypeUIEvent = 1,
		TypeMouseEvent = 2,
		TypeMutationEvent = 3,
		TypeKeyboardEvent = 4,
		TypeLastEvent = 99
	} EventImplType;

	class EventTargetImpl;
	class EventImpl : public Shared
	{
	public:
		EventImpl(EventImplType identifier);
		virtual ~EventImpl();

		DOMString type() const;

		EventTargetImpl *target() const;
		EventTargetImpl *currentTarget() const;

		unsigned short eventPhase() const;

		bool bubbles() const;
		bool cancelable() const;

		DOMTimeStamp timeStamp() const;

		void stopPropagation();
		void preventDefault();

		virtual void initEvent(const DOMString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

		// Internal
		void setTarget(EventTargetImpl *target);
		void setCurrentTarget(EventTargetImpl *currentTarget);

		void setEventPhase(unsigned short eventPhase);

		int id() const;
		EventImplType identifier() const;

		bool defaultPrevented() const;
		bool propagationStopped() const;

		void setDefaultHandled() { m_defaultHandled = true; }
		bool defaultHandled() const { return m_defaultHandled; }
		
	protected:
		DOMString m_type;
		QDateTime m_createTime;

		EventImplType m_identifier;

		EventTargetImpl *m_target;
		EventTargetImpl *m_currentTarget;

		int m_id;	
		unsigned short m_eventPhase : 2;

		bool m_bubbles : 1;
		bool m_cancelable : 1;
		bool m_propagationStopped : 1;
		bool m_defaultPrevented : 1;
		bool m_defaultHandled : 1;
	};
};

#endif

// vim:ts=4:noet
