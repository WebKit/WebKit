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

#include "DocumentFragment.h"
#include "DocumentFragmentImpl.h"
using namespace KDOM;

// The qdom way...
#define impl (static_cast<DocumentFragmentImpl *>(d))

DocumentFragment DocumentFragment::null;

DocumentFragment::DocumentFragment() : Node()
{
}

DocumentFragment::DocumentFragment(DocumentFragmentImpl *i) : Node(i)
{
}

DocumentFragment::DocumentFragment(const DocumentFragment &other) : Node()
{
	(*this) = other;	
}

DocumentFragment::DocumentFragment(const Node &other) : Node()
{
	(*this) = other;	
}

DocumentFragment::~DocumentFragment()
{
}

DocumentFragment &DocumentFragment::operator=(const DocumentFragment &other)
{
	Node::operator=(other);
	return *this;
}

KDOM_NODE_DERIVED_ASSIGN_OP(DocumentFragment, DOCUMENT_FRAGMENT_NODE)

// vim:ts=4:noet
