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
#include "Chrome.h"
#include "ChromeClient.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "EventSender.h"
#include "FrameLoader.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLPlugInElement.h"
#include "HTMLSrcsetParser.h"
#include "InspectorInstrumentation.h"
#include "JSDOMPromiseDeferred.h"
#include "LazyLoadImageObserver.h"
#include "LegacyRenderSVGImage.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderSVGImage.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

#if ENABLE(VIDEO)
#include "RenderVideo.h"
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
#include "SpatialImageControls.h"
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

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, LazyImageLoadState state)
{
    switch (state) {
    case LazyImageLoadState::None: ts << "None"; break;
    case LazyImageLoadState::Deferred: ts << "Deferred"; break;
    case LazyImageLoadState::LoadImmediately: ts << "LoadImmediately"; break;
    case LazyImageLoadState::FullImage: ts << "FullImage"; break;
    }

    return ts;
}

static TextStream& operator<<(TextStream& ts, ImageLoading loading)
{
    switch (loading) {
    case ImageLoading::Immediate: ts << "Immediate"; break;
    case ImageLoading::DeferredUntilVisible: ts << "DeferredUntilVisible"; break;
    }

    return ts;
}
#endif // !LOG_DISABLED

static ImageEventSender& loadEventSender()
{
    static NeverDestroyed<ImageEventSender> sender;
    return sender;
}

static inline bool pageIsBeingDismissed(Document& document)
{
    auto* frame = document.frame();
    return frame && frame->loader().pageDismissalEventBeingDispatched() != FrameLoader::PageDismissalType::None;
}

// https://html.spec.whatwg.org/multipage/images.html#updating-the-image-data:list-of-available-images
static bool canReuseFromListOfAvailableImages(const CachedResourceRequest& request, Document& document)
{
    CachedResourceHandle resource = MemoryCache::singleton().resourceForRequest(request.resourceRequest(), document.page()->sessionID());
    if (!resource || resource->stillNeedsLoad() || resource->isPreloaded())
        return false;

    if (resource->options().mode == FetchOptions::Mode::Cors && !document.protectedSecurityOrigin()->isSameOriginAs(*resource->origin()))
        return false;

    if (resource->options().mode != request.options().mode || resource->options().credentials != request.options().credentials)
        return false;

    return true;
}

ImageLoader::ImageLoader(Element& element)
    : m_element(element)
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
    if (CachedResourceHandle image = m_image)
        image->removeClient(*this);

    ASSERT(m_hasPendingLoadEvent || m_hasPendingErrorEvent || !loadEventSender().hasPendingEvents(*this));
    if (m_hasPendingLoadEvent || m_hasPendingErrorEvent)
        loadEventSender().cancelEvent(*this);
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
    LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " clearImageWithoutConsideringPendingLoadEvent");

    ASSERT(m_failedLoadURL.isEmpty());
    if (CachedResourceHandle oldImage = m_image) {
        m_image = nullptr;
        m_hasPendingBeforeLoadEvent = false;
        if (m_hasPendingLoadEvent || m_hasPendingErrorEvent) {
            loadEventSender().cancelEvent(*this);
            m_hasPendingLoadEvent = m_hasPendingErrorEvent = false;
        }
        m_imageComplete = true;
        oldImage->removeClient(*this);
    }

    if (CheckedPtr imageResource = renderImageResource())
        imageResource->resetAnimation();
}

