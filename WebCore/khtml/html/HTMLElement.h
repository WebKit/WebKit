/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef HTMLElementImpl_H
#define HTMLElementImpl_H

#include "dom_elementimpl.h"

namespace WebCore {

class DocumentFragment;
class HTMLCollection;
class String;

enum HTMLTagStatus { TagStatusOptional, TagStatusRequired, TagStatusForbidden };
                       
class HTMLElement : public StyledElement
{
public:
    HTMLElement(const QualifiedName& tagName, Document*);
    virtual ~HTMLElement();

    virtual bool isHTMLElement() const { return true; }

    virtual String nodeName() const;

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual PassRefPtr<Node> cloneNode(bool deep);

    PassRefPtr<HTMLCollection> children();
    
    String id() const;
    void setId(const String&);
    String title() const;
    void setTitle(const String&);
    String lang() const;
    void setLang(const String&);
    String dir() const;
    void setDir(const String&);
    String className() const;
    void setClassName(const String&);

    String innerHTML() const;
    String outerHTML() const;
    String innerText() const;
    String outerText() const;
    PassRefPtr<DocumentFragment> createContextualFragment(const String&);
    void setInnerHTML(const String&, ExceptionCode&);
    void setOuterHTML(const String&, ExceptionCode&);
    void setInnerText(const String&, ExceptionCode&);
    void setOuterText(const String&, ExceptionCode&);
    
    virtual bool isFocusable() const;
    virtual bool isContentEditable() const;
    virtual String contentEditable() const;
    virtual void setContentEditable(MappedAttribute*);
    virtual void setContentEditable(const String&);

    virtual void click(bool sendMouseEvents = false, bool showPressedLook = true);
    virtual void accessKeyAction(bool sendToAnyElement);

    virtual bool isGenericFormElement() const { return false; }

    virtual String toString() const;

    virtual HTMLTagStatus endTagRequirement() const;
    virtual int tagPriority() const;
    virtual bool childAllowed(Node* newChild); // Error-checking during parsing that checks the DTD

    // Helper function to check the DTD for a given child node.
    virtual bool checkDTD(const Node*);
    static bool inEitherTagList(const Node*);
    static bool inInlineTagList(const Node*);
    static bool inBlockTagList(const Node*);
    static bool isRecognizedTagName(const QualifiedName&);

    void setHTMLEventListener(const AtomicString& eventType, Attribute*);

protected:

    // for IMG, OBJECT and APPLET
    void addHTMLAlignment(MappedAttribute*);
};

} //namespace

#endif
