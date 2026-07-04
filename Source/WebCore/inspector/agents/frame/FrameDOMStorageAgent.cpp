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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameDOMStorageAgent.h"

#include "DOMException.h"
#include "Document.h"
#include "DocumentPage.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "StorageArea.h"
#include "StorageNamespaceProvider.h"
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDOMStorageAgent);

FrameDOMStorageAgent::FrameDOMStorageAgent(FrameAgentContext& context)
    : InspectorAgentBase("DOMStorage"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::DOMStorageFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DOMStorageBackendDispatcher::create(Ref { context.backendDispatcher }, this))
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameDOMStorageAgent::~FrameDOMStorageAgent() = default;

void FrameDOMStorageAgent::didCreateFrontendAndBackend()
{
}

void FrameDOMStorageAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Inspector::CommandResult<void> FrameDOMStorageAgent::enable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledFrameDOMStorageAgent() == this)
        return { };

    agents->setEnabledFrameDOMStorageAgent(this);

    return { };
}

Inspector::CommandResult<void> FrameDOMStorageAgent::disable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledFrameDOMStorageAgent() != this)
        return { };

    agents->setEnabledFrameDOMStorageAgent(nullptr);

    return { };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOMStorage::Item>>> FrameDOMStorageAgent::getDOMStorageItems(Ref<JSON::Object>&& storageId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr<LocalFrame> frame;
    RefPtr storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    auto storageItems = JSON::ArrayOf<JSON::ArrayOf<String>>::create();
    for (unsigned i = 0; i < storageArea->length(); ++i) {
        String key = storageArea->key(i);
        String value = storageArea->item(key);

        auto entry = JSON::ArrayOf<String>::create();
        entry->addItem(key);
        entry->addItem(value);
        storageItems->addItem(WTF::move(entry));
    }
    return storageItems;
}

Inspector::CommandResult<void> FrameDOMStorageAgent::setDOMStorageItem(Ref<JSON::Object>&& storageId, const String& key, const String& value)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr<LocalFrame> frame;
    RefPtr storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    bool quotaException = false;
    storageArea->setItem(*frame, key, value, quotaException);
    if (quotaException)
        return makeUnexpected(DOMException::name(ExceptionCode::QuotaExceededError));

    return { };
}

Inspector::CommandResult<void> FrameDOMStorageAgent::removeDOMStorageItem(Ref<JSON::Object>&& storageId, const String& key)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr<LocalFrame> frame;
    RefPtr storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    storageArea->removeItem(*frame, key);

    return { };
}

Inspector::CommandResult<void> FrameDOMStorageAgent::clearDOMStorageItems(Ref<JSON::Object>&& storageId)
{
    Inspector::Protocol::ErrorString errorString;

    RefPtr<LocalFrame> frame;
    RefPtr storageArea = findStorageArea(errorString, storageId, frame);
    if (!storageArea)
        return makeUnexpected(errorString);

    storageArea->clear(*frame);

    return { };
}

RefPtr<StorageArea> FrameDOMStorageAgent::findStorageArea(Inspector::Protocol::ErrorString& errorString, const JSON::Object& storageId, RefPtr<LocalFrame>& targetFrame)
{
    auto securityOrigin = storageId.getString("securityOrigin"_s);
    if (!securityOrigin) {
        errorString = "Missing securityOrigin in given storageId"_s;
        return nullptr;
    }

    auto isLocalStorage = storageId.getBoolean("isLocalStorage"_s);
    if (!isLocalStorage) {
        errorString = "Missing isLocalStorage in given storageId"_s;
        return nullptr;
    }

    RefPtr frame = m_inspectedFrame.get();
    RefPtr page = frame ? frame->page() : nullptr;
    RefPtr document = frame ? frame->document() : nullptr;
    if (!frame || !page || !document) {
        errorString = "Frame or page not available"_s;
        return nullptr;
    }

    // The frame agent only operates on its own frame's storage, so securityOrigin is not used
    // to locate the area (unlike the page-level agent's origin search). It is still validated
    // as a precondition: a mismatch means the storageId was routed to the wrong frame or is
    // stale after a navigation, and proceeding would mutate a different origin's storage. Fail
    // so the frontend can re-resolve, rather than silently acting on the wrong origin.
    if (protect(document->securityOrigin())->toRawString() != securityOrigin) {
        errorString = "Given securityOrigin does not match the inspected frame's origin"_s;
        return nullptr;
    }

    targetFrame = WTF::move(frame);

    if (*isLocalStorage)
        return page->storageNamespaceProvider().localStorageArea(*document);
    return page->storageNamespaceProvider().sessionStorageArea(*document);
}

} // namespace WebCore
