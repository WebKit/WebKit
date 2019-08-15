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
#include "MediaDocument.h"

#if ENABLE(VIDEO)

#include "Chrome.h"
#include "ChromeClient.h"
#include "CustomHeaderFields.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLBodyElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include "KeyboardEvent.h"
#include "NodeList.h"
#include "Page.h"
#include "RawDataDocumentParser.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIterator.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaDocument);

using namespace HTMLNames;

// FIXME: Share more code with PluginDocumentParser.
class MediaDocumentParser final : public RawDataDocumentParser {
public:
    static Ref<MediaDocumentParser> create(MediaDocument& document)
    {
        return adoptRef(*new MediaDocumentParser(document));
    }
    
private:
    MediaDocumentParser(MediaDocument& document)
        : RawDataDocumentParser { document }
        , m_outgoingReferrer { document.outgoingReferrer() }
    {
    }

    void appendBytes(DocumentWriter&, const char*, size_t) final;
    void createDocumentStructure();

    HTMLMediaElement* m_mediaElement { nullptr };
    String m_outgoingReferrer;
};
    
void MediaDocumentParser::createDocumentStructure()
{
    auto& document = *this->document();

    auto rootElement = HTMLHtmlElement::create(document);
    document.appendChild(rootElement);
    document.setCSSTarget(rootElement.ptr());
    rootElement->insertedByParser();

    if (document.frame())
        document.frame()->injectUserScripts(InjectAtDocumentStart);

#if PLATFORM(IOS_FAMILY)
    auto headElement = HTMLHeadElement::create(document);
    rootElement->appendChild(headElement);

    auto metaElement = HTMLMetaElement::create(document);
    metaElement->setAttributeWithoutSynchronization(nameAttr, AtomString("viewport", AtomString::ConstructFromLiteral));
    metaElement->setAttributeWithoutSynchronization(contentAttr, AtomString("width=device-width,initial-scale=1", AtomString::ConstructFromLiteral));
    headElement->appendChild(metaElement);
#endif

    auto body = HTMLBodyElement::create(document);
    rootElement->appendChild(body);

    auto videoElement = HTMLVideoElement::create(document);
    m_mediaElement = videoElement.ptr();
    videoElement->setAttributeWithoutSynchronization(controlsAttr, emptyAtom());
    videoElement->setAttributeWithoutSynchronization(autoplayAttr, emptyAtom());
    videoElement->setAttributeWithoutSynchronization(srcAttr, document.url().string());
    if (auto loader = makeRefPtr(document.loader()))
        videoElement->setAttributeWithoutSynchronization(typeAttr, loader->responseMIMEType());

    if (!RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled()) {
        StringBuilder elementStyle;
        elementStyle.appendLiteral("max-width: 100%; max-height: 100%;");
#if PLATFORM(IOS_FAMILY)
        elementStyle.appendLiteral("width: 100%; height: auto;");
#endif
        videoElement->setAttribute(styleAttr, elementStyle.toString());
    }

    body->appendChild(videoElement);

    RefPtr<Frame> frame = document.frame();
    if (!frame)
        return;

    frame->loader().activeDocumentLoader()->setMainResourceDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
    frame->loader().setOutgoingReferrer(document.completeURL(m_outgoingReferrer));
}

void MediaDocumentParser::appendBytes(DocumentWriter&, const char*, size_t)
{
    if (m_mediaElement)
        return;

    createDocumentStructure();
    finish();
}
    
MediaDocument::MediaDocument(PAL::SessionID sessionID, Frame* frame, const URL& url)
    : HTMLDocument(sessionID, frame, url, MediaDocumentClass)
    , m_replaceMediaElementTimer(*this, &MediaDocument::replaceMediaElementTimerFired)
{
    setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
    lockCompatibilityMode();
    if (frame)
        m_outgoingReferrer = frame->loader().outgoingReferrer();
}

MediaDocument::~MediaDocument()
{
    ASSERT(!m_replaceMediaElementTimer.isActive());
}

Ref<DocumentParser> MediaDocument::createParser()
{
    return MediaDocumentParser::create(*this);
}

