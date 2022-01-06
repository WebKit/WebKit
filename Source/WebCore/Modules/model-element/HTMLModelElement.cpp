/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "HTMLModelElement.h"

#if ENABLE(MODEL_ELEMENT)

#include "CachedResourceLoader.h"
#include "DOMPromiseProxy.h"
#include "Document.h"
#include "ElementChildIterator.h"
#include "ElementInlines.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerCA.h"
#include "HTMLModelElementCamera.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLSourceElement.h"
#include "JSDOMPromiseDeferred.h"
#include "JSEventTarget.h"
#include "JSHTMLModelElement.h"
#include "JSHTMLModelElementCamera.h"
#include "Model.h"
#include "ModelPlayer.h"
#include "ModelPlayerProvider.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderModel.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Seconds.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLModelElement);

HTMLModelElement::HTMLModelElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_readyPromise { makeUniqueRef<ReadyPromise>(*this, &HTMLModelElement::readyPromiseResolve) }
{
}

HTMLModelElement::~HTMLModelElement()
{
    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }
}

Ref<HTMLModelElement> HTMLModelElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLModelElement(tagName, document));
}

RefPtr<Model> HTMLModelElement::model() const
{
    if (!m_dataComplete)
        return nullptr;
    
    return m_model;
}

void HTMLModelElement::sourcesChanged()
{
    if (!document().hasBrowsingContext()) {
        setSourceURL(URL { });
        return;
    }

    for (auto& element : childrenOfType<HTMLSourceElement>(*this)) {
        // FIXME: for now we use the first valid URL without looking at the mime-type.
        auto url = element.getNonEmptyURLAttribute(HTMLNames::srcAttr);
        if (url.isValid()) {
            setSourceURL(url);
            return;
        }
    }

    setSourceURL(URL { });
}

void HTMLModelElement::setSourceURL(const URL& url)
{
    if (url == m_sourceURL)
        return;

    m_sourceURL = url;

    m_data.reset();
    m_dataComplete = false;

    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }

    if (m_modelPlayer)
        m_modelPlayer = nullptr;

    if (!m_readyPromise->isFulfilled())
        m_readyPromise->reject(Exception { AbortError });

    m_readyPromise = makeUniqueRef<ReadyPromise>(*this, &HTMLModelElement::readyPromiseResolve);

    if (m_sourceURL.isEmpty())
        return;

    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.destination = FetchOptions::Destination::Model;
    // FIXME: Set other options.

    auto crossOriginAttribute = parseCORSSettingsAttribute(attributeWithoutSynchronization(HTMLNames::crossoriginAttr));
    auto request = createPotentialAccessControlRequest(ResourceRequest { m_sourceURL }, WTFMove(options), document(), crossOriginAttribute);
    request.setInitiator(*this);

    auto resource = document().cachedResourceLoader().requestModelResource(WTFMove(request));
    if (!resource.has_value()) {
        m_readyPromise->reject(Exception { NetworkError });
        return;
    }

    m_data.empty();

    m_resource = resource.value();
    m_resource->addClient(*this);
}

HTMLModelElement& HTMLModelElement::readyPromiseResolve()
{
    return *this;
}

// MARK: - DOM overrides.

void HTMLModelElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    sourcesChanged();
}

// MARK: - Rendering overrides.

RenderPtr<RenderElement> HTMLModelElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderModel>(*this, WTFMove(style));
}

// MARK: - CachedRawResourceClient

void HTMLModelElement::dataReceived(CachedResource& resource, const SharedBuffer& buffer)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    m_data.append(buffer);
}

