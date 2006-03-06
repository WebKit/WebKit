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

#include "HTMLElementImpl.h"
#include "htmlnames.h"

namespace WebCore {

class HTMLCollectionImpl;
class HTMLTableSectionElementImpl;
class HTMLTableCellElementImpl;
class HTMLTableCaptionElementImpl;

class HTMLTableElementImpl : public HTMLElementImpl {
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

    HTMLTableElementImpl(DocumentImpl*);
    ~HTMLTableElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 9; }
    virtual bool checkDTD(const NodeImpl*);

    HTMLTableCaptionElementImpl* caption() const { return tCaption; }
    NodeImpl* setCaption(HTMLTableCaptionElementImpl*);

    HTMLTableSectionElementImpl* tHead() const { return head; }
    NodeImpl* setTHead(HTMLTableSectionElementImpl*);

    HTMLTableSectionElementImpl* tFoot() const { return foot; }
    NodeImpl* setTFoot(HTMLTableSectionElementImpl*);

    NodeImpl* setTBody(HTMLTableSectionElementImpl*);

    HTMLElementImpl* createTHead();
    void deleteTHead();
    HTMLElementImpl* createTFoot();
    void deleteTFoot();
    HTMLElementImpl* createCaption();
    void deleteCaption();
    HTMLElementImpl* insertRow(int index, ExceptionCode&);
    void deleteRow(int index, ExceptionCode&);

    RefPtr<HTMLCollectionImpl> rows();
    RefPtr<HTMLCollectionImpl> tBodies();

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
    virtual ContainerNodeImpl* addChild(PassRefPtr<NodeImpl>);
    virtual void childrenChanged();
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    // Used to obtain either a solid or outset border decl.
    virtual CSSMutableStyleDeclarationImpl* additionalAttributeStyleDecl();
    CSSMutableStyleDeclarationImpl* getSharedCellDecl();

    virtual void attach();
    
    virtual bool isURLAttribute(AttributeImpl*) const;

protected:
    HTMLTableSectionElementImpl* head;
    HTMLTableSectionElementImpl* foot;
    HTMLTableSectionElementImpl* firstBody;
    HTMLTableCaptionElementImpl* tCaption;

    bool m_noBorder     : 1;
    bool m_solid        : 1;
    // 14 bits unused
    unsigned short padding;
    friend class HTMLTableCellElementImpl;
};

// -------------------------------------------------------------------------

class HTMLTablePartElementImpl : public HTMLElementImpl

{
public:
    HTMLTablePartElementImpl(const QualifiedName& tagName, DocumentImpl* doc)
        : HTMLElementImpl(tagName, doc)
        { }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl*);
};

// -------------------------------------------------------------------------

class HTMLTableSectionElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableSectionElementImpl(const QualifiedName& tagName, DocumentImpl*, bool implicit);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 8; }
    virtual bool checkDTD(const NodeImpl*);
    virtual ContainerNodeImpl* addChild(PassRefPtr<NodeImpl>);
    
    HTMLElementImpl* insertRow(int index, ExceptionCode&);
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

    RefPtr<HTMLCollectionImpl> rows();
};

// -------------------------------------------------------------------------

class HTMLTableRowElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableRowElementImpl(DocumentImpl* doc)
        : HTMLTablePartElementImpl(HTMLNames::trTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 7; }
    virtual bool checkDTD(const NodeImpl*);
    virtual ContainerNodeImpl* addChild(PassRefPtr<NodeImpl>);
    
    int rowIndex() const;
    int sectionRowIndex() const;

    HTMLElementImpl* insertCell(int index, ExceptionCode&);
    void deleteCell(int index, ExceptionCode&);

    void setRowIndex(int);

    void setSectionRowIndex(int);

    RefPtr<HTMLCollectionImpl> cells();
    void setCells(HTMLCollectionImpl *, ExceptionCode&);

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

class HTMLTableCellElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableCellElementImpl(const QualifiedName& tagName, DocumentImpl*);
    ~HTMLTableCellElementImpl();

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
    virtual void parseMappedAttribute(MappedAttributeImpl*);

    // used by table cells to share style decls created by the enclosing table.
    virtual CSSMutableStyleDeclarationImpl* additionalAttributeStyleDecl();
    
    virtual bool isURLAttribute(AttributeImpl*) const;

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

class HTMLTableColElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableColElementImpl(const QualifiedName& tagName, DocumentImpl*);

    virtual HTMLTagStatus endTagRequirement() const { return hasLocalName(HTMLNames::colTag) ? TagStatusForbidden : TagStatusOptional; }
    virtual int tagPriority() const { return hasLocalName(HTMLNames::colTag) ? 0 : 1; }
    virtual bool checkDTD(const NodeImpl* newChild) { return hasLocalName(HTMLNames::colgroupTag) && newChild->hasTagName(HTMLNames::colTag); }
    void setTable(HTMLTableElementImpl* t) { table = t; }

    // overrides
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl*);

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
    HTMLTableElementImpl *table;
};

// -------------------------------------------------------------------------

class HTMLTableCaptionElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableCaptionElementImpl(DocumentImpl *doc)
        : HTMLTablePartElementImpl(HTMLNames::captionTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 5; }
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl*);

    String align() const;
    void setAlign(const String&);
};

} //namespace

#endif
