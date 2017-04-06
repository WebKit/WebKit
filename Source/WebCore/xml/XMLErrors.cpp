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
#include "SVGNames.h"
#include "Text.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

const int maxErrors = 25;

XMLErrors::XMLErrors(Document* document)
    : m_document(document)
    , m_errorCount(0)
    , m_lastErrorPosition(TextPosition::belowRangePosition())
{
}

void XMLErrors::handleError(ErrorType type, const char* message, int lineNumber, int columnNumber)
{
    handleError(type, message, TextPosition(OrdinalNumber::fromOneBasedInt(lineNumber), OrdinalNumber::fromOneBasedInt(columnNumber)));
}

void XMLErrors::handleError(ErrorType type, const char* message, TextPosition position)
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

void XMLErrors::appendErrorMessage(const String& typeString, TextPosition position, const char* message)
{
    // <typeString> on line <lineNumber> at column <columnNumber>: <message>
    m_errorMessages.append(typeString);
    m_errorMessages.appendLiteral(" on line ");
    m_errorMessages.appendNumber(position.m_line.oneBasedInt());
    m_errorMessages.appendLiteral(" at column ");
    m_errorMessages.appendNumber(position.m_column.oneBasedInt());
    m_errorMessages.appendLiteral(": ");
    m_errorMessages.append(message);
}

static inline Ref<Element> createXHTMLParserErrorHeader(Document* doc, const String& errorMessages)
{
    Ref<Element> reportElement = doc->createElement(QualifiedName(nullAtom, "parsererror", xhtmlNamespaceURI), true);

    Vector<Attribute> reportAttributes;
    reportAttributes.append(Attribute(styleAttr, "display: block; white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black"));
    reportElement->parserSetAttributes(reportAttributes);

    auto h3 = doc->createElement(h3Tag, true);
    reportElement->parserAppendChild(h3);
    h3->parserAppendChild(doc->createTextNode("This page contains the following errors:"));

    auto fixed = doc->createElement(divTag, true);
    Vector<Attribute> fixedAttributes;
    fixedAttributes.append(Attribute(styleAttr, "font-family:monospace;font-size:12px"));
    fixed->parserSetAttributes(fixedAttributes);
    reportElement->parserAppendChild(fixed);

    fixed->parserAppendChild(doc->createTextNode(errorMessages));

    h3 = doc->createElement(h3Tag, true);
    reportElement->parserAppendChild(h3);
    h3->parserAppendChild(doc->createTextNode("Below is a rendering of the page up to the first error."));

    return reportElement;
}

void XMLErrors::insertErrorMessageBlock()
{
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding
    // where the errors are located)

    // Create elements for display
    RefPtr<Element> documentElement = m_document->documentElement();
    if (!documentElement) {
        auto rootElement = m_document->createElement(htmlTag, true);
        auto body = m_document->createElement(bodyTag, true);
        rootElement->parserAppendChild(body);
        m_document->parserAppendChild(rootElement);
        documentElement = WTFMove(body);
    }
    else if (documentElement->namespaceURI() == SVGNames::svgNamespaceURI) {
        auto rootElement = m_document->createElement(htmlTag, true);
        auto head = m_document->createElement(headTag, true);
        auto style = m_document->createElement(styleTag, true);
        head->parserAppendChild(style);
        style->parserAppendChild(m_document->createTextNode("html, body { height: 100% } parsererror + svg { width: 100%; height: 100% }"));
        style->finishParsingChildren();
        rootElement->parserAppendChild(head);
        auto body = m_document->createElement(bodyTag, true);
        rootElement->parserAppendChild(body);

        m_document->parserRemoveChild(*documentElement);
        if (!documentElement->parentNode())
            body->parserAppendChild(*documentElement);

        m_document->parserAppendChild(rootElement);

        documentElement = WTFMove(body);
    }

    String errorMessages = m_errorMessages.toString();
    auto reportElement = createXHTMLParserErrorHeader(m_document, errorMessages);

#if ENABLE(XSLT)
    if (m_document->transformSourceDocument()) {
        Vector<Attribute> attributes;
        attributes.append(Attribute(styleAttr, "white-space: normal"));
        auto paragraph = m_document->createElement(pTag, true);
        paragraph->parserSetAttributes(attributes);
        paragraph->parserAppendChild(m_document->createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."));
        reportElement->parserAppendChild(paragraph);
    }
#endif

    Node* firstChild = documentElement->firstChild();
    if (firstChild)
        documentElement->parserInsertBefore(reportElement, *firstChild);
    else
        documentElement->parserAppendChild(reportElement);

    m_document->updateStyleIfNeeded();
}

} // namespace WebCore
