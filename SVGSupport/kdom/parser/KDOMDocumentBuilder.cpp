/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kurl.h>
#include <kdebug.h>

#include <qptrstack.h>

#include "kdom.h"
#include <kdom/Helper.h>
#include "NodeImpl.h"
#include "TextImpl.h"
#include "DOMString.h"
#include "EntityImpl.h"
#include "CommentImpl.h"
#include "ElementImpl.h"
#include "DocumentImpl.h"
#include "NotationImpl.h"
#include "NamedNodeMapImpl.h"
#include "CDATASectionImpl.h"
#include "DocumentTypeImpl.h"
#include "CharacterDataImpl.h"
#include "KDOMDocumentBuilder.h"
#include "EntityReferenceImpl.h"
#include "DOMImplementationImpl.h"
#include "ProcessingInstructionImpl.h"

using namespace KDOM;

class DocumentBuilder::Private
{
public:
    Private() { cdata = false; }
    virtual ~Private() { if(doc) doc->deref(); }

    bool cdata;
    DocumentImpl *doc;
    QPtrStack<NodeImpl> nodes;
};

DocumentBuilder::DocumentBuilder() : d(new Private())
{
    d->doc = 0;
}

DocumentBuilder::~DocumentBuilder()
{
    delete d;
}

DocumentImpl *DocumentBuilder::document() const
{
    return d->doc;
}

bool DocumentBuilder::startDocument(const KURL &uri)
{
    kdDebug(26001) << "DocumentBuilder::startDocument, uri = " << uri.prettyURL() << " existing doc: " << d->doc << endl;

    if(!d->doc)
    {
        DOMImplementationImpl *factory = DOMImplementationImpl::self();
        d->doc = factory->createDocument(DOMString("doc").handle(),
                                         DOMString("name").handle(),
                                         factory->defaultDocumentType(),
                                         false, 0 /* no view */);
        d->doc->ref();
    }

    if(d->nodes.count() == 0)
    {
        // In case we are dealing with an existing document
        d->doc->removeChildren();
        d->doc->setDocumentKURI(uri);

        pushNode(d->doc);
    }

    d->doc->setParsing(true);
    return true;
}

bool DocumentBuilder::endDocument()
{
    kdDebug(26001) << "DocumentBuilder::endDocument" << endl;
    if(d->doc)
        d->doc->setParsing(false);

    return true;
}

