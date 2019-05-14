/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "JSDOMPromiseDeferred.h"
#include "ServiceWorkerIdentifier.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class FetchEvent;
class FetchResponse;
class ScriptExecutionContext;

class WEBCORE_TESTSUPPORT_EXPORT ServiceWorkerInternals : public RefCounted<ServiceWorkerInternals> {
public:
    static Ref<ServiceWorkerInternals> create(ServiceWorkerIdentifier identifier) { return adoptRef(*new ServiceWorkerInternals { identifier }); }
    ~ServiceWorkerInternals();

    void setOnline(bool isOnline);
    void waitForFetchEventToFinish(FetchEvent&, DOMPromiseDeferred<IDLInterface<FetchResponse>>&&);
    Ref<FetchEvent> createBeingDispatchedFetchEvent(ScriptExecutionContext&);
    Ref<FetchResponse> createOpaqueWithBlobBodyResponse(ScriptExecutionContext&);

    Vector<String> fetchResponseHeaderList(FetchResponse&);

    String processName() const;

    bool isThrottleable() const;

private:
    explicit ServiceWorkerInternals(ServiceWorkerIdentifier);

    ServiceWorkerIdentifier m_identifier;
};

} // namespace WebCore

#endif
