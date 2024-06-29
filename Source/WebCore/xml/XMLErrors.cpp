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
#include "HTMLBodyElement.h"
#include "HTMLDivElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHeadingElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLParagraphElement.h"
#include "HTMLStyleElement.h"
#include "LocalFrame.h"
#include "SVGNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

const int maxErrors = 25;

XMLErrors::XMLErrors(Document& document)
    : m_document(document)
{
}

void XMLErrors::handleError(ErrorType type, const char* message, int lineNumber, int columnNumber)
{
    handleError(type, message, TextPosition(OrdinalNumber::fromOneBasedInt(lineNumber), OrdinalNumber::fromOneBasedInt(columnNumber)));
}

void XMLErrors::handleError(ErrorType type, const char* message, TextPosition position)
{
    if (type == fatal || (m_errorCount < maxErrors && (!m_lastErrorPosition || (m_lastErrorPosition->m_line != position.m_line && m_lastErrorPosition->m_column != position.m_column)))) {
        switch (type) {
        case warning:
            appendErrorMessage("warning"_s, position, message);
            break;
        case fatal:
        case nonFatal:
            appendErrorMessage("error"_s, position, message);
        }

        m_lastErrorPosition = position;
        ++m_errorCount;
    }
}

void XMLErrors::appendErrorMessage(ASCIILiteral typeString, TextPosition position, const char* message)
{
    // <typeString> on line <lineNumber> at column <columnNumber>: <message>
    m_errorMessages.append(typeString, " on line "_s, position.m_line.oneBasedInt(), " at column "_s, position.m_column.oneBasedInt(), ": "_s, span(message));
}

static inline Ref<Element> createXHTMLParserErrorHeader(Document& document, String&& errorMessages)
{
    Ref<Element> reportElement = document.createElement(QualifiedName(nullAtom(), "parsererror"_s, xhtmlNamespaceURI), true);

    Attribute reportAttribute(styleAttr, "display: block; white-space: pre; border: 2px solid #c77; padding: 0 1em 0 1em; margin: 1em; background-color: #fdd; color: black"_s);
    reportElement->parserSetAttributes(std::span(&reportAttribute, 1));

    auto h3 = HTMLHeadingElement::create(h3Tag, document);
    reportElement->parserAppendChild(h3);
    h3->parserAppendChild(Text::create(document, "This page contains the following errors:"_s));

    auto fixed = HTMLDivElement::create(document);
    Attribute fixedAttribute(styleAttr, "font-family:monospace;font-size:12px"_s);
    fixed->parserSetAttributes(std::span(&fixedAttribute, 1));
    reportElement->parserAppendChild(fixed);

    fixed->parserAppendChild(Text::create(document, WTFMove(errorMessages)));

    h3 = HTMLHeadingElement::create(h3Tag, document);
    reportElement->parserAppendChild(h3);
    h3->parserAppendChild(Text::create(document, "Below is a rendering of the page up to the first error."_s));

    return reportElement;
}

void XMLErrors::insertErrorMessageBlock()
{
    // One or more errors occurred during parsing of the code. Display an error block to the user above
    // the normal content (the DOM tree is created manually and includes line/col info regarding
    // where the errors are located)

    // Create elements for display
    RefPtr<Element> documentElement = m_document.documentElement();
    if (!documentElement) {
        auto rootElement = HTMLHtmlElement::create(m_document);
        auto body = HTMLBodyElement::create(m_document);
        rootElement->parserAppendChild(body);
        m_document.parserAppendChild(rootElement);
        documentElement = WTFMove(body);
    } else if (documentElement->namespaceURI() == SVGNames::svgNamespaceURI) {
        auto rootElement = HTMLHtmlElement::create(m_document);
        auto head = HTMLHeadElement::create(m_document);
        auto style = HTMLStyleElement::create(m_document);
        head->parserAppendChild(style);
        style->parserAppendChild(m_document.createTextNode("html, body { height: 100% } parsererror + svg { width: 100%; height: 100% }"_s));
        style->finishParsingChildren();
        rootElement->parserAppendChild(head);
        auto body = HTMLBodyElement::create(m_document);
        rootElement->parserAppendChild(body);

        m_document.parserRemoveChild(*documentElement);
        if (!documentElement->parentNode())
            body->parserAppendChild(*documentElement);

        m_document.parserAppendChild(rootElement);

        documentElement = WTFMove(body);
    }

    auto reportElement = createXHTMLParserErrorHeader(m_document, m_errorMessages.toString());

#if ENABLE(XSLT)
    if (m_document.transformSourceDocument()) {
        Attribute attribute(styleAttr, "white-space: normal"_s);
        auto paragraph = HTMLParagraphElement::create(m_document);
        paragraph->parserSetAttributes(std::span(&attribute, 1));
        paragraph->parserAppendChild(m_document.createTextNode("This document was created as the result of an XSL transformation. The line and column numbers given are from the transformed result."_s));
        reportElement->parserAppendChild(paragraph);
    }
#endif

    Node* firstChild = documentElement->firstChild();
    if (firstChild)
        documentElement->parserInsertBefore(reportElement, *firstChild);
    else
        documentElement->parserAppendChild(reportElement);

    m_document.updateStyleIfNeeded();
}

} // namespace WebCore
