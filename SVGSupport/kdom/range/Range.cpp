/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "Range.h"
#include "Document.h"
#include "NodeImpl.h"
#include "RangeImpl.h"
#include "kdomrange.h"
#include "DocumentImpl.h"
#include "RangeException.h"
#include "DOMException.h"
#include "DocumentFragment.h"
#include "RangeExceptionImpl.h"

#include "RangeConstants.h"
#include "Range.lut.h"
using namespace KJS;
using namespace KDOM;

/*
@begin Range::s_hashTable 7
 startContainer				RangeConstants::StartContainer				DontDelete|ReadOnly
 startOffset				RangeConstants::StartOffset					DontDelete|ReadOnly
 endContainer				RangeConstants::EndContainer				DontDelete|ReadOnly
 endOffset					RangeConstants::EndOffset					DontDelete|ReadOnly
 collapsed					RangeConstants::Collapsed					DontDelete|ReadOnly
 commonAncestorContainer	RangeConstants::CommonAncestorContainer		DontDelete|ReadOnly
@end
@begin RangeProto::s_hashTable 19
 setStart					RangeConstants::SetStart					DontDelete|Function 2
 setEnd						RangeConstants::SetEnd						DontDelete|Function 2
 setStartAfter				RangeConstants::SetStartAfter				DontDelete|Function 1
 setStartBefore				RangeConstants::SetStartBefore				DontDelete|Function 1
 setEndAfter				RangeConstants::SetEndAfter					DontDelete|Function 1
 setEndBefore				RangeConstants::SetEndBefore				DontDelete|Function 1
 collapse					RangeConstants::Collapse					DontDelete|Function 1 
 selectNode					RangeConstants::SelectNode					DontDelete|Function 1
 selectNodeContents			RangeConstants::SelectNodeContents			DontDelete|Function 1
 compareBoundaryPoints		RangeConstants::CompareBoundaryPoints		DontDelete|Function 2
 deleteContents				RangeConstants::DeleteContents				DontDelete|Function 0 
 extractContents			RangeConstants::ExtractContents				DontDelete|Function 0 
 cloneContents				RangeConstants::CloneContents				DontDelete|Function 0 
 insertNode					RangeConstants::InsertNode					DontDelete|Function 1 
 surroundContents			RangeConstants::SurroundContents			DontDelete|Function 1
 cloneRange					RangeConstants::CloneRange					DontDelete|Function 0
 toString					RangeConstants::ToString					DontDelete|Function 0
 detach						RangeConstants::Detach						DontDelete|Function 0
@end
*/

KDOM_IMPLEMENT_PROTOTYPE("Range", RangeProto, RangeProtoFunc)

Value Range::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case RangeConstants::StartContainer:
			return getDOMNode(exec, startContainer());
		case RangeConstants::StartOffset:
			return Number(startOffset());
		case RangeConstants::EndContainer:
			return getDOMNode(exec, endContainer());
		case RangeConstants::EndOffset:
			return Number(endOffset());
		case RangeConstants::Collapsed:
			return KJS::Boolean(collapsed());
		case RangeConstants::CommonAncestorContainer:
			return getDOMNode(exec, commonAncestorContainer());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(RangeException)
	return Undefined();
}

Value RangeProtoFunc::call(KJS::ExecState *exec, KJS::Object &thisObj, const List &args)
{
	KDOM_CHECK_THIS(Range)
	
//	KDOM_ENTER_SAFE
	
//	KDOM_ENTER_SAFE

	switch(id)
	{
		case RangeConstants::SetEnd:
		{
			Node parent = ecma_cast<Node>(exec, args[0], &toNode);
			int offset = args[1].toInt32(exec);
			obj.setEnd(parent, offset);
			return Undefined();
		}
		case RangeConstants::SetStart:
		{
			Node parent = ecma_cast<Node>(exec, args[0], &toNode);
			int offset = args[1].toInt32(exec);
			obj.setStart(parent, offset);
			return Undefined();
		}	
		case RangeConstants::SetStartAfter:
		{
			Node refNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.setStartAfter(refNode);
			return Undefined();
		}
 		case RangeConstants::SetStartBefore:
 		{
			Node refNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.setStartBefore(refNode);
			return Undefined();
		}
		case RangeConstants::SetEndAfter:
 		{
			Node refNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.setEndAfter(refNode);
			return Undefined();
		}
		case RangeConstants::SetEndBefore:
 		{
			Node refNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.setEndBefore(refNode);
			return Undefined();
		}
		case RangeConstants::Collapse:
		{
			obj.collapse( args[0].toBoolean(exec) );
			return Undefined();
		}
 		case RangeConstants::SelectNode:
		{
			Node refNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.selectNode(refNode);
			return Undefined();
		}
 		case RangeConstants::SelectNodeContents:
		{
			Node refNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.selectNodeContents(refNode);
			return Undefined();
		}
 		case RangeConstants::CompareBoundaryPoints:
		{
			int how = args[0].toInt32(exec);
			Range sourceRange = ecma_cast<Range>(exec, args[1], &toRange);
			return Number(obj.compareBoundaryPoints(static_cast<CompareHow>(how) ,sourceRange));
		}
 		case RangeConstants::DeleteContents:
		{
			obj.deleteContents();
			return Undefined();
		}
 		case RangeConstants::ExtractContents:
		{
			return getDOMNode(exec, obj.extractContents());
		}
 		case RangeConstants::CloneContents:
		{
			return getDOMNode(exec, obj.cloneContents());
		}
 		case RangeConstants::InsertNode:
		{
			Node newNode = ecma_cast<Node>(exec, args[0], &toNode);
			obj.insertNode(newNode);
			return Undefined();
		}
 		case RangeConstants::SurroundContents:
		{
			Node newParent = ecma_cast<Node>(exec, args[0], &toNode);
			obj.surroundContents(newParent);
			return Undefined();
		}
 		case RangeConstants::CloneRange:
			return safe_cache<Range>(exec, obj.cloneRange());
		case RangeConstants::ToString:
			return getDOMString(obj.toString());
		case RangeConstants::Detach:
			obj.detach();
			return Undefined();
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
			break;
	}

//	KDOM_LEAVE_CALL_SAFE(RangeException)
			
//	KDOM_LEAVE_CALL_SAFE(DOMException)
	
	return Undefined();
}

