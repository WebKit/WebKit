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

#import "config.h"

#import "EnvironmentUtilities.h"
#import "WKBase.h"
#import "WebAuthnProcess.h"
#import "XPCServiceEntryPoint.h"

#if ENABLE(WEB_AUTHN)

namespace WebKit {

class WebAuthnServiceInitializerDelegate : public XPCServiceInitializerDelegate {
public:
    WebAuthnServiceInitializerDelegate(OSObjectPtr<xpc_connection_t> connection, xpc_object_t initializerMessage)
        : XPCServiceInitializerDelegate(WTFMove(connection), initializerMessage)
    {
    }
};

template<>
void initializeAuxiliaryProcess<WebAuthnProcess>(AuxiliaryProcessInitializationParameters&& parameters)
{
    static NeverDestroyed<WebAuthnProcess> WebAuthnProcess(WTFMove(parameters));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

extern "C" WK_EXPORT void WEBAUTHN_SERVICE_INITIALIZER(xpc_connection_t connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage);

void WEBAUTHN_SERVICE_INITIALIZER(xpc_connection_t connection, xpc_object_t initializerMessage, xpc_object_t priorityBoostMessage)
{
    WTF::initializeMainThread();

#if ENABLE(WEB_AUTHN)
    WebKit::XPCServiceInitializer<WebKit::WebAuthnProcess, WebKit::WebAuthnServiceInitializerDelegate>(connection, initializerMessage, priorityBoostMessage);
#endif // ENABLE(WEB_AUTHN)
}
