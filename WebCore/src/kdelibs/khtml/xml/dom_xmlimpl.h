/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
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
 * $Id$
 */

#ifndef _DOM_XmlImpl_h_
#define _DOM_XmlImpl_h_

#include "dom_nodeimpl.h"
#include "css_stylesheetimpl.h"
#include "misc/loader.h"
#include "misc/loader_client.h"

#include <qxml.h>

namespace DOM {

    class DocumentImpl;

class DOMString;

class EntityImpl : public NodeBaseImpl
{
public:
    EntityImpl(DocumentPtr *doc);
    EntityImpl(DocumentPtr *doc, DOMString _name);
    EntityImpl(DocumentPtr *doc, DOMString _publicId, DOMString _systemId, DOMString _notationName);
    virtual ~EntityImpl();

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;

    virtual DOMString publicId() const;
    virtual DOMString systemId() const;
    virtual DOMString notationName() const;
    virtual bool childTypeAllowed( unsigned short type );
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );
protected:
    DOMStringImpl *m_publicId;
    DOMStringImpl *m_systemId;
    DOMStringImpl *m_notationName;
    DOMStringImpl *m_name;
};


class EntityReferenceImpl : public NodeBaseImpl
{
public:
    EntityReferenceImpl(DocumentPtr *doc);
    EntityReferenceImpl(DocumentPtr *doc, DOMStringImpl *_entityName);
    virtual ~EntityReferenceImpl();

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual bool childTypeAllowed( unsigned short type );
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );
protected:
    DOMStringImpl *m_entityName;
};

class NotationImpl : public NodeBaseImpl
{
public:
    NotationImpl(DocumentPtr *doc);
    NotationImpl(DocumentPtr *doc, DOMString _name, DOMString _publicId, DOMString _systemId);
    virtual ~NotationImpl();

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;

    virtual DOMString publicId() const;
    virtual DOMString systemId() const;
    virtual bool childTypeAllowed( unsigned short type );
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );
protected:
    DOMStringImpl *m_name;
    DOMStringImpl *m_publicId;
    DOMStringImpl *m_systemId;
};


class ProcessingInstructionImpl : public NodeBaseImpl, private khtml::CachedObjectClient
{
public:
    ProcessingInstructionImpl(DocumentPtr *doc);
    ProcessingInstructionImpl(DocumentPtr *doc, DOMString _target, DOMString _data);
    virtual ~ProcessingInstructionImpl();

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual DOMString nodeValue() const;

    virtual DOMString target() const;
    virtual DOMString data() const;
    virtual void setData( const DOMString &_data );
    virtual bool childTypeAllowed( unsigned short type );
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );
    StyleSheetImpl *sheet() const;
    void checkStyleSheet();
    virtual void setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheet);

protected:
    DOMStringImpl *m_target;
    DOMStringImpl *m_data;
    khtml::CachedCSSStyleSheet *m_cachedSheet;
    CSSStyleSheetImpl *m_sheet;
};

class XMLAttributeReader : public QXmlDefaultHandler
{
public:
    XMLAttributeReader(QString _attrString);
    virtual ~XMLAttributeReader();
    QXmlAttributes readAttrs(bool &ok);
    bool startElement(const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts);

protected:
    QXmlAttributes attrs;
    QString m_attrString;
};

}; //namespace

#endif
