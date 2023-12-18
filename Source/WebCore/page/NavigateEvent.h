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

#include "AbortSignal.h"
#include "DOMFormData.h"
#include "Event.h"
#include "EventInit.h"
#include "LocalDOMWindowProperty.h"
#include "NavigationDestination.h"
#include "NavigationInterceptHandler.h"
#include "NavigationNavigationType.h"

namespace WebCore {

class NavigateEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(NavigateEvent);
public:
    struct Init : EventInit {
        NavigationNavigationType navigationType { NavigationNavigationType::Push };
        RefPtr<NavigationDestination> destination;
        RefPtr<AbortSignal> signal;
        RefPtr<DOMFormData> formData;
        String downloadRequest;
        JSC::JSValue info;
        bool canIntercept { false };
        bool userInitiated { false };
        bool hashChange { false };
        bool hasUAVisualTransition { false };
    };

    enum class NavigationFocusReset : bool {
        AfterTransition,
        Manual,
    };

    enum class NavigationScrollBehavior : bool {
        AfterTransition,
        Manual,
    };

    struct NavigationInterceptOptions {
        RefPtr<NavigationInterceptHandler> handler;
        NavigationFocusReset focusReset;
        NavigationScrollBehavior scroll;
    };

    static Ref<NavigateEvent> create(const AtomString& type, const Init&);

    NavigationNavigationType navigationType() const { return m_navigationType; };
    bool canIntercept() const { return m_canIntercept; };
    bool userInitiated() const { return m_userInitiated; };
    bool hashChange() const { return m_hashChange; };
    bool hasUAVisualTransition() const { return m_hasUAVisualTransition; };
    RefPtr<NavigationDestination> destination() { return m_destination; };
    RefPtr<AbortSignal> signal() { return m_signal; };
    RefPtr<DOMFormData> formData() { return m_formData; };
    String downloadRequest() { return m_downloadRequest; };
    JSC::JSValue info() { return m_info; };

    void intercept(NavigationInterceptOptions&&);
    void scroll();

private:
    NavigateEvent(const AtomString& type, const Init&);

    EventInterface eventInterface() const override;

    NavigationNavigationType m_navigationType;
    RefPtr<NavigationDestination> m_destination;
    RefPtr<AbortSignal> m_signal;
    RefPtr<DOMFormData> m_formData;
    String m_downloadRequest;
    JSC::JSValue m_info;
    bool m_canIntercept { false };
    bool m_userInitiated { false };
    bool m_hashChange { false };
    bool m_hasUAVisualTransition { false };
};

} // namespace WebCore