void HTMLModelElement::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    auto invalidateResourceHandleAndUpdateRenderer = [&] {
        m_resource->removeClient(*this);
        m_resource = nullptr;

        if (auto* renderer = this->renderer())
            renderer->updateFromElement();
    };

    if (resource.loadFailedOrCanceled()) {
        m_data.reset();

        invalidateResourceHandleAndUpdateRenderer();

        m_readyPromise->reject(Exception { NetworkError });
        return;
    }

    m_dataComplete = true;
    m_model = Model::create(m_data.takeAsContiguous().get(), resource.mimeType(), resource.url());

    invalidateResourceHandleAndUpdateRenderer();

    m_readyPromise->resolve(*this);

    modelDidChange();
}

// MARK: - ModelPlayer support

void HTMLModelElement::modelDidChange()
{
    // FIXME: For the early returns here, we should probably inform the page that things have
    // failed to render. For the case of no-renderer, we should probably also build the model
    // when/if a renderer is created.

    auto page = document().page();
    if (!page)
        return;

    auto* renderer = this->renderer();
    if (!renderer)
        return;

    m_modelPlayer = page->modelPlayerProvider().createModelPlayer(*this);
    if (!m_modelPlayer)
        return;

    // FIXME: We need to tell the player if the size changes as well, so passing this
    // in with load probably doesn't make sense.
    auto size = renderer->absoluteBoundingBoxRect(false).size();
    m_modelPlayer->load(*m_model, size);
}

bool HTMLModelElement::usesPlatformLayer() const
{
    return m_modelPlayer && m_modelPlayer->layer();
}

PlatformLayer* HTMLModelElement::platformLayer() const
{
    return m_modelPlayer->layer();
}

void HTMLModelElement::didFinishLoading(ModelPlayer& modelPlayer)
{
    ASSERT_UNUSED(modelPlayer, &modelPlayer == m_modelPlayer);

    if (auto* renderer = this->renderer())
        renderer->updateFromElement();
}

void HTMLModelElement::didFailLoading(ModelPlayer& modelPlayer, const ResourceError&)
{
    ASSERT_UNUSED(modelPlayer, &modelPlayer == m_modelPlayer);
}

GraphicsLayer::PlatformLayerID HTMLModelElement::platformLayerID()
{
    auto* page = document().page();
    if (!page)
        return 0;

    if (!is<RenderLayerModelObject>(this->renderer()))
        return 0;

    auto& renderLayerModelObject = downcast<RenderLayerModelObject>(*this->renderer());
    if (!renderLayerModelObject.isComposited() || !renderLayerModelObject.layer() || !renderLayerModelObject.layer()->backing())
        return 0;

    auto* graphicsLayer = renderLayerModelObject.layer()->backing()->graphicsLayer();
    if (!graphicsLayer)
        return 0;

    return graphicsLayer->contentsLayerIDForModel();
}

// MARK: - Fullscreen support.

void HTMLModelElement::enterFullscreen()
{
    m_modelPlayer->enterFullscreen();
}

// MARK: - Interaction support.

void HTMLModelElement::defaultEventHandler(Event& event)
{
    if (!m_modelPlayer || !m_modelPlayer->supportsMouseInteraction())
        return;

    auto type = event.type();
    if (type != eventNames().mousedownEvent && type != eventNames().mousemoveEvent && type != eventNames().mouseupEvent)
        return;

    ASSERT(is<MouseEvent>(event));
    auto& mouseEvent = downcast<MouseEvent>(event);

    if (mouseEvent.button() != LeftButton)
        return;

    if (type == eventNames().mousedownEvent && !m_isDragging && !event.defaultPrevented())
        dragDidStart(mouseEvent);
    else if (type == eventNames().mousemoveEvent && m_isDragging)
        dragDidChange(mouseEvent);
    else if (type == eventNames().mouseupEvent && m_isDragging)
        dragDidEnd(mouseEvent);
}

void HTMLModelElement::dragDidStart(MouseEvent& event)
{
    ASSERT(!m_isDragging);

    RefPtr frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler().setCapturingMouseEventsElement(this);
    event.setDefaultHandled();
    m_isDragging = true;

    if (m_modelPlayer)
        m_modelPlayer->handleMouseDown(event.pageLocation(), event.timeStamp());
}

