/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef DOM_XmlImpl_h_
#define DOM_XmlImpl_h_

#include "CachedObjectClient.h"
#include "ContainerNodeImpl.h"

namespace khtml {
    class CachedCSSStyleSheet;
}

namespace DOM {

class DocumentImpl;
class DOMString;
class ProcessingInstruction;
class StyleSheetImpl;

class EntityImpl : public ContainerNodeImpl
{
public:
    EntityImpl(DocumentImpl*);
    EntityImpl(DocumentImpl*, const DOMString& name);
    EntityImpl(DocumentImpl*, const DOMString& publicId, const DOMString& systemId, const DOMString& notationName);

    // DOM methods & attributes for Entity
    DOMString publicId() const { return m_publicId.get(); }
    DOMString systemId() const { return m_systemId.get(); }
    DOMString notationName() const { return m_notationName.get(); }

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual bool childTypeAllowed(unsigned short type);
    virtual DOMString toString() const;

private:
    RefPtr<DOMStringImpl> m_publicId;
    RefPtr<DOMStringImpl> m_systemId;
    RefPtr<DOMStringImpl> m_notationName;
    RefPtr<DOMStringImpl> m_name;
};

class EntityReferenceImpl : public ContainerNodeImpl
{
public:
    EntityReferenceImpl(DocumentImpl*);
    EntityReferenceImpl(DocumentImpl*, DOMStringImpl* entityName);

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual bool childTypeAllowed(unsigned short type);
    virtual DOMString toString() const;

private:
    RefPtr<DOMStringImpl> m_entityName;
};

class NotationImpl : public ContainerNodeImpl
{
public:
    NotationImpl(DocumentImpl*);
    NotationImpl(DocumentImpl*, const DOMString& name, const DOMString& publicId, const DOMString& systemId);

    // DOM methods & attributes for Notation
    DOMString publicId() const { return m_publicId.get(); }
    DOMString systemId() const { return m_systemId. get(); }

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual bool childTypeAllowed(unsigned short type);

private:
    RefPtr<DOMStringImpl> m_name;
    RefPtr<DOMStringImpl> m_publicId;
    RefPtr<DOMStringImpl> m_systemId;
};


class ProcessingInstructionImpl : public ContainerNodeImpl, private khtml::CachedObjectClient
{
public:
    ProcessingInstructionImpl(DocumentImpl*);
    ProcessingInstructionImpl(DocumentImpl*, const DOMString& target, const DOMString& data);
    virtual ~ProcessingInstructionImpl();

    // DOM methods & attributes for Notation
    DOMString target() const { return m_target.get(); }
    DOMString data() const { return m_data.get(); }
    void setData(const DOMString &data, int &exceptioncode);

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual DOMString nodeValue() const;
    virtual void setNodeValue(const DOMString& nodeValue, int &exceptioncode);
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual bool childTypeAllowed( unsigned short type );

    // Other methods (not part of DOM)
    DOMString localHref() const { return m_localHref.get(); }
    StyleSheetImpl* sheet() const { return m_sheet.get(); }
    bool checkStyleSheet();
    virtual void setStyleSheet(const DOMString& URL, const DOMString& sheet);
    void setStyleSheet(StyleSheetImpl*);
    bool isLoading() const;
    void sheetLoaded();
    virtual DOMString toString() const;

#ifdef KHTML_XSLT
    bool isXSL() const { return m_isXSL; }
#endif

private:
    RefPtr<DOMStringImpl> m_target;
    RefPtr<DOMStringImpl> m_data;
    RefPtr<DOMStringImpl> m_localHref;
    khtml::CachedObject* m_cachedSheet;
    RefPtr<StyleSheetImpl> m_sheet;
    bool m_loading;
#ifdef KHTML_XSLT
    bool m_isXSL;
#endif
};

} //namespace

#endif
