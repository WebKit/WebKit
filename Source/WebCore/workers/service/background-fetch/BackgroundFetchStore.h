/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "ServiceWorkerRegistrationKey.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class BackgroundFetchEngine;
struct BackgroundFetchInformation;
struct BackgroundFetchOptions;
struct BackgroundFetchRecordInformation;
struct BackgroundFetchRequest;
struct ExceptionData;
class ResourceError;
class ResourceResponse;
struct RetrieveRecordsOptions;
class SWServerRegistration;
class SharedBuffer;

class BackgroundFetchStore : public ThreadSafeRefCounted<BackgroundFetchStore, WTF::DestructionThread::MainRunLoop> {
public:
    virtual ~BackgroundFetchStore() = default;

    virtual void initialize(BackgroundFetchEngine&, const ServiceWorkerRegistrationKey&, CompletionHandler<void()>&&) = 0;
    virtual void clearRecords(const ServiceWorkerRegistrationKey&, const String&, CompletionHandler<void()>&& = [] { }) = 0;
    virtual void clearAllRecords(const ServiceWorkerRegistrationKey&, CompletionHandler<void()>&& = [] { }) = 0;

    enum class StoreResult { OK, QuotaError, InternalError };
    virtual void storeNewRecord(const ServiceWorkerRegistrationKey&, const String&, size_t, const BackgroundFetchRequest&, CompletionHandler<void(StoreResult)>&&) = 0;
    virtual void storeRecordResponse(const ServiceWorkerRegistrationKey&, const String&, size_t, ResourceResponse&&, CompletionHandler<void(StoreResult)>&&) = 0;
    virtual void storeRecordResponseBodyChunk(const ServiceWorkerRegistrationKey&, const String&, size_t, const SharedBuffer&, CompletionHandler<void(StoreResult)>&&) = 0;

    using RetrieveRecordResponseBodyCallback = Function<void(Expected<RefPtr<SharedBuffer>, ResourceError>&&)>;
    virtual void retrieveResponseBody(const ServiceWorkerRegistrationKey&, const String&, size_t, RetrieveRecordResponseBodyCallback&&) = 0;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
