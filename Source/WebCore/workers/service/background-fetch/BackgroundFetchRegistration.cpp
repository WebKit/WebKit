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

#include "config.h"
#include "BackgroundFetchRegistration.h"

#if ENABLE(SERVICE_WORKER)

#include "BackgroundFetchManager.h"
#include "BackgroundFetchRecordInformation.h"
#include "CacheQueryOptions.h"
#include "EventNames.h"
#include "RetrieveRecordsOptions.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerRegistrationBackgroundFetchAPI.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(BackgroundFetchRegistration);

Ref<BackgroundFetchRegistration> BackgroundFetchRegistration::create(ScriptExecutionContext& context, BackgroundFetchInformation&& information)
{
    auto registration = adoptRef(*new BackgroundFetchRegistration(context, WTFMove(information)));
    registration->suspendIfNeeded();
    return registration;
}

BackgroundFetchRegistration::BackgroundFetchRegistration(ScriptExecutionContext& context, BackgroundFetchInformation&& information)
    : ActiveDOMObject(&context)
    , m_information(WTFMove(information))
{
}

BackgroundFetchRegistration::~BackgroundFetchRegistration()
{
}

void BackgroundFetchRegistration::abort(ScriptExecutionContext&, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    promise.reject(Exception { NotSupportedError });
}

void BackgroundFetchRegistration::match(ScriptExecutionContext&, RequestInfo&&, const CacheQueryOptions&, DOMPromiseDeferred<IDLInterface<BackgroundFetchRecord>>&& promise)
{
    promise.reject(Exception { NotSupportedError });
}

void BackgroundFetchRegistration::matchAll(ScriptExecutionContext&, std::optional<RequestInfo>&&, const CacheQueryOptions&, DOMPromiseDeferred<IDLSequence<IDLInterface<BackgroundFetchRecord>>>&& promise)
{
    promise.reject(Exception { NotSupportedError });
}

void BackgroundFetchRegistration::updateInformation(const BackgroundFetchInformation& information)
{
    ASSERT(m_information.registrationIdentifier == information.registrationIdentifier);
    ASSERT(m_information.identifier == information.identifier);
    ASSERT(m_information.recordsAvailable);

    if (m_information.downloaded == information.downloaded && m_information.uploaded == information.uploaded && m_information.result == information.result && m_information.failureReason == information.failureReason)
        return;
    
    m_information.uploadTotal = information.uploadTotal;
    m_information.uploaded = information.uploaded;
    m_information.downloadTotal = information.downloadTotal;
    m_information.downloaded = information.downloaded;
    m_information.result = information.result;
    m_information.failureReason = information.failureReason;
    m_information.recordsAvailable = information.recordsAvailable;
    
    dispatchEvent(Event::create(eventNames().progressEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

const char* BackgroundFetchRegistration::activeDOMObjectName() const
{
    return "BackgroundFetchRegistration";
}

void BackgroundFetchRegistration::stop()
{
}

bool BackgroundFetchRegistration::virtualHasPendingActivity() const
{
    return m_information.recordsAvailable;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)