void HTMLModelElement::dragDidChange(MouseEvent& event)
{
    ASSERT(m_isDragging);

    event.setDefaultHandled();

    if (m_modelPlayer)
        m_modelPlayer->handleMouseMove(event.pageLocation(), event.timeStamp());
}

void HTMLModelElement::dragDidEnd(MouseEvent& event)
{
    ASSERT(m_isDragging);

    RefPtr frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    event.setDefaultHandled();
    m_isDragging = false;

    if (m_modelPlayer)
        m_modelPlayer->handleMouseUp(event.pageLocation(), event.timeStamp());
}

// MARK: - Camera support.

void HTMLModelElement::getCamera(CameraPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->getCamera([promise = WTFMove(promise)] (std::optional<HTMLModelElementCamera> camera) mutable {
        if (!camera)
            promise.reject();
        else
            promise.resolve(*camera);
    });
}

void HTMLModelElement::setCamera(HTMLModelElementCamera camera, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setCamera(camera, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

// MARK: - Animations support.

void HTMLModelElement::isPlayingAnimation(IsPlayingAnimationPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isPlayingAnimation([promise = WTFMove(promise)] (std::optional<bool> isPlaying) mutable {
        if (!isPlaying)
            promise.reject();
        else
            promise.resolve(*isPlaying);
    });
}

void HTMLModelElement::setAnimationIsPlaying(bool isPlaying, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setAnimationIsPlaying(isPlaying, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

void HTMLModelElement::playAnimation(DOMPromiseDeferred<void>&& promise)
{
    setAnimationIsPlaying(true, WTFMove(promise));
}

void HTMLModelElement::pauseAnimation(DOMPromiseDeferred<void>&& promise)
{
    setAnimationIsPlaying(false, WTFMove(promise));
}

void HTMLModelElement::isLoopingAnimation(IsLoopingAnimationPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isLoopingAnimation([promise = WTFMove(promise)] (std::optional<bool> isLooping) mutable {
        if (!isLooping)
            promise.reject();
        else
            promise.resolve(*isLooping);
    });
}

void HTMLModelElement::setIsLoopingAnimation(bool isLooping, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setIsLoopingAnimation(isLooping, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

void HTMLModelElement::animationDuration(DurationPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->animationDuration([promise = WTFMove(promise)] (std::optional<Seconds> duration) mutable {
        if (!duration)
            promise.reject();
        else
            promise.resolve(duration->seconds());
    });
}

void HTMLModelElement::animationCurrentTime(CurrentTimePromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->animationCurrentTime([promise = WTFMove(promise)] (std::optional<Seconds> currentTime) mutable {
        if (!currentTime)
            promise.reject();
        else
            promise.resolve(currentTime->seconds());
    });
}

void HTMLModelElement::setAnimationCurrentTime(double currentTime, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setAnimationCurrentTime(Seconds(currentTime), [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

// MARK: - Audio support.

void HTMLModelElement::hasAudio(HasAudioPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isPlayingAnimation([promise = WTFMove(promise)] (std::optional<bool> hasAudio) mutable {
        if (!hasAudio)
            promise.reject();
        else
            promise.resolve(*hasAudio);
    });
}

void HTMLModelElement::isMuted(IsMutedPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isPlayingAnimation([promise = WTFMove(promise)] (std::optional<bool> isMuted) mutable {
        if (!isMuted)
            promise.reject();
        else
            promise.resolve(*isMuted);
    });
}

void HTMLModelElement::setIsMuted(bool isMuted, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setIsMuted(isMuted, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

#if PLATFORM(COCOA)
Vector<RetainPtr<id>> HTMLModelElement::accessibilityChildren()
{
    if (!m_modelPlayer)
        return { };
    return m_modelPlayer->accessibilityChildren();
}
#endif

}

#endif // ENABLE(MODEL_ELEMENT)
