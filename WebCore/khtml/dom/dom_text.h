/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 1 Specification (Recommendation)
 * http://www.w3.org/TR/REC-DOM-Level-1/
 * Copyright © World Wide Web Consortium , (Massachusetts Institute of
 * Technology , Institut National de Recherche en Informatique et en
 * Automatique , Keio University ). All Rights Reserved.
 *
 */
#ifndef _DOM_CharacterData_h_
#define _DOM_CharacterData_h_

#include <dom/dom_node.h>

namespace DOM {

class DOMString;
class CharacterDataImpl;

/**
 * The <code> CharacterData </code> interface extends Node with a set
 * of attributes and methods for accessing character data in the DOM.
 * For clarity this set is defined here rather than on each object
 * that uses these attributes and methods. No DOM objects correspond
 * directly to <code> CharacterData </code> , though <code> Text
 * </code> and others do inherit the interface from it. All <code>
 * offset</code>s in this interface start from 0.
 *
 */
class CharacterData : public Node
{
#if APPLE_CHANGES
    friend class CharacterDataImpl;
#endif

public:
    CharacterData();
    CharacterData(const CharacterData &other);
    CharacterData(const Node &other) : Node()
         {(*this)=other;}

    CharacterData & operator = (const Node &other);
    CharacterData & operator = (const CharacterData &other);

    ~CharacterData();

    /**
     * The character data of the node that implements this interface.
     * The DOM implementation may not put arbitrary limits on the
     * amount of data that may be stored in a <code> CharacterData
     * </code> node. However, implementation limits may mean that the
     * entirety of a node's data may not fit into a single <code>
     * DOMString </code> . In such cases, the user may call <code>
     * substringData </code> to retrieve the data in appropriately
     * sized pieces.
     *
     * @exception DOMException
     * DOMSTRING_SIZE_ERR: Raised when it would return more characters
     * than fit in a <code> DOMString </code> variable on the
     * implementation platform.
     *
     */
    DOMString data() const;

    /**
     * see @ref data
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly.
     *
     */
    void setData( const DOMString & );

    /**
     * The number of characters that are available through <code> data
     * </code> and the <code> substringData </code> method below. This
     * may have the value zero, i.e., <code> CharacterData </code>
     * nodes may be empty.
     *
     */
    unsigned long length() const;

    /**
     * Extracts a range of data from the node.
     *
     * @param offset Start offset of substring to extract.
     *
     * @param count The number of characters to extract.
     *
     * @return The specified substring. If the sum of <code> offset
     * </code> and <code> count </code> exceeds the <code> length
     * </code> , then all characters to the end of the data are
     * returned.
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified offset is negative or
     * greater than the number of characters in <code> data </code> ,
     * or if the specified <code> count </code> is negative.
     *
     *  DOMSTRING_SIZE_ERR: Raised if the specified range of text does
     * not fit into a <code> DOMString </code> .
     *
     */
    DOMString substringData ( const unsigned long offset, const unsigned long count );

    /**
     * Append the string to the end of the character data of the node.
     * Upon success, <code> data </code> provides access to the
     * concatenation of <code> data </code> and the <code> DOMString
     * </code> specified.
     *
     * @param arg The <code> DOMString </code> to append.
     *
     * @return
     *
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
     *
     */
    void appendData ( const DOMString &arg );

    /**
     * Insert a string at the specified character offset.
     *
     * @param offset The character offset at which to insert.
     *
     * @param arg The <code> DOMString </code> to insert.
     *
     * @return
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified offset is negative or
     * greater than the number of characters in <code> data </code> .
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
     *
     */
    void insertData ( const unsigned long offset, const DOMString &arg );

    /**
     * Remove a range of characters from the node. Upon success,
     * <code> data </code> and <code> length </code> reflect the
     * change.
     *
     * @param offset The offset from which to remove characters.
     *
     * @param count The number of characters to delete. If the sum of
     * <code> offset </code> and <code> count </code> exceeds <code>
     * length </code> then all characters from <code> offset </code>
     * to the end of the data are deleted.
     *
     * @return
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified offset is negative or
     * greater than the number of characters in <code> data </code> ,
     * or if the specified <code> count </code> is negative.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
     *
     */
    void deleteData ( const unsigned long offset, const unsigned long count );

