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
#ifndef _DOM_XML_h
#define _DOM_XML_h

#include <dom/dom_text.h>
#include <dom/css_stylesheet.h>

namespace DOM {

class CDATASectionImpl;
class EntityImpl;
class EntityReferenceImpl;
class NotationImpl;
class ProcessingInstructionImpl;



/**
 * CDATA sections are used to escape blocks of text containing
 * characters that would otherwise be regarded as markup. The only
 * delimiter that is recognized in a CDATA section is the "]]&gt;"
 * string that ends the CDATA section. CDATA sections can not be
 * nested. The primary purpose is for including material such as XML
 * fragments, without needing to escape all the delimiters.
 *
 *  The <code> DOMString </code> attribute of the <code> Text </code>
 * node holds the text that is contained by the CDATA section. Note
 * that this may contain characters that need to be escaped outside of
 * CDATA sections and that, depending on the character encoding
 * ("charset") chosen for serialization, it may be impossible to write
 * out some characters as part of a CDATA section.
 *
 *  The <code> CDATASection </code> interface inherits the <code>
 * CharacterData </code> interface through the <code> Text </code>
 * interface. Adjacent <code> CDATASections </code> nodes are not
 * merged by use of the Element.normalize() method.
 *
 */
class CDATASection : public Text
{
    friend class Document;
public:
    CDATASection();
    CDATASection(const CDATASection &other);
    CDATASection(const Node &other) : Text()
         {(*this)=other;}

    CDATASection & operator = (const Node &other);
    CDATASection & operator = (const CDATASection &other);

    ~CDATASection();
protected:
    CDATASection(CDATASectionImpl *i);
};

class DOMString;

/**
 * This interface represents an entity, either parsed or unparsed, in
 * an XML document. Note that this models the entity itself not the
 * entity declaration. <code> Entity </code> declaration modeling has
 * been left for a later Level of the DOM specification.
 *
 *  The <code> nodeName </code> attribute that is inherited from
 * <code> Node </code> contains the name of the entity.
 *
 *  An XML processor may choose to completely expand entities before
 * the structure model is passed to the DOM; in this case there will
 * be no <code> EntityReference </code> nodes in the document tree.
 *
 *  XML does not mandate that a non-validating XML processor read and
 * process entity declarations made in the external subset or declared
 * in external parameter entities. This means that parsed entities
 * declared in the external subset need not be expanded by some
 * classes of applications, and that the replacement value of the
 * entity may not be available. When the replacement value is
 * available, the corresponding <code> Entity </code> node's child
 * list represents the structure of that replacement text. Otherwise,
 * the child list is empty.
 *
 *  The resolution of the children of the <code> Entity </code> (the
 * replacement value) may be lazily evaluated; actions by the user
 * (such as calling the <code> childNodes </code> method on the <code>
 * Entity </code> Node) are assumed to trigger the evaluation.
 *
 *  The DOM Level 1 does not support editing <code> Entity </code>
 * nodes; if a user wants to make changes to the contents of an <code>
 * Entity </code> , every related <code> EntityReference </code> node
 * has to be replaced in the structure model by a clone of the <code>
 * Entity </code> 's contents, and then the desired changes must be
 * made to each of those clones instead. All the descendants of an
 * <code> Entity </code> node are readonly.
 *
 *  An <code> Entity </code> node does not have any parent.
 *
 */
class Entity : public Node
{
public:
    Entity();
    Entity(const Entity &other);
    Entity(const Node &other) : Node()
         {(*this)=other;}

    Entity & operator = (const Node &other);
    Entity & operator = (const Entity &other);

    ~Entity();

    /**
     * The public identifier associated with the entity, if specified.
     * If the public identifier was not specified, this is <code> null
     * </code> .
     *
     */
    DOMString publicId() const;

    /**
     * The system identifier associated with the entity, if specified.
     * If the system identifier was not specified, this is <code> null
     * </code> .
     *
     */
    DOMString systemId() const;

