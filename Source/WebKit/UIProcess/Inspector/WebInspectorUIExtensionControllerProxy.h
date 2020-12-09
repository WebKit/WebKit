/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR_EXTENSIONS)

#include "InspectorExtensionTypes.h"
#include "MessageReceiver.h"
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebPageProxy;

class WebInspectorUIExtensionControllerProxy final
    : public IPC::MessageReceiver
    , public CanMakeWeakPtr<WebInspectorUIExtensionControllerProxy> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebInspectorUIExtensionControllerProxy);
public:
    explicit WebInspectorUIExtensionControllerProxy(WebPageProxy& inspectorPage);
    virtual ~WebInspectorUIExtensionControllerProxy();

    // API.
    void registerExtension(const InspectorExtensionID&, const String& displayName, WTF::CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&&);
    void unregisterExtension(const InspectorExtensionID&, WTF::CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&&);
    void createTabForExtension(const InspectorExtensionID&, const String& tabName, const URL& tabIconURL, const URL& sourceURL, WTF::CompletionHandler<void(Expected<InspectorExtensionTabID, InspectorExtensionError>)>&&);

    // Notifications.
    void inspectorFrontendLoaded();

private:
    void whenFrontendHasLoaded(Function<void()>&&);

    WeakPtr<WebPageProxy> m_inspectorPage;

    // Used to queue actions such as registering extensions that happen early on.
    // There's no point sending these before the frontend is fully loaded.
    Vector<Function<void()>> m_frontendLoadedCallbackQueue;

    bool m_frontendLoaded { false };
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
