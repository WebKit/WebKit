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

#include "xml_tokenizer.h"

#include "dom_docimpl.h"
#include "dom_node.h"
#include "dom_elementimpl.h"
#include "dom_textimpl.h"
#include "dom_xmlimpl.h"
#include "html/html_headimpl.h"
#include "rendering/render_object.h"
#include "css/css_stylesheetimpl.h"

#include "misc/loader.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include <qtextstream.h>
#include <kdebug.h>
#include <klocale.h>

using namespace DOM;
using namespace khtml;

XMLHandler::XMLHandler(DocumentPtr *_doc, KHTMLView *_view)
{
    m_doc = _doc;
    if ( m_doc ) m_doc->ref();
    m_view = _view;
    m_currentNode = _doc->document();
}


XMLHandler::~XMLHandler()
{
    if ( m_doc ) m_doc->deref();
}


QString XMLHandler::errorProtocol()
{
    return errorProt;
}


bool XMLHandler::startDocument()
{
    // at the beginning of parsing: do some initialization
    errorProt = "";
    state = StateInit;

    return TRUE;
}


bool XMLHandler::startElement( const QString& namespaceURI, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts )
{
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    ElementImpl *newElement;
    if (namespaceURI.isNull())
        newElement = m_doc->document()->createElement(qName);
    else
        newElement = m_doc->document()->createElementNS(namespaceURI,qName);

    // ### handle exceptions
    int i;
    for (i = 0; i < atts.length(); i++)
        newElement->setAttribute(atts.localName(i),atts.value(i));
    if (m_currentNode->addChild(newElement)) {
        if (m_view)
            newElement->attach();
        m_currentNode = newElement;
        return TRUE;
    }
    else {
        delete newElement;
        return FALSE;
    }
}


bool XMLHandler::endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
{
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    if (m_currentNode->parentNode() != 0) {
        if (m_currentNode->renderer())
            m_currentNode->renderer()->close();
        m_currentNode = m_currentNode->parentNode();
    }
// ###  else error

    return TRUE;
}


bool XMLHandler::startCDATA()
{
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    NodeImpl *newNode = m_doc->document()->createCDATASection("");
    if (m_currentNode->addChild(newNode)) {
        if (m_view)
            newNode->attach();
        m_currentNode = newNode;
        return TRUE;
    }
    else {
        delete newNode;
        return FALSE;
    }

}

bool XMLHandler::endCDATA()
{
    if (m_currentNode->parentNode() != 0)
        m_currentNode = m_currentNode->parentNode();
    return true;
}

bool XMLHandler::characters( const QString& ch )
{
    if (ch.stripWhiteSpace().isEmpty())
        return TRUE;

    if (m_currentNode->nodeType() == Node::TEXT_NODE || m_currentNode->nodeType() == Node::CDATA_SECTION_NODE
        || enterText()) {
        static_cast<TextImpl*>(m_currentNode)->appendData(ch);
        return TRUE;
    }
    else
        return FALSE;
}

bool XMLHandler::comment(const QString & ch)
{
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    // ### handle exceptions
    m_currentNode->addChild(m_doc->document()->createComment(ch));
    return true;
}

bool XMLHandler::processingInstruction(const QString &target, const QString &data)
{
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    // ### handle exceptions
    ProcessingInstructionImpl *pi = m_doc->document()->createProcessingInstruction(target,data);
    m_currentNode->addChild(pi);
    pi->checkStyleSheet();
    return true;
}


QString XMLHandler::errorString()
{
    return "the document is not in the correct file format";
}


bool XMLHandler::fatalError( const QXmlParseException& exception )
{
    errorProt += QString( "fatal parsing error: %1 in line %2, column %3\n" )
        .arg( exception.message() )
        .arg( exception.lineNumber() )
        .arg( exception.columnNumber() );

    errorLine = exception.lineNumber();
    errorCol = exception.columnNumber();

    return false;
}

bool XMLHandler::enterText()
{
    NodeImpl *newNode = m_doc->document()->createTextNode("");
    if (m_currentNode->addChild(newNode)) {
        if (m_view)
            newNode->attach();
        m_currentNode = newNode;
        return TRUE;
    }
    else {
        delete newNode;
        return FALSE;
    }
}

