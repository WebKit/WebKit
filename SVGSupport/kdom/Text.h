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

#ifndef KDOM_Text_H
#define KDOM_Text_H

#include <kdom/CharacterData.h>

namespace KDOM
{
	class TextImpl;

	/**
	 * @short The Text interface inherits from CharacterData and represents the
	 * textual content (termed character data in XML) of an Element or Attr.
	 *
	 * If there is no markup inside an element's content, the text is contained
	 * in a single object implementing the Text interface that is the only child
	 * of the element. If there is markup, it is parsed into the information
	 * items (elements, comments, etc.) and Text nodes that form the list of
	 * children of the element.
	 *
	 * When a document is first made available via the DOM, there is only one
	 * Text node for each block of text. Users may create adjacent Text nodes
	 * that represent the contents of a given element without any intervening
	 * markup, but should be aware that there is no way to represent the
	 * separations between these nodes in XML or HTML, so they will not
	 * (in general) persist between DOM editing sessions. The Node.normalize()
	 * method merges any such adjacent Text objects into a single node for each
	 * block of text.
	 *
	 * No lexical check is done on the content of a Text node and, depending
	 * on its position in the document, some characters must be escaped during
	 * serialization using character references; e.g. the characters
	 * "<&" if the textual content is part of an element or of an attribute,
	 * the character sequence "]]>" when part of an element, the quotation mark
	 * character " or the apostrophe character ' when part of an attribute.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
	 */
	class Text : public CharacterData 
	{
	public:
		Text();
		explicit Text(TextImpl *i);
		Text(const Text &other);
		Text(const Node &other);
		virtual ~Text();

		// Operators
		Text &operator=(const Text &other);
		Text &operator=(const Node &other);

		// 'Text' functions
		/**
		 * Breaks this node into two nodes at the specified offset, keeping both
		 * in the tree as siblings. After being split, this node will contain all
		 * the content up to the offset point. A new node of the same type, which
		 * contains all the content at and after the offset point, is returned. If
		 * the original node had a parent node, the new node is inserted as the
		 * next sibling of the original node. When the offset is equal to the
		 * length of this node, the new node has no data.
		 *
		 * @param offset The 16-bit unit offset at which to split, starting from 0.
		 *
		 * @returns The new node, of the same type as this node.
		 */
		Text splitText(unsigned long offset);

		/**
		 * Returns whether this text node contains element content whitespace,
		 * often abusively called "ignorable whitespace". The text node is
		 * determined to contain whitespace in element content during the load
		 * of the document or if validation occurs while using
		 * Document.normalizeDocument().
		 */
		bool isElementContentWhitespace() const; // DOM3

		/**
		 * Returns all text of Text nodes logically-adjacent text nodes to
		 * this node, concatenated in document order.
		 */
		DOMString wholeText() const; // DOM3

		/**
		 * Replaces the text of the current node and all logically-adjacent
		 * text nodes with the specified text. All logically-adjacent text
		 * nodes are removed including the current node unless it was the
		 * recipient of the replacement text.
		 *
		 * @param content The content of the replacing Text node.
		 *
		 * @returns The Text node created with the specified content.
		 */
		Text replaceWholeText(const DOMString &content); // DOM3

		// Internal
		KDOM_INTERNAL(Text)

	public: // EcmaScript section
		KDOM_GET
		KDOM_FORWARDPUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
	};
};

KDOM_DEFINE_PROTOTYPE(TextProto)
KDOM_IMPLEMENT_PROTOFUNC(TextProtoFunc, Text)

#endif

// vim:ts=4:noet
