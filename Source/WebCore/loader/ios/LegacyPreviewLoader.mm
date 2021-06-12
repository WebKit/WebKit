/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameLoaderClient.h"
#import "LegacyPreviewLoaderClient.h"
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
    if (auto client = testingClient())
        return client.releaseNonNull();
    if (auto client = loader.frameLoader()->client().createPreviewLoaderClient(previewFileName, previewType))
        return client.releaseNonNull();
    return emptyClient();
}

bool LegacyPreviewLoader::didReceiveBuffer(const SharedBuffer& buffer)
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "LegacyPreviewLoader appending buffer with size %ld.", buffer.size());
    m_originalData->append(buffer);
    m_converter->updateMainResource();
    m_client->didReceiveBuffer(buffer);
    return true;
}

bool LegacyPreviewLoader::didFinishLoading()
{
    if (m_finishedLoadingDataIntoConverter)
        return false;

    LOG(Network, "LegacyPreviewLoader finished appending data.");
    m_finishedLoadingDataIntoConverter = true;
    m_converter->finishUpdating();
    m_client->didFinishLoading();
    return true;
}

void LegacyPreviewLoader::didFail()
{
    if (m_finishedLoadingDataIntoConverter)
        return;

    LOG(Network, "LegacyPreviewLoader failed.");
    m_finishedLoadingDataIntoConverter = true;
    m_converter->failedUpdating();
    m_client->didFail();
    m_converter = nullptr;
}

void LegacyPreviewLoader::previewConverterDidStartConverting(PreviewConverter& converter)
{
    auto resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    ASSERT(!m_hasProcessedResponse);
    m_originalData->clear();
    resourceLoader->documentLoader()->setPreviewConverter(WTFMove(m_converter));
    auto response { converter.previewResponse() };

    if (m_shouldDecidePolicyBeforeLoading) {
        m_hasProcessedResponse = true;
        resourceLoader->didReceivePreviewResponse(response);
        return;
    }

    resourceLoader->didReceiveResponse(response, [this, weakThis = makeWeakPtr(static_cast<PreviewConverterClient&>(*this)), converter = makeRef(converter)] {
        if (!weakThis)
            return;

        m_hasProcessedResponse = true;

        auto resourceLoader = m_resourceLoader.get();
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

void LegacyPreviewLoader::previewConverterDidReceiveData(PreviewConverter&, const SharedBuffer& data)
{
    auto resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    if (data.isEmpty())
        return;

    if (!m_hasProcessedResponse)
        return;

    auto dataCopy = data.copy();
    resourceLoader->didReceiveBuffer(WTFMove(dataCopy), dataCopy->size(), DataPayloadBytes);
}

void LegacyPreviewLoader::previewConverterDidFinishConverting(PreviewConverter&)
{
    auto resourceLoader = m_resourceLoader.get();
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
    if (auto resourceLoader = m_resourceLoader.get())
        resourceLoader->didFail(resourceLoader->cannotShowURLError());
}

void LegacyPreviewLoader::previewConverterDidFailConverting(PreviewConverter& converter)
{
    auto resourceLoader = m_resourceLoader.get();
    if (!resourceLoader)
        return;

    if (resourceLoader->reachedTerminalState())
        return;

    resourceLoader->didFail(converter.previewError());
}

void LegacyPreviewLoader::providePasswordForPreviewConverter(PreviewConverter& converter, CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT_UNUSED(converter, &converter == m_converter);

    auto resourceLoader = m_resourceLoader.get();
    if (!resourceLoader) {
        completionHandler({ });
        return;
    }

    if (resourceLoader->reachedTerminalState()) {
        completionHandler({ });
        return;
    }

    if (!m_client->supportsPasswordEntry()) {
        completionHandler({ });
        return;
    }

    m_client->didRequestPassword(WTFMove(completionHandler));
}

void LegacyPreviewLoader::provideMainResourceForPreviewConverter(PreviewConverter& converter, CompletionHandler<void(const SharedBuffer*)>&& completionHandler)
{
    ASSERT_UNUSED(converter, &converter == m_converter);
    completionHandler(m_originalData.ptr());
}

LegacyPreviewLoader::~LegacyPreviewLoader() = default;

LegacyPreviewLoader::LegacyPreviewLoader(ResourceLoader& loader, const ResourceResponse& response)
    : m_converter { PreviewConverter::create(response, *this) }
    , m_client { makeClient(loader, m_converter->previewFileName(), m_converter->previewUTI()) }
    , m_originalData { SharedBuffer::create() }
    , m_resourceLoader { makeWeakPtr(loader) }
    , m_shouldDecidePolicyBeforeLoading { loader.frame()->settings().shouldDecidePolicyBeforeLoadingQuickLookPreview() }
{
    ASSERT(PreviewConverter::supportsMIMEType(response.mimeType()));
    m_converter->addClient(*this);
    LOG(Network, "LegacyPreviewLoader created with preview file name \"%s\".", m_converter->previewFileName().utf8().data());
}

bool LegacyPreviewLoader::didReceiveData(const uint8_t* data, unsigned length)
{
    return didReceiveBuffer(SharedBuffer::create(data, length).get());
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