void XMLHandler::exitText()
{
    NodeImpl* par = m_currentNode->parentNode();
    if (par != 0)
    {
        if (!sheetElemId.isEmpty() && par->isElementNode())
        {
            QValueList<DOMString>::Iterator it;
            for( it = sheetElemId.begin(); it != sheetElemId.end(); ++it )
            {
                if (static_cast<ElementImpl*>(par)->getAttribute("id")==*it)
                {
    //                kdDebug() << "sheet found:" << endl;
                    DOMString sheet= static_cast<TextImpl*>(m_currentNode)->data();
    //                kdDebug() << sheet.string() << endl;
                    CSSStyleSheetImpl *styleSheet = new CSSStyleSheetImpl(m_doc->document());
                    styleSheet->parseString(sheet);
                    m_doc->document()->createSelector();
                }
            }
        }

        m_currentNode = par;
    }
}

bool XMLHandler::attributeDecl(const QString &/*eName*/, const QString &/*aName*/, const QString &/*type*/,
                               const QString &/*valueDefault*/, const QString &/*value*/)
{
    // qt's xml parser (as of 2.2.3) does not currently give us values for type, valueDefault and
    // value. When it does, we can store these somewhere and have default attributes on elements
    return true;
}

bool XMLHandler::externalEntityDecl(const QString &/*name*/, const QString &/*publicId*/, const QString &/*systemId*/)
{
    // ### insert these too - is there anything special we have to do here?
    return true;
}

bool XMLHandler::internalEntityDecl(const QString &name, const QString &value)
{
    EntityImpl *e = new EntityImpl(m_doc,name);
    // ### further parse entities inside the value and add them as separate nodes (or entityreferences)?
    e->addChild(m_doc->document()->createTextNode(value));
    static_cast<GenericRONamedNodeMapImpl*>(m_doc->document()->doctype()->entities())->addNode(e);
    return true;
}

bool XMLHandler::notationDecl(const QString &name, const QString &publicId, const QString &systemId)
{
    NotationImpl *n = new NotationImpl(m_doc,name,publicId,systemId);
    static_cast<GenericRONamedNodeMapImpl*>(m_doc->document()->doctype()->notations())->addNode(n);
    return true;
}

bool XMLHandler::unparsedEntityDecl(const QString &/*name*/, const QString &/*publicId*/,
                                    const QString &/*systemId*/, const QString &/*notationName*/)
{
    // ###
    return true;
}


//------------------------------------------------------------------------------



XMLTokenizer::XMLTokenizer(DOM::DocumentPtr *_doc, KHTMLView *_view)
{
    m_doc = _doc;
    if ( m_doc ) m_doc->ref();
    m_view = _view;
    m_xmlCode = "";
    m_scriptsIt = 0;
    m_cachedScript = 0;
}

XMLTokenizer::~XMLTokenizer()
{
    if ( m_doc ) m_doc->deref();
    if (m_scriptsIt)
        delete m_scriptsIt;
    if (m_cachedScript)
        m_cachedScript->deref(this);
}


void XMLTokenizer::begin()
{
}

void XMLTokenizer::write( const QString &str, bool /*appendData*/ )
{
    m_xmlCode += str;
}

void XMLTokenizer::end()
{
    emit finishedParsing();
}

