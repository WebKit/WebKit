/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "APIObject.h"
#include <WebCore/AuthenticatorTransport.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
enum class ClientDataType : bool;
}

namespace WebKit {
class AuthenticatorManager;
}

namespace API {

class WebAuthenticationPanelClient;

class WebAuthenticationPanel final : public ObjectImpl<Object::Type::WebAuthenticationPanel>, public CanMakeWeakPtr<WebAuthenticationPanel> {
public:
    using TransportSet = HashSet<WebCore::AuthenticatorTransport, WTF::IntHash<WebCore::AuthenticatorTransport>, WTF::StrongEnumHashTraits<WebCore::AuthenticatorTransport>>;

    static Ref<WebAuthenticationPanel> create(const WebKit::AuthenticatorManager&, const WTF::String& rpId, const TransportSet&, WebCore::ClientDataType);
    ~WebAuthenticationPanel();

    WTF::String rpId() const { return m_rpId; }
    const Vector<WebCore::AuthenticatorTransport>& transports() const { return m_transports; }
    WebCore::ClientDataType clientDataType() const { return m_clientDataType; }
    void cancel() const;

    const WebAuthenticationPanelClient& client() const { return m_client.get(); }
    void setClient(UniqueRef<WebAuthenticationPanelClient>&&);

private:
    WebAuthenticationPanel(const WebKit::AuthenticatorManager&, const WTF::String& rpId, const TransportSet&, WebCore::ClientDataType);

    WeakPtr<WebKit::AuthenticatorManager> m_manager;
    WTF::String m_rpId;
    UniqueRef<WebAuthenticationPanelClient> m_client;
    Vector<WebCore::AuthenticatorTransport> m_transports;
    WebCore::ClientDataType m_clientDataType;
};

} // namespace API

#endif // ENABLE(WEB_AUTHN)