void ImageLoader::updateFromElement(RelevantMutation relevantMutation)
{
    // If we're not making renderers for the page, then don't load images. We don't want to slow
    // down the raw HTML parsing case by loading images we don't intend to display.
    Ref document = element().document();
    if (!document->hasLivingRenderTree())
        return;

    AtomString attr = protectedElement()->imageSourceURL();

    LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " updateFromElement, current URL is " << attr);

    // Avoid loading a URL we already failed to load.
    if (!m_failedLoadURL.isEmpty() && attr == m_failedLoadURL)
        return;

    // Do not load any image if the 'src' attribute is missing or if it is
    // an empty string.
    CachedResourceHandle<CachedImage> newImage;
    if (!attr.isNull() && !StringView(attr).containsOnly<isASCIIWhitespace<UChar>>()) {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.contentSecurityPolicyImposition = protectedElement()->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;
        options.loadedFromPluginElement = is<HTMLPlugInElement>(element()) ? LoadedFromPluginElement::Yes : LoadedFromPluginElement::No;
        options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
        options.serviceWorkersMode = is<HTMLPlugInElement>(element()) ? ServiceWorkersMode::None : ServiceWorkersMode::All;
        RefPtr imageElement = dynamicDowncast<HTMLImageElement>(element());
        if (imageElement) {
            options.referrerPolicy = imageElement->referrerPolicy();
            options.fetchPriorityHint = imageElement->fetchPriorityHint();
            if (imageElement->usesSrcsetOrPicture())
                options.initiator = Initiator::Imageset;
        }

        auto crossOriginAttribute = protectedElement()->attributeWithoutSynchronization(HTMLNames::crossoriginAttr);

        // Use URL from original request for same URL loads in order to preserve the original base URL.
        URL imageURL;
        if (m_image && attr == m_pendingURL)
            imageURL = m_image->url();
        else {
            if (imageElement) {
                // It is possible that attributes are bulk-set via Element::parserSetAttributes. In that case, it is possible that attribute vectors are already configured,
                // but corresponding attributeChanged is not called yet. This causes inconsistency in HTMLImageElement. Eventually, we will get the consistent state, but
                // if "src" attributeChanged is not called yet, imageURL can be null and it does not work well for ResourceRequest.
                // In this case, we should behave same as attr.isNull().
                imageURL = imageElement->currentURL();
                if (imageURL.isNull()) {
                    didUpdateCachedImage(relevantMutation, WTFMove(newImage));
                    return;
                }
            } else
                imageURL = document->completeURL(attr);
            m_pendingURL = attr;
        }
        ResourceRequest resourceRequest(imageURL);
        resourceRequest.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(m_element));

        auto request = createPotentialAccessControlRequest(WTFMove(resourceRequest), WTFMove(options), document, crossOriginAttribute);
        request.setInitiator(element());

        if (m_loadManually) {
            Ref cachedResourceLoader = document->cachedResourceLoader();
            bool autoLoadOtherImages = cachedResourceLoader->autoLoadImages();
            cachedResourceLoader->setAutoLoadImages(false);
            RefPtr page = m_element->document().page();
            // FIXME: We shouldn't do an explicit `new` here.
            newImage = new CachedImage(WTFMove(request), page->sessionID(), &page->cookieJar());
            newImage->setStatus(CachedResource::Pending);
            newImage->setLoading(true);
            cachedResourceLoader->m_documentResources.set(newImage->url().string(), newImage.get());
            cachedResourceLoader->setAutoLoadImages(autoLoadOtherImages);
        } else {
#if !LOG_DISABLED
            auto oldState = m_lazyImageLoadState;
#endif
            if (m_lazyImageLoadState == LazyImageLoadState::None && imageElement) {
                if (imageElement->isLazyLoadable() && document->settings().lazyImageLoadingEnabled() && !canReuseFromListOfAvailableImages(request, document)) {
                    m_lazyImageLoadState = LazyImageLoadState::Deferred;
                    request.setIgnoreForRequestCount(true);
                }
            }
            auto imageLoading = (m_lazyImageLoadState == LazyImageLoadState::Deferred) ? ImageLoading::DeferredUntilVisible : ImageLoading::Immediate;
            newImage = document->protectedCachedResourceLoader()->requestImage(WTFMove(request), imageLoading).value_or(nullptr);
            LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " updateFromElement " << element() << " - state changed from " << oldState << " to " << m_lazyImageLoadState << ", loading is " << imageLoading << " new image " << newImage.get());
        }

#if ENABLE(MULTI_REPRESENTATION_HEIC)
        // Adaptive image glyphs need to load both the high fidelity HEIC and the
        // fallback PNG resource, as both resources are treated as an atomic unit.
        if (imageElement && imageElement->isMultiRepresentationHEIC()) {
            auto fallbackURL = imageElement->src();
            if (!fallbackURL.isNull()) {
                ResourceRequest resourceRequest(fallbackURL);
                resourceRequest.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(*imageElement));

                auto request = createPotentialAccessControlRequest(WTFMove(resourceRequest), WTFMove(options), document, crossOriginAttribute);
                request.setInitiator(*imageElement);

                document->protectedCachedResourceLoader()->requestImage(WTFMove(request));
            }
        }
