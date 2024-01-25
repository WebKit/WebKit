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

#include "ContextDestructionObserver.h"
#include "EventHandler.h"
#include "EventTarget.h"
#include <wtf/RefCounted.h>
#include <wtf/UUID.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class NavigationHistoryEntry final : public RefCounted<NavigationHistoryEntry>, public EventTarget, public ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED(NavigationHistoryEntry);
public:
    using RefCounted<NavigationHistoryEntry>::ref;
    using RefCounted<NavigationHistoryEntry>::deref;

    static Ref<NavigationHistoryEntry> create(ScriptExecutionContext* context, const URL& url) { return adoptRef(*new NavigationHistoryEntry(context, url)); }

    const String& url() const { return m_url.string(); };
    String key() const { return m_key.toString(); };
    String id() const { return m_id.toString(); };
    uint64_t index() const { return m_index; };
    bool sameDocument() const { return m_sameDocument; };
    const JSC::JSValue& getState() const { return m_state; };

private:
    NavigationHistoryEntry(ScriptExecutionContext*, const URL&);

    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    const URL m_url;
    const WTF::UUID m_key;
    const WTF::UUID m_id;
    uint64_t m_index;
    JSC::JSValue m_state;
    bool m_sameDocument;
};

} // namespace WebCore
