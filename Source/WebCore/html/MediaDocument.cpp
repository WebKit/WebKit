/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "DocumentLoader.h"
#include "DocumentSettingsValues.h"
#include "DocumentView.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "HTMLBodyElement.h"
#include "HTMLEmbedElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include "KeyboardEvent.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "MouseEvent.h"
#include "NodeList.h"
#include "Page.h"
#include "RawDataDocumentParser.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserScriptTypes.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaDocument);

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

    void appendBytes(DocumentWriter&, std::span<const uint8_t>) final;
    void createDocumentStructure();

    WeakPtr<HTMLMediaElement> m_mediaElement;
    String m_outgoingReferrer;
};
    
void MediaDocumentParser::createDocumentStructure()
{
    Ref document = *this->document();

    Ref rootElement = HTMLHtmlElement::create(document);
    document->appendChild(rootElement);
    document->setCSSTarget(rootElement.ptr());

    if (RefPtr frame = document->frame())
        frame->injectUserScripts(UserScriptInjectionTime::DocumentStart);

#if PLATFORM(IOS_FAMILY)
    Ref headElement = HTMLHeadElement::create(document);
    rootElement->appendChild(headElement);

    Ref metaElement = HTMLMetaElement::create(document);
    metaElement->setAttributeWithoutSynchronization(nameAttr, "viewport"_s);
    metaElement->setAttributeWithoutSynchronization(contentAttr, "width=device-width,initial-scale=1"_s);
    headElement->appendChild(metaElement);
#endif

    Ref body = HTMLBodyElement::create(document);
    rootElement->appendChild(body);

    Ref videoElement = HTMLVideoElement::create(document);
    m_mediaElement = videoElement.get();
    videoElement->setAttributeWithoutSynchronization(controlsAttr, emptyAtom());
    videoElement->setAttributeWithoutSynchronization(autoplayAttr, emptyAtom());
    videoElement->setAttributeWithoutSynchronization(srcAttr, AtomString { document->url().string() });
    if (RefPtr loader = document->loader())
        videoElement->setAttributeWithoutSynchronization(typeAttr, AtomString { loader->responseMIMEType() });

    body->appendChild(videoElement);
    document->setHasVisuallyNonEmptyCustomContent();

    RefPtr frame = document->frame();
    if (!frame)
        return;

    protect(frame->loader().activeDocumentLoader())->setMainResourceDataBufferingPolicy(DataBufferingPolicy::DoNotBufferData);
    frame->loader().setOutgoingReferrer(document->completeURL(m_outgoingReferrer));
}

void MediaDocumentParser::appendBytes(DocumentWriter&, std::span<const uint8_t>)
{
    if (m_mediaElement)
        return;

    createDocumentStructure();
    finish();
}
    
MediaDocument::MediaDocument(LocalFrame* frame, const Settings& settings, const URL& url)
    : HTMLDocument(frame, settings, url, { }, { DocumentClass::Media })
{
    setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);
    lockCompatibilityMode();
    if (frame)
        m_outgoingReferrer = frame->loader().outgoingReferrer();
}

MediaDocument::~MediaDocument() = default;

Ref<DocumentParser> MediaDocument::createParser()
{
    return MediaDocumentParser::create(*this);
}

static inline HTMLVideoElement* descendantVideoElement(ContainerNode& node)
{
    if (auto* video = dynamicDowncast<HTMLVideoElement>(node))
        return video;

    return descendantsOfType<HTMLVideoElement>(node).first();
}

void MediaDocument::replaceMediaElementTimerFired()
{
    RefPtr htmlBody = bodyOrFrameset();
    if (!htmlBody)
        return;

    // Set body margin width and height to 0 as that is what a PluginDocument uses.
    htmlBody->setAttributeWithoutSynchronization(marginwidthAttr, "0"_s);
    htmlBody->setAttributeWithoutSynchronization(marginheightAttr, "0"_s);

    if (RefPtr videoElement = descendantVideoElement(*htmlBody)) {
        Ref embedElement = HTMLEmbedElement::create(*this);

        embedElement->setAttributeWithoutSynchronization(widthAttr, "100%"_s);
        embedElement->setAttributeWithoutSynchronization(heightAttr, "100%"_s);
        embedElement->setAttributeWithoutSynchronization(nameAttr, "plugin"_s);
        embedElement->setAttributeWithoutSynchronization(srcAttr, AtomString { url().string() });

        ASSERT(loader());
        if (RefPtr loader = this->loader())
            embedElement->setAttributeWithoutSynchronization(typeAttr, AtomString { loader->writer().mimeType() });

        protect(videoElement->parentNode())->replaceChild(embedElement, *videoElement);
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
