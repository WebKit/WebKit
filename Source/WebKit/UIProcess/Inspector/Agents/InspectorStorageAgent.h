/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "WebPageInspectorAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace API {
class HTTPCookieStore;
}

namespace WebKit {

class WebPageProxy;

class InspectorStorageAgent final : public InspectorAgentBase, public Inspector::StorageBackendDispatcherHandler, public CanMakeCheckedPtr<InspectorStorageAgent> {
    WTF_MAKE_NONCOPYABLE(InspectorStorageAgent);
    WTF_MAKE_TZONE_ALLOCATED(InspectorStorageAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InspectorStorageAgent);
public:
    InspectorStorageAgent(WebPageAgentContext&);
    ~InspectorStorageAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend();
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // StorageBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    void getCookies(RefPtr<JSON::Object>&& filter, RefPtr<JSON::Object>&& partition, Ref<GetCookiesCallback>&&);
    void setCookie(Ref<JSON::Object>&& cookie, RefPtr<JSON::Object>&& partition, Ref<SetCookieCallback>&&);
    void deleteCookies(RefPtr<JSON::Object>&& filter, RefPtr<JSON::Object>&& partition, Ref<DeleteCookiesCallback>&&);

private:
    Inspector::Protocol::ErrorStringOr<Ref<API::HTTPCookieStore>> cookieStoreForPartition(const RefPtr<JSON::Object>& partition);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Storage::PartitionKey>> makePartitionKey(const RefPtr<JSON::Object>& partition);

    const Ref<Inspector::StorageBackendDispatcher> m_backendDispatcher;
    WeakRef<WebPageProxy> m_inspectedPage;
};

} // namespace WebKit
