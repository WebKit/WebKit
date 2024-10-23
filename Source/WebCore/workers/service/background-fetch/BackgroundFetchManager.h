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

#include "BackgroundFetchRegistration.h"
#include "JSDOMPromiseDeferred.h"
#include "ServiceWorkerTypes.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

struct BackgroundFetchInformation;
class BackgroundFetchRegistration;
struct BackgroundFetchRegistrationData;
struct BackgroundFetchOptions;
class FetchRequest;
class ServiceWorkerRegistration;

class BackgroundFetchManager : public RefCounted<BackgroundFetchManager>, public CanMakeWeakPtr<BackgroundFetchManager> {
public:
    static Ref<BackgroundFetchManager> create(ServiceWorkerRegistration& registration) { return adoptRef(*new BackgroundFetchManager(registration)); }
    ~BackgroundFetchManager();

    using RequestInfo = std::variant<RefPtr<FetchRequest>, String>;
    using Requests = std::variant<RefPtr<FetchRequest>, String, Vector<RequestInfo>>;
    void fetch(ScriptExecutionContext&, const String&, Requests&&, BackgroundFetchOptions&&, DOMPromiseDeferred<IDLInterface<BackgroundFetchRegistration>>&&);
    void get(ScriptExecutionContext&, const String&, DOMPromiseDeferred<IDLNullable<IDLInterface<BackgroundFetchRegistration>>>&&);
    void getIds(ScriptExecutionContext&, DOMPromiseDeferred<IDLSequence<IDLDOMString>>&&);

    RefPtr<BackgroundFetchRegistration> existingBackgroundFetchRegistration(const String& identifier) { return m_backgroundFetchRegistrations.get(identifier); }
    Ref<BackgroundFetchRegistration> backgroundFetchRegistrationInstance(ScriptExecutionContext&, BackgroundFetchInformation&&);

private:
    explicit BackgroundFetchManager(ServiceWorkerRegistration&);

    ServiceWorkerRegistrationIdentifier m_identifier;
    HashMap<String, Ref<BackgroundFetchRegistration>> m_backgroundFetchRegistrations;
};

} // namespace WebCore
