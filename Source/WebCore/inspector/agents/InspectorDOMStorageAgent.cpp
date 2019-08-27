/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "config.h"
#include "InspectorDOMStorageAgent.h"

#include "DOMException.h"
#include "DOMWindow.h"
#include "Database.h"
#include "Document.h"
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "Storage.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StorageType.h"
#include "VoidCallback.h"
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/JSONValues.h>


namespace WebCore {

using namespace Inspector;

InspectorDOMStorageAgent::InspectorDOMStorageAgent(PageAgentContext& context)
    : InspectorAgentBase("DOMStorage"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::DOMStorageFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DOMStorageBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
{
}

InspectorDOMStorageAgent::~InspectorDOMStorageAgent() = default;

void InspectorDOMStorageAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorDOMStorageAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    ErrorString ignored;
    disable(ignored);
}

void InspectorDOMStorageAgent::enable(ErrorString& errorString)
{
    if (m_instrumentingAgents.inspectorDOMStorageAgent() == this) {
        errorString = "DOMStorage domain already enabled"_s;
        return;
    }

    m_instrumentingAgents.setInspectorDOMStorageAgent(this);
}

void InspectorDOMStorageAgent::disable(ErrorString& errorString)
{
    if (m_instrumentingAgents.inspectorDOMStorageAgent() != this) {
        errorString = "DOMStorage domain already disabled"_s;
        return;
    }

    m_instrumentingAgents.setInspectorDOMStorageAgent(nullptr);
}

void InspectorDOMStorageAgent::getDOMStorageItems(ErrorString& errorString, const JSON::Object& storageId, RefPtr<JSON::ArrayOf<JSON::ArrayOf<String>>>& items)
{
    Frame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea) {
        errorString = "Missing storage for given storageId"_s;
        return;
    }

    auto storageItems = JSON::ArrayOf<JSON::ArrayOf<String>>::create();

    for (unsigned i = 0; i < storageArea->length(); ++i) {
        String key = storageArea->key(i);
        String value = storageArea->item(key);

        auto entry = JSON::ArrayOf<String>::create();
        entry->addItem(key);
        entry->addItem(value);
        storageItems->addItem(WTFMove(entry));
    }

    items = WTFMove(storageItems);
}

void InspectorDOMStorageAgent::setDOMStorageItem(ErrorString& errorString, const JSON::Object& storageId, const String& key, const String& value)
{
    Frame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea) {
        errorString = "Missing storage for given storageId"_s;
        return;
    }

    bool quotaException = false;
    storageArea->setItem(frame, key, value, quotaException);
    if (quotaException)
        errorString = DOMException::name(QuotaExceededError);
}

void InspectorDOMStorageAgent::removeDOMStorageItem(ErrorString& errorString, const JSON::Object& storageId, const String& key)
{
    Frame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea) {
        errorString = "Missing storage for given storageId"_s;
        return;
    }

    storageArea->removeItem(frame, key);
}

String InspectorDOMStorageAgent::storageId(Storage& storage)
{
    Document* document = storage.frame()->document();
    ASSERT(document);
    DOMWindow* window = document->domWindow();
    ASSERT(window);
    Ref<SecurityOrigin> securityOrigin = document->securityOrigin();
    bool isLocalStorage = window->optionalLocalStorage() == &storage;
    return InspectorDOMStorageAgent::storageId(securityOrigin.ptr(), isLocalStorage)->toJSONString();
}

RefPtr<Inspector::Protocol::DOMStorage::StorageId> InspectorDOMStorageAgent::storageId(SecurityOrigin* securityOrigin, bool isLocalStorage)
{
    return Inspector::Protocol::DOMStorage::StorageId::create()
        .setSecurityOrigin(securityOrigin->toRawString())
        .setIsLocalStorage(isLocalStorage)
        .release();
}

void InspectorDOMStorageAgent::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin)
{
    RefPtr<Inspector::Protocol::DOMStorage::StorageId> id = InspectorDOMStorageAgent::storageId(securityOrigin, storageType == StorageType::Local);

    if (key.isNull())
        m_frontendDispatcher->domStorageItemsCleared(id);
    else if (newValue.isNull())
        m_frontendDispatcher->domStorageItemRemoved(id, key);
    else if (oldValue.isNull())
        m_frontendDispatcher->domStorageItemAdded(id, key, newValue);
    else
        m_frontendDispatcher->domStorageItemUpdated(id, key, oldValue, newValue);
}

RefPtr<StorageArea> InspectorDOMStorageAgent::findStorageArea(ErrorString& errorString, const JSON::Object& storageId, Frame*& targetFrame)
{
    String securityOrigin;
    if (!storageId.getString("securityOrigin"_s, securityOrigin)) {
        errorString = "Missing securityOrigin in given storageId";
        return nullptr;
    }

    bool isLocalStorage = false;
    if (!storageId.getBoolean("isLocalStorage"_s, isLocalStorage)) {
        errorString = "Missing isLocalStorage in given storageId";
        return nullptr;
    }

    targetFrame = InspectorPageAgent::findFrameWithSecurityOrigin(m_inspectedPage, securityOrigin);
    if (!targetFrame) {
        errorString = "Missing frame for given securityOrigin"_s;
        return nullptr;
    }

    if (!isLocalStorage)
        return m_inspectedPage.sessionStorage()->storageArea(targetFrame->document()->securityOrigin().data());
    return m_inspectedPage.storageNamespaceProvider().localStorageArea(*targetFrame->document());
}

} // namespace WebCore
