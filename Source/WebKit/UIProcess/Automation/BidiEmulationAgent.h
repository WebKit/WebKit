/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEBDRIVER_BIDI)

#include "WebDriverBidiBackendDispatchers.h"
#include <WebCore/ScrollTypes.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebAutomationSession;
class WebPageProxy;

class BidiEmulationAgent final : public Inspector::BidiEmulationBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiEmulationAgent);
public:
    BidiEmulationAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiEmulationAgent() override;

    void didCreatePage(WebPageProxy&);

    // Inspector::BidiEmulationBackendDispatcherHandler methods.
    Inspector::CommandResult<void> setScrollbarTypeOverride(Ref<JSON::Value>&& scrollbarType, RefPtr<JSON::Array>&& optionalContexts, RefPtr<JSON::Array>&& optionalUserContexts) override;

private:
    std::optional<WebCore::ScrollbarStyle> scrollbarTypeOverrideForPage(const WebPageProxy&) const;

    WeakPtr<WebAutomationSession> m_session;
    const Ref<Inspector::BidiEmulationBackendDispatcher> m_emulationDomainDispatcher;

    WeakHashMap<WebPageProxy, WebCore::ScrollbarStyle> m_scrollbarTypeOverridesForBrowsingContexts;
    HashMap<String, WebCore::ScrollbarStyle> m_scrollbarTypeOverridesForUserContexts;
    std::optional<WebCore::ScrollbarStyle> m_scrollbarTypeOverride;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
