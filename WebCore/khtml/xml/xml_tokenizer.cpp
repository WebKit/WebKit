/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
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
 */


#include "xml_tokenizer.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom_xmlimpl.h"
#include "html/html_headimpl.h"
#include "rendering/render_object.h"
#include "misc/htmltags.h"
#include "misc/htmlattrs.h"
#include "misc/loader.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include <qvariant.h>
#include <kdebug.h>
#include <klocale.h>

using namespace DOM;

namespace khtml {

const int maxErrors = 25;

XMLHandler::XMLHandler(DocumentPtr *_doc, KHTMLView *_view)
    : errorLine(0), m_errorCount(0)
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
    m_errorCount = 0;
    state = StateInit;

    return true;
}


bool XMLHandler::startElement( const QString& namespaceURI, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts )
{
    if (m_errorCount) return true;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    int exceptioncode = 0;
    ElementImpl *newElement = m_doc->document()->createElementNS(namespaceURI,qName,exceptioncode);
    if (!newElement)
        return false;

    int i;
    for (i = 0; i < atts.length(); i++) {
        // FIXME: qualified name not supported for attributes! The prefix has been lost.
        DOMString uri(atts.uri(i));
        DOMString ln(atts.localName(i));
        DOMString val(atts.value(i));
        NodeImpl::Id id = m_doc->document()->attrId(uri.implementation(),
                                                    ln.implementation(),
                                                    false /* allocate */);
        newElement->setAttribute(id, val.implementation(), exceptioncode);
        if (exceptioncode) // exception setting attributes
            return false;
    }

    // FIXME: This hack ensures implicit table bodies get constructed in XHTML and XML files.
    // We want to consolidate this with the HTML parser and HTML DOM code at some point.
    // For now, it's too risky to rip that code up.
    if (m_currentNode->id() == ID_TABLE &&
        newElement->id() == ID_TR &&
        m_currentNode->isHTMLElement() && newElement->isHTMLElement()) {
        NodeImpl* implicitTBody =
           new HTMLTableSectionElementImpl( m_doc, ID_TBODY, true /* implicit */ );
        m_currentNode->addChild(implicitTBody);
        if (m_view && !implicitTBody->attached())
            implicitTBody->attach();
        m_currentNode = implicitTBody;
    }

    if (m_currentNode->addChild(newElement)) {
        if (m_view && !newElement->attached())
            newElement->attach();
        m_currentNode = newElement;
        return true;
    }
    else {
        
        delete newElement;
        return false;
    }

    // ### DOM spec states: "if there is no markup inside an element's content, the text is contained in a
    // single object implementing the Text interface that is the only child of the element."... do we
    // need to ensure that empty elements always have an empty text child?
}


bool XMLHandler::endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
{
    if (m_errorCount) return true;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    if (m_currentNode->parentNode() != 0) {
        m_currentNode->closeRenderer();
        do {
            m_currentNode = m_currentNode->parentNode();
        } while (m_currentNode && m_currentNode->implicitNode());
    }
// ###  else error

    return true;
}


bool XMLHandler::startCDATA()
{
    if (m_errorCount) return true;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    NodeImpl *newNode = m_doc->document()->createCDATASection("");
    if (m_currentNode->addChild(newNode)) {
        if (m_view && !newNode->attached())
            newNode->attach();
        m_currentNode = newNode;
        return true;
    }
    else {
        delete newNode;
        return false;
    }

}

bool XMLHandler::endCDATA()
{
    if (m_errorCount) return true;
    
    if (m_currentNode->parentNode() != 0)
        m_currentNode = m_currentNode->parentNode();
    return true;
}

bool XMLHandler::characters( const QString& ch )
{
    if (m_errorCount) return true;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE ||
        m_currentNode->nodeType() == Node::CDATA_SECTION_NODE ||
        enterText()) {

        int exceptioncode = 0;
        static_cast<TextImpl*>(m_currentNode)->appendData(ch,exceptioncode);
        if (exceptioncode)
            return false;
        return true;
    }
    else
        return false;
}

bool XMLHandler::comment(const QString & ch)
{
    if (m_errorCount) return true;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    // ### handle exceptions
    m_currentNode->addChild(m_doc->document()->createComment(ch));
    return true;
}

