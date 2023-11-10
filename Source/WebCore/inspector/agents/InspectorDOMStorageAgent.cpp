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
#include "Database.h"
#include "Document.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
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
    disable();
}

Protocol::ErrorStringOr<void> InspectorDOMStorageAgent::enable()
{
    if (m_instrumentingAgents.enabledDOMStorageAgent() == this)
        return makeUnexpected("DOMStorage domain already enabled"_s);

    m_instrumentingAgents.setEnabledDOMStorageAgent(this);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMStorageAgent::disable()
{
    if (m_instrumentingAgents.enabledDOMStorageAgent() != this)
        return makeUnexpected("DOMStorage domain already disabled"_s);

    m_instrumentingAgents.setEnabledDOMStorageAgent(nullptr);

    return { };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::DOMStorage::Item>>> InspectorDOMStorageAgent::getDOMStorageItems(Ref<JSON::Object>&& storageId)
{
    Protocol::ErrorString errorString;

    LocalFrame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, WTFMove(storageId), frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    auto storageItems = JSON::ArrayOf<JSON::ArrayOf<String>>::create();
    for (unsigned i = 0; i < storageArea->length(); ++i) {
        String key = storageArea->key(i);
        String value = storageArea->item(key);

        auto entry = JSON::ArrayOf<String>::create();
        entry->addItem(key);
        entry->addItem(value);
        storageItems->addItem(WTFMove(entry));
    }
    return storageItems;
}

Protocol::ErrorStringOr<void> InspectorDOMStorageAgent::setDOMStorageItem(Ref<JSON::Object>&& storageId, const String& key, const String& value)
{
    Protocol::ErrorString errorString;

    LocalFrame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, WTFMove(storageId), frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    bool quotaException = false;
    storageArea->setItem(*frame, key, value, quotaException);
    if (quotaException)
        return makeUnexpected(DOMException::name(ExceptionCode::QuotaExceededError));

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMStorageAgent::removeDOMStorageItem(Ref<JSON::Object>&& storageId, const String& key)
{
    Protocol::ErrorString errorString;

    LocalFrame* frame;
    RefPtr<StorageArea> storageArea = findStorageArea(errorString, WTFMove(storageId), frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    storageArea->removeItem(*frame, key);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMStorageAgent::clearDOMStorageItems(Ref<JSON::Object>&& storageId)
{
    Protocol::ErrorString errorString;

    LocalFrame* frame;
    auto storageArea = findStorageArea(errorString, WTFMove(storageId), frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    storageArea->clear(*frame);

    return { };
}

String InspectorDOMStorageAgent::storageId(Storage& storage)
{
    auto* document = storage.frame()->document();
    ASSERT(document);
    auto* window = document->domWindow();
    ASSERT(window);
    Ref<SecurityOrigin> securityOrigin = document->securityOrigin();
    bool isLocalStorage = window->optionalLocalStorage() == &storage;
    return InspectorDOMStorageAgent::storageId(securityOrigin, isLocalStorage)->toJSONString();
}

Ref<Protocol::DOMStorage::StorageId> InspectorDOMStorageAgent::storageId(const SecurityOrigin& securityOrigin, bool isLocalStorage)
{
    return Protocol::DOMStorage::StorageId::create()
        .setSecurityOrigin(securityOrigin.toRawString())
        .setIsLocalStorage(isLocalStorage)
        .release();
}

void InspectorDOMStorageAgent::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, const SecurityOrigin& securityOrigin)
{
    auto id = InspectorDOMStorageAgent::storageId(securityOrigin, storageType == StorageType::Local);

    if (key.isNull())
        m_frontendDispatcher->domStorageItemsCleared(WTFMove(id));
    else if (newValue.isNull())
        m_frontendDispatcher->domStorageItemRemoved(WTFMove(id), key);
    else if (oldValue.isNull())
        m_frontendDispatcher->domStorageItemAdded(WTFMove(id), key, newValue);
    else
        m_frontendDispatcher->domStorageItemUpdated(WTFMove(id), key, oldValue, newValue);
}

RefPtr<StorageArea> InspectorDOMStorageAgent::findStorageArea(Protocol::ErrorString& errorString, Ref<JSON::Object>&& storageId, LocalFrame*& targetFrame)
{
    auto securityOrigin = storageId->getString(Protocol::DOMStorage::StorageId::securityOriginKey);
    if (!securityOrigin) {
        errorString = "Missing securityOrigin in given storageId"_s;
        return nullptr;
    }

    auto isLocalStorage = storageId->getBoolean(Protocol::DOMStorage::StorageId::isLocalStorageKey);
    if (!isLocalStorage) {
        errorString = "Missing isLocalStorage in given storageId"_s;
        return nullptr;
    }

    targetFrame = InspectorPageAgent::findFrameWithSecurityOrigin(m_inspectedPage, securityOrigin);
    if (!targetFrame) {
        errorString = "Missing frame for given securityOrigin"_s;
        return nullptr;
    }

    auto& document = *targetFrame->document();
    if (!*isLocalStorage)
        return m_inspectedPage.storageNamespaceProvider().sessionStorageArea(document);
    return m_inspectedPage.storageNamespaceProvider().localStorageArea(document);
}

} // namespace WebCore
