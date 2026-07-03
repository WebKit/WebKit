/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "EventLoop.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "SecurityOriginData.h"
#include "Supplementable.h"
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AbortSignal;
class CachedResource;
class Document;
class FetchLaterResult;
class FormData;
class NetworkLoadMetrics;
class WeakPtrImplWithEventTargetData;
enum class LoadWillContinueInAnotherProcess : bool;

// Per-Document registry that owns pending fetchLater() requests until they
// are activated (dispatched to the network process) or aborted. Handles:
//   * quota accounting (per-Document reserved bucket, distinct from the
//     shared keepalive bucket used by sendBeacon/fetch({keepalive:true}))
//   * activateAfter timers
//   * AbortSignal cancellation
//   * lifecycle-driven activation on bfcache entry and document destruction
//
// This first cut does NOT delegate quota across frames. Every Document owns
// its own bucket (QUOTA_BYTES_PER_ORIGIN). Iframe / permissions-policy
// delegation, and the larger 640 KiB per-top-level-document reserved pool it
// slices from, are planned for a follow-up patch.
//
// Lifetime: owned by the Document via Supplement<Document> (unique_ptr).
// ref()/deref() are forwarded to the Document, matching the NavigatorBeacon
// pattern for supplements that need to satisfy a RefCounted-shaped client
// interface (here, CachedRawResourceClient).
class DeferredFetchRegistry final : public Supplement<Document>, private CachedRawResourceClient {
    WTF_MAKE_TZONE_ALLOCATED(DeferredFetchRegistry);
public:
    // https://fetch.spec.whatwg.org/#available-deferred-fetch-quota
    // The spec defines a 640 KiB per-top-level-document "reserved" pool that
    // can be delegated across origins in 64 KiB per-origin chunks. Because
    // this first cut does not implement cross-frame delegation, the effective
    // limit collapses to the per-origin 64 KiB slice.
    static constexpr uint64_t QUOTA_BYTES_PER_ORIGIN = 64 * 1024;

    explicit DeferredFetchRegistry(Document&);
    ~DeferredFetchRegistry();

    static Ref<DeferredFetchRegistry> ensure(Document&);
    static RefPtr<DeferredFetchRegistry> from(Document&);

    // Supplement/CachedRawResourceClient ref-count forwarding.
    void NODELETE ref() const final;
    void deref() const final;

    // Returns null on quota exhaustion; the caller (fetchLater()) should
    // throw QuotaExceededError.
    RefPtr<FetchLaterResult> addDeferredFetch(ResourceRequest&&, ResourceLoaderOptions&&, RefPtr<FormData>&& body, uint64_t requestBytes, AbortSignal*, std::optional<Seconds> activateAfter);

    // Available quota for future fetchLater() calls to the given reporting
    // origin (the origin of the request's URL). Per-origin bucket = 64 KiB.
    uint64_t availableBytesFor(const SecurityOriginData&) const;
    size_t pendingCount() const { return m_records.size(); }

    // Lifecycle hooks called by Document.
    void documentIsAboutToEnterBackForwardCache();
    void documentIsBeingDestroyed();

private:
    class Record;

    static ASCIILiteral supplementName();
    bool isDeferredFetchRegistry() const final { return true; }

    void removeRecord(Record&);
    void activateRecord(Record&);
    void activateAllRecords();

    // CachedRawResourceClient
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    Vector<std::unique_ptr<Record>> m_records;
    HashMap<SecurityOriginData, uint64_t> m_bytesUsedByOrigin;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DeferredFetchRegistry)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isDeferredFetchRegistry(); }
SPECIALIZE_TYPE_TRAITS_END()
