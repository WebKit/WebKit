/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "XMLTokenizer.h"

#include "CDATASection.h"
#include "CachedScript.h"
#include "Comment.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "ImageLoader.h"
#include "ProcessingInstruction.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "TextResourceDecoder.h"
#include "TreeDepthLimit.h"
#include <wtf/text/CString.h>
#include <wtf/StringExtras.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#include "SVGStyleElement.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int maxErrors = 25;

#if ENABLE(WML)
bool XMLTokenizer::isWMLDocument() const
{
    if (m_doc)
        return m_doc->isWMLDocument();

    return false;
}
#endif

void XMLTokenizer::pushCurrentNode(Node* n)
{
    ASSERT(n);
    ASSERT(m_currentNode);
    if (n != m_doc)
        n->ref();
    m_currentNodeStack.append(m_currentNode);
    m_currentNode = n;
    if (m_currentNodeStack.size() > maxDOMTreeDepth)
        handleError(fatal, "Excessive node nesting.", lineNumber(), columnNumber());
}

void XMLTokenizer::popCurrentNode()
{
    if (!m_currentNode)
        return;
    ASSERT(m_currentNodeStack.size());

    if (m_currentNode != m_doc)
        m_currentNode->deref();

    m_currentNode = m_currentNodeStack.last();
    m_currentNodeStack.removeLast();
}

void XMLTokenizer::clearCurrentNodeStack()
{
    if (m_currentNode && m_currentNode != m_doc)
        m_currentNode->deref();
    m_currentNode = 0;

    if (m_currentNodeStack.size()) { // Aborted parsing.
        for (size_t i = m_currentNodeStack.size() - 1; i != 0; --i)
            m_currentNodeStack[i]->deref();
        if (m_currentNodeStack[0] && m_currentNodeStack[0] != m_doc)
            m_currentNodeStack[0]->deref();
        m_currentNodeStack.clear();
    }
}

void XMLTokenizer::write(const SegmentedString& s, bool /*appendData*/)
{
    String parseString = s.toString();
    
    if (m_sawXSLTransform || !m_sawFirstElement)
        m_originalSourceForTransform += parseString;

    if (m_parserStopped || m_sawXSLTransform)
        return;
    
    if (m_parserPaused) {
        m_pendingSrc.append(s);
        return;
    }
    
    doWrite(s.toString());
    
    // After parsing, go ahead and dispatch image beforeload events.
    ImageLoader::dispatchPendingBeforeLoadEvents();
}

void XMLTokenizer::handleError(ErrorType type, const char* m, int lineNumber, int columnNumber)
{
    if (type == fatal || (m_errorCount < maxErrors && m_lastErrorLine != lineNumber && m_lastErrorColumn != columnNumber)) {
        switch (type) {
            case warning:
                m_errorMessages += String::format("warning on line %d at column %d: %s", lineNumber, columnNumber, m);
                break;
            case fatal:
            case nonFatal:
                m_errorMessages += String::format("error on line %d at column %d: %s", lineNumber, columnNumber, m);
        }
        
        m_lastErrorLine = lineNumber;
        m_lastErrorColumn = columnNumber;
        ++m_errorCount;
    }
    
    if (type != warning)
        m_sawError = true;
    
    if (type == fatal)
        stopParsing();    
}

bool XMLTokenizer::enterText()
{
#if !USE(QXMLSTREAM)
    ASSERT(m_bufferedText.size() == 0);
#endif
    RefPtr<Node> newNode = Text::create(m_doc, "");
    if (!m_currentNode->addChild(newNode.get()))
        return false;
    pushCurrentNode(newNode.get());
    return true;
}

#if !USE(QXMLSTREAM)
static inline String toString(const xmlChar* str, unsigned len)
{
    return UTF8Encoding().decode(reinterpret_cast<const char*>(str), len);
}
#endif


void XMLTokenizer::exitText()
{
    if (m_parserStopped)
        return;

    if (!m_currentNode || !m_currentNode->isTextNode())
        return;

#if !USE(QXMLSTREAM)
    ExceptionCode ec = 0;
    static_cast<Text*>(m_currentNode)->appendData(toString(m_bufferedText.data(), m_bufferedText.size()), ec);
    Vector<xmlChar> empty;
    m_bufferedText.swap(empty);
#endif

    if (m_view && m_currentNode && !m_currentNode->attached())
        m_currentNode->attach();

    popCurrentNode();
}

