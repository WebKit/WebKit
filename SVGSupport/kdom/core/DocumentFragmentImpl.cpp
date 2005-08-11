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

#include "DocumentFragmentImpl.h"
#include "DocumentImpl.h"

using namespace KDOM;

DocumentFragmentImpl::DocumentFragmentImpl(DocumentImpl *doc) : NodeBaseImpl(doc)
{
}

DocumentFragmentImpl::~DocumentFragmentImpl()
{
}

DOMString DocumentFragmentImpl::nodeName() const
{
	return "#document-fragment";
}

unsigned short DocumentFragmentImpl::nodeType() const
{
	return DOCUMENT_FRAGMENT_NODE;
}

bool DocumentFragmentImpl::childTypeAllowed(unsigned short type) const
{
	switch(type)
	{
		case ELEMENT_NODE:
		case TEXT_NODE:
		case COMMENT_NODE:
		case PROCESSING_INSTRUCTION_NODE:
		case CDATA_SECTION_NODE:
		case ENTITY_REFERENCE_NODE:
			return true;
		default:
			return false;
	}
}

NodeImpl *DocumentFragmentImpl::cloneNode(bool deep, DocumentImpl *doc) const
{
    DocumentFragmentImpl *clone = doc->createDocumentFragment();
    if(deep)
        cloneChildNodes(clone, doc);

    return clone;
}

// vim:ts=4:noet
