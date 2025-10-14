/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WebAssemblyDebuggable.h"

#if ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)

#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <JavaScriptCore/RemoteInspector.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebAssemblyDebuggable);

Ref<WebAssemblyDebuggable> WebAssemblyDebuggable::create(WebProcessProxy& process)
{
    return adoptRef(*new WebAssemblyDebuggable(process));
}

WebAssemblyDebuggable::WebAssemblyDebuggable(WebProcessProxy& process)
    : m_process(process)
{
}

void WebAssemblyDebuggable::detachFromProcess()
{
    m_process = nullptr;
}

WebAssemblyDebuggable::~WebAssemblyDebuggable() = default;

std::optional<ProcessID> WebAssemblyDebuggable::webContentProcessPID() const
{
    RefPtr process = m_process.get();
    if (!process)
        return std::nullopt;

    // When WebAssemblyDebuggable is created, the WebContent process is guaranteed to have
    // finished launching (see didFinishLaunching -> createWebAssemblyInspectorTarget).
    // Therefore, processID() must return a valid non-zero PID.
    auto pid = process->processID();
    RELEASE_ASSERT(pid);
    return pid;
}

String WebAssemblyDebuggable::name() const
{
    RefPtr process = m_process.get();
    if (!process)
        return "WebAssembly Debug Server"_s;

    // When process exists, PID must be valid (see webContentProcessPID() and lifetime_proof.md)
    auto pid = process->processID();
    RELEASE_ASSERT(pid);
    return makeString("WebAssembly Debug Server (WebContent PID "_s, pid, ")"_s);
}

String WebAssemblyDebuggable::url() const
{
    // For WebAssembly debugging, url() and name() should be the same
    // to avoid confusion about different identifiers
    return name();
}

bool WebAssemblyDebuggable::hasLocalDebugger() const
{
    return false;
}

void WebAssemblyDebuggable::connect(FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    m_frontendChannel = &channel;

    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, isAutomaticConnection, immediatelyPause] {
        RefPtr process = m_process.get();
        if (!process)
            return;

        // Send IPC message to WebContent process to connect WebAssembly target
        process->connectWebAssemblyInspectorTarget(isAutomaticConnection, immediatelyPause);
    });
}

void WebAssemblyDebuggable::disconnect(FrontendChannel& channel)
{
    m_frontendChannel = nullptr;

    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }] {
        RefPtr process = m_process.get();
        if (!process)
            return;

        // Send IPC message to WebContent process to disconnect WebAssembly target
        process->disconnectWebAssemblyInspectorTarget();
    });
}

void WebAssemblyDebuggable::dispatchMessageFromRemote(String&& message)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, message = WTFMove(message).isolatedCopy()]() mutable {
        RefPtr process = m_process.get();
        if (!process)
            return;

        // Forward message to WebContent process via IPC
        process->dispatchWebAssemblyInspectorMessage(WTFMove(message));
    });
}

void WebAssemblyDebuggable::setIndicating(bool indicating)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, indicating] {
        RefPtr process = m_process.get();
        if (!process)
            return;

        // Send IPC message to WebContent process to set indicating state
        process->setWebAssemblyInspectorTargetIndicating(indicating);
    });
}

void WebAssemblyDebuggable::setNameOverride(const String& name)
{
    m_nameOverride = name;
    update();
}

void WebAssemblyDebuggable::sendResponseToFrontend(const String& response)
{
    if (!m_frontendChannel)
        return;

    // Forward WebAssembly debugging response to the connected RWI frontend
    m_frontendChannel->sendMessageToFrontend(response);
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR) && ENABLE(WEBASSEMBLY)