void XMLTokenizer::end()
{
    doEnd();

    // doEnd() could process a script tag, thus pausing parsing.
    if (m_parserPaused)
        return;

    if (m_sawError)
        insertErrorMessageBlock();
    else {
        exitText();
        m_doc->styleSelectorChanged(RecalcStyleImmediately);
    }
    
    clearCurrentNodeStack();
    if (!m_parsingFragment)
        m_doc->finishedParsing();    
}

void XMLTokenizer::finish()
{
    if (m_parserPaused)
        m_finishCalled = true;
    else
        end();
}

static inline RefPtr<Element> createXHTMLParserErrorHeader(Document* doc, const String& errorMessages) 
{
    RefPtr<Element> reportElement = doc->createElement(QualifiedName(nullAtom, "parsererror", xhtmlNamespaceURI), false);
    reportElement->setAttribute(styleAttr, "display: block; white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black");
    
    ExceptionCode ec = 0;
    RefPtr<Element> h3 = doc->createElement(h3Tag, false);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("This page contains the following errors:"), ec);

    RefPtr<Element> fixed = doc->createElement(divTag, false);
    reportElement->appendChild(fixed.get(), ec);
    fixed->setAttribute(styleAttr, "font-family:monospace;font-size:12px");
    fixed->appendChild(doc->createTextNode(errorMessages), ec);

    h3 = doc->createElement(h3Tag, false);
    reportElement->appendChild(h3.get(), ec);
    h3->appendChild(doc->createTextNode("Below is a rendering of the page up to the first error."), ec);
    
    return reportElement;
}

void XMLTokenizer::insertErrorMessageBlock()
{
#if USE(QXMLSTREAM)
    if (m_parsingFragment)
        return;
#endif
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding 
    // where the errors are located)

    // Create elements for display
    ExceptionCode ec = 0;
    Document* doc = m_doc;
    Node* documentElement = doc->documentElement();
    if (!documentElement) {
        RefPtr<Node> rootElement = doc->createElement(htmlTag, false);
        doc->appendChild(rootElement, ec);
        RefPtr<Node> body = doc->createElement(bodyTag, false);
        rootElement->appendChild(body, ec);
        documentElement = body.get();
    }
#if ENABLE(SVG)
    else if (documentElement->namespaceURI() == SVGNames::svgNamespaceURI) {
        RefPtr<Node> rootElement = doc->createElement(htmlTag, false);
        RefPtr<Node> body = doc->createElement(bodyTag, false);
        rootElement->appendChild(body, ec);
        body->appendChild(documentElement, ec);
        doc->appendChild(rootElement.get(), ec);
        documentElement = body.get();
    }
#endif
#if ENABLE(WML)
    else if (isWMLDocument()) {
        RefPtr<Node> rootElement = doc->createElement(htmlTag, false);
        RefPtr<Node> body = doc->createElement(bodyTag, false);
        rootElement->appendChild(body, ec);
        body->appendChild(documentElement, ec);
        doc->appendChild(rootElement.get(), ec);
        documentElement = body.get();
    }
#endif

    RefPtr<Element> reportElement = createXHTMLParserErrorHeader(doc, m_errorMessages);
    documentElement->insertBefore(reportElement, documentElement->firstChild(), ec);
#if ENABLE(XSLT)
    if (doc->transformSourceDocument()) {
        RefPtr<Element> par = doc->createElement(pTag, false);
        reportElement->appendChild(par, ec);
        par->setAttribute(styleAttr, "white-space: normal");
        par->appendChild(doc->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."), ec);
    }
#endif
    doc->updateStyleIfNeeded();
}

void XMLTokenizer::notifyFinished(CachedResource* unusedResource)
{
    ASSERT_UNUSED(unusedResource, unusedResource == m_pendingScript);
    ASSERT(m_pendingScript->accessCount() > 0);
        
    ScriptSourceCode sourceCode(m_pendingScript.get());
    bool errorOccurred = m_pendingScript->errorOccurred();

    m_pendingScript->removeClient(this);
    m_pendingScript = 0;
    
    RefPtr<Element> e = m_scriptElement;
    m_scriptElement = 0;

    ScriptElement* scriptElement = toScriptElement(e.get());
    ASSERT(scriptElement);

    if (errorOccurred) 
        scriptElement->dispatchErrorEvent();
    else {
        m_view->frame()->script()->executeScript(sourceCode);
        scriptElement->dispatchLoadEvent();
    }

    m_scriptElement = 0;
    
    if (!m_requestingScript)
        resumeParsing();
}

bool XMLTokenizer::isWaitingForScripts() const
{
    return m_pendingScript;
}

void XMLTokenizer::pauseParsing()
{
    if (m_parsingFragment)
        return;
    
    m_parserPaused = true;
}

}
