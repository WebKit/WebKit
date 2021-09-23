/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>

#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace Inspector {
class BackendDispatcher;
class FrontendChannel;
class FrontendRouter;
}

namespace WebKit {

class WebPageProxy;

class WebPageInspectorEmulationAgent : public Inspector::InspectorAgentBase, public Inspector::EmulationBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(WebPageInspectorEmulationAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebPageInspectorEmulationAgent(Inspector::BackendDispatcher& backendDispatcher, WebPageProxy& page);
    ~WebPageInspectorEmulationAgent() override;

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    void setDeviceMetricsOverride(int width, int height, bool fixedlayout, std::optional<double>&& deviceScaleFactor, Ref<SetDeviceMetricsOverrideCallback>&&) override;
    Inspector::Protocol::ErrorStringOr<void> setJavaScriptEnabled(bool enabled) override;
    Inspector::Protocol::ErrorStringOr<void> setAuthCredentials(const String&, const String&) override;
    Inspector::Protocol::ErrorStringOr<void> setActiveAndFocused(std::optional<bool>&&) override;
    Inspector::Protocol::ErrorStringOr<void> grantPermissions(const String& origin, Ref<JSON::Array>&& permissions) override;
    Inspector::Protocol::ErrorStringOr<void> resetPermissions() override;

    void didShowPage();

private:
    void setSize(int width, int height, Ref<SetDeviceMetricsOverrideCallback>&& callback);
    void platformSetSize(int width, int height, Function<void (const String& error)>&&);

    Ref<Inspector::EmulationBackendDispatcher> m_backendDispatcher;
    WebPageProxy& m_page;
    Vector<Function<void()>> m_commandsToRunWhenShown;
    HashMap<String, HashSet<String>> m_permissions;
};

} // namespace WebKit
