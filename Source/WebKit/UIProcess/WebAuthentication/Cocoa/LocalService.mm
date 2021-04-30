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
#import "LocalService.h"

#if ENABLE(WEB_AUTHN)

#import "LocalAuthenticator.h"
#import "LocalConnection.h"

#import "AppAttestInternalSoftLink.h"
#import "LocalAuthenticationSoftLink.h"

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/LocalServiceAdditions.h>
#else
#define LOCAL_SERVICE_ADDITIONS
#endif

namespace WebKit {

LocalService::LocalService(Observer& observer)
    : AuthenticatorTransportService(observer)
{
}

bool LocalService::isAvailable()
{
LOCAL_SERVICE_ADDITIONS

    auto context = adoptNS([allocLAContextInstance() init]);
    NSError *error = nil;
    auto result = [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error];
    if ((!result || error) && error.code != LAErrorBiometryLockout) {
        LOG_ERROR("Couldn't find local authenticators: %@", error);
        return false;
    }

#if HAVE(APPLE_ATTESTATION)
    if (!AppAttest_WebAuthentication_IsSupported()) {
        LOG_ERROR("Device is unable to support Apple attestation features.");
        return false;
    }
#else
    return false;
#endif

    return true;
}

void LocalService::startDiscoveryInternal()
{
    if (!platformStartDiscovery() || !observer())
        return;
    observer()->authenticatorAdded(LocalAuthenticator::create(createLocalConnection()));
}

bool LocalService::platformStartDiscovery() const
{
    return LocalService::isAvailable();
}

UniqueRef<LocalConnection> LocalService::createLocalConnection() const
{
    return makeUniqueRef<LocalConnection>();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