#endif

        // If we do not have an image here, it means that a cross-site
        // violation occurred, or that the image was blocked via Content
        // Security Policy, or the page is being dismissed. Trigger an
        // error event if the page is not being dismissed.
        if (!newImage && !pageIsBeingDismissed(document)) {
            m_failedLoadURL = attr;
            m_hasPendingErrorEvent = true;
            loadEventSender().dispatchEventSoon(*this, eventNames().errorEvent);
        } else
            clearFailedLoadURL();
    } else if (!attr.isNull()) {
        // Fire an error event if the url is empty.
        m_failedLoadURL = attr;
        m_hasPendingErrorEvent = true;
        loadEventSender().dispatchEventSoon(*this, eventNames().errorEvent);
    }

    didUpdateCachedImage(relevantMutation, WTFMove(newImage));
}

void ImageLoader::didUpdateCachedImage(RelevantMutation relevantMutation, CachedResourceHandle<CachedImage>&& newImage)
{
    LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " didUpdateCachedImage " << newImage.get());

    Ref document = element().document();

    CachedResourceHandle oldImage = m_image;
    if (newImage != oldImage || relevantMutation == RelevantMutation::Yes) {
        LOG_WITH_STREAM(LazyLoading, stream << " switching from old image " << oldImage.get() << " to image " << newImage.get() << " " << (newImage ? newImage->url() : URL()));

        m_hasPendingBeforeLoadEvent = false;
        if (m_hasPendingLoadEvent) {
            loadEventSender().cancelEvent(*this, eventNames().loadEvent);
            m_hasPendingLoadEvent = false;
        }

        // Cancel error events that belong to the previous load, which is now cancelled by changing the src attribute.
        // If newImage is null and m_hasPendingErrorEvent is true, we know the error event has been just posted by
        // this load and we should not cancel the event.
        // FIXME: If both previous load and this one got blocked with an error, we can receive one error event instead of two.
        if (m_hasPendingErrorEvent && newImage) {
            loadEventSender().cancelEvent(*this, eventNames().errorEvent);
            m_hasPendingErrorEvent = false;
        }

        m_image = newImage;
        m_hasPendingBeforeLoadEvent = !document->isImageDocument() && newImage;
        m_hasPendingLoadEvent = newImage;
        m_imageComplete = !newImage;

        if (newImage) {
            if (!document->isImageDocument())
                dispatchPendingBeforeLoadEvent();
            else
                updateRenderer();

            if (m_lazyImageLoadState == LazyImageLoadState::Deferred)
                LazyLoadImageObserver::observe(element());

            // If newImage is cached, addClient() will result in the load event
            // being queued to fire.
            newImage->addClient(*this);
        } else
            resetLazyImageLoading(element().protectedDocument());

        if (oldImage) {
            oldImage->removeClient(*this);
            updateRenderer();
        }
    }

    if (CheckedPtr imageResource = renderImageResource())
        imageResource->resetAnimation();

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::updateFromElementIgnoringPreviousError(RelevantMutation relevantMutation)
{
    clearFailedLoadURL();
    updateFromElement(relevantMutation);
}

CachedResourceHandle<CachedImage> ImageLoader::protectedImage() const
{
    return m_image;
}

void ImageLoader::updateFromElementIgnoringPreviousErrorToSameValue()
{
    if (!m_image || !m_image->allowsCaching() || !m_failedLoadURL.isEmpty() || element().document().activeServiceWorker()) {
        updateFromElementIgnoringPreviousError(RelevantMutation::Yes);
        return;
    }
    if (m_hasPendingLoadEvent || !m_pendingURL.isEmpty())
        return;
    ASSERT(m_image);
    m_hasPendingLoadEvent = true;
    notifyFinished(*protectedImage(), NetworkLoadMetrics { });
}

static inline void resolvePromises(Vector<RefPtr<DeferredPromise>>& promises)
{
    ASSERT(!promises.isEmpty());
    auto promisesToBeResolved = std::exchange(promises, { });
    for (auto& promise : promisesToBeResolved)
        promise->resolve();
}

static inline void rejectPromises(Vector<RefPtr<DeferredPromise>>& promises, ASCIILiteral message)
{
    ASSERT(!promises.isEmpty());
    auto promisesToBeRejected = std::exchange(promises, { });
    for (auto& promise : promisesToBeRejected)
        promise->reject(Exception { ExceptionCode::EncodingError, message });
}

inline void ImageLoader::resolveDecodePromises()
{
    resolvePromises(m_decodingPromises);
}

inline void ImageLoader::rejectDecodePromises(ASCIILiteral message)
{
    rejectPromises(m_decodingPromises, message);
}

void ImageLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " notifyFinished - hasPendingLoadEvent " << m_hasPendingLoadEvent);

    ASSERT(m_failedLoadURL.isEmpty());
    ASSERT_UNUSED(resource, &resource == m_image.get());

    m_pendingURL = { };

    if (isDeferred()) {
        LazyLoadImageObserver::unobserve(protectedElement(), protectedDocument());
        m_lazyImageLoadState = LazyImageLoadState::FullImage;
        LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " notifyFinished() for element " << element() << " setting lazy load state to " << m_lazyImageLoadState);
    }

    m_imageComplete = true;
    if (!hasPendingBeforeLoadEvent())
        updateRenderer();

    if (!m_hasPendingLoadEvent)
        return;

    if (m_image->resourceError().isAccessControl()) {
        URL imageURL = m_image->url();

        clearImageWithoutConsideringPendingLoadEvent();

        m_hasPendingErrorEvent = true;
        loadEventSender().dispatchEventSoon(*this, eventNames().errorEvent);

        auto message = makeString("Cannot load image "_s, imageURL.string(), " due to access control checks."_s);
        protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);

        if (hasPendingDecodePromises())
            rejectDecodePromises("Access control error."_s);
        
        ASSERT(!m_hasPendingLoadEvent);

        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }

    if (m_image->wasCanceled()) {
        if (hasPendingDecodePromises())
            rejectDecodePromises("Loading was canceled."_s);
        m_hasPendingLoadEvent = false;
        // Only consider updating the protection ref-count of the Element immediately before returning
        // from this function as doing so might result in the destruction of this ImageLoader.
        updatedHasPendingEvent();
        return;
    }

    if (hasPendingDecodePromises())
        decode();
    loadEventSender().dispatchEventSoon(*this, eventNames().loadEvent);
#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (RefPtr page = element().document().protectedPage())
        page->chrome().client().updateImageSource(protectedElement().get());
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    if (RefPtr imageElement = dynamicDowncast<HTMLImageElement>(element()))
        SpatialImageControls::updateSpatialImageControls(*imageElement);
#endif
}

RenderImageResource* ImageLoader::renderImageResource()
{
    CheckedPtr renderer = element().renderer();
    if (!renderer)
        return nullptr;

    // We don't return style generated image because it doesn't belong to the ImageLoader.
    // See <https://bugs.webkit.org/show_bug.cgi?id=42840>
    if (auto* renderImage = dynamicDowncast<RenderImage>(*renderer); renderImage && !renderImage->isGeneratedContent())
        return &renderImage->imageResource();

    if (auto* svgImage = dynamicDowncast<LegacyRenderSVGImage>(*renderer))
        return &svgImage->imageResource();

    if (auto* svgImage = dynamicDowncast<RenderSVGImage>(*renderer))
        return &svgImage->imageResource();

#if ENABLE(VIDEO)
    if (auto* renderVideo = dynamicDowncast<RenderVideo>(*renderer))
        return &renderVideo->imageResource();
#endif

    return nullptr;
}

void ImageLoader::updateRenderer()
{
    CheckedPtr imageResource = renderImageResource();

    if (!imageResource)
        return;

    // Only update the renderer if it doesn't have an image or if what we have
    // is a complete image. This prevents flickering in the case where a dynamic
    // change is happening between two images.
    CachedImage* cachedImage = imageResource->cachedImage();
    if (m_image != cachedImage && (m_imageComplete || !cachedImage))
        imageResource->setCachedImage(CachedResourceHandle { m_image });
}

void ImageLoader::updatedHasPendingEvent()
{
    // If an Element that does image loading is removed from the DOM the load/error event for the image is still observable.
    // As long as the ImageLoader is actively loading, the Element itself needs to be ref'ed to keep it from being
    // destroyed by DOM manipulation or garbage collection.
    // If such an Element wishes for the load to stop when removed from the DOM it needs to stop the ImageLoader explicitly.
    bool wasProtected = m_elementIsProtected;

    // Because of lazy image loading, an image's load may be deferred indefinitely. To avoid leaking the element, we only
    // protect it once the load has actually started.
    bool imageWillBeLoadedLater = m_image && !m_image->isLoading() && m_image->stillNeedsLoad();

    m_elementIsProtected = (m_hasPendingLoadEvent && !imageWillBeLoadedLater) || m_hasPendingErrorEvent;
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
        rejectDecodePromises("Inactive document."_s);
        return;
    }

    auto attr = protectedElement()->imageSourceURL();
    if (StringView(attr).containsOnly<isASCIIWhitespace<UChar>>()) {
        rejectDecodePromises("Missing source URL."_s);
        return;
    }

    if (m_imageComplete)
        decode();
}

