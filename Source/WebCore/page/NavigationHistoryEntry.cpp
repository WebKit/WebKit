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
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(NavigationHistoryEntry);

NavigationHistoryEntry::NavigationHistoryEntry(ScriptExecutionContext* context, Ref<HistoryItem>&& historyItem, String urlString, WTF::UUID key, RefPtr<SerializedScriptValue>&& state, WTF::UUID id)
    : ContextDestructionObserver(context)
    , m_urlString(urlString)
    , m_key(key)
    , m_id(id)
    , m_state(state)
    , m_associatedHistoryItem(WTFMove(historyItem))
{
}

Ref<NavigationHistoryEntry> NavigationHistoryEntry::create(ScriptExecutionContext* context, const NavigationHistoryEntry& other)
{
    Ref historyItem = other.m_associatedHistoryItem;
    RefPtr state = historyItem->navigationAPIStateObject();
    if (!state)
        state = other.m_state;
    return adoptRef(*new NavigationHistoryEntry(context, WTFMove(historyItem), other.m_urlString, other.m_key, WTFMove(state), other.m_id));
}

ScriptExecutionContext* NavigationHistoryEntry::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

enum EventTargetInterfaceType NavigationHistoryEntry::eventTargetInterface() const
{
    return EventTargetInterfaceType::NavigationHistoryEntry;
}

const String& NavigationHistoryEntry::url() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->isFullyActive())
        return nullString();
    return m_urlString;
}

String NavigationHistoryEntry::key() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->isFullyActive())
        return nullString();
    return m_key.toString();
}

String NavigationHistoryEntry::id() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->isFullyActive())
        return nullString();
    return m_id.toString();
}

uint64_t NavigationHistoryEntry::index() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->isFullyActive())
        return -1;
    return document->domWindow()->navigation().entries().findIf([this] (auto& entry) {
        return entry.ptr() == this;
    });
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigationhistoryentry-samedocument
bool NavigationHistoryEntry::sameDocument() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->isFullyActive())
        return false;
    RefPtr currentItem = document->frame()->checkedHistory()->currentItem();
    if (!currentItem)
        return false;
    return currentItem->documentSequenceNumber() == m_associatedHistoryItem->documentSequenceNumber();
}

JSC::JSValue NavigationHistoryEntry::getState(JSDOMGlobalObject& globalObject) const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->isFullyActive())
        return JSC::jsUndefined();

    if (!m_state)
        return JSC::jsUndefined();

    return m_state->deserialize(globalObject, &globalObject, SerializationErrorMode::Throwing);
}

void NavigationHistoryEntry::setState(RefPtr<SerializedScriptValue>&& state)
{
    m_state = state;
    m_associatedHistoryItem->setNavigationAPIStateObject(WTFMove(state));
}

} // namespace WebCore
