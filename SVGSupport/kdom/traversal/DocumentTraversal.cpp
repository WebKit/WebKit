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

#include "DocumentTraversal.h"
#include "DocumentTraversalImpl.h"
#include "Ecma.h"
#include "NodeFilter.h"
#include "NodeImpl.h"
#include "NodeIterator.h"
#include "TreeWalker.h"
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "TraversalConstants.h"
#include "DocumentTraversal.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DocumentTraversal::s_hashTable 2
 dummy				DocumentTraversalConstants::Dummy				DontDelete|ReadOnly
@end
@begin DocumentTraversalProto::s_hashTable 3
 createNodeIterator	DocumentTraversalConstants::CreateNodeIterator	DontDelete|Function 4
 createTreeWalker 	DocumentTraversalConstants::CreateTreeWalker	DontDelete|Function 4
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("DocumentTraversal", DocumentTraversalProto, DocumentTraversalProtoFunc)

ValueImp *DocumentTraversal::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
}

ValueImp *DocumentTraversalProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	DocumentTraversal obj(cast(exec, thisObj));
	Q_ASSERT(obj.d != 0);
	KDOM_ENTER_SAFE

	switch(id)
	{
		case DocumentTraversalConstants::CreateNodeIterator:
		{
			Node root = ecma_cast<Node>(exec, args[0], &toNode);
			unsigned short whatToShow = args[1]->toUInt16(exec);
			bool entityReferenceExpansion = args[3]->toBoolean(exec);
			return obj.createNodeIterator(root, whatToShow, NodeFilter::null, entityReferenceExpansion).cache(exec);
		}
		case DocumentTraversalConstants::CreateTreeWalker:
		{
			Node root = ecma_cast<Node>(exec, args[0], &toNode);
			unsigned short whatToShow = args[1]->toUInt16(exec);
			bool entityReferenceExpansion = args[3]->toBoolean(exec);
			return obj.createTreeWalker(root, whatToShow, NodeFilter::null, entityReferenceExpansion).cache(exec);
		}
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

DocumentTraversal DocumentTraversal::null;

DocumentTraversal::DocumentTraversal() : d(0)
{
}

DocumentTraversal::DocumentTraversal(DocumentTraversalImpl *i) : d(i)
{
}

DocumentTraversal::DocumentTraversal(const DocumentTraversal &other) : d(0)
{
        (*this) = other;
}

DocumentTraversal::~DocumentTraversal()
{
}

DocumentTraversal &DocumentTraversal::operator=(const DocumentTraversal &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

DocumentTraversal &DocumentTraversal::operator=(DocumentTraversalImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

NodeIterator DocumentTraversal::createNodeIterator(const Node &root, short whatToShow, const NodeFilter &filter, bool entityReferenceExpansion) const
{
	if(!d)
		return NodeIterator::null;

	return NodeIterator(d->createNodeIterator(static_cast<NodeImpl *>(root.handle()), whatToShow, filter.handle(), entityReferenceExpansion));
}

TreeWalker DocumentTraversal::createTreeWalker(const Node &root, short whatToShow, const NodeFilter &filter, bool entityReferenceExpansion) const
{
	if(!d)
		return TreeWalker::null;

	return TreeWalker(d->createTreeWalker(static_cast<NodeImpl *>(root.handle()), whatToShow, filter.handle(), entityReferenceExpansion));
}

// vim:ts=4:noet
