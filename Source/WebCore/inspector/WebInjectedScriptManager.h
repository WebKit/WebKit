/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "CommandLineAPIHost.h"
#include <JavaScriptCore/InjectedScriptManager.h>
#include <wtf/FastMalloc.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class LocalDOMWindow;

class WebInjectedScriptManager final : public Inspector::InjectedScriptManager, public RefCounted<WebInjectedScriptManager> {
    WTF_MAKE_NONCOPYABLE(WebInjectedScriptManager);
    WTF_MAKE_TZONE_ALLOCATED(WebInjectedScriptManager);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebInjectedScriptManager);
public:
    static Ref<WebInjectedScriptManager> create(Inspector::InspectorEnvironment&, Ref<Inspector::InjectedScriptHost>&&);

    ~WebInjectedScriptManager() final;

    CommandLineAPIHost* commandLineAPIHost() const { return m_commandLineAPIHost.get(); }

    void addClient();
    void removeClient();

    void connect() final;
    void disconnect() final;
    void discardInjectedScripts() final;

    void discardInjectedScriptsFor(LocalDOMWindow&);

private:
    bool isWebInjectedScriptManager() const final { return true; }

    WebInjectedScriptManager(Inspector::InspectorEnvironment&, Ref<Inspector::InjectedScriptHost>&&);

    void didCreateInjectedScript(const Inspector::InjectedScript&) final;

    RefPtr<CommandLineAPIHost> m_commandLineAPIHost;
    int m_clientCount { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebInjectedScriptManager) \
    static bool isType(const Inspector::InjectedScriptManager& manager) { return manager.isWebInjectedScriptManager(); } \
SPECIALIZE_TYPE_TRAITS_END()