    /**
     * Replace the characters starting at the specified character
     * offset with the specified string.
     *
     * @param offset The offset from which to start replacing.
     *
     * @param count The number of characters to replace. If the sum of
     * <code> offset </code> and <code> count </code> exceeds <code>
     * length </code> , then all characters to the end of the data are
     * replaced (i.e., the effect is the same as a <code> remove
     * </code> method call with the same range, followed by an <code>
     * append </code> method invocation).
     *
     * @param arg The <code> DOMString </code> with which the range
     * must be replaced.
     *
     * @return
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified offset is negative or
     * greater than the number of characters in <code> data </code> ,
     * or if the specified <code> count </code> is negative.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
     *
     */
    void replaceData ( const unsigned long offset, const unsigned long count, const DOMString &arg );

protected:
    CharacterData(CharacterDataImpl *i);
};


class CommentImpl;

/**
 * This represents the content of a comment, i.e., all the characters
 * between the starting ' <code> &lt;!-- </code> ' and ending ' <code>
 * --&gt; </code> '. Note that this is the definition of a comment in
 * XML, and, in practice, HTML, although some HTML tools may implement
 * the full SGML comment structure.
 *
 */
class Comment : public CharacterData
{
    friend class Document;

public:
    Comment();
    Comment(const Comment &other);
    Comment(const Node &other) : CharacterData()
         {(*this)=other;}

    Comment & operator = (const Node &other);
    Comment & operator = (const Comment &other);

    ~Comment();

protected:
    Comment(CommentImpl *i);
};

class TextImpl;

/**
 * The <code> Text </code> interface represents the textual content
 * (termed <a href="&xml-spec;#syntax"> character data </a> in XML) of
 * an <code> Element </code> or <code> Attr </code> . If there is no
 * markup inside an element's content, the text is contained in a
 * single object implementing the <code> Text </code> interface that
 * is the only child of the element. If there is markup, it is parsed
 * into a list of elements and <code> Text </code> nodes that form the
 * list of children of the element.
 *
 *  When a document is first made available via the DOM, there is only
 * one <code> Text </code> node for each block of text. Users may
 * create adjacent <code> Text </code> nodes that represent the
 * contents of a given element without any intervening markup, but
 * should be aware that there is no way to represent the separations
 * between these nodes in XML or HTML, so they will not (in general)
 * persist between DOM editing sessions. The <code> normalize()
 * </code> method on <code> Element </code> merges any such adjacent
 * <code> Text </code> objects into a single node for each block of
 * text; this is recommended before employing operations that depend
 * on a particular document structure, such as navigation with <code>
 * XPointers. </code>
 *
 */
class Text : public CharacterData
{
    friend class Document;
#if APPLE_CHANGES
    friend class TextImpl;
#endif
public:
    Text();
    Text(const Text &other);
    Text(const Node &other) : CharacterData()
         {(*this)=other;}

    Text & operator = (const Node &other);
    Text & operator = (const Text &other);

    ~Text();

    /**
     * Breaks this <code> Text </code> node into two Text nodes at the
     * specified offset, keeping both in the tree as siblings. This
     * node then only contains all the content up to the <code> offset
     * </code> point. And a new <code> Text </code> node, which is
     * inserted as the next sibling of this node, contains all the
     * content at and after the <code> offset </code> point.
     *
     * @param offset The offset at which to split, starting from 0.
     *
     * @return The new <code> Text </code> node.
     *
     * @exception DOMException
     * INDEX_SIZE_ERR: Raised if the specified offset is negative or
     * greater than the number of characters in <code> data </code> .
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly.
     *
     */
    Text splitText ( const unsigned long offset );

protected:
    Text(TextImpl *i);

};

}; //namespace
#endif
