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
#include "ElementChildIterator.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLSourceElement.h"
#include "RenderModel.h"
#include <wtf/IsoMallocInlines.h>
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

RefPtr<SharedBuffer> HTMLModelElement::modelData() const
{
    if (!m_dataComplete)
        return nullptr;

    return m_data;
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

    m_data = nullptr;
    m_dataComplete = false;

    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }

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

    m_data = SharedBuffer::create();

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

void HTMLModelElement::dataReceived(CachedResource& resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    ASSERT(m_data);
    m_data->append(data, dataLength);
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
        m_data = nullptr;

        invalidateResourceHandleAndUpdateRenderer();

        m_readyPromise->reject(Exception { NetworkError });
        return;
    }

    m_dataComplete = true;

    invalidateResourceHandleAndUpdateRenderer();

    m_readyPromise->resolve(*this);
}

}

#endif // ENABLE(MODEL_ELEMENT)
