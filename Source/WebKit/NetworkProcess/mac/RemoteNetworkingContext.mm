/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "RemoteNetworkingContext.h"

#import "CookieStorageUtilsCF.h"
#import "LegacyCustomProtocolManager.h"
#import "NetworkProcess.h"
#import "NetworkSession.h"
#import "NetworkSessionCreationParameters.h"
#import "WebErrors.h"
#import "WebsiteDataStoreParameters.h"
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/ResourceError.h>
#import <pal/SessionID.h>
#import <wtf/MainThread.h>
#import <wtf/text/StringConcatenateNumbers.h>

namespace WebKit {

void RemoteNetworkingContext::ensureWebsiteDataStoreSession(NetworkProcess& networkProcess, const WebsiteDataStoreParameters& parameters)
{
    auto sessionID = parameters.networkSessionParameters.sessionID;
    if (networkProcess.storageSession(sessionID))
        return;

    String base = networkProcess.uiProcessBundleIdentifier();
    if (base.isNull())
        base = [[NSBundle mainBundle] bundleIdentifier];

    if (!sessionID.isEphemeral())
        SandboxExtension::consumePermanently(parameters.cookieStoragePathExtensionHandle);

    RetainPtr<CFHTTPCookieStorageRef> uiProcessCookieStorage;
    if (!sessionID.isEphemeral() && !parameters.uiProcessCookieStorageIdentifier.isEmpty() && parameters.networkSessionParameters.sessionID != PAL::SessionID::defaultSessionID())
        uiProcessCookieStorage = cookieStorageFromIdentifyingData(parameters.uiProcessCookieStorageIdentifier);

    networkProcess.ensureSession(sessionID, parameters.networkSessionParameters.shouldUseTestingNetworkSession, makeString(base, '.', sessionID.toUInt64()), WTFMove(uiProcessCookieStorage));

    networkProcess.setSession(sessionID, NetworkSession::create(networkProcess, parameters.networkSessionParameters));
}

}
