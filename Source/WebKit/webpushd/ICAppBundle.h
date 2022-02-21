/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(INSTALL_COORDINATION_BUNDLES)

#import "InstallCoordinationSPI.h"
#import "PushAppBundle.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/WTFString.h>

@class NSError;
@class _WKAppInstallCoordinatorObserver;

namespace WebPushD {

class ClientConnection;

class ICAppBundle : public PushAppBundle, public CanMakeWeakPtr<ICAppBundle> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ICAppBundle> create(ClientConnection& connection, const String& originString, PushAppBundleClient& client)
    {
        return adoptRef(*new ICAppBundle(connection, originString, client));
    }

    void bundleCreationSucceeded();
    void bundleCreationFailed(NSError *);

    static void getOriginsWithRegistrations(ClientConnection&, CompletionHandler<void(const Vector<String>&)>&&);

private:
    ICAppBundle(ClientConnection&, const String& originString, PushAppBundleClient&);

    const String& getBundleIdentifier();

    void checkForExistingBundle() final;
    void deleteExistingBundle() final;
    void createBundle() final;
    void stop() final;

    void didDeleteExistingBundleWithError(NSError *);

    RetainPtr<IXRestoringDemotedAppInstallCoordinator> m_appInstallCoordinator;
    RetainPtr<_WKAppInstallCoordinatorObserver> m_appInstallObserver;

    String m_originString;
    RefPtr<ClientConnection> m_clientConnection;

    String m_bundleIdentifier;
};

} // namespace WebPushD

#endif // ENABLE(INSTALL_COORDINATION_BUNDLES)
