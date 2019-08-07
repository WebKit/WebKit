/*
 * Copyright (C) 2007-2018 Apple Inc.  All rights reserved.
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

#pragma once

#include "DOMWindowProperty.h"
#include "ExceptionOr.h"
#include "JSValueInWrappedObject.h"
#include "ScriptWrappable.h"
#include "SerializedScriptValue.h"
#include <wtf/WallTime.h>

namespace WebCore {

class Document;

class History final : public ScriptWrappable, public RefCounted<History>, public DOMWindowProperty {
    WTF_MAKE_ISO_ALLOCATED(History);
public:
    static Ref<History> create(DOMWindow& window) { return adoptRef(*new History(window)); }

    unsigned length() const;
    
    enum class ScrollRestoration {
        Auto,
        Manual
    };

    ExceptionOr<ScrollRestoration> scrollRestoration() const;
    ExceptionOr<void> setScrollRestoration(ScrollRestoration);

    SerializedScriptValue* state();
    JSValueInWrappedObject& cachedState();
    JSValueInWrappedObject& cachedStateForGC() { return m_cachedState; }

    void back();
    void forward();
    void go(int);

    void back(Document&);
    void forward(Document&);
    void go(Document&, int);

    bool isSameAsCurrentState(SerializedScriptValue*) const;

    ExceptionOr<void> pushState(RefPtr<SerializedScriptValue>&& data, const String& title, const String& urlString);
    ExceptionOr<void> replaceState(RefPtr<SerializedScriptValue>&& data, const String& title, const String& urlString);

private:
    explicit History(DOMWindow&);

    enum class StateObjectType { Push, Replace };
    ExceptionOr<void> stateObjectAdded(RefPtr<SerializedScriptValue>&&, const String& title, const String& url, StateObjectType);
    bool stateChanged() const;

    URL urlForState(const String& url);

    SerializedScriptValue* stateInternal() const;

    RefPtr<SerializedScriptValue> m_lastStateObjectRequested;
    JSValueInWrappedObject m_cachedState;

    unsigned m_currentStateObjectTimeSpanObjectsAdded { 0 };
    WallTime m_currentStateObjectTimeSpanStart;

    // For the main frame's History object to keep track of all state object usage.
    uint64_t m_totalStateObjectUsage { 0 };

    // For each individual History object to keep track of the most recent state object added.
    uint64_t m_mostRecentStateObjectUsage { 0 };
};

inline ExceptionOr<void> History::pushState(RefPtr<SerializedScriptValue>&& data, const String& title, const String& urlString)
{
    return stateObjectAdded(WTFMove(data), title, urlString, StateObjectType::Push);
}

inline ExceptionOr<void> History::replaceState(RefPtr<SerializedScriptValue>&& data, const String& title, const String& urlString)
{
    return stateObjectAdded(WTFMove(data), title, urlString, StateObjectType::Replace);
}

} // namespace WebCore
