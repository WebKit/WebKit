/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright (C) 2021 Metrological Group B.V.
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerFontLoadRequest.h"

#include "Font.h"
#include "FontCustomPlatformData.h"
#include "FontSelectionAlgorithm.h"
#include "ResourceLoaderOptions.h"
#include "ServiceWorker.h"
#include "SharedBuffer.h"
#include "WOFFFileFormat.h"
#include "WorkerGlobalScope.h"
#include "WorkerThreadableLoader.h"

namespace WebCore {

WorkerFontLoadRequest::WorkerFontLoadRequest(URL&& url, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : m_url(WTFMove(url))
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

void WorkerFontLoadRequest::load(WorkerGlobalScope& workerGlobalScope)
{
    m_context = makeWeakPtr(static_cast<ScriptExecutionContext*>(&workerGlobalScope));

    ResourceRequest request { m_url };
    ASSERT(request.httpMethod() == "GET");

    FetchOptions fetchOptions;
    fetchOptions.mode = FetchOptions::Mode::SameOrigin;
    fetchOptions.credentials = workerGlobalScope.credentials();
    fetchOptions.cache = FetchOptions::Cache::Default;
    fetchOptions.redirect = FetchOptions::Redirect::Follow;
    fetchOptions.destination = FetchOptions::Destination::Worker;

    ThreadableLoaderOptions options { WTFMove(fetchOptions) };
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.contentSecurityPolicyEnforcement = m_context->shouldBypassMainWorldContentSecurityPolicy() ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceChildSrcDirective;
    options.loadedFromOpaqueSource = m_loadedFromOpaqueSource;

    options.serviceWorkersMode = ServiceWorkersMode::All;
#if ENABLE(SERVICE_WORKER)
    if (auto* activeServiceWorker = workerGlobalScope.activeServiceWorker())
        options.serviceWorkerRegistrationIdentifier = activeServiceWorker->registrationIdentifier();
#endif

    WorkerThreadableLoader::loadResourceSynchronously(workerGlobalScope, WTFMove(request), *this, options);
}

bool WorkerFontLoadRequest::ensureCustomFontData(const AtomString&)
{
    if (!m_fontCustomPlatformData && !m_errorOccurred && !m_isLoading) {
        convertWOFFToSfntIfNecessary(m_data);
        if (m_data) {
            m_fontCustomPlatformData = createFontCustomPlatformData(*m_data, m_url.fragmentIdentifier().toString());
            if (!m_fontCustomPlatformData)
                m_errorOccurred = true;
        }
    }

    return m_fontCustomPlatformData.get();
}

RefPtr<Font> WorkerFontLoadRequest::createFont(const FontDescription& fontDescription, const AtomString&, bool syntheticBold, bool syntheticItalic, const FontFeatureSettings& fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities)
{
    ASSERT(m_fontCustomPlatformData);
    ASSERT(m_context);
    return Font::create(m_fontCustomPlatformData->fontPlatformData(fontDescription, syntheticBold, syntheticItalic, fontFaceFeatures, fontFaceCapabilities), Font::Origin::Remote, &m_context->fontCache());
}

void WorkerFontLoadRequest::setClient(FontLoadRequestClient* client)
{
    m_fontLoadRequestClient = client;

    if (m_notifyOnClientSet) {
        m_notifyOnClientSet = false;
        m_fontLoadRequestClient->fontLoaded(*this);
    }
}

void WorkerFontLoadRequest::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    if (response.httpStatusCode() / 100 != 2 && response.httpStatusCode())
        m_errorOccurred = true;
}

void WorkerFontLoadRequest::didReceiveData(const uint8_t* data, int dataLength)
{
    if (m_errorOccurred)
        return;

    if (!m_data)
        m_data = SharedBuffer::create();

    m_data->append(data, dataLength);
}

void WorkerFontLoadRequest::didFinishLoading(unsigned long)
{
    m_isLoading = false;

    if (!m_errorOccurred) {
        if (m_fontLoadRequestClient)
            m_fontLoadRequestClient->fontLoaded(*this);
        else
            m_notifyOnClientSet = true;
    }
}

void WorkerFontLoadRequest::didFail(const ResourceError&)
{
    m_errorOccurred = true;
}

} // namespace WebCore
