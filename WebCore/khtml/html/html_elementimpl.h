/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef HTML_ELEMENTIMPL_H
#define HTML_ELEMENTIMPL_H

#include "xml/dom_elementimpl.h"
#include "htmlnames.h"

namespace DOM {

class DocumentFragmentImpl;
class DOMString;
class HTMLCollectionImpl;

enum HTMLTagStatus { TagStatusOptional, TagStatusRequired, TagStatusForbidden };
                       
class HTMLElementImpl : public StyledElementImpl
{
public:
    HTMLElementImpl(const QualifiedName& tagName, DocumentPtr *doc);
    virtual ~HTMLElementImpl();

    virtual bool isHTMLElement() const { return true; }

    virtual DOMString nodeName() const;

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl* attr);

    virtual NodeImpl* cloneNode(bool deep);

    SharedPtr<HTMLCollectionImpl> children();
    
    DOMString id() const;
    void setId(const DOMString &value);
    DOMString title() const;
    void setTitle(const DOMString &value);
    DOMString lang() const;
    void setLang(const DOMString &value);
    DOMString dir() const;
    void setDir(const DOMString &value);
    DOMString className() const;
    void setClassName(const DOMString &value);

    DOMString innerHTML() const;
    DOMString outerHTML() const;
    DOMString innerText() const;
    DOMString outerText() const;
    DocumentFragmentImpl *createContextualFragment(const DOMString &html);
    void setInnerHTML(const DOMString &html, int &exception);
    void setOuterHTML(const DOMString &html, int &exception);
    void setInnerText(const DOMString &text, int &exception);
    void setOuterText(const DOMString &text, int &exception);
    
    virtual bool isFocusable() const;
    virtual bool isContentEditable() const;
    virtual DOMString contentEditable() const;
    virtual void setContentEditable(MappedAttributeImpl* attr);
    virtual void setContentEditable(const DOMString &enabled);

    virtual void click(bool sendMouseEvents);
    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool isGenericFormElement() const { return false; }

    virtual DOMString toString() const;

    virtual HTMLTagStatus endTagRequirement() const;
    virtual int tagPriority() const;
    virtual bool childAllowed(NodeImpl* newChild); // Error-checking during parsing that checks the DTD

    // Helper function to check the DTD for a given child node.
    virtual bool checkDTD(const NodeImpl* newChild);
    static bool inEitherTagList(const NodeImpl* newChild);
    static bool inInlineTagList(const NodeImpl* newChild);
    static bool inBlockTagList(const NodeImpl* newChild);
    static bool isRecognizedTagName(const QualifiedName& tagName);

protected:

    // for IMG, OBJECT and APPLET
    void addHTMLAlignment(MappedAttributeImpl* htmlAttr);
};

} //namespace

#endif
