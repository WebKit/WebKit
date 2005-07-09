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

#include "Comment.h"
#include "CommentImpl.h"

namespace KDOM
{

// The qdom way...
#define impl (static_cast<CommentImpl *>(d))

Comment Comment::null;

Comment::Comment() : CharacterData()
{
}

Comment::Comment(CommentImpl *i) : CharacterData(i)
{
}

Comment::Comment(const Comment &other) : CharacterData()
{
	(*this) = other;
}

Comment::Comment(const Node &other) : CharacterData()
{
	(*this) = other;
}

Comment::~Comment()
{
}

Comment &Comment::operator=(const Comment &other)
{
	CharacterData::operator=(other);
	return *this;
}

Comment &Comment::operator=(const Node &other)
{
	NodeImpl *ohandle = static_cast<NodeImpl *>(other.handle());
	if(d != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != COMMENT_NODE)
		{
			if(d)
				d->deref();
			d = 0;
		}
		else
			Node::operator=(other);
	}

	return *this;
}

}

// vim:ts=4:noet