bool XMLHandler::processingInstruction(const QString &target, const QString &data)
{
    if (m_errorCount) return true;
    
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    // ### handle exceptions
    ProcessingInstructionImpl *pi = m_doc->document()->createProcessingInstruction(target,data);
    m_currentNode->addChild(pi);
    // don't load stylesheets for standalone documents
    if (m_doc->document()->part()) {
	pi->checkStyleSheet();
    }
    return true;
}


QString XMLHandler::errorString()
{
#if APPLE_CHANGES
    // FIXME: Does the user ever see this?
    return "error";
#else
    return i18n("the document is not in the correct file format");
#endif
}

bool XMLHandler::warning( const QXmlParseException& exception )
{
#if APPLE_CHANGES
    errorProt += QString("warning on line %2 at column %3: %1")
#else
    errorProt += i18n( "warning: %1 in line %2, column %3\n" )
#endif
        .arg( exception.message() )
        .arg( exception.lineNumber() )
        .arg( exception.columnNumber() );
    
    errorLine = exception.lineNumber();
    errorCol = exception.columnNumber();
    
    return true;
}

bool XMLHandler::error( const QXmlParseException& exception )
{
    if (m_errorCount >= maxErrors) return true;
    
    if (errorLine == exception.lineNumber() && errorCol == exception.columnNumber())
        return true; // Only report 1 error for any given line/col position to reduce noise.
    
    m_errorCount++;
    
#if APPLE_CHANGES
    errorProt += QString("error on line %2 at column %3: %1")
#else
    errorProt += i18n( "error: %1 in line %2, column %3\n" )
#endif
        .arg( exception.message() )
        .arg( exception.lineNumber() )
        .arg( exception.columnNumber() );
    
    errorLine = exception.lineNumber();
    errorCol = exception.columnNumber();
    
    return true;
}

bool XMLHandler::fatalError( const QXmlParseException& exception )
{
#if APPLE_CHANGES
    errorProt += QString("error on line %2 at column %3: %1")
#else
    errorProt += i18n( "fatal error: %1 in line %2, column %3\n" )
#endif
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
        m_currentNode = newNode;
        return true;
    }
    else {
        delete newNode;
        return false;
    }
}

void XMLHandler::exitText()
{
    if (m_view && m_currentNode && !m_currentNode->attached())
        m_currentNode->attach();
    
    NodeImpl* par = m_currentNode->parentNode();
    if (par != 0)
        m_currentNode = par;
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
// ### FIXME
//     if (m_doc->document()->doctype())
//         static_cast<GenericRONamedNodeMapImpl*>(m_doc->document()->doctype()->entities())->addNode(e);
    return true;
}

