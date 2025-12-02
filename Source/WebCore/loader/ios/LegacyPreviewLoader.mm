/*
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LegacyPreviewLoader.h"

#if USE(QUICK_LOOK)

#import "DocumentLoader.h"
#import "FrameLoader.h"
#import "LegacyPreviewLoaderClient.h"
#import "LocalFrame.h"
#import "LocalFrameLoaderClient.h"
#import "Logging.h"
#import "PreviewConverter.h"
#import "QuickLook.h"
#import "ResourceLoader.h"
#import "Settings.h"
#import <wtf/NeverDestroyed.h>

namespace WebCore {

static RefPtr<LegacyPreviewLoaderClient>& testingClient()
{
    static NeverDestroyed<RefPtr<LegacyPreviewLoaderClient>> testingClient;
    return testingClient.get();
}

static LegacyPreviewLoaderClient& emptyClient()
{
    static NeverDestroyed<LegacyPreviewLoaderClient> emptyClient;
    return emptyClient.get();
}

static Ref<LegacyPreviewLoaderClient> makeClient(const ResourceLoader& loader, const String& previewFileName, const String& previewType)
{
    if (RefPtr client = testingClient())
        return client.releaseNonNull();
    if (!loader.frameLoader())
        return emptyClient();
    if (RefPtr client = loader.frameLoader()->client().createPreviewLoaderClient(previewFileName, previewType))
        return client.releaseNonNull();
    return emptyClient();
}

Ref<LegacyPreviewLoader> LegacyPreviewLoader::create(ResourceLoader& loader, const ResourceResponse& response)
{
    return adoptRef(*new LegacyPreviewLoader(loader, response));
}

LegacyPreviewLoader::LegacyPreviewLoader(ResourceLoader& loader, const ResourceResponse& response)
    : m_converter { PreviewConverter::create(response, *this) }
    , m_client { makeClient(loader, m_converter->previewFileName(), m_converter->previewUTI()) }
    , m_resourceLoader { loader }
    , m_shouldDecidePolicyBeforeLoading { loader.frame()->settings().shouldDecidePolicyBeforeLoadingQuickLookPreview() }
{
    ASSERT(PreviewConverter::supportsMIMEType(response.mimeType()));
    protectedConverter()->addClient(*this);
    LOG(Network, "LegacyPreviewLoader created with preview file name \"%s\".", m_converter->previewFileName().utf8().data());
}

LegacyPreviewLoader::~LegacyPreviewLoader() = default;

RefPtr<PreviewConverter> LegacyPreviewLoader::protectedConverter() const
{
    return m_converter;
}

bool LegacyPreviewLoader::didReceiveData(const SharedBuffer& buffer)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "LegacyPreviewLoader appending buffer with size %ld.", buffer.size());
    m_originalData.append(buffer);
    protectedConverter()->updateMainResource();
    m_client->didReceiveData(buffer);
    return true;
}

bool LegacyPreviewLoader::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "LegacyPreviewLoader finished appending data.");
    m_finishedLoadingDataIntoConverter = true;
    protectedConverter()->finishUpdating();
    m_client->didFinishLoading();
    return true;
}

void LegacyPreviewLoader::didFail()
{
    if (m_finishedLoadingDataIntoConverter)
        return;

    LOG(Network, "LegacyPreviewLoader failed.");
    m_finishedLoadingDataIntoConverter = true;
    protectedConverter()->failedUpdating();
    m_client->didFail();
    m_converter = nullptr;
}

void LegacyPreviewLoader::previewConverterDidStartConverting(PreviewConverter& converter)
{
    RefPtr resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    ASSERT(!m_hasProcessedResponse);
    m_originalData.reset();
    resourceLoader->protectedDocumentLoader()->setPreviewConverter(std::exchange(m_converter, nullptr));
    auto response { converter.previewResponse() };

    if (m_shouldDecidePolicyBeforeLoading) {
        m_hasProcessedResponse = true;
        resourceLoader->didReceivePreviewResponse(WTFMove(response));
        return;
    }

    resourceLoader->didReceiveResponse(WTFMove(response), [this, weakThis = WeakPtr { static_cast<PreviewConverterClient&>(*this) }, converter = Ref { converter }] {
        if (!weakThis)
            return;

        m_hasProcessedResponse = true;

        RefPtr resourceLoader = m_resourceLoader.get();
        if (!resourceLoader)
            return;

        if (resourceLoader->reachedTerminalState())
            return;

        if (!converter->previewData().isEmpty()) {
            auto bufferSize = converter->previewData().size();
            resourceLoader->didReceiveBuffer(converter->previewData().copy(), bufferSize, DataPayloadBytes);
        }

        if (resourceLoader->reachedTerminalState())
            return;

        if (m_needsToCallDidFinishLoading) {
            m_needsToCallDidFinishLoading = false;
            resourceLoader->didFinishLoading(NetworkLoadMetrics { });
        }
    });
}

void LegacyPreviewLoader::previewConverterDidReceiveData(PreviewConverter&, const FragmentedSharedBuffer& data)
{
    RefPtr resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    if (data.isEmpty())
        return;

    if (!m_hasProcessedResponse)
        return;

    resourceLoader->didReceiveBuffer(data, data.size(), DataPayloadBytes);
}

void LegacyPreviewLoader::previewConverterDidFinishConverting(PreviewConverter&)
{
    RefPtr resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    if (!m_hasProcessedResponse) {
        m_needsToCallDidFinishLoading = true;
        return;
    }

    resourceLoader->didFinishLoading(NetworkLoadMetrics { });
}

void LegacyPreviewLoader::previewConverterDidFailUpdating(PreviewConverter&)
{
    if (RefPtr resourceLoader = m_resourceLoader.get())
        resourceLoader->didFail(resourceLoader->cannotShowURLError());
}

void LegacyPreviewLoader::previewConverterDidFailConverting(PreviewConverter& converter)
{
    RefPtr resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    resourceLoader->didFail(converter.previewError());
}

void LegacyPreviewLoader::providePasswordForPreviewConverter(PreviewConverter& converter, Function<void(const String&)>&& completionHandler)
{
    ASSERT_UNUSED(converter, &converter == m_converter);

    RefPtr resourceLoader = m_resourceLoader.get();
    if (!resourceLoader) {
        completionHandler({ });
        return;
    }

    if (resourceLoader->reachedTerminalState()) {
        completionHandler({ });
        return;
    }

    Ref client = m_client;
    if (!client->supportsPasswordEntry()) {
        completionHandler({ });
        return;
    }

    client->didRequestPassword(WTFMove(completionHandler));
}

void LegacyPreviewLoader::provideMainResourceForPreviewConverter(PreviewConverter& converter, CompletionHandler<void(Ref<FragmentedSharedBuffer>&&)>&& completionHandler)
{
    ASSERT_UNUSED(converter, &converter == m_converter);
    completionHandler(m_originalData.copy());
}

bool LegacyPreviewLoader::didReceiveResponse(const ResourceResponse&)
{
    return !m_shouldDecidePolicyBeforeLoading;
}

void LegacyPreviewLoader::setClientForTesting(RefPtr<LegacyPreviewLoaderClient>&& client)
{
    testingClient() = WTFMove(client);
}

} // namespace WebCore

#endif // USE(QUICK_LOOK)
