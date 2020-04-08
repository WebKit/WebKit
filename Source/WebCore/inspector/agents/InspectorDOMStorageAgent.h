/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "InspectorWebAgentBase.h"
#include "StorageArea.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class DOMStorageFrontendDispatcher;
}

namespace WebCore {

class Frame;
class Page;
class SecurityOrigin;
class Storage;

typedef String ErrorString;

class InspectorDOMStorageAgent final : public InspectorAgentBase, public Inspector::DOMStorageBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMStorageAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorDOMStorageAgent(PageAgentContext&);
    ~InspectorDOMStorageAgent() override;

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // DOMStorageBackendDispatcherHandler
    void enable(ErrorString&) override;
    void disable(ErrorString&) override;
    void getDOMStorageItems(ErrorString&, const JSON::Object& storageId, RefPtr<JSON::ArrayOf<JSON::ArrayOf<String>>>& items) override;
    void setDOMStorageItem(ErrorString&, const JSON::Object& storageId, const String& key, const String& value) override;
    void removeDOMStorageItem(ErrorString&, const JSON::Object& storageId, const String& key) override;
    void clearDOMStorageItems(ErrorString&, const JSON::Object& storageId) override;

    // InspectorInstrumentation
    void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*);

    // CommandLineAPI
    static String storageId(Storage&);
    static RefPtr<Inspector::Protocol::DOMStorage::StorageId> storageId(SecurityOrigin*, bool isLocalStorage);

private:
    RefPtr<StorageArea> findStorageArea(ErrorString&, const JSON::Object&, Frame*&);

    std::unique_ptr<Inspector::DOMStorageFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::DOMStorageBackendDispatcher> m_backendDispatcher;

    Page& m_inspectedPage;
};

} // namespace WebCore
