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

#include "kdom.h"
#include "TextImpl.h"
#include "DocumentImpl.h"

using namespace KDOM;

TextImpl::TextImpl(DocumentPtr *doc, DOMStringImpl *text) : CharacterDataImpl(doc)
{
	str = text;
	if(str)
		str->ref();
}

TextImpl::~TextImpl()
{
}

DOMStringImpl *TextImpl::nodeName() const
{
	return new DOMStringImpl("#text");
}

unsigned short TextImpl::nodeType() const
{
	return TEXT_NODE;
}

NodeImpl *TextImpl::cloneNode(bool, DocumentPtr *doc) const
{
	return doc->document()->createTextNode(data());
}

TextImpl *TextImpl::splitText(unsigned long offset)
{
	// INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than
	//  the number of 16-bit units in data.
	if(offset > length() || (long)offset < 0)
		throw new DOMExceptionImpl(INDEX_SIZE_ERR);
 
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	DOMStringImpl *oldStr = str;
	TextImpl *p = new TextImpl(docPtr(), substringData(offset, length() - offset));
	
	str = str->copy();
	str->ref();
	str->remove(offset, length() - offset);

	// TODO: dispatchModifiedEvent(oldStr);
	oldStr->deref();

	if(parentNode())
		parentNode()->insertBefore(p, next);

	return p;
}

bool TextImpl::isElementContentWhitespace() const
{
	return (str && str->string().stripWhiteSpace().isEmpty()) &&
		   parentNode() && parentNode()->nodeType() == ELEMENT_NODE &&
		   (previousSibling() || nextSibling());
}

DOMStringImpl *TextImpl::wholeText() const
{
	DOMStringImpl *ret = new DOMStringImpl();

	QPtrList<NodeImpl> nodes = logicallyAdjacentTextNodes();
	QPtrListIterator<NodeImpl> it(nodes);
	for(;it.current(); ++it)
		ret->append((*it)->nodeValue());

	return ret;
}

bool TextImpl::checkChildren(NodeImpl *_node, const QPtrList<NodeImpl> &nodes) const
{
	for(NodeImpl *n = _node->firstChild();n;n = n->nextSibling())
	{
		if(n->nodeType() != ENTITY_REFERENCE_NODE && !nodes.containsRef(n))
			return false;

		checkChildren(n, nodes);
	}

	return true;
}

TextImpl *TextImpl::replaceWholeText(DOMStringImpl *content)
{
	TextImpl *replacement = 0;
	bool haveReplaced = false;
	if(isReadOnly() && content && content->length() > 0)
		replacement = ownerDocument()->createTextNode(content);

	QPtrList<NodeImpl> nodes = logicallyAdjacentTextNodes();
	QPtrList<NodeImpl> removables;
	QPtrListIterator<NodeImpl> it(nodes);
	for(;it.current(); ++it)
	{
		NodeImpl *n = (*it);
		if(n == this && !(!content || content->length() == 0 || isReadOnly()))
			continue;

		while(n->parentNode())
		{
			if(!n->parentNode()->isReadOnly())
			{
				if(!removables.containsRef(n))
					removables.append(n);
				break;
			}

			n = n->parentNode();
		}
	}

	QPtrListIterator<NodeImpl> it2(removables);
	for(;it2.current(); ++it2)
	{
		NodeImpl *removable = (*it2);
		if(!checkChildren(removable, nodes))
		{
			// NO_MODIFICATION_ALLOWED_ERR: Raised if one of the Text nodes being replaced is readonly.
			throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
		}

		if(replacement && !haveReplaced)
			removable->parentNode()->replaceChild(replacement, removable);
		else
			removable->parentNode()->removeChild(removable);
	}

	if(replacement)
		return replacement;

	if(!content || content->isEmpty())
		return 0;

	setData(content);
	return this;
}

QPtrList<NodeImpl> TextImpl::logicallyAdjacentTextNodes() const
{
	NodeImpl *n = const_cast<TextImpl *>(this);
	bool go = false;
	while(true)
	{
		NodeImpl *previous = 0;
		if(go)
			previous = n->lastChild();

		if(!previous)
		{
			previous = n->previousSibling();
			go = true;
		}
		if(!previous)
		{
			previous = n->parentNode();
			go = false;
			if(!previous || previous->nodeType() != ENTITY_REFERENCE_NODE)
				break;
		}

		if(previous && previous->nodeType() != TEXT_NODE &&
		   previous->nodeType() != CDATA_SECTION_NODE &&
		   previous->nodeType() != ENTITY_REFERENCE_NODE)
			break;

		n = previous;
	}

	QPtrList<NodeImpl> ret;
	go = true;
	while(true)
	{
		if(n->nodeType() != ENTITY_REFERENCE_NODE)
			ret.append(n);

		NodeImpl *_next = 0;
		if(go)
			_next = n->firstChild();

		if(!_next)
		{
			_next = n->nextSibling();
			go = true;
		}

		if(!_next)
		{
			_next = n->parentNode();
			go = false;
			if(!_next || _next->nodeType() != ENTITY_REFERENCE_NODE)
				break;
		}

		if(_next && _next->nodeType() != TEXT_NODE &&
		   _next->nodeType() != CDATA_SECTION_NODE &&
		   _next->nodeType() != ENTITY_REFERENCE_NODE)
			break;

		n = _next;
	}

	return ret;
}

// vim:ts=4:noet
