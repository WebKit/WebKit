/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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
#include "WebInjectedScriptManager.h"

#include "CommandLineAPIModule.h"
#include "Document.h"
#include "JSExecState.h"
#include "LocalDOMWindow.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebInjectedScriptManager);

Ref<WebInjectedScriptManager> WebInjectedScriptManager::create(InspectorEnvironment& environment, Ref<InjectedScriptHost>&& host)
{
    return adoptRef(*new WebInjectedScriptManager(environment, WTF::move(host)));
}

WebInjectedScriptManager::WebInjectedScriptManager(InspectorEnvironment& environment, Ref<InjectedScriptHost>&& host)
    : InjectedScriptManager(environment, WTF::move(host))
{
}

WebInjectedScriptManager::~WebInjectedScriptManager()
{
    ASSERT(!m_clientCount);
    if (m_clientCount > 0) {
        m_clientCount = 0;
        disconnect();
    }
}

void WebInjectedScriptManager::addClient()
{
    ++m_clientCount;
    if (m_clientCount == 1)
        connect();
}

void WebInjectedScriptManager::removeClient()
{
    ASSERT(m_clientCount > 0);
    --m_clientCount;
    if (!m_clientCount) {
        // FIXME <https://webkit.org/b/305415>: Figure out why the commandLineAPIHost may still be used after the last client disconnects, and call disconnect here instead.
        discardInjectedScripts();
    }
}

void WebInjectedScriptManager::connect()
{
    InjectedScriptManager::connect();

    m_commandLineAPIHost = CommandLineAPIHost::create();
}

void WebInjectedScriptManager::disconnect()
{
    InjectedScriptManager::disconnect();

    m_commandLineAPIHost = nullptr;
}

void WebInjectedScriptManager::discardInjectedScripts()
{
    InjectedScriptManager::discardInjectedScripts();

    if (m_commandLineAPIHost)
        m_commandLineAPIHost->clearAllWrappers();
}

void WebInjectedScriptManager::didCreateInjectedScript(const Inspector::InjectedScript& injectedScript)
{
    CommandLineAPIModule::injectIfNeeded(this, injectedScript);
}

void WebInjectedScriptManager::discardInjectedScriptsFor(LocalDOMWindow& window)
{
    if (m_scriptStateToId.isEmpty())
        return;

    RefPtr document = window.document();
    if (!document)
        return;

    m_idToInjectedScript.removeIf([document](auto& entry) {
        return executionContext(entry.value.globalObject()) == document;
    });
    m_scriptStateToId.removeIf([document](auto& entry) {
        return executionContext(entry.key) == document;
    });
}

} // namespace WebCore
