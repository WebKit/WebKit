/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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

#pragma once

#include <WebCore/CachedResourceClient.h>
#include <WebCore/CachedResourceHandle.h>
#include <WebCore/FetchOptionsDestination.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/FastMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CachedScript;
class Document;
class NetworkLoadMetrics;
class WeakPtrImplWithEventTargetData;
enum class LoadWillContinueInAnotherProcess : bool;

class LoadableSpeculationRules final : public RefCounted<LoadableSpeculationRules>, public CachedResourceClient {
public:
    static Ref<LoadableSpeculationRules> create(Document&, const URL&);
    ~LoadableSpeculationRules();

    // CachedResourceClient.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    bool load(Document&, const URL&);
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess = LoadWillContinueInAnotherProcess::No) final;

private:
    LoadableSpeculationRules(Document&, const URL&);

    CachedResourceHandle<CachedScript> requestSpeculationRules(Document&, const URL& sourceURL);

    CachedResourceHandle<CachedScript> m_cachedScript;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    URL m_url;
};

} // namespace WebCore
