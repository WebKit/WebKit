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

    return true;
}


bool XMLHandler::startElement( const QString& namespaceURI, const QString& /*localName*/, const QString& qName, const QXmlAttributes& atts )
{
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();

    ElementImpl *newElement;
    newElement = m_doc->document()->createElementNS(namespaceURI,qName);

    int i;
    for (i = 0; i < atts.length(); i++) {
        int exceptioncode = 0;
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
    if (m_currentNode->nodeType() == Node::TEXT_NODE)
        exitText();
    if (m_currentNode->parentNode() != 0) {
        if (m_currentNode->renderer())
            m_currentNode->renderer()->close();
        m_currentNode = m_currentNode->parentNode();
    }
// ###  else error

    return true;
}


bool XMLHandler::startCDATA()
{
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
    if (m_currentNode->parentNode() != 0)
        m_currentNode = m_currentNode->parentNode();
    return true;
}

bool XMLHandler::characters( const QString& ch )
{
    if (ch.stripWhiteSpace().isEmpty())
        return true;

    if (m_currentNode->nodeType() == Node::TEXT_NODE ||
        m_currentNode->nodeType() == Node::CDATA_SECTION_NODE ||
        enterText()) {

        unsigned short parentId = m_currentNode->parentNode() ? m_currentNode->parentNode()->id() : 0;
        if (parentId == ID_SCRIPT || parentId == ID_STYLE || parentId == ID_XMP || parentId == ID_TEXTAREA) {
            // ### hack.. preserve whitespace for script, style, xmp and textarea... is this the correct
            // way of doing this?
            int exceptioncode = 0;
            static_cast<TextImpl*>(m_currentNode)->appendData(ch,exceptioncode);
            if (exceptioncode)
                return false;
        }
        else {
            // for all others, simplify the whitespace
            int exceptioncode = 0;
            static_cast<TextImpl*>(m_currentNode)->appendData(ch.simplifyWhiteSpace(),exceptioncode);
            if (exceptioncode)
                return false;
        }
        return true;
    }
    else
        return false;
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
    return i18n("the document is not in the correct file format");
}


bool XMLHandler::fatalError( const QXmlParseException& exception )
{
    errorProt += i18n( "fatal parsing error: %1 in line %2, column %3" )
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

void XMLHandler::exitText()
{
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

    if (!ok) {
        // An error occurred during parsing of the code. Display an error page to the user (the DOM
        // tree is created manually and includes an excerpt from the code where the error is located)

        // ### for multiple error messages, display the code for each (can this happen?)

        // Clear the document
        int exceptioncode = 0;
        while (m_doc->document()->hasChildNodes())
            static_cast<NodeImpl*>(m_doc->document())->removeChild(m_doc->document()->firstChild(),exceptioncode);

        QTextIStream stream(&m_xmlCode);
        unsigned long lineno;
        for (lineno = 0; lineno < handler.errorLine-1; lineno++)
          stream.readLine();
        QString line = stream.readLine();

        unsigned long colno;
        QString errorLocPtr = "";
        for (colno = 0; colno < handler.errorCol-1; colno++)
            errorLocPtr += " ";
        errorLocPtr += "^";

        // Create elements for display
        DocumentImpl *doc = m_doc->document();
        NodeImpl *html = doc->createElementNS(XHTML_NAMESPACE,"html");
        NodeImpl   *body = doc->createElementNS(XHTML_NAMESPACE,"body");
        NodeImpl     *h1 = doc->createElementNS(XHTML_NAMESPACE,"h1");
        NodeImpl       *headingText = doc->createTextNode(i18n("XML parsing error"));
        NodeImpl     *errorText = doc->createTextNode(handler.errorProtocol());
        NodeImpl     *hr = doc->createElementNS(XHTML_NAMESPACE,"hr");
        NodeImpl     *pre = doc->createElementNS(XHTML_NAMESPACE,"pre");
        NodeImpl       *lineText = doc->createTextNode(line+"\n");
        NodeImpl       *errorLocText = doc->createTextNode(errorLocPtr);

        // Construct DOM tree. We ignore exceptions as we assume they will not be thrown here (due to the
        // fact we are using a known tag set)
        doc->appendChild(html,exceptioncode);
        html->appendChild(body,exceptioncode);
        body->appendChild(h1,exceptioncode);
        h1->appendChild(headingText,exceptioncode);
        body->appendChild(errorText,exceptioncode);
        body->appendChild(hr,exceptioncode);
        body->appendChild(pre,exceptioncode);
        pre->appendChild(lineText,exceptioncode);
        pre->appendChild(errorLocText,exceptioncode);

        // Close the renderers so that they update their display correctly
        // ### this should not be necessary, but requires changes in the rendering code...
        h1->renderer()->close();
        pre->renderer()->close();
        body->renderer()->close();

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

        if (scriptSrc != "") {
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

#include "xml_tokenizer.moc"

