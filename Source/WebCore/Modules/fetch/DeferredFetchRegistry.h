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

#include "HTTPHeaderMap.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "SecurityOriginData.h"
#include "Supplementable.h"
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AbortSignal;
class Document;
class FetchLaterResult;
class FormData;
class WeakPtrImplWithEventTargetData;

class DeferredFetchRegistry final : public Supplement<Document> {
    WTF_MAKE_TZONE_ALLOCATED(DeferredFetchRegistry);
public:
    explicit DeferredFetchRegistry(Document&);
    ~DeferredFetchRegistry();

    static DeferredFetchRegistry& ensure(Document&);
    static DeferredFetchRegistry* from(Document&);

    RefPtr<FetchLaterResult> addDeferredFetch(ResourceRequest&&, HTTPHeaderMap&& originalRequestHeaders, ResourceLoaderOptions&&, RefPtr<FormData>&&, uint64_t requestBytes, AbortSignal*, std::optional<Seconds> activateAfter);
    uint64_t availableBytesFor(const SecurityOriginData&);
    size_t pendingCount() const { return m_records.size(); }

    void documentIsAboutToEnterBackForwardCache();
    void documentIsBeingDestroyed();

private:
    class Record;
    using RecordIdentifier = uint64_t;

    static ASCIILiteral supplementName();
    bool isDeferredFetchRegistry() const final { return true; }

    void removeRecord(RecordIdentifier);
    void activateRecord(RecordIdentifier);
    void activateAllRecords();
    void releaseQuota(const Record&);

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    Vector<std::unique_ptr<Record>> m_records;
    HashMap<SecurityOriginData, uint64_t> m_bytesUsedByOrigin;
    uint64_t m_totalBytesUsed { 0 };
    RecordIdentifier m_nextRecordIdentifier { 1 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DeferredFetchRegistry)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isDeferredFetchRegistry(); }
SPECIALIZE_TYPE_TRAITS_END()
