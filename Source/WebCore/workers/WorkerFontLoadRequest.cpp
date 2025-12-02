/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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
#include "FontCreationContext.h"
#include "FontCustomPlatformData.h"
#include "FontSelectionAlgorithm.h"
#include "ResourceLoaderOptions.h"
#include "ServiceWorker.h"
#include "WOFFFileFormat.h"
#include "WorkerGlobalScope.h"
#include "WorkerThreadableLoader.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerFontLoadRequest);

Ref<WorkerFontLoadRequest> WorkerFontLoadRequest::create(URL&& url, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return adoptRef(*new WorkerFontLoadRequest(WTFMove(url), loadedFromOpaqueSource));
}

WorkerFontLoadRequest::WorkerFontLoadRequest(URL&& url, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : m_url(WTFMove(url))
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

void WorkerFontLoadRequest::load(WorkerGlobalScope& workerGlobalScope)
{
    m_context = workerGlobalScope;

    ResourceRequest request { URL { m_url } };
    ASSERT(request.httpMethod() == "GET"_s);

    FetchOptions fetchOptions;
    fetchOptions.mode = FetchOptions::Mode::SameOrigin;
    fetchOptions.credentials = workerGlobalScope.credentials();
    fetchOptions.cache = FetchOptions::Cache::Default;
    fetchOptions.redirect = FetchOptions::Redirect::Follow;
    fetchOptions.destination = FetchOptions::Destination::Worker;

    ThreadableLoaderOptions options { WTFMove(fetchOptions) };
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.contentSecurityPolicyEnforcement = Ref { *m_context }->shouldBypassMainWorldContentSecurityPolicy() ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceWorkerSrcDirective;
    options.loadedFromOpaqueSource = m_loadedFromOpaqueSource;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

    options.serviceWorkersMode = ServiceWorkersMode::All;
    if (auto* activeServiceWorker = workerGlobalScope.activeServiceWorker())
        options.serviceWorkerRegistrationIdentifier = activeServiceWorker->registrationIdentifier();

    WorkerThreadableLoader::loadResourceSynchronously(workerGlobalScope, WTFMove(request), *this, options);
}

bool WorkerFontLoadRequest::ensureCustomFontData()
{
    if (!m_fontCustomPlatformData && !m_errorOccurred && !m_isLoading) {
        RefPtr<SharedBuffer> contiguousData;
        if (m_data)
            contiguousData = m_data.takeAsContiguous();
        convertWOFFToSfntIfNecessary(contiguousData);
        if (contiguousData) {
            RefPtr fontCustomPlatformData = FontCustomPlatformData::create(*contiguousData, m_url.fragmentIdentifier().toString());
            m_data.append(*contiguousData);
            if (!fontCustomPlatformData) {
                m_errorOccurred = true;
                return false;
            }
            lazyInitialize(m_fontCustomPlatformData, fontCustomPlatformData.releaseNonNull());
        }
    }

    return m_fontCustomPlatformData.get();
}

RefPtr<Font> WorkerFontLoadRequest::createFont(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, const FontCreationContext& fontCreationContext)
{
    ASSERT(m_fontCustomPlatformData);
    ASSERT(m_context);
    return Font::create(m_fontCustomPlatformData->fontPlatformData(fontDescription, syntheticBold, syntheticItalic, fontCreationContext), Font::Origin::Remote);
}

void WorkerFontLoadRequest::setClient(FontLoadRequestClient* client)
{
    m_fontLoadRequestClient = client;

    if (m_notifyOnClientSet) {
        m_notifyOnClientSet = false;
        client->fontLoaded(*this);
    }
}

void WorkerFontLoadRequest::didReceiveResponse(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const ResourceResponse& response)
{
    if (response.httpStatusCode() / 100 != 2 && response.httpStatusCode())
        m_errorOccurred = true;
}

void WorkerFontLoadRequest::didReceiveData(const SharedBuffer& buffer)
{
    if (m_errorOccurred)
        return;

    m_data.append(buffer);
}

void WorkerFontLoadRequest::didFinishLoading(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const NetworkLoadMetrics&)
{
    m_isLoading = false;

    if (!m_errorOccurred) {
        if (CheckedPtr client = m_fontLoadRequestClient.get())
            client->fontLoaded(*this);
        else
            m_notifyOnClientSet = true;
    }
}

void WorkerFontLoadRequest::didFail(std::optional<ScriptExecutionContextIdentifier>, const ResourceError&)
{
    m_errorOccurred = true;
    if (CheckedPtr client = m_fontLoadRequestClient.get())
        client->fontLoaded(*this);
}

} // namespace WebCore
