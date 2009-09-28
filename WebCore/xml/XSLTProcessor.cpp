/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@webkit.org>
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

#if ENABLE(XSLT)

#include "XSLTProcessor.h"

#include "DOMImplementation.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLTokenizer.h" // for parseHTMLDocumentFragment
#include "Page.h"
#include "Text.h"
#include "TextResourceDecoder.h"
#include "XMLTokenizer.h"
#include "loader.h"
#include "markup.h"
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

namespace WebCore {

static inline void transformTextStringToXHTMLDocumentString(String& text)
{
    // Modify the output so that it is a well-formed XHTML document with a <pre> tag enclosing the text.
    text.replace('&', "&amp;");
    text.replace('<', "&lt;");
    text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
        "<head><title/></head>\n"
        "<body>\n"
        "<pre>" + text + "</pre>\n"
        "</body>\n"
        "</html>\n";
}

PassRefPtr<Document> XSLTProcessor::createDocumentFromSource(const String& sourceString,
    const String& sourceEncoding, const String& sourceMIMEType, Node* sourceNode, Frame* frame)
{
    RefPtr<Document> ownerDocument = sourceNode->document();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    String documentSource = sourceString;

    RefPtr<Document> result;
    if (sourceMIMEType == "text/plain") {
        result = ownerDocument->implementation()->createDocument(frame);
        transformTextStringToXHTMLDocumentString(documentSource);
    } else
        result = ownerDocument->implementation()->createDocument(sourceMIMEType, frame, false);

    // Before parsing, we need to save & detach the old document and get the new document
    // in place. We have to do this only if we're rendering the result document.
    if (frame) {
        if (FrameView* view = frame->view())
            view->clear();
        result->setTransformSourceDocument(frame->document());
        frame->setDocument(result);
    }

    if (sourceIsDocument)
        result->setURL(ownerDocument->url());
    result->open();

    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create(sourceMIMEType);
    decoder->setEncoding(sourceEncoding.isEmpty() ? UTF8Encoding() : TextEncoding(sourceEncoding), TextResourceDecoder::EncodingFromXMLHeader);
    result->setDecoder(decoder.release());

    result->write(documentSource);
    result->finishParsing();
    result->close();

    return result.release();
}

static inline RefPtr<DocumentFragment> createFragmentFromSource(const String& sourceString, const String& sourceMIMEType, Document* outputDoc)
{
    RefPtr<DocumentFragment> fragment = DocumentFragment::create(outputDoc);

    if (sourceMIMEType == "text/html")
        parseHTMLDocumentFragment(sourceString, fragment.get());
    else if (sourceMIMEType == "text/plain")
        fragment->addChild(Text::create(outputDoc, sourceString));
    else {
        bool successfulParse = parseXMLDocumentFragment(sourceString, fragment.get(), outputDoc->documentElement());
        if (!successfulParse)
            return 0;
    }

    // FIXME: Do we need to mess with URLs here?

    return fragment;
}

PassRefPtr<Document> XSLTProcessor::transformToDocument(Node* sourceNode)
{
    String resultMIMEType;
    String resultString;
    String resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createDocumentFromSource(resultString, resultEncoding, resultMIMEType, sourceNode, 0);
}

PassRefPtr<DocumentFragment> XSLTProcessor::transformToFragment(Node* sourceNode, Document* outputDoc)
{
    String resultMIMEType;
    String resultString;
    String resultEncoding;

    // If the output document is HTML, default to HTML method.
    if (outputDoc->isHTMLDocument())
        resultMIMEType = "text/html";

    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createFragmentFromSource(resultString, resultMIMEType, outputDoc);
}

void XSLTProcessor::setParameter(const String& /*namespaceURI*/, const String& localName, const String& value)
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    m_parameters.set(localName, value);
}

String XSLTProcessor::getParameter(const String& /*namespaceURI*/, const String& localName) const
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    return m_parameters.get(localName);
}

void XSLTProcessor::removeParameter(const String& /*namespaceURI*/, const String& localName)
{
    // FIXME: namespace support?
    m_parameters.remove(localName);
}

void XSLTProcessor::reset()
{
    m_stylesheet.clear();
    m_stylesheetRootNode.clear();
    m_parameters.clear();
}

} // namespace WebCore

#endif // ENABLE(XSLT)
