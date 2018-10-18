/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "Connection.h"
#include "MessageReceiver.h"
#include "WebContextSupplement.h"
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

namespace API {
class GeolocationProvider;
}

namespace WebKit {

class WebGeolocationPosition;
class WebProcessPool;

class WebGeolocationManagerProxy : public API::ObjectImpl<API::Object::Type::GeolocationManager>, public WebContextSupplement, private IPC::MessageReceiver {
public:
    static const char* supplementName();

    static Ref<WebGeolocationManagerProxy> create(WebProcessPool*);

    void setProvider(std::unique_ptr<API::GeolocationProvider>&&);

    void providerDidChangePosition(WebGeolocationPosition*);
    void providerDidFailToDeterminePosition(const String& errorMessage = String());
#if PLATFORM(IOS_FAMILY)
    void resetPermissions();
#endif

    using API::Object::ref;
    using API::Object::deref;

private:
    explicit WebGeolocationManagerProxy(WebProcessPool*);

    // WebContextSupplement
    void processPoolDestroyed() override;
    void processDidClose(WebProcessProxy*) override;
    void refWebContextSupplement() override;
    void derefWebContextSupplement() override;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    bool isUpdating() const { return !m_updateRequesters.isEmpty(); }
    bool isHighAccuracyEnabled() const { return !m_highAccuracyRequesters.isEmpty(); }

    void startUpdating(IPC::Connection&);
    void stopUpdating(IPC::Connection&);
    void removeRequester(const IPC::Connection::Client*);
    void setEnableHighAccuracy(IPC::Connection&, bool);

    HashSet<const IPC::Connection::Client*> m_updateRequesters;
    HashSet<const IPC::Connection::Client*> m_highAccuracyRequesters;

    std::unique_ptr<API::GeolocationProvider> m_provider;
};

} // namespace WebKit
