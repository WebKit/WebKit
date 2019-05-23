/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorClient.h"
#include "WebURLSchemeHandler.h"

namespace WTF {
class URL;
}

namespace WebKit {

class WebURLSchemeTask;

class RemoteInspectorProtocolHandler final : public RemoteInspectorObserver, public WebURLSchemeHandler {
public:
    static Ref<RemoteInspectorProtocolHandler> create(WebPageProxy& page) { return adoptRef(*new RemoteInspectorProtocolHandler(page)); }

    void inspect(const String& hostAndPort, ConnectionID, TargetID, const String& type);

private:
    RemoteInspectorProtocolHandler(WebPageProxy& page)
        : m_page(page) { }

    // RemoteInspectorObserver
    void targetListChanged(RemoteInspectorClient&) final;
    void connectionClosed(RemoteInspectorClient&) final { }

    // WebURLSchemeHandler
    void platformStartTask(WebPageProxy&, WebURLSchemeTask&) final;
    void platformStopTask(WebPageProxy&, WebURLSchemeTask&) final { }
    void platformTaskCompleted(WebURLSchemeTask&) final { }

    HashMap<String, std::unique_ptr<RemoteInspectorClient>> m_inspectorClients;
    WebPageProxy& m_page;
};

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
