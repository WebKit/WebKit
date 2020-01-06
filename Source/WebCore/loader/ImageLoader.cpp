/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "ImageLoader.h"

#include "BitmapImage.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "EventSender.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "InspectorInstrumentation.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderSVGImage.h"
#include <wtf/NeverDestroyed.h>

#if ENABLE(VIDEO)
#include "RenderVideo.h"
#endif

#if ASSERT_ENABLED
// ImageLoader objects are allocated as members of other objects, so generic pointer check would always fail.
namespace WTF {

template<> struct ValueCheck<WebCore::ImageLoader*> {
    typedef WebCore::ImageLoader* TraitType;
    static void checkConsistency(const WebCore::ImageLoader* p)
    {
        if (!p)
            return;
        ValueCheck<WebCore::Element*>::checkConsistency(&p->element());
    }
};

} // namespace WTF
#endif // ASSERT_ENABLED

namespace WebCore {

static ImageEventSender& beforeLoadEventSender()
{
    static NeverDestroyed<ImageEventSender> sender(eventNames().beforeloadEvent);
    return sender;
}

static ImageEventSender& loadEventSender()
{
    static NeverDestroyed<ImageEventSender> sender(eventNames().loadEvent);
    return sender;
}

static ImageEventSender& errorEventSender()
{
    static NeverDestroyed<ImageEventSender> sender(eventNames().errorEvent);
    return sender;
}

static inline bool pageIsBeingDismissed(Document& document)
{
    Frame* frame = document.frame();
    return frame && frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None;
}

ImageLoader::ImageLoader(Element& element)
    : m_element(element)
    , m_image(nullptr)
    , m_derefElementTimer(*this, &ImageLoader::timerFired)
    , m_hasPendingBeforeLoadEvent(false)
    , m_hasPendingLoadEvent(false)
    , m_hasPendingErrorEvent(false)
    , m_imageComplete(true)
    , m_loadManually(false)
    , m_elementIsProtected(false)
{
}

ImageLoader::~ImageLoader()
{
    if (m_image)
        m_image->removeClient(*this);

    ASSERT(m_hasPendingBeforeLoadEvent || !beforeLoadEventSender().hasPendingEvents(*this));
    if (m_hasPendingBeforeLoadEvent)
        beforeLoadEventSender().cancelEvent(*this);

    ASSERT(m_hasPendingLoadEvent || !loadEventSender().hasPendingEvents(*this));
    if (m_hasPendingLoadEvent)
        loadEventSender().cancelEvent(*this);

    ASSERT(m_hasPendingErrorEvent || !errorEventSender().hasPendingEvents(*this));
    if (m_hasPendingErrorEvent)
        errorEventSender().cancelEvent(*this);
}

void ImageLoader::clearImage()
{
    clearImageWithoutConsideringPendingLoadEvent();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::clearImageWithoutConsideringPendingLoadEvent()
{
    ASSERT(m_failedLoadURL.isEmpty());
    CachedImage* oldImage = m_image.get();
    if (oldImage) {
        m_image = nullptr;
        if (m_hasPendingBeforeLoadEvent) {
            beforeLoadEventSender().cancelEvent(*this);
            m_hasPendingBeforeLoadEvent = false;
        }
        if (m_hasPendingLoadEvent) {
            loadEventSender().cancelEvent(*this);
            m_hasPendingLoadEvent = false;
        }
        if (m_hasPendingErrorEvent) {
            errorEventSender().cancelEvent(*this);
            m_hasPendingErrorEvent = false;
        }
        m_imageComplete = true;
        if (oldImage)
            oldImage->removeClient(*this);
    }

    if (RenderImageResource* imageResource = renderImageResource())
        imageResource->resetAnimation();
}

void ImageLoader::updateFromElement()
{
    // If we're not making renderers for the page, then don't load images. We don't want to slow
    // down the raw HTML parsing case by loading images we don't intend to display.
    Document& document = element().document();
    if (!document.hasLivingRenderTree())
        return;

    AtomString attr = element().imageSourceURL();

    // Avoid loading a URL we already failed to load.
    if (!m_failedLoadURL.isEmpty() && attr == m_failedLoadURL)
        return;

    // Do not load any image if the 'src' attribute is missing or if it is
    // an empty string.
    CachedResourceHandle<CachedImage> newImage = nullptr;
    if (!attr.isNull() && !stripLeadingAndTrailingHTMLSpaces(attr).isEmpty()) {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.contentSecurityPolicyImposition = element().isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
        options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

        auto crossOriginAttribute = element().attributeWithoutSynchronization(HTMLNames::crossoriginAttr);

        ResourceRequest resourceRequest(document.completeURL(sourceURI(attr)));
        resourceRequest.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(m_element));

        auto request = createPotentialAccessControlRequest(WTFMove(resourceRequest), WTFMove(options), document, crossOriginAttribute);
        request.setInitiator(element());

        if (m_loadManually) {
            bool autoLoadOtherImages = document.cachedResourceLoader().autoLoadImages();
            document.cachedResourceLoader().setAutoLoadImages(false);
            auto* page = m_element.document().page();
            newImage = new CachedImage(WTFMove(request), page->sessionID(), &page->cookieJar());
            newImage->setStatus(CachedResource::Pending);
            newImage->setLoading(true);
            document.cachedResourceLoader().m_documentResources.set(newImage->url(), newImage.get());
            document.cachedResourceLoader().setAutoLoadImages(autoLoadOtherImages);
        } else
            newImage = document.cachedResourceLoader().requestImage(WTFMove(request)).value_or(nullptr);

        // If we do not have an image here, it means that a cross-site
        // violation occurred, or that the image was blocked via Content
        // Security Policy, or the page is being dismissed. Trigger an
        // error event if the page is not being dismissed.
        if (!newImage && !pageIsBeingDismissed(document)) {
            m_failedLoadURL = attr;
            m_hasPendingErrorEvent = true;
            errorEventSender().dispatchEventSoon(*this);
        } else
            clearFailedLoadURL();
    } else if (!attr.isNull()) {
        // Fire an error event if the url is empty.
        m_failedLoadURL = attr;
        m_hasPendingErrorEvent = true;
        errorEventSender().dispatchEventSoon(*this);
    }

    CachedImage* oldImage = m_image.get();
    if (newImage != oldImage) {
        if (m_hasPendingBeforeLoadEvent) {
            beforeLoadEventSender().cancelEvent(*this);
            m_hasPendingBeforeLoadEvent = false;
        }
        if (m_hasPendingLoadEvent) {
            loadEventSender().cancelEvent(*this);
            m_hasPendingLoadEvent = false;
        }

        // Cancel error events that belong to the previous load, which is now cancelled by changing the src attribute.
        // If newImage is null and m_hasPendingErrorEvent is true, we know the error event has been just posted by
        // this load and we should not cancel the event.
        // FIXME: If both previous load and this one got blocked with an error, we can receive one error event instead of two.
        if (m_hasPendingErrorEvent && newImage) {
            errorEventSender().cancelEvent(*this);
            m_hasPendingErrorEvent = false;
        }

        m_image = newImage;
        m_hasPendingBeforeLoadEvent = !document.isImageDocument() && newImage;
        m_hasPendingLoadEvent = newImage;
        m_imageComplete = !newImage;

        if (newImage) {
            if (!document.isImageDocument()) {
                if (!document.hasListenerType(Document::BEFORELOAD_LISTENER))
                    dispatchPendingBeforeLoadEvent();
                else
                    beforeLoadEventSender().dispatchEventSoon(*this);
            } else
                updateRenderer();

            // If newImage is cached, addClient() will result in the load event
            // being queued to fire. Ensure this happens after beforeload is
            // dispatched.
            newImage->addClient(*this);
        }
        if (oldImage) {
            oldImage->removeClient(*this);
            updateRenderer();
        }
    }

    if (RenderImageResource* imageResource = renderImageResource())
        imageResource->resetAnimation();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::updateFromElementIgnoringPreviousError()
{
    clearFailedLoadURL();
    updateFromElement();
}

static inline void resolvePromises(Vector<RefPtr<DeferredPromise>>& promises)
{
    ASSERT(!promises.isEmpty());
    auto promisesToBeResolved = std::exchange(promises, { });
    for (auto& promise : promisesToBeResolved)
        promise->resolve();
}

static inline void rejectPromises(Vector<RefPtr<DeferredPromise>>& promises, const char* message)
{
    ASSERT(!promises.isEmpty());
    auto promisesToBeRejected = std::exchange(promises, { });
    for (auto& promise : promisesToBeRejected)
        promise->reject(Exception { EncodingError, message });
}

inline void ImageLoader::resolveDecodePromises()
{
    resolvePromises(m_decodingPromises);
}

inline void ImageLoader::rejectDecodePromises(const char* message)
{
    rejectPromises(m_decodingPromises, message);
}

void ImageLoader::notifyFinished(CachedResource& resource)
{
    ASSERT(m_failedLoadURL.isEmpty());
    ASSERT_UNUSED(resource, &resource == m_image.get());

    m_imageComplete = true;
    if (!hasPendingBeforeLoadEvent())
        updateRenderer();

    if (!m_hasPendingLoadEvent)
        return;

    if (m_image->resourceError().isAccessControl()) {
        URL imageURL = m_image->url();

        clearImageWithoutConsideringPendingLoadEvent();

        m_hasPendingErrorEvent = true;
        errorEventSender().dispatchEventSoon(*this);

        auto message = makeString("Cannot load image ", imageURL.string(), " due to access control checks.");
        element().document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);

        if (hasPendingDecodePromises())
            rejectDecodePromises("Access control error.");
        
        ASSERT(!m_hasPendingLoadEvent);

        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }

    if (m_image->wasCanceled()) {
        if (hasPendingDecodePromises())
            rejectDecodePromises("Loading was canceled.");
        m_hasPendingLoadEvent = false;
        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }

    if (hasPendingDecodePromises())
        decode();
    loadEventSender().dispatchEventSoon(*this);
}

RenderImageResource* ImageLoader::renderImageResource()
{
    auto* renderer = element().renderer();
    if (!renderer)
        return nullptr;

    // We don't return style generated image because it doesn't belong to the ImageLoader.
    // See <https://bugs.webkit.org/show_bug.cgi?id=42840>
    if (is<RenderImage>(*renderer) && !downcast<RenderImage>(*renderer).isGeneratedContent())
        return &downcast<RenderImage>(*renderer).imageResource();

    if (is<RenderSVGImage>(*renderer))
        return &downcast<RenderSVGImage>(*renderer).imageResource();

#if ENABLE(VIDEO)
    if (is<RenderVideo>(*renderer))
        return &downcast<RenderVideo>(*renderer).imageResource();
#endif

    return nullptr;
}

void ImageLoader::updateRenderer()
{
    RenderImageResource* imageResource = renderImageResource();

    if (!imageResource)
        return;

    // Only update the renderer if it doesn't have an image or if what we have
    // is a complete image. This prevents flickering in the case where a dynamic
    // change is happening between two images.
    CachedImage* cachedImage = imageResource->cachedImage();
    if (m_image != cachedImage && (m_imageComplete || !cachedImage))
        imageResource->setCachedImage(m_image.get());
}

void ImageLoader::updatedHasPendingEvent()
{
    // If an Element that does image loading is removed from the DOM the load/error event for the image is still observable.
    // As long as the ImageLoader is actively loading, the Element itself needs to be ref'ed to keep it from being
    // destroyed by DOM manipulation or garbage collection.
    // If such an Element wishes for the load to stop when removed from the DOM it needs to stop the ImageLoader explicitly.
    bool wasProtected = m_elementIsProtected;
    m_elementIsProtected = m_hasPendingLoadEvent || m_hasPendingErrorEvent;
    if (wasProtected == m_elementIsProtected)
        return;

    if (m_elementIsProtected) {
        if (m_derefElementTimer.isActive())
            m_derefElementTimer.stop();
        else
            m_protectedElement = &element();
    } else {
        ASSERT(!m_derefElementTimer.isActive());
        m_derefElementTimer.startOneShot(0_s);
    }   
}

void ImageLoader::decode(Ref<DeferredPromise>&& promise)
{
    m_decodingPromises.append(WTFMove(promise));

    if (!element().document().domWindow()) {
        rejectDecodePromises("Inactive document.");
        return;
    }

    AtomString attr = element().imageSourceURL();
    if (stripLeadingAndTrailingHTMLSpaces(attr).isEmpty()) {
        rejectDecodePromises("Missing source URL.");
        return;
    }

    if (m_imageComplete)
        decode();
}

void ImageLoader::decode()
{
    ASSERT(hasPendingDecodePromises());
    
    if (!element().document().domWindow()) {
        rejectDecodePromises("Inactive document.");
        return;
    }

    if (!m_image || !m_image->image() || m_image->errorOccurred()) {
        rejectDecodePromises("Loading error.");
        return;
    }

    Image* image = m_image->image();
    if (!is<BitmapImage>(image)) {
        resolveDecodePromises();
        return;
    }

    auto& bitmapImage = downcast<BitmapImage>(*image);
    bitmapImage.decode([promises = WTFMove(m_decodingPromises)]() mutable {
        resolvePromises(promises);
    });
}

void ImageLoader::timerFired()
{
    m_protectedElement = nullptr;
}

void ImageLoader::dispatchPendingEvent(ImageEventSender* eventSender)
{
    ASSERT(eventSender == &beforeLoadEventSender() || eventSender == &loadEventSender() || eventSender == &errorEventSender());
    const AtomString& eventType = eventSender->eventType();
    if (eventType == eventNames().beforeloadEvent)
        dispatchPendingBeforeLoadEvent();
    if (eventType == eventNames().loadEvent)
        dispatchPendingLoadEvent();
    if (eventType == eventNames().errorEvent)
        dispatchPendingErrorEvent();
}

void ImageLoader::dispatchPendingBeforeLoadEvent()
{
    if (!m_hasPendingBeforeLoadEvent)
        return;
    if (!m_image)
        return;
    if (!element().document().hasLivingRenderTree())
        return;
    m_hasPendingBeforeLoadEvent = false;
    Ref<Document> originalDocument = element().document();
    if (element().dispatchBeforeLoadEvent(m_image->url())) {
        bool didEventListenerDisconnectThisElement = !element().isConnected() || &element().document() != originalDocument.ptr();
        if (didEventListenerDisconnectThisElement)
            return;
        
        updateRenderer();
        return;
    }
    if (m_image) {
        m_image->removeClient(*this);
        m_image = nullptr;
    }

    loadEventSender().cancelEvent(*this);
    m_hasPendingLoadEvent = false;
    
    if (is<HTMLObjectElement>(element()))
        downcast<HTMLObjectElement>(element()).renderFallbackContent();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingLoadEvent()
{
    if (!m_hasPendingLoadEvent)
        return;
    if (!m_image)
        return;
    m_hasPendingLoadEvent = false;
    if (element().document().hasLivingRenderTree())
        dispatchLoadEvent();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingErrorEvent()
{
    if (!m_hasPendingErrorEvent)
        return;
    m_hasPendingErrorEvent = false;
    if (element().document().hasLivingRenderTree())
        element().dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingBeforeLoadEvents()
{
    beforeLoadEventSender().dispatchPendingEvents();
}

void ImageLoader::dispatchPendingLoadEvents()
{
    loadEventSender().dispatchPendingEvents();
}

void ImageLoader::dispatchPendingErrorEvents()
{
    errorEventSender().dispatchPendingEvents();
}

void ImageLoader::elementDidMoveToNewDocument()
{
    clearFailedLoadURL();
    clearImage();
}

inline void ImageLoader::clearFailedLoadURL()
{
    m_failedLoadURL = nullAtom();
}

}
