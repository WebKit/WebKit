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

#include "DOMPromiseProxy.h"
#include "EventTarget.h"
#include "LocalDOMWindowProperty.h"
#include "NavigationHistoryEntry.h"
#include "NavigationTransition.h"
#include <JavaScriptCore/JSCJSValue.h>

namespace WebCore {

class Navigation final : public RefCounted<Navigation>, public EventTarget, public ContextDestructionObserver, public LocalDOMWindowProperty {
    WTF_MAKE_ISO_ALLOCATED(Navigation);
public:
    ~Navigation();

    static Ref<Navigation> create(ScriptExecutionContext* context, LocalDOMWindow& window) { return adoptRef(*new Navigation(context, window)); }

    using RefCounted<Navigation>::ref;
    using RefCounted<Navigation>::deref;

    enum class HistoryBehavior : uint8_t {
        Auto,
        Push,
        Replace,
    };

    struct UpdateCurrentEntryOptions {
        JSC::JSValue state;
    };

    struct Options {
        JSC::JSValue info;
    };

    struct NavigateOptions : Options {
        JSC::JSValue state;
        HistoryBehavior history;
    };

    struct ReloadOptions : Options {
        JSC::JSValue state;
    };

    struct Result {
        bool todo;
        // NavigationHistoryEntryPromise committed;
        // NavigationHistoryEntryPromise finished;
    };

    Vector<Ref<NavigationHistoryEntry>> entries() { return m_entries; };
    RefPtr<NavigationHistoryEntry> currentEntry() { return m_currentEntry; };
    RefPtr<NavigationTransition> transition() { return m_transition; };

    bool canGoBack() const { return m_canGoBack; };
    bool canGoForward() const { return m_canGoForward; };

    Result navigate(const String& url, NavigateOptions&&);

    Result reload(ReloadOptions&&);

    Result traverseTo(const String& key, Options&&);
    Result back(Options&&);
    Result forward(Options&&);

    void updateCurrentEntry(UpdateCurrentEntryOptions&&);

private:
    Navigation(ScriptExecutionContext*, LocalDOMWindow&);

    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    RefPtr<NavigationHistoryEntry> m_currentEntry;
    RefPtr<NavigationTransition> m_transition;
    bool m_canGoBack { false };
    bool m_canGoForward { false };
    Vector<Ref<NavigationHistoryEntry>> m_entries;
};

} // namespace WebCore
