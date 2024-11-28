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
#import "VirtualLocalConnection.h"

#if ENABLE(WEB_AUTHN)

#import "VirtualAuthenticatorConfiguration.h"
#import <JavaScriptCore/ArrayBuffer.h>
#import <Security/SecItem.h>
#import <WebCore/AuthenticatorAssertionResponse.h>
#import <WebCore/ExceptionData.h>
#import <wtf/Ref.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/SecuritySPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/WTFString.h>

#import "LocalAuthenticationSoftLink.h"

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(VirtualLocalConnection);

Ref<VirtualLocalConnection> VirtualLocalConnection::create(const VirtualAuthenticatorConfiguration& configuration)
{
    return adoptRef(*new VirtualLocalConnection(configuration));
}

VirtualLocalConnection::VirtualLocalConnection(const VirtualAuthenticatorConfiguration& configuration)
    : m_configuration(configuration)
{
}

void VirtualLocalConnection::verifyUser(const String&, ClientDataType, SecAccessControlRef, WebCore::UserVerificationRequirement, UserVerificationCallback&& callback)
{
    // Mock async operations.
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, callback = WTFMove(callback)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback(UserVerification::No, adoptNS([allocLAContextInstance() init]).get());
            return;
        }
        ASSERT(protectedThis->m_configuration.transport == AuthenticatorTransport::Internal);

        UserVerification userVerification = protectedThis->m_configuration.isUserVerified ? UserVerification::Yes : UserVerification::Presence;

        callback(userVerification, adoptNS([allocLAContextInstance() init]).get());
    });
}

void VirtualLocalConnection::verifyUser(SecAccessControlRef, LAContext *, CompletionHandler<void(UserVerification)>&& callback)
{
    // Mock async operations.
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, callback = WTFMove(callback)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback(UserVerification::No);
            return;
        }
        ASSERT(protectedThis->m_configuration.transport == AuthenticatorTransport::Internal);

        UserVerification userVerification = protectedThis->m_configuration.isUserVerified ? UserVerification::Yes : UserVerification::Presence;

        callback(userVerification);
    });
}

void VirtualLocalConnection::filterResponses(Vector<Ref<AuthenticatorAssertionResponse>>& responses) const
{
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