Range Range::null;

Range::Range()
{
	// a range can't exist by itself - it must be associated with a document
	d = 0;
}

Range::Range(RangeImpl *i) : d(i)
{
	if(d)
		d->ref();
}

Range::Range(const Range &other)
{
	d = other.d;
	if(d)
		d->ref();
}

Range::Range(const Document &rootContainer)
{
	if(rootContainer != Document::null)
	{
		d = new RangeImpl(static_cast<DocumentImpl *>(rootContainer.handle()));
		d->ref();
	}
	else
		d = 0;
}

Range::Range(const Node &startContainer, const long startOffset, const Node &endContainer, const long endOffset)
{
	if(startContainer == Node::null || endContainer == Node::null)
		throw new DOMExceptionImpl(NOT_FOUND_ERR);

	if(startContainer.ownerDocument() == Document::null ||
	   startContainer.ownerDocument() != endContainer.ownerDocument())
		throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

	d = new RangeImpl(static_cast<DocumentImpl *>(startContainer.ownerDocument().handle()),
					  static_cast<NodeImpl *>(startContainer.handle()), startOffset,
					  static_cast<NodeImpl *>(endContainer.handle()), endOffset);
	d->ref();
}

KDOM_IMPL_DTOR_ASSIGN_OP(Range)

Node Range::startContainer() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return Node(d->startContainer());
}

long Range::startOffset() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return d->startOffset();
}

Node Range::endContainer() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return Node(d->endContainer());
}

long Range::endOffset() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return d->endOffset();
}

bool Range::collapsed() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return d->collapsed();
}

Node Range::commonAncestorContainer() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return Node(d->commonAncestorContainer());
}

void Range::setStart(const Node &refNode, long offset)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->setStart(static_cast<NodeImpl *>(refNode.handle()), offset);
}

void Range::setEnd(const Node &refNode, long offset)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->setEnd(static_cast<NodeImpl *>(refNode.handle()), offset);
}

void Range::setStartBefore(const Node &refNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->setStartBefore(static_cast<NodeImpl *>(refNode.handle()));
}

void Range::setStartAfter(const Node &refNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->setStartAfter(static_cast<NodeImpl *>(refNode.handle()));
}

void Range::setEndBefore(const Node &refNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->setEndBefore(static_cast<NodeImpl *>(refNode.handle()));
}

void Range::setEndAfter(const Node &refNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->setEndAfter(static_cast<NodeImpl *>(refNode.handle()));
}

void Range::collapse(bool toStart)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->collapse(toStart);
}

void Range::selectNode(const Node &refNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->selectNode(static_cast<NodeImpl *>(refNode.handle()));
}

void Range::selectNodeContents(const Node &refNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->selectNodeContents(static_cast<NodeImpl *>(refNode.handle()));
}

short Range::compareBoundaryPoints(unsigned short how, const Range &sourceRange)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return d->compareBoundaryPoints(how, static_cast<RangeImpl *>(sourceRange.handle()));
}

bool Range::boundaryPointsValid()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return d->boundaryPointsValid();
}

void Range::deleteContents()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->deleteContents();
}

DocumentFragment Range::extractContents()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return DocumentFragment(d->extractContents());
}

DocumentFragment Range::cloneContents()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return DocumentFragment(d->cloneContents());
}

void Range::insertNode(const Node &newNode)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);
    
	d->insertNode(static_cast<NodeImpl *>(newNode.handle()));
}

void Range::surroundContents(const Node &newParent)
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->surroundContents(static_cast<NodeImpl *>(newParent.handle()));
}

Range Range::cloneRange()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return Range(d->cloneRange());
}

DOMString Range::toString()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

    return d->toString();
}


void Range::detach()
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	d->detach();
}

bool Range::isDetached() const
{
	if(!d)
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	return d->isDetached();
}

// vim:ts=4:noet
