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

class HTMLCollectionImpl;
class HTMLTableSectionElementImpl;
class HTMLTableCellElementImpl;
class HTMLTableCaptionElementImpl;

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

    HTMLTableElementImpl(DocumentImpl *doc);
    ~HTMLTableElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 9; }
    virtual bool checkDTD(const NodeImpl* newChild);

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
    HTMLElementImpl *insertRow ( int index, int &exceptioncode );
    void deleteRow ( int index, int &exceptioncode );

    RefPtr<HTMLCollectionImpl> rows();
    RefPtr<HTMLCollectionImpl> tBodies();

    DOMString align() const;
    void setAlign( const DOMString & );

    DOMString bgColor() const;
    void setBgColor( const DOMString & );

    DOMString border() const;
    void setBorder( const DOMString & );

    DOMString cellPadding() const;
    void setCellPadding( const DOMString & );

    DOMString cellSpacing() const;
    void setCellSpacing( const DOMString & );

    DOMString frame() const;
    void setFrame( const DOMString & );

    DOMString rules() const;
    void setRules( const DOMString & );

    DOMString summary() const;
    void setSummary( const DOMString & );

    DOMString width() const;
    void setWidth( const DOMString & );

    // overrides
    virtual NodeImpl *addChild(NodeImpl *child);
    virtual void childrenChanged();
    
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    // Used to obtain either a solid or outset border decl.
    virtual CSSMutableStyleDeclarationImpl* additionalAttributeStyleDecl();
    CSSMutableStyleDeclarationImpl* getSharedCellDecl();

    virtual void attach();
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

protected:
    HTMLTableSectionElementImpl *head;
    HTMLTableSectionElementImpl *foot;
    HTMLTableSectionElementImpl *firstBody;
    HTMLTableCaptionElementImpl *tCaption;

    bool m_noBorder     : 1;
    bool m_solid        : 1;
    uint unused         : 14;
    ushort padding      : 16;
    friend class HTMLTableCellElementImpl;
};

// -------------------------------------------------------------------------

class HTMLTablePartElementImpl : public HTMLElementImpl

{
public:
    HTMLTablePartElementImpl(const QualifiedName& tagName, DocumentImpl *doc)
        : HTMLElementImpl(tagName, doc)
        { }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);
};

// -------------------------------------------------------------------------

class HTMLTableSectionElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableSectionElementImpl(const QualifiedName& tagName, DocumentImpl *doc, bool implicit);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 8; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual NodeImpl *addChild(NodeImpl *child);
    
    HTMLElementImpl *insertRow ( int index, int& exceptioncode );
    void deleteRow ( int index, int& exceptioncode );

    int numRows() const;

    DOMString align() const;
    void setAlign( const DOMString & );

    DOMString ch() const;
    void setCh( const DOMString & );

    DOMString chOff() const;
    void setChOff( const DOMString & );

    DOMString vAlign() const;
    void setVAlign( const DOMString & );

    RefPtr<HTMLCollectionImpl> rows();
};

// -------------------------------------------------------------------------

class HTMLTableRowElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableRowElementImpl(DocumentImpl *doc)
        : HTMLTablePartElementImpl(HTMLNames::trTag, doc) {}

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 7; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual NodeImpl *addChild(NodeImpl *child);
    
    int rowIndex() const;
    int sectionRowIndex() const;

    HTMLElementImpl *insertCell ( int index, int &exceptioncode );
    void deleteCell ( int index, int &exceptioncode );

    void setRowIndex( int  );

    void setSectionRowIndex( int  );

    RefPtr<HTMLCollectionImpl> cells();
    void setCells(HTMLCollectionImpl *, int &exception);

    DOMString align() const;
    void setAlign( const DOMString & );

    DOMString bgColor() const;
    void setBgColor( const DOMString & );

    DOMString ch() const;
    void setCh( const DOMString & );

    DOMString chOff() const;
    void setChOff( const DOMString & );

    DOMString vAlign() const;
    void setVAlign( const DOMString & );

protected:
    int ncols;
};

// -------------------------------------------------------------------------

class HTMLTableCellElementImpl : public HTMLTablePartElementImpl
{
public:
    HTMLTableCellElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
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
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    // used by table cells to share style decls created by the enclosing table.
    virtual CSSMutableStyleDeclarationImpl* additionalAttributeStyleDecl();
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    void setCellIndex( int  );

    DOMString abbr() const;
    void setAbbr( const DOMString & );

    DOMString align() const;
    void setAlign( const DOMString & );

    DOMString axis() const;
    void setAxis( const DOMString & );

    DOMString bgColor() const;
    void setBgColor( const DOMString & );

    DOMString ch() const;
    void setCh( const DOMString & );

    DOMString chOff() const;
    void setChOff( const DOMString & );

    void setColSpan( int  );

    DOMString headers() const;
    void setHeaders( const DOMString & );

    DOMString height() const;
    void setHeight( const DOMString & );

    bool noWrap() const;
    void setNoWrap( bool );

    void setRowSpan( int );

    DOMString scope() const;
    void setScope( const DOMString & );

    DOMString vAlign() const;
    void setVAlign( const DOMString & );

    DOMString width() const;
    void setWidth( const DOMString & );

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
    HTMLTableColElementImpl(const QualifiedName& tagName, DocumentImpl *doc);

    virtual HTMLTagStatus endTagRequirement() const { return hasLocalName(HTMLNames::colTag) ? TagStatusForbidden : TagStatusOptional; }
    virtual int tagPriority() const { return hasLocalName(HTMLNames::colTag) ? 0 : 1; }
    virtual bool checkDTD(const NodeImpl* newChild) { return hasLocalName(HTMLNames::colgroupTag) && newChild->hasTagName(HTMLNames::colTag); }
    void setTable(HTMLTableElementImpl *t) { table = t; }

    // overrides
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    int span() const { return _span; }

    DOMString align() const;
    void setAlign( const DOMString & );

    DOMString ch() const;
    void setCh( const DOMString & );

    DOMString chOff() const;
    void setChOff( const DOMString & );

    void setSpan( int  );

    DOMString vAlign() const;
    void setVAlign( const DOMString & );

    DOMString width() const;
    void setWidth( const DOMString & );

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
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    DOMString align() const;
    void setAlign(const DOMString&);
};

} //namespace

#endif
