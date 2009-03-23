/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaDocument.h"

#include "DocumentLoader.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "Text.h"
#include "XMLTokenizer.h"

namespace WebCore {

using namespace HTMLNames;

class MediaTokenizer : public Tokenizer {
public:
    MediaTokenizer(Document* doc) : m_doc(doc), m_mediaElement(0) {}
        
private:
    virtual void write(const SegmentedString&, bool appendData);
    virtual void stopParsing();
    virtual void finish();
    virtual bool isWaitingForScripts() const;
        
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char* data, int len);
        
    void createDocumentStructure();

    Document* m_doc;
    HTMLMediaElement* m_mediaElement;
};

void MediaTokenizer::write(const SegmentedString&, bool)
{
    ASSERT_NOT_REACHED();
}
    
void MediaTokenizer::createDocumentStructure()
{
    ExceptionCode ec;
    RefPtr<Element> rootElement = m_doc->createElement(htmlTag, false);
    m_doc->appendChild(rootElement, ec);
        
    RefPtr<Element> body = m_doc->createElement(bodyTag, false);
    body->setAttribute(styleAttr, "background-color: rgb(38,38,38);");

    rootElement->appendChild(body, ec);
        
    RefPtr<Element> mediaElement = m_doc->createElement(videoTag, false);
        
    m_mediaElement = static_cast<HTMLVideoElement*>(mediaElement.get());
    m_mediaElement->setAttribute(controlsAttr, "");
    m_mediaElement->setAttribute(autoplayAttr, "");
    m_mediaElement->setAttribute(styleAttr, "margin: auto; position: absolute; top: 0; right: 0; bottom: 0; left: 0;");

    m_mediaElement->setAttribute(nameAttr, "media");
    m_mediaElement->setSrc(m_doc->url());
    
    body->appendChild(mediaElement, ec);

    Frame* frame = m_doc->frame();
    if (!frame)
        return;

    frame->loader()->activeDocumentLoader()->mainResourceLoader()->setShouldBufferData(false);
}
    
bool MediaTokenizer::writeRawData(const char*, int)
{
    ASSERT(!m_mediaElement);
    if (m_mediaElement)
        return false;
        
    createDocumentStructure();
    finish();
    return false;
}
    
void MediaTokenizer::stopParsing()
{
    Tokenizer::stopParsing();        
}
    
void MediaTokenizer::finish()
{
    if (!m_parserStopped) 
        m_doc->finishedParsing();
}
    
bool MediaTokenizer::isWaitingForScripts() const
{
    // A media document is never waiting for scripts
    return false;
}
    
MediaDocument::MediaDocument(Frame* frame)
    : HTMLDocument(frame)
{
    setParseMode(Compat);
}

Tokenizer* MediaDocument::createTokenizer()
{
    return new MediaTokenizer(this);
}

void MediaDocument::defaultEventHandler(Event* event)
{
    // Match the default Quicktime plugin behavior to allow 
    // clicking and double-clicking to pause and play the media.
    Node* targetNode = event->target()->toNode();
    if (targetNode && targetNode->hasTagName(videoTag)) {
        HTMLVideoElement* video = static_cast<HTMLVideoElement*>(targetNode);
        if (event->type() == eventNames().clickEvent) {
            if (!video->canPlay()) {
                video->pause();
                event->setDefaultHandled();
            }
        } else if (event->type() == eventNames().dblclickEvent) {
            if (video->canPlay()) {
                video->play();
                event->setDefaultHandled();
            }
        }
    }
}

}
#endif