    /**
     * For unparsed entities, the name of the notation for the entity.
     * For parsed entities, this is <code> null </code> .
     *
     */
    DOMString notationName() const;
protected:
    Entity(EntityImpl *i);
};


/**
 * <code> EntityReference </code> objects may be inserted into the
 * structure model when an entity reference is in the source document,
 * or when the user wishes to insert an entity reference. Note that
 * character references and references to predefined entities are
 * considered to be expanded by the HTML or XML processor so that
 * characters are represented by their Unicode equivalent rather than
 * by an entity reference. Moreover, the XML processor may completely
 * expand references to entities while building the structure model,
 * instead of providing <code> EntityReference </code> objects. If it
 * does provide such objects, then for a given <code> EntityReference
 * </code> node, it may be that there is no <code> Entity </code> node
 * representing the referenced entity; but if such an <code> Entity
 * </code> exists, then the child list of the <code> EntityReference
 * </code> node is the same as that of the <code> Entity </code> node.
 * As with the <code> Entity </code> node, all descendants of the
 * <code> EntityReference </code> are readonly.
 *
 *  The resolution of the children of the <code> EntityReference
 * </code> (the replacement value of the referenced <code> Entity
 * </code> ) may be lazily evaluated; actions by the user (such as
 * calling the <code> childNodes </code> method on the <code>
 * EntityReference </code> node) are assumed to trigger the
 * evaluation.
 *
 */
class EntityReference : public Node
{
    friend class Document;
public:
    EntityReference();
    EntityReference(const EntityReference &other);
    EntityReference(const Node &other) : Node()
         {(*this)=other;}

    EntityReference & operator = (const Node &other);
    EntityReference & operator = (const EntityReference &other);

    ~EntityReference();
protected:
    EntityReference(EntityReferenceImpl *i);
};

class DOMString;

/**
 * This interface represents a notation declared in the DTD. A
 * notation either declares, by name, the format of an unparsed entity
 * (see section 4.7 of the XML 1.0 specification), or is used for
 * formal declaration of Processing Instruction targets (see section
 * 2.6 of the XML 1.0 specification). The <code> nodeName </code>
 * attribute inherited from <code> Node </code> is set to the declared
 * name of the notation.
 *
 *  The DOM Level 1 does not support editing <code> Notation </code>
 * nodes; they are therefore readonly.
 *
 *  A <code> Notation </code> node does not have any parent.
 *
 */
class Notation : public Node
{
public:
    Notation();
    Notation(const Notation &other);
    Notation(const Node &other) : Node()
         {(*this)=other;}

    Notation & operator = (const Node &other);
    Notation & operator = (const Notation &other);

    ~Notation();

    /**
     * The public identifier of this notation. If the public
     * identifier was not specified, this is <code> null </code> .
     *
     */
    DOMString publicId() const;

    /**
     * The system identifier of this notation. If the system
     * identifier was not specified, this is <code> null </code> .
     *
     */
    DOMString systemId() const;
protected:
    Notation(NotationImpl *i);
};


/**
 * The <code> ProcessingInstruction </code> interface represents a
 * &quot;processing instruction&quot;, used in XML as a way to keep
 * processor-specific information in the text of the document.
 *
 */
class ProcessingInstruction : public Node
{
    friend class Document;
#if APPLE_CHANGES
    friend class ProcessingInstructionImpl;
#endif
public:
    ProcessingInstruction();
    ProcessingInstruction(const ProcessingInstruction &other);
    ProcessingInstruction(const Node &other) : Node()
         {(*this)=other;}

    ProcessingInstruction & operator = (const Node &other);
    ProcessingInstruction & operator = (const ProcessingInstruction &other);

    ~ProcessingInstruction();

    /**
     * The target of this processing instruction. XML defines this as
     * being the first token following the markup that begins the
     * processing instruction.
     *
     */
    DOMString target() const;

    /**
     * The content of this processing instruction. This is from the
     * first non white space character after the target to the
     * character immediately preceding the <code> ?&gt; </code> .
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
     * Introduced in DOM Level 2
     * This method is from the LinkStyle interface
     *
     * The style sheet.
     */
    StyleSheet sheet() const;

protected:
    ProcessingInstruction(ProcessingInstructionImpl *i);
};

}; //namespace
#endif