bool DocumentBuilder::startElement(const DOMString &tagName)
{
    kdDebug(26001) << "DocumentBuilder::startElement, tagName = " << tagName << endl;

    NodeImpl *current = d->nodes.current();

    try
    {
        NodeImpl *n = d->doc->createElement(tagName.handle());
        if(current)
            current->addChild(n);

        pushNode(n);
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::endElement(const DOMString &tagName)
{
    kdDebug(26001) << "DocumentBuilder::endElement, tagName = " << tagName << endl;

    NodeImpl *current = d->nodes.current();
    if(!current)
        return false;

    try
    {
        current->close();
        
        popNode();
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::startElementNS(const DOMString &namespaceURI, const DOMString &prefix, const DOMString &localName)
{
    kdDebug(26001) << "DocumentBuilder::startElementNS, namespaceURI = " << namespaceURI << " prefix = " << prefix << " localName = " << localName << endl;
    return startElementNS(namespaceURI, prefix.isEmpty() ? localName : prefix + ":" + localName);
}

bool DocumentBuilder::startElementNS(const DOMString &namespaceURI, const DOMString &qName)
{
    kdDebug(26001) << "DocumentBuilder::startElementNS, namespaceURI = " << namespaceURI << " qName = " << qName << endl;

    NodeImpl *current = d->nodes.current();

    try
    {
        NodeImpl *n = d->doc->createElementNS(namespaceURI.handle(), qName.handle());
        if(current)
            current->addChild(n);

        pushNode(n);
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

void DocumentBuilder::startElementEnd()
{
    NodeImpl *current = d->nodes.current();
    if(current && !current->attached() && !d->doc->hasPendingSheets()) // may be malformed xml...
        current->attach();
}

bool DocumentBuilder::endElementNS(const DOMString &namespaceURI, const DOMString &prefix, const DOMString &localName)
{
    kdDebug(26001) << "DocumentBuilder::endElementNS, namespaceURI = " << namespaceURI << " localName = " << prefix << " qName = " << localName << endl;

    NodeImpl *current = d->nodes.current();

    try
    {
        if(current) // may be malformed xml...
            current->close();
        
        popNode();
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

void DocumentBuilder::startAttributeNS(const DOMString &namespaceURI, const DOMString &qName, const DOMString &value)
{
    kdDebug(26001) << "DocumentBuilder::startAttributeNS, namespaceURI = " << namespaceURI << " qName = " << qName << " value = " << value << endl;

    NodeImpl *current = d->nodes.current();
    if(!current)
        return;

    try
    {
        if(current->nodeType() == ELEMENT_NODE)
            static_cast<ElementImpl *>(current)->setAttributeNS(namespaceURI.handle(), qName.handle(), value.handle());
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }
}

void DocumentBuilder::startAttribute(const DOMString &localName, const DOMString &value)
{
    kdDebug(26001) << "DocumentBuilder::startAttribute, localName = " << localName << " value = " << value << endl;

    NodeImpl *current = d->nodes.current();
    if(!current)
        return;

    try
    {
        if(current->nodeType() == ELEMENT_NODE)
            static_cast<ElementImpl *>(current)->setAttribute(localName.handle(), value.handle());
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }
}

bool DocumentBuilder::characters(const DOMString &ch)
{
    kdDebug(26001) << "DocumentBuilder::characters, ch = " << ch << endl;

    NodeImpl *current = d->nodes.current();
    if(!current)
        return false;
    
    try
    {
        if(d->cdata)
            current->addChild(d->doc->createCDATASection(ch.handle()));
        else
        {
            // if we just read character data into a Text node, append the new data
            NodeImpl *lastChild = current->lastChild();
            if(lastChild && lastChild->nodeType() == TEXT_NODE)
                static_cast<TextImpl *>(lastChild)->appendData(ch.handle());
            else
                current->addChild(d->doc->createTextNode(ch.handle()));
        }

        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::comment(const DOMString &ch)
{
    kdDebug(26001) << "DocumentBuilder::comment, ch = " << ch << endl;

    NodeImpl *current = d->nodes.current();
    if(!current)
        return false;
    
    try
    {
        if(ch.string().simplifyWhiteSpace().isEmpty())
            return true;

        current->addChild(d->doc->createComment(ch.handle()));
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::startDTD(const DOMString &name, const DOMString &publicId, const DOMString &systemId)
{
    kdDebug(26001) << "DocumentBuilder::startDTD, name = " << name << " publicId = " << publicId << " systemId = " << systemId << endl;

    try
    {
        DOMImplementationImpl *factory = DOMImplementationImpl::self();
        d->doc->setDocType(factory->createDocumentType(name.handle(), publicId.handle(), systemId.handle()));
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::startCDATA()
{
    kdDebug(26001) << "DocumentBuilder::startCDATA" << endl;
    
    d->cdata = true;
    return true;
}

bool DocumentBuilder::endCDATA()
{
    kdDebug(26001) << "DocumentBuilder::endCDATA" << endl;

    d->cdata = false;
    return true;
}

bool DocumentBuilder::startPI(const DOMString &target, const DOMString &data)
{
    kdDebug(26001) << "DocumentBuilder::startPI, target = " << target << " data = " << data << endl;

    NodeImpl *current = d->nodes.current();
    if(!current)
        return false;

    try
    {
        ProcessingInstructionImpl *pi = d->doc->createProcessingInstruction(target.handle(), data.handle());
        current->addChild(pi);
        pi->checkStyleSheet();
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::internalEntityDecl(const DOMString &name, const DOMString &value, bool deep)
{
    kdDebug(26001) << "DocumentBuilder::internalEntityDecl, name = " << name << " value = " << value << endl;
    
    try
    {
        NamedNodeMapImpl *entities = (d->doc->doctype() ? d->doc->doctype()->entities() : 0);
        if(entities && !entities->getNamedItem(name.handle())) // skip duplicates
        {
            EntityImpl *i = new EntityImpl(d->doc->docPtr(), name.handle());
            entities->setNamedItem(i);

            if(deep)
                pushNode(i);
            else
                i->addChild(d->doc->createTextNode(value.handle()));
        }

        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::internalEntityDeclEnd()
{
    kdDebug(26001) << "DocumentBuilder::internalEntityDeclEnd" << endl;

    try
    {
        popNode();
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::externalEntityDecl(const DOMString &name, const DOMString &publicId, const DOMString &systemId)
{
    kdDebug(26001) << "DocumentBuilder::externalEntityDecl, name = " << name << " publicId = " << publicId << " systemId = " << systemId << endl;

    try
    {
        NamedNodeMapImpl *entities = (d->doc->doctype() ? d->doc->doctype()->entities() : 0);
        if(entities && !entities->getNamedItem(name.handle())) // skip duplicates
        {
            EntityImpl *i = new EntityImpl(d->doc->docPtr(), name.handle(), publicId.handle(), systemId.handle(), 0);
            entities->setNamedItem(i);
        }

        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::unparsedEntityDecl(const DOMString &name, const DOMString &publicId, const DOMString &systemId, const DOMString &notationName)
{
    kdDebug(26001) << "DocumentBuilder::unparsedEntityDecl, name = " << name << " publicId = " << publicId << " systemId = " << systemId << " notationName " << notationName << endl;

    try
    {
        EntityImpl *i = new EntityImpl(d->doc->docPtr(), name.handle(), publicId.handle(), systemId.handle(), notationName.handle());
        
        NamedNodeMapImpl *entities = (d->doc->doctype() ? d->doc->doctype()->entities() : 0);
        if(entities)
            entities->setNamedItem(i);
        
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::notationDecl(const DOMString &name, const DOMString &publicId, const DOMString &systemId)
{
    kdDebug(26001) << "DocumentBuilder::notationDecl, name = " << name << " publicId = " << publicId << " systemId = " << systemId << endl;

    try
    {
        NotationImpl *i = new NotationImpl(d->doc->docPtr(), name.handle(), publicId.handle(), systemId.handle());

        NamedNodeMapImpl *notations = (d->doc->doctype() ? d->doc->doctype()->notations() : 0);
        if(notations)
            notations->setNamedItem(i);
        
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::entityReferenceStart(const DOMString &name)
{
    kdDebug(26001) << "DocumentBuilder::entityReferenceStart, name = " << name << endl;

    try
    {
        pushNode(d->doc->createEntityReference(name.handle()));
        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

bool DocumentBuilder::entityReferenceEnd(const DOMString &name)
{
    kdDebug(26001) << "DocumentBuilder::entityReferenceEnd, name = " << name << endl;

    try
    {
        NodeImpl *children = popNode();
        NodeImpl *current = d->nodes.current();
        if(current)
            current->addChild(children);

        return true;
    }
    catch(DOMExceptionImpl *e)
    {
        Helper::ShowException(e);
    }

    return false;
}

void DocumentBuilder::pushNode(NodeImpl *node)
{
    d->nodes.push(node);
}

NodeImpl *DocumentBuilder::popNode()
{
    return d->nodes.pop();
}

NodeImpl *DocumentBuilder::currentNode() const
{
    return d->nodes.current();
}

void DocumentBuilder::setDocument(DocumentImpl *doc)
{
    d->doc = doc;
}

// vim:ts=4:noet
