/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#import "AuthenticationManager.h"

#if HAVE(SEC_KEY_PROXY)

#import "AuthenticationChallengeDisposition.h"
#import "ClientCertificateAuthenticationXPCConstants.h"
#import "Connection.h"
#import "XPCUtilities.h"
#import <WebCore/Credential.h>
#import <pal/spi/cocoa/NSXPCConnectionSPI.h>
#import <pal/spi/cocoa/SecKeyProxySPI.h>
#import <wtf/MainThread.h>
#import <wtf/darwin/XPCExtras.h>

namespace WebKit {

void AuthenticationManager::initializeConnection(IPC::Connection* connection)
{
    RELEASE_ASSERT(isMainRunLoop());

    if (!connection || !connection->xpcConnection()) {
        ASSERT_NOT_REACHED();
        return;
    }

    WeakPtr weakThis { *this };
    // The following xpc event handler overwrites the boostrap event handler and is only used
    // to capture client certificate credential.
    xpc_connection_set_event_handler(protect(connection->xpcConnection()).get(), ^(xpc_object_t event) {
#if USE(EXIT_XPC_MESSAGE_WORKAROUND)
        handleXPCExitMessage(event);
#endif
        callOnMainRunLoop([event = OSObjectPtr(event), weakThis = weakThis] {
            RELEASE_ASSERT(isMainRunLoop());

            xpc_type_t type = xpc_get_type(event.get());
            if (type == XPC_TYPE_ERROR || !weakThis)
                return;

            if (type != XPC_TYPE_DICTIONARY || xpcDictionaryGetString(event.get(), ClientCertificateAuthentication::XPCMessageNameKey) != ClientCertificateAuthentication::XPCMessageNameValue) {
                ASSERT_NOT_REACHED();
                return;
            }

            auto challengeID = xpc_dictionary_get_uint64(event.get(), ClientCertificateAuthentication::XPCChallengeIDKey);
            if (!challengeID)
                return;

            OSObjectPtr<xpc_object_t> xpcEndPoint = xpc_dictionary_get_value(event.get(), ClientCertificateAuthentication::XPCSecKeyProxyEndpointKey);
            if (!xpcEndPoint || xpc_get_type(xpcEndPoint.get()) != XPC_TYPE_ENDPOINT)
                return;
            auto endPoint = adoptNS([[NSXPCListenerEndpoint alloc] init]);
            [endPoint _setEndpoint:xpcEndPoint.get()];
            NSError *error = nil;
            auto identity = adoptCF([SecKeyProxy createIdentityFromEndpoint:endPoint.get() error:&error]);
            if (!identity || error) {
                LOG_ERROR("Couldn't create identity from end point: %@", error);
                return;
            }

            OSObjectPtr<xpc_object_t> certificateDataArray = xpc_dictionary_get_array(event.get(), ClientCertificateAuthentication::XPCCertificatesKey);
            if (!certificateDataArray)
                return;
            RetainPtr<NSMutableArray> certificates;
            if (auto total = xpc_array_get_count(certificateDataArray.get())) {
                certificates = [NSMutableArray arrayWithCapacity:total];
                for (size_t i = 0; i < total; i++) {
                    OSObjectPtr<xpc_object_t> certificateData = xpc_array_get_value(certificateDataArray.get(), i);
                    RetainPtr cfData = adoptCF(CFDataCreate(nullptr, static_cast<const UInt8*>(xpc_data_get_bytes_ptr(certificateData.get())), xpc_data_get_length(certificateData.get())));
                    RetainPtr certificate = adoptCF(SecCertificateCreateWithData(nullptr, cfData.get()));
                    if (!certificate)
                        return;
                    [certificates addObject:(__bridge id)certificate.get()];
                }
            }

            auto persistence = xpc_dictionary_get_uint64(event.get(), ClientCertificateAuthentication::XPCPersistenceKey);
            if (persistence > static_cast<uint64_t>(NSURLCredentialPersistenceSynchronizable))
                return;

            weakThis->completeAuthenticationChallenge(ObjectIdentifier<AuthenticationChallengeIdentifierType>(challengeID), AuthenticationChallengeDisposition::UseCredential, WebCore::Credential(adoptNS([[NSURLCredential alloc] initWithIdentity:identity.get() certificates:certificates.get() persistence:(NSURLCredentialPersistence)persistence]).get()));
        });
    });
}

} // namespace WebKit

#endif
