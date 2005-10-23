/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <qstring.h>

#include "loader_client.h"
#include "khtmlview.h"

#include "dom_kdomdocumentwrapper.h"
#include "dom_kdomnodetreewrapper.h"
#include "dom_nodeimpl.h"

#include "xml_tokenizer.h"
#include "xml_kdomtokenizer.h"

#include "kdom/core/DOMConfigurationImpl.h"
#include "kdom/backends/libxml/LibXMLParser.h"
#include "kdom/core/DocumentImpl.h"

#include "ksvg2/svg/SVGDocumentImpl.h"
#include "ksvg2/misc/KSVGDocumentBuilder.h"


namespace DOM {
    class KDOMDocumentWrapperImpl;
};

namespace khtml {

class TokenizerString;
class KDOMTokenizer : public Tokenizer, public CachedObjectClient
{
public:
    KDOMTokenizer(DOM::DocumentPtr *, KHTMLView * = 0);
    ~KDOMTokenizer();

    // from Tokenizer
    virtual void write(const TokenizerString &str, bool);
    virtual void finish();
    virtual void setOnHold(bool onHold);
    virtual bool isWaitingForScripts() const;

private:
    DOM::KDOMDocumentWrapperImpl *documentWrapper();

    DOM::DocumentPtr *m_doc;
    KHTMLView *m_view;

    QString m_xmlCode;
};

}; // namespace


using namespace khtml;
using namespace KSVG;
using namespace DOM;

// --------------------------------

KDOMTokenizer::KDOMTokenizer(DOM::DocumentPtr *_doc, KHTMLView *_view)
    : m_doc(_doc), m_view(_view)
{
    if (m_doc)
        m_doc->ref();
    
    //FIXME: KDOMTokenizer should use this in a fashion similiar to how
    //HTMLTokenizer uses loadStopped, in the future.
    m_parserStopped = false;
}

KDOMTokenizer::~KDOMTokenizer()
{
    if (m_doc)
        m_doc->deref();
}

KDOMDocumentWrapperImpl *KDOMTokenizer::documentWrapper()
{
    return m_doc ? static_cast<KDOMDocumentWrapperImpl *>(m_doc->document()) : NULL;
}

void KDOMTokenizer::write(const TokenizerString &s, bool /*appendData*/ )
{
    m_xmlCode += s.toString();
}

void KDOMTokenizer::setOnHold(bool onHold)
{
    // Will we need to implement this when we do incremental XML parsing?
}

void KDOMTokenizer::finish()
{
    KSVG::DocumentBuilder *builder = new KSVG::DocumentBuilder(documentWrapper()->svgView());
    KDOM::LibXMLParser *parser = new KDOM::LibXMLParser(KURL());
    parser->setDocumentBuilder(builder);
    
    parser->domConfig()->setParameter(KDOM::ENTITIES.handle(), false);
    parser->domConfig()->setParameter(KDOM::ELEMENT_CONTENT_WHITESPACE.handle(), false);
    
    // Feed the parser the whole document (hack)
    parser->doOneShotParse(m_xmlCode.utf8(), m_xmlCode.length());
    
    SVGDocumentImpl *svgDoc = static_cast<SVGDocumentImpl *>(parser->document());
    NodeImpl *wrappedDoc = new KDOMNodeTreeWrapperImpl(m_doc, svgDoc);
    delete parser; // builder is owned (deleted) by parser
    
    if (documentWrapper()->addChild(wrappedDoc)) {
        KCanvasView *primaryView = documentWrapper()->svgView()->canvasView();
        svgDoc->setCanvasView(primaryView);
        if (m_view && !wrappedDoc->attached())
            wrappedDoc->attach();
    }
    else
        delete wrappedDoc;

    emit finishedParsing();
}


bool KDOMTokenizer::isWaitingForScripts() const
{
    return false;
}

namespace khtml {

Tokenizer *newKDOMTokenizer(DOM::DocumentPtr *d, KHTMLView *v)
{
    return new KDOMTokenizer(d, v);
}

};
