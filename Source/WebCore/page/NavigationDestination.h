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

#include "EventHandler.h"
#include "LocalDOMWindowProperty.h"
#include "NavigationHistoryEntry.h"
#include "ScriptWrappable.h"

namespace JSC {
class JSValue;
}

namespace WebCore {

class NavigationDestination final : public RefCounted<NavigationDestination>, public ScriptWrappable {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(NavigationDestination);
public:
    static Ref<NavigationDestination> create(const URL& url, RefPtr<NavigationHistoryEntry>&& entry, bool isSameDocument) { return adoptRef(*new NavigationDestination(url, WTFMove(entry), isSameDocument)); };

    const URL& url() const { return m_url; };
    String key() const { return m_entry ? m_entry->key() : String(); };
    String id() const { return m_entry ? m_entry->id() : String(); };
    int64_t index() const { return m_entry ? m_entry->index() : -1; };
    bool sameDocument() const { return m_isSameDocument; };
    JSC::JSValue getState(JSDOMGlobalObject&) const;
    void setStateObject(SerializedScriptValue* stateObject) { m_stateObject = stateObject; }

private:
    explicit NavigationDestination(const URL&, RefPtr<NavigationHistoryEntry>&&, bool isSameDocument);

    RefPtr<NavigationHistoryEntry> m_entry;
    URL m_url;
    bool m_isSameDocument;
    RefPtr<SerializedScriptValue> m_stateObject;
};

} // namespace WebCore
