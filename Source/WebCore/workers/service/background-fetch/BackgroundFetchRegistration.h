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

#include "ActiveDOMObject.h"
#include "BackgroundFetchFailureReason.h"
#include "BackgroundFetchInformation.h"
#include "BackgroundFetchResult.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class BackgroundFetchRecord;
class FetchRequest;
struct BackgroundFetchInformation;
struct BackgroundFetchRecordInformation;
struct CacheQueryOptions;

class BackgroundFetchRegistration final : public RefCounted<BackgroundFetchRegistration>, public EventTarget, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(BackgroundFetchRegistration);
public:
    static Ref<BackgroundFetchRegistration> create(ScriptExecutionContext&, BackgroundFetchInformation&&);
    ~BackgroundFetchRegistration();

    static void updateIfExisting(ScriptExecutionContext&, const BackgroundFetchInformation&);

    const String& id() const { return m_information.identifier; }
    uint64_t uploadTotal() const { return m_information.uploadTotal; }
    uint64_t uploaded() const { return m_information.uploaded; }
    uint64_t downloadTotal() const { return m_information.downloadTotal; }
    uint64_t downloaded() const { return m_information.downloaded; }
    BackgroundFetchResult result() const { return m_information.result; }
    BackgroundFetchFailureReason failureReason() const { return m_information.failureReason; }
    bool recordsAvailable() const { return m_information.recordsAvailable; }

    using RequestInfo = std::variant<RefPtr<FetchRequest>, String>;

    void abort(ScriptExecutionContext&, DOMPromiseDeferred<IDLBoolean>&&);
    void match(ScriptExecutionContext&, RequestInfo&&, const CacheQueryOptions&, DOMPromiseDeferred<IDLInterface<BackgroundFetchRecord>>&&);
    void matchAll(ScriptExecutionContext&, std::optional<RequestInfo>&&, const CacheQueryOptions&, DOMPromiseDeferred<IDLSequence<IDLInterface<BackgroundFetchRecord>>>&&);

    void updateInformation(const BackgroundFetchInformation&);

    using RefCounted::ref;
    using RefCounted::deref;

private:
    BackgroundFetchRegistration(ScriptExecutionContext&, BackgroundFetchInformation&&);

    ServiceWorkerRegistrationIdentifier registrationIdentifier() const { return m_information.registrationIdentifier; }

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return BackgroundFetchRegistrationEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    void stop() final;
    bool virtualHasPendingActivity() const final;

    BackgroundFetchInformation m_information;
};

} // namespace WebCore
