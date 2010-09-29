/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLNames.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "RawDataDocumentParser.h"
#include "RenderEmbeddedObject.h"
#include "Settings.h"

namespace WebCore {
    
using namespace HTMLNames;

// FIXME: Share more code with MediaDocumentParser.
class PluginDocumentParser : public RawDataDocumentParser {
public:
    static PassRefPtr<PluginDocumentParser> create(PluginDocument* document)
    {
        return adoptRef(new PluginDocumentParser(document));
    }

    static Widget* pluginWidgetFromDocument(Document*);
    
private:
    PluginDocumentParser(Document* document)
        : RawDataDocumentParser(document)
        , m_embedElement(0)
    {
    }

    virtual void appendBytes(DocumentWriter*, const char*, int, bool);

    void createDocumentStructure();

    HTMLEmbedElement* m_embedElement;
};

Widget* PluginDocumentParser::pluginWidgetFromDocument(Document* doc)
{
    ASSERT(doc);
    RefPtr<Element> body = doc->body();
    if (body) {
        RefPtr<Node> node = body->firstChild();
        if (node && node->renderer()) {
            ASSERT(node->renderer()->isEmbeddedObject());
            return toRenderEmbeddedObject(node->renderer())->widget();
        }
    }
    return 0;
}

void PluginDocumentParser::createDocumentStructure()
{
    ExceptionCode ec;
    RefPtr<Element> rootElement = document()->createElement(htmlTag, false);
    document()->appendChild(rootElement, ec);

    if (document()->frame() && document()->frame()->loader())
        document()->frame()->loader()->dispatchDocumentElementAvailable();

    RefPtr<Element> body = document()->createElement(bodyTag, false);
    body->setAttribute(marginwidthAttr, "0");
    body->setAttribute(marginheightAttr, "0");
    body->setAttribute(bgcolorAttr, "rgb(38,38,38)");

    rootElement->appendChild(body, ec);
        
    RefPtr<Element> embedElement = document()->createElement(embedTag, false);
        
    m_embedElement = static_cast<HTMLEmbedElement*>(embedElement.get());
    m_embedElement->setAttribute(widthAttr, "100%");
    m_embedElement->setAttribute(heightAttr, "100%");
    
    m_embedElement->setAttribute(nameAttr, "plugin");
    m_embedElement->setAttribute(srcAttr, document()->url().string());
    m_embedElement->setAttribute(typeAttr, document()->frame()->loader()->writer()->mimeType());
    
    body->appendChild(embedElement, ec);    
}

void PluginDocumentParser::appendBytes(DocumentWriter*, const char*, int, bool)
{
    ASSERT(!m_embedElement);
    if (m_embedElement)
        return;

    createDocumentStructure();

    Frame* frame = document()->frame();
    if (!frame)
        return;
    Settings* settings = frame->settings();
    if (!settings || !frame->loader()->subframeLoader()->allowPlugins(NotAboutToInstantiatePlugin))
        return;

    document()->updateLayout();

    if (RenderPart* renderer = m_embedElement->renderPart()) {
        if (Widget* widget = renderer->widget()) {
            frame->loader()->client()->redirectDataToPlugin(widget);
            // In a plugin document, the main resource is the plugin. If we have a null widget, that means
            // the loading of the plugin was cancelled, which gives us a null mainResourceLoader(), so we
            // need to have this call in a null check of the widget or of mainResourceLoader().
            frame->loader()->activeDocumentLoader()->mainResourceLoader()->setShouldBufferData(false);
        }
    }

    finish();
}

PluginDocument::PluginDocument(Frame* frame, const KURL& url)
    : HTMLDocument(frame, url)
    , m_shouldLoadPluginManually(true)
{
    setCompatibilityMode(QuirksMode);
    lockCompatibilityMode();
}

PassRefPtr<DocumentParser> PluginDocument::createParser()
{
    return PluginDocumentParser::create(this);
}

Widget* PluginDocument::pluginWidget()
{
    return PluginDocumentParser::pluginWidgetFromDocument(this);
}

Node* PluginDocument::pluginNode()
{
    RefPtr<Element> body_element = body();
    if (body_element)
        return body_element->firstChild();

    return 0;
}

void PluginDocument::cancelManualPluginLoad()
{
    // PluginDocument::cancelManualPluginLoad should only be called once, but there are issues
    // with how many times we call beforeload on object elements. <rdar://problem/8441094>.
    if (!shouldLoadPluginManually())
        return;

    frame()->loader()->activeDocumentLoader()->mainResourceLoader()->cancel();
    setShouldLoadPluginManually(false);
}

}