void ImageLoader::decode()
{
    ASSERT(hasPendingDecodePromises());
    
    if (!element().document().domWindow()) {
        rejectDecodePromises("Inactive document."_s);
        return;
    }

    if (!m_image || !m_image->image() || m_image->errorOccurred()) {
        rejectDecodePromises("Loading error."_s);
        return;
    }

    RefPtr bitmapImage = dynamicDowncast<BitmapImage>(m_image->image());
    if (!bitmapImage) {
        resolveDecodePromises();
        return;
    }

    bitmapImage->decode([promises = WTFMove(m_decodingPromises)](DecodingStatus decodingStatus) mutable {
        ASSERT(decodingStatus != DecodingStatus::Decoding);
        if (decodingStatus == DecodingStatus::Invalid)
            rejectPromises(promises, "Decoding error."_s);
        else
            resolvePromises(promises);
    });
}

void ImageLoader::timerFired()
{
    m_protectedElement = nullptr;
}

bool ImageLoader::hasPendingActivity() const
{
    // Because of lazy image loading, an image's load may be deferred indefinitely. To avoid leaking the element, we only
    // protect it once the load has actually started.
    bool imageWillBeLoadedLater = m_image && !m_image->isLoading() && m_image->stillNeedsLoad();
    return (m_hasPendingLoadEvent && !imageWillBeLoadedLater) || m_hasPendingErrorEvent;
}

void ImageLoader::dispatchPendingEvent(ImageEventSender* eventSender, const AtomString& eventType)
{
    ASSERT_UNUSED(eventSender, eventSender == &loadEventSender());
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
    if (!element().isConnected())
        return;
    updateRenderer();
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
    loadEventSender().cancelEvent(*this, eventNames().errorEvent);
    if (element().document().hasLivingRenderTree())
        protectedElement()->dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));

    // Only consider updating the protection ref-count of the Element immediately before returning
    // from this function as doing so might result in the destruction of this ImageLoader.
    updatedHasPendingEvent();
}

void ImageLoader::dispatchPendingLoadEvents(Page* page)
{
    loadEventSender().dispatchPendingEvents(page);
}

void ImageLoader::elementDidMoveToNewDocument(Document& oldDocument)
{
    clearFailedLoadURL();
    clearImage();
    resetLazyImageLoading(oldDocument);
}

inline void ImageLoader::clearFailedLoadURL()
{
    m_failedLoadURL = nullAtom();
}

void ImageLoader::loadDeferredImage()
{
    LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " loadDeferredImage - state is " << m_lazyImageLoadState);

    if (m_lazyImageLoadState != LazyImageLoadState::Deferred)
        return;
    m_lazyImageLoadState = LazyImageLoadState::LoadImmediately;
    updateFromElement(RelevantMutation::No);
}

void ImageLoader::resetLazyImageLoading(Document& document)
{
    LOG_WITH_STREAM(LazyLoading, stream << "ImageLoader " << this << " resetLazyImageLoading - state is " << m_lazyImageLoadState);

    if (isDeferred())
        LazyLoadImageObserver::unobserve(protectedElement(), document);
    m_lazyImageLoadState = LazyImageLoadState::None;
}

VisibleInViewportState ImageLoader::imageVisibleInViewport(const Document& document) const
{
    if (&element().document() != &document)
        return VisibleInViewportState::No;

    CheckedPtr renderReplaced = dynamicDowncast<RenderReplaced>(element().renderer());
    return renderReplaced && renderReplaced->isContentLikelyVisibleInViewport() ? VisibleInViewportState::Yes : VisibleInViewportState::No;
}

bool ImageLoader::shouldIgnoreCandidateWhenLoadingFromArchive(const ImageCandidate& candidate) const
{
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (candidate.originAttribute == ImageCandidate::SrcOrigin)
        return false;

    Ref document = element().document();
    RefPtr loader = document->loader();
    if (!loader || !loader->hasArchiveResourceCollection())
        return false;

    auto candidateURL = URL { protectedElement()->resolveURLStringIfNeeded(candidate.string.toString()) };
    if (loader->archiveResourceForURL(candidateURL))
        return false;

    RefPtr page = document->protectedPage();
    return !page || !page->allowsLoadFromURL(candidateURL, MainFrameMainResource::No);
#else
    UNUSED_PARAM(candidate);
    return false;
#endif
}

} // namespace WebCore
