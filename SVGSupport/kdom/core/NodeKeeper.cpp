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

#include "NodeImpl.h"
#include "NodeKeeper.h"

#undef REFCOUNT_DEBUG

using namespace KDOM;
	
NodeKeeper::NodeKeeper()
{
#ifdef REFCOUNT_DEBUG
	kdDebug() << "NODEKEEPER CTOR!" << endl;
#endif

	m_destructing = false;
	m_nodeList.setAutoDelete(false);
}

NodeKeeper::~NodeKeeper()
{
#ifdef REFCOUNT_DEBUG
	kdDebug() << "NODEKEEPER DTOR!" << endl;
#endif

	m_destructing = true; // Set destruction mode
	QPtrList<NodeImpl>::ConstIterator it = m_nodeList.begin();
	QPtrList<NodeImpl>::ConstIterator end = m_nodeList.end();

	for(; it != end; ++it)
	{
		NodeImpl *impl = *it;
		Q_ASSERT(impl != 0);

#ifdef REFCOUNT_DEBUG
		kdDebug() << "-------> killNode " << impl << " ref: " << impl->refCount() << " name : " << impl->nodeName() << endl;
#endif

		impl->detach();
		impl->deref();

#ifdef REFCOUNT_DEBUG
		kdDebug() << "-------> done with killNode " << impl << endl;
#endif
	}

#ifdef REFCOUNT_DEBUG
	kdDebug() << "NODEKEEPER DTOR FINISHED!" << endl;
#endif
}

void NodeKeeper::addNode(NodeImpl *impl)
{
	Q_ASSERT(impl != 0);

#ifdef REFCOUNT_DEBUG
	kdDebug() << "-------> addNode " << impl << " ref: " << impl->refCount() << " name : " << impl->nodeName() << endl;
#endif

	impl->ref();
	m_nodeList.append(impl);

#ifdef REFCOUNT_DEBUG
	kdDebug() << "-------> done with addNode " << impl << endl;
#endif
}

void NodeKeeper::removeNode(NodeImpl *impl)
{
	Q_ASSERT(impl != 0);

#ifdef REFCOUNT_DEBUG
	kdDebug() << "-------> removeNode " << impl << " ref: " << impl->refCount() << " name : " << impl->nodeName() << endl;
#endif

	m_nodeList.remove(impl);
	impl->deref();

#ifdef REFCOUNT_DEBUG
	kdDebug() << "-------> done with removeNode " << impl << endl;
#endif
}

bool NodeKeeper::destructing() const
{
	return m_destructing;
}

// vim:ts=4:noet
