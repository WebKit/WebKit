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

#include "TreeWalker.h"
#include "TreeWalkerImpl.h"
#include "Ecma.h"
#include <kdom/traversal/NodeFilter.h>
#include "kdom.h"
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "TraversalConstants.h"
#include "TreeWalker.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin TreeWalker::s_hashTable 5
 root					TreeWalkerConstants::Root					DontDelete|ReadOnly
 whatToShow				TreeWalkerConstants::WhatToShow				DontDelete|ReadOnly
 filter					TreeWalkerConstants::Filter					DontDelete|ReadOnly
 expandEntityReferences	TreeWalkerConstants::ExpandEntityReferences	DontDelete|ReadOnly
 currentNode			TreeWalkerConstants::CurrentNode			DontDelete
@end
@begin TreeWalkerProto::s_hashTable 9
 parentNode			TreeWalkerConstants::ParentNode			DontDelete|Function 1
 firstChild			TreeWalkerConstants::FirstChild			DontDelete|Function 1
 lastChild			TreeWalkerConstants::LastChild			DontDelete|Function 1
 previousSibling	TreeWalkerConstants::PreviousSibling	DontDelete|Function 1
 nextSibling		TreeWalkerConstants::NextSibling		DontDelete|Function 1
 previousNode		TreeWalkerConstants::PreviousNode		DontDelete|Function 1
 nextNode			TreeWalkerConstants::NextNode			DontDelete|Function 1
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("TreeWalker", TreeWalkerProto, TreeWalkerProtoFunc)

ValueImp *TreeWalker::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case TreeWalkerConstants::Root:
			return getDOMNode(exec, root());
		case TreeWalkerConstants::WhatToShow:
			return Number(whatToShow());
	//	case TreeWalkerConstants::Filter:
	//		return getDOMString(filter());
		case TreeWalkerConstants::ExpandEntityReferences:
			return KJS::Boolean(expandEntityReferences());
		case TreeWalkerConstants::CurrentNode:
			return getDOMNode(exec, currentNode());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
	return Undefined();
};

void TreeWalker::putValueProperty(ExecState *exec, int token, ValueImp *, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case TreeWalkerConstants::CurrentNode:
			throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
			break;
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(DOMException)
}

ValueImp *TreeWalkerProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &)
{
	KDOM_CHECK_THIS(TreeWalker)
	KDOM_ENTER_SAFE

	switch(id)
	{
		case TreeWalkerConstants::ParentNode:
			return getDOMNode(exec, obj.parentNode());
		case TreeWalkerConstants::FirstChild:
			return getDOMNode(exec, obj.firstChild());
		case TreeWalkerConstants::LastChild:
			return getDOMNode(exec, obj.lastChild());
		case TreeWalkerConstants::PreviousSibling:
			return getDOMNode(exec, obj.previousSibling());
		case TreeWalkerConstants::NextSibling:
			return getDOMNode(exec, obj.nextSibling());
		case TreeWalkerConstants::PreviousNode:
			return getDOMNode(exec, obj.previousNode());
		case TreeWalkerConstants::NextNode:
			return getDOMNode(exec, obj.nextNode());
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(DOMException)
	return Undefined();
}

TreeWalker TreeWalker::null;

TreeWalker::TreeWalker() : d(0)
{
}

TreeWalker::TreeWalker(TreeWalkerImpl *p) : d(p)
{
}

TreeWalker::TreeWalker(const TreeWalker &other) : d(0)
{
	(*this) = other;
}

TreeWalker::~TreeWalker()
{
	if(d)
		d->deref();
}

TreeWalker &TreeWalker::operator=(const TreeWalker &other)
{
	if(d)
		d->deref();

	d = other.d;

	if(d)
		d->ref();

	return (*this);
}

bool TreeWalker::operator==(const TreeWalker &other) const
{
	return d == other.d;
}

bool TreeWalker::operator!=(const TreeWalker &other) const
{
	return !operator==(other);
}

Node TreeWalker::root() const
{
	if(!d)
		return Node::null;

	return Node(d->root());
}

short TreeWalker::whatToShow() const
{
	if(!d)
		return 0;

	return d->whatToShow();
}

NodeFilter TreeWalker::filter() const
{
	if(!d)
		return NodeFilter::null;

	return NodeFilter(d->filter());
}

bool TreeWalker::expandEntityReferences() const
{
	if(!d)
		return false;

	return d->expandEntityReferences();
}

void TreeWalker::setCurrentNode(const Node &/*currentNode*/)
{
	// FIXME!
	// throw Exception?
}

Node TreeWalker::currentNode() const
{
	if(!d)
		return Node::null;

	return Node(d->currentNode());
}

Node TreeWalker::parentNode()
{
	if(!d)
		return Node::null;

	return Node(d->parentNode());
}

Node TreeWalker::firstChild()
{
	if(!d)
		return Node::null;

	return Node(d->firstChild());
}

Node TreeWalker::lastChild()
{
	if(!d)
		return Node::null;

	return Node(d->lastChild());
}

Node TreeWalker::previousSibling()
{
	if(!d)
		return Node::null;

	return Node(d->previousSibling());
}

Node TreeWalker::nextSibling()
{
	if(!d)
		return Node::null;

	return Node(d->nextSibling());
}

Node TreeWalker::previousNode()
{
	if(!d)
		return Node::null;

	return Node(d->previousNode());
}

Node TreeWalker::nextNode()
{
	if(!d)
		return Node::null;

	return Node(d->nextNode());
}

// vim:ts=4:noet
