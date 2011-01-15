/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WebGeolocationManagerProxy_h
#define WebGeolocationManagerProxy_h

#include "APIObject.h"
#include "MessageID.h"
#include "WebGeolocationProvider.h"

namespace CoreIPC {
class ArgumentDecoder;
class Connection;
}

namespace WebKit {

class WebContext;
class WebGeolocationPosition;

class WebGeolocationManagerProxy : public APIObject {
public:
    static const Type APIType = TypeGeolocationManager;

    static PassRefPtr<WebGeolocationManagerProxy> create(WebContext*);
    virtual ~WebGeolocationManagerProxy();

    void invalidate();
    void clearContext() { m_context = 0; }

    void initializeProvider(const WKGeolocationProvider*);

    void providerDidChangePosition(WebGeolocationPosition*);
    void providerDidFailToDeterminePosition();

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

private:
    explicit WebGeolocationManagerProxy(WebContext*);

    virtual Type type() const { return APIType; }

    // Implemented in generated WebGeolocationManagerProxyMessageReceiver.cpp
    void didReceiveWebGeolocationManagerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    void startUpdating();
    void stopUpdating();

    bool m_isUpdating;

    WebContext* m_context;
    WebGeolocationProvider m_provider;
};

} // namespace WebKit

#endif // WebGeolocationManagerProxy_h
