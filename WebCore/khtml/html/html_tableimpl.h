/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef HTML_TABLEIMPL_H
#define HTML_TABLEIMPL_H

#include "html/html_elementimpl.h"

namespace DOM {

class DOMString;
class HTMLTableElementImpl;
class HTMLTableSectionElementImpl;
class HTMLTableSectionElement;
class HTMLTableRowElementImpl;
class HTMLTableRowElement;
class HTMLTableCellElementImpl;
class HTMLTableCellElement;
class HTMLTableColElementImpl;
class HTMLTableColElement;
class HTMLTableCaptionElementImpl;
class HTMLTableCaptionElement;
class HTMLElement;
class HTMLCollection;
class CSSStyleDeclarationImpl;

class HTMLTableElementImpl : public HTMLElementImpl
{
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

    HTMLTableElementImpl(DocumentPtr *doc);
    ~HTMLTableElementImpl();

    virtual Id id() const;

    HTMLTableCaptionElementImpl *caption() const { return tCaption; }
    NodeImpl *setCaption( HTMLTableCaptionElementImpl * );

    HTMLTableSectionElementImpl *tHead() const { return head; }
    NodeImpl *setTHead( HTMLTableSectionElementImpl * );

    HTMLTableSectionElementImpl *tFoot() const { return foot; }
    NodeImpl *setTFoot( HTMLTableSectionElementImpl * );

    NodeImpl *setTBody( HTMLTableSectionElementImpl * );

    HTMLElementImpl *createTHead (  );
    void deleteTHead (  );
    HTMLElementImpl *createTFoot (  );
    void deleteTFoot (  );
    HTMLElementImpl *createCaption (  );
    void deleteCaption (  );
    HTMLElementImpl *insertRow ( long index, int &exceptioncode );
    void deleteRow ( long index, int &exceptioncode );

    // overrides
    virtual NodeImpl *addChild(NodeImpl *child);
    
    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    // Used to obtain either a solid or outset border decl.
    virtual CSSStyleDeclarationImpl* additionalAttributeStyleDecl();
    CSSStyleDeclarationImpl* getSharedCellDecl();

    virtual void attach();
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

protected:
    HTMLTableSectionElementImpl *head;
    HTMLTableSectionElementImpl *foot;
    HTMLTableSectionElementImpl *firstBody;
    HTMLTableCaptionElementImpl *tCaption;

#if 0
    Frame frame;
    Rules rules;
#endif
  
    bool m_noBorder     : 1;
    bool m_solid        : 1;
    uint unused		: 14;
    ushort padding	: 16;
    friend class HTMLTableCellElementImpl;
};

// -------------------------------------------------------------------------

class HTMLTablePartElementImpl : public HTMLElementImpl

{
public:
    HTMLTablePartElementImpl(DocumentPtr *doc)
        : HTMLElementImpl(doc)
        { }

    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
};

// -------------------------------------------------------------------------

class HTMLTableSectionElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableSectionElementImpl(DocumentPtr *doc, ushort tagid, bool implicit);

    ~HTMLTableSectionElementImpl();

    virtual Id id() const;

    virtual NodeImpl *addChild(NodeImpl *child);
    
    HTMLElementImpl *insertRow ( long index, int& exceptioncode );
    void deleteRow ( long index, int& exceptioncode );

    int numRows() const;

protected:
    ushort _id;
};

// -------------------------------------------------------------------------

class HTMLTableRowElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableRowElementImpl(DocumentPtr *doc)
        : HTMLTablePartElementImpl(doc) {}

    virtual Id id() const;

    virtual NodeImpl *addChild(NodeImpl *child);
    
    long rowIndex() const;
    long sectionRowIndex() const;

    HTMLElementImpl *insertCell ( long index, int &exceptioncode );
    void deleteCell ( long index, int &exceptioncode );

protected:
    int ncols;
};

// -------------------------------------------------------------------------

class HTMLTableCellElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableCellElementImpl(DocumentPtr *doc, int tagId);
    ~HTMLTableCellElementImpl();

    // ### FIX these two...
    long cellIndex() const { return 0; }

    int col() const { return _col; }
    void setCol(int col) { _col = col; }
    int row() const { return _row; }
    void setRow(int r) { _row = r; }

    int colSpan() const { return cSpan; }
    int rowSpan() const { return rSpan; }
    
    virtual Id id() const { return _id; }
    
    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    virtual void attach();

    // used by table cells to share style decls created by the enclosing table.
    virtual CSSStyleDeclarationImpl* additionalAttributeStyleDecl();
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

protected:
    int _row;
    int _col;
    int rSpan;
    int cSpan;
    int _id;
    int rowHeight;
    bool m_solid        : 1;
};

// -------------------------------------------------------------------------

class HTMLTableColElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableColElementImpl(DocumentPtr *doc, ushort i);

    virtual Id id() const;

    void setTable(HTMLTableElementImpl *t) { table = t; }

    // overrides
    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    int span() const { return _span; }

protected:
    // could be ID_COL or ID_COLGROUP ... The DOM is not quite clear on
    // this, but since both elements work quite similar, we use one
    // DOMElement for them...
    ushort _id;
    int _span;
    HTMLTableElementImpl *table;
};

// -------------------------------------------------------------------------

class HTMLTableCaptionElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableCaptionElementImpl(DocumentPtr *doc)
        : HTMLTablePartElementImpl(doc) {}

    virtual Id id() const;
    
    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);
};

}; //namespace

#endif

