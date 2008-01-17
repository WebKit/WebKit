/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginDocument.h"

#include "DocumentLoader.h"
#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLNames.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "RenderWidget.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "Text.h"
#include "XMLTokenizer.h"

namespace WebCore {
    
using namespace HTMLNames;
    
class PluginTokenizer : public Tokenizer {
public:
    PluginTokenizer(Document* doc) : m_doc(doc), m_embedElement(0) {}
        
    virtual bool write(const SegmentedString&, bool appendData);
    virtual void stopParsing();
    virtual void finish();
    virtual bool isWaitingForScripts() const;
        
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char* data, int len);
        
    void createDocumentStructure();
private:
    Document* m_doc;
    HTMLEmbedElement* m_embedElement;
};
    
bool PluginTokenizer::write(const SegmentedString& s, bool appendData)
{
    ASSERT_NOT_REACHED();
    return false;
}
    
void PluginTokenizer::createDocumentStructure()
{
    ExceptionCode ec;
    RefPtr<Element> rootElement = m_doc->createElementNS(xhtmlNamespaceURI, "html", ec);
    m_doc->appendChild(rootElement, ec);
        
    RefPtr<Element> body = m_doc->createElementNS(xhtmlNamespaceURI, "body", ec);
    body->setAttribute(marginwidthAttr, "0");
    body->setAttribute(marginheightAttr, "0");
    body->setAttribute(bgcolorAttr, "rgb(38,38,38)");

    rootElement->appendChild(body, ec);
        
    RefPtr<Element> embedElement = m_doc->createElementNS(xhtmlNamespaceURI, "embed", ec);
        
    m_embedElement = static_cast<HTMLEmbedElement*>(embedElement.get());
    m_embedElement->setAttribute(widthAttr, "100%");
    m_embedElement->setAttribute(heightAttr, "100%");
    
    m_embedElement->setAttribute(nameAttr, "plugin");
    m_embedElement->setSrc(m_doc->url());
    m_embedElement->setType(m_doc->frame()->loader()->responseMIMEType());
    
    body->appendChild(embedElement, ec);    
}
    
bool PluginTokenizer::writeRawData(const char* data, int len)
{
    if (!m_embedElement) {
        createDocumentStructure();

        if (Frame* frame = m_doc->frame()) {
            Settings* settings = frame->settings();
            if (settings && settings->arePluginsEnabled()) {
                m_doc->updateLayout();
            
                if (RenderWidget* renderer = static_cast<RenderWidget*>(m_embedElement->renderer())) {
                    frame->loader()->client()->redirectDataToPlugin(renderer->widget());
                    frame->loader()->activeDocumentLoader()->mainResourceLoader()->setShouldBufferData(false);
                }
            
                finish();
            }
        }
        
        return false;
    }
    
    ASSERT_NOT_REACHED();
    
    return false;
}
    
void PluginTokenizer::stopParsing()
{
    Tokenizer::stopParsing();        
}
    
void PluginTokenizer::finish()
{
    if (!m_parserStopped) 
        m_doc->finishedParsing();            
}
    
bool PluginTokenizer::isWaitingForScripts() const
{
    // A plugin document is never waiting for scripts
    return false;
}
    
PluginDocument::PluginDocument(DOMImplementation* implementation, Frame* frame)
    : HTMLDocument(implementation, frame)
{
    setParseMode(Compat);
}
    
Tokenizer* PluginDocument::createTokenizer()
{
    return new PluginTokenizer(this);
}
    
}
