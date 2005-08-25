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

#ifndef KDOM_MouseEventImpl_H
#define KDOM_MouseEventImpl_H

#include <kdom/events/impl/UIEventImpl.h>

class QMouseEvent;

namespace KDOM
{
	class EventTargetImpl;
	class AbstractViewImpl;
	class MouseEventImpl : public UIEventImpl
	{
	public:
		MouseEventImpl(EventImplType identifier);
		virtual ~MouseEventImpl();

		long screenX() const;
		long screenY() const;
		
		long clientX() const;
		long clientY() const;
		
		bool ctrlKey() const;
		bool shiftKey() const;
		bool altKey() const;
		bool metaKey() const;
		
		unsigned short button() const;

		EventTargetImpl *relatedTarget() const;
		void setRelatedTarget(EventTargetImpl *target);

		void initMouseEvent(DOMStringImpl *typeArg, bool canBubbleArg, bool cancelableArg, AbstractViewImpl *viewArg, long detailArg, long screenXArg, long screenYArg, long clientXArg, long clientYArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg, unsigned short buttonArg, EventTargetImpl *relatedTargetArg);
		void initMouseEvent(DOMStringImpl *typeArg, QMouseEvent *qevent, float scale = 1.0);

		// Helpers
		QMouseEvent *qEvent() const { return m_qevent; }
		void setQEvent(QMouseEvent *event) { m_qevent = event; }
	
	private:
		long m_screenX, m_screenY;
		long m_clientX, m_clientY;

		bool m_ctrlKey : 1;
		bool m_altKey : 1;
		bool m_shiftKey : 1;
		bool m_metaKey : 1;

		unsigned short m_button;
		EventTargetImpl *m_relatedTarget;

		QMouseEvent *m_qevent;
	};
};

#endif

// vim:ts=4:noet
