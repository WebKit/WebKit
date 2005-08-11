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

#ifndef KDOM_Comment_H
#define KDOM_Comment_H

#include <kdom/CharacterData.h>

namespace KDOM
{
	class CommentImpl;

	/**
	 * @short This interface inherits from CharacterData and
	 * represents the content of a comment, i.e., all the characters
	 * between the starting '<!--' and ending '-->'.
	 *
	 * Note that this is the definition of a comment in XML, and, in
	 * practice, HTML, although some HTML tools may implement the full
	 * SGML comment structure.
	 *
	 * No lexical check is done on the content of a comment and it
	 * is therefore possible to have the character sequence "--"
	 * (double-hyphen) in the content, which is illegal in a comment
	 * per section 2.5 of [XML 1.0]. The presence of this character
	 * sequence must generate a fatal error during serialization. 
	 *
	 * @author Rob Buis <buis@kde.org>
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 */
	class Comment : public CharacterData 
	{
	public:
		Comment();
		explicit Comment(CommentImpl *i);
		Comment(const Comment &other);
		Comment(const Node &other);
		virtual ~Comment();

		// Operators
		Comment &operator=(const Comment &other);
		Comment &operator=(const Node &other);

		// Internal
		KDOM_INTERNAL(Comment)
	};
};

#endif

// vim:ts=4:noet
