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

#pragma once

#include "ActiveDOMObject.h"
#include "EventHandler.h"
#include "EventTarget.h"
#include "HistoryItem.h"
#include "ReferrerPolicy.h"
#include "ScriptExecutionContextIdentifier.h"
#include <wtf/RefCounted.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class JSDOMGlobalObject;
class Navigation;
class SerializedScriptValue;

class NavigationHistoryEntry final : public RefCounted<NavigationHistoryEntry>, public EventTarget, public ActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(NavigationHistoryEntry);
public:
    static Ref<NavigationHistoryEntry> create(Navigation&, Ref<HistoryItem>&&);
    static Ref<NavigationHistoryEntry> create(Navigation&, const NavigationHistoryEntry&);

    ~NavigationHistoryEntry();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    const String& url() const;
    String key() const;
    String id() const;
    uint64_t index() const;
    bool sameDocument() const;
    JSC::JSValue getState(JSDOMGlobalObject&) const;

    void setState(RefPtr<SerializedScriptValue>&&);
    SerializedScriptValue* state() const { return m_state.get(); }

    HistoryItem& associatedHistoryItem() const { return m_associatedHistoryItem; }

    void dispatchDisposeEvent();

private:
    struct DocumentState {
        static DocumentState fromContext(ScriptExecutionContext*);

        std::optional<ScriptExecutionContextIdentifier> identifier;
        ReferrerPolicy referrerPolicy { ReferrerPolicy::Default };
    };

    NavigationHistoryEntry(Navigation&, const DocumentState&, Ref<HistoryItem>&&, String urlString, WTF::UUID key, RefPtr<SerializedScriptValue>&& state = { }, WTF::UUID = WTF::UUID::createVersion4());

    // ActiveDOMObject.
    bool virtualHasPendingActivity() const final;

    // EventTarget.
    enum EventTargetInterfaceType eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    using ActiveDOMObject::protectedScriptExecutionContext;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void eventListenersDidChange() final;

    WeakPtr<Navigation, WeakPtrImplWithEventTargetData> m_navigation;
    const String m_urlString;
    const WTF::UUID m_key;
    const WTF::UUID m_id;
    RefPtr<SerializedScriptValue> m_state;
    const Ref<HistoryItem> m_associatedHistoryItem;
    DocumentState m_originalDocumentState;
    bool m_hasDisposeEventListener { false };
    bool m_hasDispatchedDisposeEvent { false };
};

} // namespace WebCore
