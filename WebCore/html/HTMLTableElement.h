/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef HTMLTableElement_H
#define HTMLTableElement_H

#include "HTMLElement.h"

namespace WebCore {

class HTMLCollection;
class HTMLTableSectionElement;
class HTMLTableCellElement;
class HTMLTableCaptionElement;

class HTMLTableElement : public HTMLElement {
public:
    enum Rules {
        None    = 0x00,
        RGroups = 0x01,
        CGroups = 0x02,
        Groups  = 0x03,
        Rows    = 0x05,
        Cols    = 0x0a,
        All     = 0x0f
    };
    enum Frame {
        Void   = 0x00,
        Above  = 0x01,
        Below  = 0x02,
        Lhs    = 0x04,
        Rhs    = 0x08,
        Hsides = 0x03,
        Vsides = 0x0c,
        Box    = 0x0f
    };

    HTMLTableElement(Document*);
    ~HTMLTableElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 9; }
    virtual bool checkDTD(const Node*);

    HTMLTableCaptionElement* caption() const { return tCaption; }
    Node* setCaption(HTMLTableCaptionElement*);

    HTMLTableSectionElement* tHead() const { return head; }
    Node* setTHead(HTMLTableSectionElement*);

    HTMLTableSectionElement* tFoot() const { return foot; }
    Node* setTFoot(HTMLTableSectionElement*);

    Node* setTBody(HTMLTableSectionElement*);

    HTMLElement* createTHead();
    void deleteTHead();
    HTMLElement* createTFoot();
    void deleteTFoot();
    HTMLElement* createCaption();
    void deleteCaption();
    HTMLElement* insertRow(int index, ExceptionCode&);
    void deleteRow(int index, ExceptionCode&);

    PassRefPtr<HTMLCollection> rows();
    PassRefPtr<HTMLCollection> tBodies();

    String align() const;
    void setAlign(const String&);

    String bgColor() const;
    void setBgColor(const String&);

    String border() const;
    void setBorder(const String&);

    String cellPadding() const;
    void setCellPadding(const String&);

    String cellSpacing() const;
    void setCellSpacing(const String&);

    String frame() const;
    void setFrame(const String&);

    String rules() const;
    void setRules(const String&);

    String summary() const;
    void setSummary(const String&);

    String width() const;
    void setWidth(const String&);

    // overrides
    virtual ContainerNode* addChild(PassRefPtr<Node>);
    virtual void childrenChanged();
    
    virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    // Used to obtain either a solid or outset border decl.
    virtual CSSMutableStyleDeclaration* additionalAttributeStyleDecl();
    CSSMutableStyleDeclaration* getSharedCellDecl();

    virtual void attach();
    
    virtual bool isURLAttribute(Attribute*) const;

protected:
    HTMLTableSectionElement* head;
    HTMLTableSectionElement* foot;
    HTMLTableSectionElement* firstBody;
    HTMLTableCaptionElement* tCaption;

    bool m_noBorder     : 1;
    bool m_solid        : 1;
    // 14 bits unused
    unsigned short padding;
    friend class HTMLTableCellElement;
};

} //namespace

#endif