static inline HTMLVideoElement* descendantVideoElement(ContainerNode& node)
{
    if (is<HTMLVideoElement>(node))
        return downcast<HTMLVideoElement>(&node);

    return descendantsOfType<HTMLVideoElement>(node).first();
}

static inline HTMLVideoElement* ancestorVideoElement(Node* node)
{
    while (node && !is<HTMLVideoElement>(*node))
        node = node->parentOrShadowHostNode();

    return downcast<HTMLVideoElement>(node);
}

void MediaDocument::defaultEventHandler(Event& event)
{
    // Modern media controls have their own event handling to determine when to
    // pause or resume playback.
    if (RuntimeEnabledFeatures::sharedFeatures().modernMediaControlsEnabled())
        return;
    
    // Match the default Quicktime plugin behavior to allow
    // clicking and double-clicking to pause and play the media.
    if (!is<Node>(event.target()))
        return;
    auto& targetNode = downcast<Node>(*event.target());

    if (auto video = makeRefPtr(ancestorVideoElement(&targetNode))) {
        if (event.type() == eventNames().clickEvent) {
            if (!video->canPlay()) {
                video->pause();
                event.setDefaultHandled();
            }
        } else if (event.type() == eventNames().dblclickEvent) {
            if (video->canPlay()) {
                video->play();
                event.setDefaultHandled();
            }
        }
    }

    if (!is<ContainerNode>(targetNode))
        return;
    auto& targetContainer = downcast<ContainerNode>(targetNode);

    if (event.type() == eventNames().keydownEvent && is<KeyboardEvent>(event)) {
        auto video = makeRefPtr(descendantVideoElement(targetContainer));
        if (!video)
            return;

        auto& keyboardEvent = downcast<KeyboardEvent>(event);
        if (keyboardEvent.keyIdentifier() == "U+0020") { // space
            if (video->paused()) {
                if (video->canPlay())
                    video->play();
            } else
                video->pause();
            keyboardEvent.setDefaultHandled();
        }
    }
}

void MediaDocument::mediaElementSawUnsupportedTracks()
{
    // The HTMLMediaElement was told it has something that the underlying 
    // MediaPlayer cannot handle so we should switch from <video> to <embed> 
    // and let the plugin handle this. Don't do it immediately as this 
    // function may be called directly from a media engine callback, and 
    // replaceChild will destroy the element, media player, and media engine.
    m_replaceMediaElementTimer.startOneShot(0_s);
}

void MediaDocument::replaceMediaElementTimerFired()
{
    auto htmlBody = makeRefPtr(bodyOrFrameset());
    if (!htmlBody)
        return;

    // Set body margin width and height to 0 as that is what a PluginDocument uses.
    htmlBody->setAttributeWithoutSynchronization(marginwidthAttr, AtomString("0", AtomString::ConstructFromLiteral));
    htmlBody->setAttributeWithoutSynchronization(marginheightAttr, AtomString("0", AtomString::ConstructFromLiteral));

    if (auto videoElement = makeRefPtr(descendantVideoElement(*htmlBody))) {
        auto embedElement = HTMLEmbedElement::create(*this);

        embedElement->setAttributeWithoutSynchronization(widthAttr, AtomString("100%", AtomString::ConstructFromLiteral));
        embedElement->setAttributeWithoutSynchronization(heightAttr, AtomString("100%", AtomString::ConstructFromLiteral));
        embedElement->setAttributeWithoutSynchronization(nameAttr, AtomString("plugin", AtomString::ConstructFromLiteral));
        embedElement->setAttributeWithoutSynchronization(srcAttr, url().string());

        ASSERT(loader());
        if (auto loader = makeRefPtr(this->loader()))
            embedElement->setAttributeWithoutSynchronization(typeAttr, loader->writer().mimeType());

        videoElement->parentNode()->replaceChild(embedElement, *videoElement);
    }
}

void MediaDocument::mediaElementNaturalSizeChanged(const IntSize& newSize)
{
    if (ownerElement())
        return;

    if (newSize.isZero())
        return;

    if (page())
        page()->chrome().client().imageOrMediaDocumentSizeChanged(newSize);
}

}

#endif
