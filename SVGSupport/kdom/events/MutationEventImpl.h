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

#ifndef KDOM_MutationEventImpl_H
#define KDOM_MutationEventImpl_H

#include <kdom/events/impl/EventImpl.h>

namespace KDOM
{
	class NodeImpl;
	class MutationEventImpl : public EventImpl
	{
	public:
		MutationEventImpl(EventImplType identifier);
		virtual ~MutationEventImpl();

		NodeImpl *relatedNode() const;
		
		DOMString prevValue() const;
		DOMString newValue() const;
		DOMString attrName() const;

		unsigned short attrChange() const;

		void initMutationEvent(const DOMString &typeArg, bool canBubbleArg, bool cancelableArg, NodeImpl *relatedNodeArg, const DOMString &prevValueArg, const DOMString &newValueArg, const DOMString &attrNameArg, unsigned short attrChangeArg);

	private:
		DOMString m_prevValue;
		DOMString m_newValue;
		DOMString m_attrName;

		unsigned short m_attrChange;

		NodeImpl *m_relatedNode;
	};
};

#endif

// vim:ts=4:noet