bool XMLHandler::notationDecl(const QString &name, const QString &publicId, const QString &systemId)
{
// ### FIXME
//     if (m_doc->document()->doctype()) {
//         NotationImpl *n = new NotationImpl(m_doc,name,publicId,systemId);
//         static_cast<GenericRONamedNodeMapImpl*>(m_doc->document()->doctype()->notations())->addNode(n);
//     }
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

void XMLTokenizer::write(const TokenizerString &s, bool /*appendData*/ )
{
    m_xmlCode += s.toString();
}

void XMLTokenizer::end()
{
    emit finishedParsing();
}

void XMLTokenizer::finish()
{
    // parse xml file
    XMLHandler* handler = m_doc->document()->createTokenHandler();
    QXmlInputSource source;
    source.setData(m_xmlCode);
    QXmlSimpleReader reader;
    reader.setContentHandler( handler );
    reader.setLexicalHandler( handler );
    reader.setErrorHandler( handler );
    reader.setDeclHandler( handler );
    reader.setDTDHandler( handler );
    bool ok = reader.parse( source );

    if (!ok) {
        // One or more errors occurred during parsing of the code. Display an error block to the user above
        // the normal content (the DOM tree is created manually and includes line/col info regarding 
        // where the errors are located)

        // Create elements for display
        int exceptioncode = 0;
        DocumentImpl *doc = m_doc->document();
        NodeImpl* root = doc->documentElement();
        if (!root) {
            root = doc->createElementNS(XHTML_NAMESPACE, "html", exceptioncode);
            NodeImpl* body = doc->createElementNS(XHTML_NAMESPACE, "body", exceptioncode);
            root->appendChild(body, exceptioncode);
            doc->appendChild(root, exceptioncode);
            root = body;
        }

        ElementImpl* reportDiv = doc->createElementNS(XHTML_NAMESPACE, "div", exceptioncode);
        reportDiv->setAttribute(ATTR_STYLE, "white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black");
        ElementImpl* h3 = doc->createElementNS(XHTML_NAMESPACE, "h3", exceptioncode);
        h3->appendChild(doc->createTextNode("This page contains the following errors:"), exceptioncode);
        reportDiv->appendChild(h3, exceptioncode);
        ElementImpl* fixed = doc->createElementNS(XHTML_NAMESPACE, "div", exceptioncode);
        fixed->setAttribute(ATTR_STYLE, "font-family:monospace;font-size:12px");
        NodeImpl* textNode = doc->createTextNode(handler->errorProtocol());
        fixed->appendChild(textNode, exceptioncode);
        reportDiv->appendChild(fixed, exceptioncode);
        h3 = doc->createElementNS(XHTML_NAMESPACE, "h3", exceptioncode);
        h3->appendChild(doc->createTextNode("Below is a rendering of the page up to the first error."), exceptioncode);
        reportDiv->appendChild(h3, exceptioncode);
        
        root->insertBefore(reportDiv, root->firstChild(), exceptioncode);

        m_doc->document()->recalcStyle( NodeImpl::Inherit );
        m_doc->document()->updateRendering();

        end();
    }
    else {
        // Parsing was successful. Now locate all html <script> tags in the document and execute them
        // one by one
        addScripts(m_doc->document());
        m_scriptsIt = new QPtrListIterator<HTMLScriptElementImpl>(m_scripts);
        executeScripts();
    }

    delete handler;
}

void XMLTokenizer::addScripts(NodeImpl *n)
{
    // Recursively go through the entire document tree, looking for html <script> tags. For each of these
    // that is found, add it to the m_scripts list from which they will be executed

    if (n->id() == ID_SCRIPT) {
        m_scripts.append(static_cast<HTMLScriptElementImpl*>(n));
    }

    NodeImpl *child;
    for (child = n->firstChild(); child; child = child->nextSibling())
        addScripts(child);
}

void XMLTokenizer::executeScripts()
{
    // Iterate through all of the html <script> tags in the document. For those that have a src attribute,
    // start loading the script and return (executeScripts() will be called again once the script is loaded
    // and continue where it left off). For scripts that don't have a src attribute, execute the code
    // inside the tag
    while (m_scriptsIt->current()) {
        DOMString scriptSrc = m_scriptsIt->current()->getAttribute(ATTR_SRC);
        QString charset = m_scriptsIt->current()->getAttribute(ATTR_CHARSET).string();

	// don't load external scripts for standalone documents (for now)
        if (scriptSrc != "" && m_doc->document()->part()) {
            // we have a src attribute
            m_cachedScript = m_doc->document()->docLoader()->requestScript(scriptSrc, charset);
            ++(*m_scriptsIt);
            m_cachedScript->ref(this); // will call executeScripts() again if already cached
            return;
        }
        else {
            // no src attribute - execute from contents of tag
            QString scriptCode = "";
            NodeImpl *child;
            for (child = m_scriptsIt->current()->firstChild(); child; child = child->nextSibling()) {
                if (child->nodeType() == Node::TEXT_NODE || child->nodeType() == Node::CDATA_SECTION_NODE) {
                    scriptCode += static_cast<TextImpl*>(child)->data().string();
                }
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

    // All scripts have finished executing, so calculate the style for the document and close
    // the last element
    m_doc->document()->updateStyleSelector();

    // We are now finished parsing
    end();
}

void XMLTokenizer::notifyFinished(CachedObject *finishedObj)
{
    // This is called when a script has finished loading that was requested from executeScripts(). We execute
    // the script, and then call executeScripts() again to continue iterating through the list of scripts in
    // the document
    if (finishedObj == m_cachedScript) {
        DOMString scriptSource = m_cachedScript->script();
        m_cachedScript->deref(this);
        m_cachedScript = 0;
        m_view->part()->executeScript(scriptSource.string());
        executeScripts();
    }
}

bool XMLTokenizer::isWaitingForScripts()
{
    return m_cachedScript != 0;
}

}

#include "xml_tokenizer.moc"
