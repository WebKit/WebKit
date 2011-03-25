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

#if ENABLE(XSLT)
#include "XMLTreeViewer.h"


#include "Base64.h"
#include "Element.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Page.h"
#include "ProcessingInstruction.h"
#include "Settings.h"
#include "TransformSource.h"
#include "XLinkNames.h"
#include "XMLViewerXSL.h"
#include "XPathResult.h"
#include "XSLStyleSheet.h"
#include "XSLTProcessor.h"

#include <libxslt/xslt.h>

#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif
#if ENABLE(SVG)
#include "SVGNames.h"
#endif
#if ENABLE(WML)
#include "WMLNames.h"
#endif

using namespace std;

namespace WebCore {

XMLTreeViewer::XMLTreeViewer(Document* document)
    : m_document(document)
{
}

bool XMLTreeViewer::hasNoStyleInformation() const
{
    if (m_document->sawElementsInKnownNamespaces() || m_document->transformSourceDocument())
        return false;

    if (!m_document->frame() || !m_document->frame()->page())
        return false;

    if (!m_document->frame()->page()->settings()->developerExtrasEnabled())
        return false;

    if (m_document->frame()->tree()->parent(true))
        return false; // This document is not in a top frame

    if (m_document->frame()->loader()->opener())
        return false; // This document is not opened manually by user
    return true;
}

void XMLTreeViewer::transformDocumentToTreeView()
{
    String sheetString(reinterpret_cast<const char*>(XMLViewer_xsl), sizeof(XMLViewer_xsl));
    RefPtr<XSLStyleSheet> styleSheet = XSLStyleSheet::createForXMLTreeViewer(m_document, sheetString);

    RefPtr<XSLTProcessor> processor = XSLTProcessor::create();
    processor->setXSLStyleSheet(styleSheet);

    processor->setParameter("", "xml_has_no_style_message", "This XML file does not appear to have any style information associated with it. The document tree is shown below.");

    String resultMIMEType;
    String newSource;
    String resultEncoding;

    Frame* frame = m_document->frame();
    // FIXME: We should introduce error handling
    if (processor->transformToString(m_document, resultMIMEType, newSource, resultEncoding))
        processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, m_document, frame);

    // Adding source xml for dealing with namespaces and CDATA issues and for extensions use.
    Element* sourceXmlElement = frame->document()->getElementById(AtomicString("source-xml"));
    if (sourceXmlElement)
        m_document->cloneChildNodes(sourceXmlElement);

    // New document should have been loaded in frame. Tell it to use view source styles.
    frame->document()->setUsesViewSourceStyles(true);
    frame->document()->styleSelectorChanged(RecalcStyleImmediately);

}

} // namespace WebCore

#endif // ENABLE(XSLT)
