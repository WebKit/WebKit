/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "AuthenticationChallengeProxy.h"

#if HAVE(SEC_KEY_PROXY)

#import "ClientCertificateAuthenticationXPCConstants.h"
#import "Connection.h"
#import "SecKeyProxyStore.h"
#import <pal/spi/cocoa/NSXPCConnectionSPI.h>
#import <pal/spi/cocoa/SecKeyProxySPI.h>

namespace WebKit {

void AuthenticationChallengeProxy::sendClientCertificateCredentialOverXpc(uint64_t challengeID, const WebCore::Credential& credential) const
{
    ASSERT(m_secKeyProxyStore);
    ASSERT(m_secKeyProxyStore->isInitialized());

    auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), clientCertificateAuthenticationXPCMessageNameKey, clientCertificateAuthenticationXPCMessageNameValue);
    xpc_dictionary_set_uint64(message.get(), clientCertificateAuthenticationXPCChallengeIDKey, challengeID);
    xpc_dictionary_set_value(message.get(), clientCertificateAuthenticationXPCSecKeyProxyEndpointKey, m_secKeyProxyStore->get().endpoint._endpoint);
    auto certificateDataArray = adoptOSObject(xpc_array_create(nullptr, 0));
    for (id certificate in credential.nsCredential().certificates) {
        auto data = adoptCF(SecCertificateCopyData((SecCertificateRef)certificate));
        xpc_array_append_value(certificateDataArray.get(), adoptOSObject(xpc_data_create(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()))).get());
    }
    xpc_dictionary_set_value(message.get(), clientCertificateAuthenticationXPCCertificatesKey, certificateDataArray.get());
    xpc_dictionary_set_uint64(message.get(), clientCertificateAuthenticationXPCPersistenceKey, static_cast<uint64_t>(credential.nsCredential().persistence));

    xpc_connection_send_message(m_connection->xpcConnection(), message.get());
}

} // namespace WebKit

#endif
