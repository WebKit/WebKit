/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XMLErrors.h"

#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Text.h"
#include <wtf/text/WTFString.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

const int maxErrors = 25;

XMLErrors::XMLErrors(Document* document)
    : m_document(document)
    , m_errorCount(0)
    , m_lastErrorPosition(TextPosition1::belowRangePosition())
{
}

void XMLErrors::handleError(ErrorType type, const char* message, int lineNumber, int columnNumber)
{
    handleError(type, message, TextPosition1(WTF::OneBasedNumber::fromOneBasedInt(lineNumber), WTF::OneBasedNumber::fromOneBasedInt(columnNumber)));
}

void XMLErrors::handleError(ErrorType type, const char* message, TextPosition1 position)
{
    if (type == fatal || (m_errorCount < maxErrors && m_lastErrorPosition.m_line != position.m_line && m_lastErrorPosition.m_column != position.m_column)) {
        switch (type) {
        case warning:
            appendErrorMessage("warning", position, message);
            break;
        case fatal:
        case nonFatal:
            appendErrorMessage("error", position, message);
        }

        m_lastErrorPosition = position;
        ++m_errorCount;
    }
}

void XMLErrors::appendErrorMessage(const String& typeString, TextPosition1 position, const char* message)
{
    // <typeString> on line <lineNumber> at column <columnNumber>: <message>
    m_errorMessages.append(typeString);
    m_errorMessages.append(" on line ");
    m_errorMessages.append(String::number(position.m_line.oneBasedInt()));
    m_errorMessages.append(" at column ");
    m_errorMessages.append(String::number(position.m_column.oneBasedInt()));
    m_errorMessages.append(": ");
    m_errorMessages.append(message);
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

void XMLErrors::insertErrorMessageBlock()
{
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding
    // where the errors are located)

    // Create elements for display
    ExceptionCode ec = 0;
    RefPtr<Element> documentElement = m_document->documentElement();
    if (!documentElement) {
        RefPtr<Element> rootElement = m_document->createElement(htmlTag, false);
        m_document->appendChild(rootElement, ec);
        RefPtr<Element> body = m_document->createElement(bodyTag, false);
        rootElement->appendChild(body, ec);
        documentElement = body.get();
    }
#if ENABLE(SVG)
    else if (documentElement->namespaceURI() == SVGNames::svgNamespaceURI) {
        RefPtr<Element> rootElement = m_document->createElement(htmlTag, false);
        RefPtr<Element> body = m_document->createElement(bodyTag, false);
        rootElement->appendChild(body, ec);
        body->appendChild(documentElement, ec);
        m_document->appendChild(rootElement.get(), ec);
        documentElement = body.get();
    }
#endif
    String errorMessages = m_errorMessages.toString();
    RefPtr<Element> reportElement = createXHTMLParserErrorHeader(m_document, errorMessages);
    documentElement->insertBefore(reportElement, documentElement->firstChild(), ec);
#if ENABLE(XSLT)
    if (m_document->transformSourceDocument()) {
        RefPtr<Element> paragraph = m_document->createElement(pTag, false);
        paragraph->setAttribute(styleAttr, "white-space: normal");
        paragraph->appendChild(m_document->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."), ec);
        reportElement->appendChild(paragraph.release(), ec);
    }
#endif
    m_document->updateStyleIfNeeded();
}

} // namespace WebCore
