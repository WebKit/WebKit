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

#ifndef HTMLTableElementImpl_H
#define HTMLTableElementImpl_H

#include "HTMLElement.h"
#include "htmlnames.h"

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

    RefPtr<HTMLCollection> rows();
    RefPtr<HTMLCollection> tBodies();

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
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute *attr);

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

// -------------------------------------------------------------------------

class HTMLTablePartElement : public HTMLElement

{
public:
    HTMLTablePartElement(const QualifiedName& tagName, Document* doc)
        : HTMLElement(tagName, doc)
        { }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);
};

// -------------------------------------------------------------------------

class HTMLTableSectionElement : public HTMLTablePartElement
{
public:
    HTMLTableSectionElement(const QualifiedName& tagName, Document*, bool implicit);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 8; }
    virtual bool checkDTD(const Node*);
    virtual ContainerNode* addChild(PassRefPtr<Node>);
    
    HTMLElement* insertRow(int index, ExceptionCode&);
    void deleteRow(int index, ExceptionCode&);

    int numRows() const;

    String align() const;
    void setAlign(const String&);

    String ch() const;
    void setCh(const String&);

    String chOff() const;
    void setChOff(const String&);

    String vAlign() const;
    void setVAlign(const String&);

    RefPtr<HTMLCollection> rows();
};

// -------------------------------------------------------------------------

class HTMLTableRowElement : public HTMLTablePartElement
{
public:
    HTMLTableRowElement(Document* doc)
        : HTMLTablePartElement(HTMLNames::trTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 7; }
    virtual bool checkDTD(const Node*);
    virtual ContainerNode* addChild(PassRefPtr<Node>);
    
    int rowIndex() const;
    int sectionRowIndex() const;

    HTMLElement* insertCell(int index, ExceptionCode&);
    void deleteCell(int index, ExceptionCode&);

    void setRowIndex(int);

    void setSectionRowIndex(int);

    RefPtr<HTMLCollection> cells();
    void setCells(HTMLCollection *, ExceptionCode&);

    String align() const;
    void setAlign(const String&);

    String bgColor() const;
    void setBgColor(const String&);

    String ch() const;
    void setCh(const String&);

    String chOff() const;
    void setChOff(const String&);

    String vAlign() const;
    void setVAlign(const String&);

protected:
    int ncols;
};

// -------------------------------------------------------------------------

class HTMLTableCellElement : public HTMLTablePartElement
{
public:
    HTMLTableCellElement(const QualifiedName& tagName, Document*);
    ~HTMLTableCellElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 6; }

    int cellIndex() const;

    int col() const { return _col; }
    void setCol(int col) { _col = col; }
    int row() const { return _row; }
    void setRow(int r) { _row = r; }

    int colSpan() const { return cSpan; }
    int rowSpan() const { return rSpan; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    // used by table cells to share style decls created by the enclosing table.
    virtual CSSMutableStyleDeclaration* additionalAttributeStyleDecl();
    
    virtual bool isURLAttribute(Attribute*) const;

    void setCellIndex(int);

    String abbr() const;
    void setAbbr(const String&);

    String align() const;
    void setAlign(const String&);

    String axis() const;
    void setAxis(const String&);

    String bgColor() const;
    void setBgColor(const String&);

    String ch() const;
    void setCh(const String&);

    String chOff() const;
    void setChOff(const String&);

    void setColSpan(int);

    String headers() const;
    void setHeaders(const String&);

    String height() const;
    void setHeight(const String&);

    bool noWrap() const;
    void setNoWrap(bool);

    void setRowSpan(int);

    String scope() const;
    void setScope(const String&);

    String vAlign() const;
    void setVAlign(const String&);

    String width() const;
    void setWidth(const String&);

protected:
    int _row;
    int _col;
    int rSpan;
    int cSpan;
    int rowHeight;
    bool m_solid        : 1;
};

// -------------------------------------------------------------------------

class HTMLTableColElement : public HTMLTablePartElement
{
public:
    HTMLTableColElement(const QualifiedName& tagName, Document*);

    virtual HTMLTagStatus endTagRequirement() const { return hasLocalName(HTMLNames::colTag) ? TagStatusForbidden : TagStatusOptional; }
    virtual int tagPriority() const { return hasLocalName(HTMLNames::colTag) ? 0 : 1; }
    virtual bool checkDTD(const Node* newChild) { return hasLocalName(HTMLNames::colgroupTag) && newChild->hasTagName(HTMLNames::colTag); }
    void setTable(HTMLTableElement* t) { table = t; }

    // overrides
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    int span() const { return _span; }

    String align() const;
    void setAlign(const String&);

    String ch() const;
    void setCh(const String&);

    String chOff() const;
    void setChOff(const String&);

    void setSpan(int);

    String vAlign() const;
    void setVAlign(const String&);

    String width() const;
    void setWidth(const String&);

protected:
    int _span;
    HTMLTableElement *table;
};

// -------------------------------------------------------------------------

class HTMLTableCaptionElement : public HTMLTablePartElement
{
public:
    HTMLTableCaptionElement(Document *doc)
        : HTMLTablePartElement(HTMLNames::captionTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    String align() const;
    void setAlign(const String&);
};

} //namespace

#endif
