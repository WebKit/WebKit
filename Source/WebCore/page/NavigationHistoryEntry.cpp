/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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
#include "NavigationHistoryEntry.h"

#include "FrameLoader.h"
#include "HistoryController.h"
#include "JSDOMGlobalObject.h"
#include "LocalDOMWindow.h"
#include "Navigation.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(NavigationHistoryEntry);

NavigationHistoryEntry::NavigationHistoryEntry(ScriptExecutionContext* context, Ref<HistoryItem>& historyItem)
    : ContextDestructionObserver(context)
    , m_url(historyItem->url())
    , m_key(WTF::UUID::createVersion4())
    , m_id(WTF::UUID::createVersion4())
    , m_associatedHistoryItem(historyItem)
    , m_documentSequenceNumber(historyItem->documentSequenceNumber())
{
}

NavigationHistoryEntry::NavigationHistoryEntry(ScriptExecutionContext* context, const URL& url)
    : ContextDestructionObserver(context)
    , m_url(url)
    , m_key(WTF::UUID::createVersion4())
    , m_id(WTF::UUID::createVersion4())
{
}

ScriptExecutionContext* NavigationHistoryEntry::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

enum EventTargetInterfaceType NavigationHistoryEntry::eventTargetInterface() const
{
    return EventTargetInterfaceType::NavigationHistoryEntry;
}

uint64_t NavigationHistoryEntry::index() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->domWindow())
        return -1;
    return document->domWindow()->navigation().entries().findIf([this] (auto& entry) {
        return entry.ptr() == this;
    });
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigationhistoryentry-samedocument
bool NavigationHistoryEntry::sameDocument() const
{
    if (!m_documentSequenceNumber)
        return false;
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->frame())
        return false;
    RefPtr currentItem = document->frame()->checkedHistory()->currentItem();
    if (!currentItem)
        return false;
    return currentItem->documentSequenceNumber() == *m_documentSequenceNumber;
}

JSC::JSValue NavigationHistoryEntry::getState(JSDOMGlobalObject& globalObject) const
{
    if (!m_associatedHistoryItem)
        return JSC::jsUndefined();

    auto stateObject = m_associatedHistoryItem->get().navigationAPIStateObject();
    if (!stateObject)
        return JSC::jsUndefined();

    return stateObject->deserialize(globalObject, &globalObject, SerializationErrorMode::Throwing);
}

void NavigationHistoryEntry::setState(RefPtr<SerializedScriptValue>&& state)
{
    if (!m_associatedHistoryItem)
        return;

    m_associatedHistoryItem->get().setNavigationAPIStateObject(WTFMove(state));
}

} // namespace WebCore