void XMLTokenizer::finish()
{
    // parse xml file
    XMLHandler handler(m_doc,m_view);
    QXmlInputSource source;
    source.setData(m_xmlCode);
    QXmlSimpleReader reader;
    reader.setContentHandler( &handler );
    reader.setLexicalHandler( &handler );
    reader.setErrorHandler( &handler );
    reader.setDeclHandler( &handler );
    reader.setDTDHandler( &handler );
    bool ok = reader.parse( source );
    // ### handle exceptions inserting nodes
    if (!ok) {
        kdDebug(6036) << "Error during XML parsing: " << handler.errorProtocol() << endl;

        int exceptioncode;
        while (m_doc->document()->hasChildNodes())
            static_cast<NodeImpl*>(m_doc->document())->removeChild(m_doc->document()->firstChild(),exceptioncode);

        // construct a HTML page giving the error message
        // ### for multiple error messages, display the code for each
        QTextIStream stream(&m_xmlCode);
        unsigned long lineno;
        for (lineno = 0; lineno < handler.errorLine-1; lineno++)
          stream.readLine();
        QString line = stream.readLine();

        m_doc->document()->appendChild(m_doc->document()->createElementNS("http://www.w3.org/1999/xhtml","html"),exceptioncode);
        NodeImpl *body = m_doc->document()->createElementNS("http://www.w3.org/1999/xhtml","body");
        m_doc->document()->firstChild()->appendChild(body,exceptioncode);

        NodeImpl *h1 = m_doc->document()->createElementNS("http://www.w3.org/1999/xhtml","h1");
        body->appendChild(h1,exceptioncode);
        h1->appendChild(m_doc->document()->createTextNode(i18n("XML parsing error")),exceptioncode);
        h1->renderer()->close();

        body->appendChild(m_doc->document()->createTextNode(handler.errorProtocol()),exceptioncode);
        body->appendChild(m_doc->document()->createElementNS("http://www.w3.org/1999/xhtml","hr"),exceptioncode);
        NodeImpl *pre = m_doc->document()->createElementNS("http://www.w3.org/1999/xhtml","pre");
        body->appendChild(pre,exceptioncode);
        pre->appendChild(m_doc->document()->createTextNode(line+"\n"),exceptioncode);

        unsigned long colno;
        QString indent = "";
        for (colno = 0; colno < handler.errorCol-1; colno++)
            indent += " ";

        pre->appendChild(m_doc->document()->createTextNode(indent+"^"),exceptioncode);
        pre->renderer()->close();

        body->renderer()->close();
        m_doc->document()->applyChanges();
        m_doc->document()->updateRendering();

        end();
    }
    else {
        addScripts(m_doc->document());
        m_scriptsIt = new QListIterator<HTMLScriptElementImpl>(m_scripts);
        executeScripts();
    }

}

void XMLTokenizer::addScripts(NodeImpl *n)
{
    if (n->nodeName() == "SCRIPT") { // ### also check that namespace is html (and SCRIPT should be lowercase)
        m_scripts.append(static_cast<HTMLScriptElementImpl*>(n));
    }

    NodeImpl *child;
    for (child = n->firstChild(); child; child = child->nextSibling())
        addScripts(child);
}

void XMLTokenizer::executeScripts()
{
    while (m_scriptsIt->current()) {
        DOMString scriptSrc = m_scriptsIt->current()->getAttribute("src");
        QString charset = m_scriptsIt->current()->getAttribute( "charset" ).string();
        if (scriptSrc != "") {
            m_cachedScript = m_doc->document()->docLoader()->requestScript(scriptSrc, m_doc->document()->baseURL(), charset);
            ++(*m_scriptsIt);
            m_cachedScript->ref(this); // will call executeScripts() again if already cached
            return;
        }
        else {
            QString scriptCode = "";
            NodeImpl *child;
            for (child = m_scriptsIt->current()->firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == Node::TEXT_NODE || child->nodeType() == Node::CDATA_SECTION_NODE)
                    scriptCode += static_cast<TextImpl*>(child)->data().string();
            }
            // the script cannot do document.write until we support incremental parsing
            // ### handle the case where the script deletes the node or redirects to
            // another page, etc. (also in notifyFinished())
            // ### the script may add another script node after this one which should be executed
            if (m_view) {
                m_view->part()->executeScript(scriptCode);
            }
            ++(*m_scriptsIt);
        }
    }
    if (!m_scriptsIt->current()) {
        end(); // this actually causes us to be deleted
    }
}

void XMLTokenizer::notifyFinished(CachedObject *finishedObj)
{
    if (finishedObj == m_cachedScript) {
        DOMString scriptSource = m_cachedScript->script();
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        m_view->part()->executeScript(scriptSource.string());
        executeScripts();
    }
}

#include "xml_tokenizer.moc"

