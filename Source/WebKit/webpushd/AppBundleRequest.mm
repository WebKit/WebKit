/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "AppBundleRequest.h"

#import "ICAppBundle.h"
#import "MockAppBundleForTesting.h"
#import "WebPushDaemon.h"
#import <JavaScriptCore/ConsoleTypes.h>

namespace WebPushD {

AppBundleRequest::AppBundleRequest(ClientConnection& connection, const String& originString)
    : m_connection(connection)
    , m_originString(originString)
{
}

AppBundleRequest::~AppBundleRequest()
{
    ASSERT(!m_appBundle);
}

void AppBundleRequest::start()
{
    ASSERT(m_connection);

    m_connection->broadcastDebugMessage(makeString("Starting ", transactionDescription(), " request for origin ", m_originString));

    m_transaction = adoptOSObject(os_transaction_create(transactionDescription()));

    if (m_connection->useMockBundlesForTesting())
        m_appBundle = MockAppBundleForTesting::create(m_originString, m_connection->hostAppCodeSigningIdentifier(), *this);
    else {
#if ENABLE(INSTALL_COORDINATION_BUNDLES)
        m_appBundle = ICAppBundle::create(*m_connection, m_originString, *this);
#else
        m_connection->broadcastDebugMessage("Client is trying to initiate app bundle request without having configured mock app bundles for testing. About to crash..."_s);
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    startInternal();
}

void AppBundleRequest::cancel()
{
    ASSERT(m_connection);

    if (m_appBundle) {
        m_appBundle->stop();
        m_appBundle = nullptr;
    }
}

void AppBundleRequest::cleanupAfterCompletionHandler()
{
    if (m_appBundle) {
        m_appBundle->detachFromClient();
        m_appBundle = nullptr;
    }

    m_transaction = nullptr;

    if (m_connection)
        m_connection->didCompleteAppBundleRequest(*this);
}

// App bundle permissions

void AppBundlePermissionsRequest::startInternal()
{
    m_appBundle->checkForExistingBundle();
}

void AppBundlePermissionsRequest::didCheckForExistingBundle(PushAppBundle& bundle, PushAppBundleExists exists)
{
    ASSERT_UNUSED(bundle, &bundle == m_appBundle.get());

    m_connection->broadcastDebugMessage(makeString("Origin ", m_originString, " app bundle request: didCheckForExistingBundle - ", exists == PushAppBundleExists::Yes ? "Exists" : "Does not exist"));

    if (exists == PushAppBundleExists::Yes)
        return callCompletionHandlerAndCleanup(true);

    m_appBundle->createBundle();
}

void AppBundlePermissionsRequest::didCreateAppBundle(PushAppBundle& bundle, PushAppBundleCreationResult result)
{
    ASSERT_UNUSED(bundle, &bundle == m_appBundle.get());

    m_connection->broadcastDebugMessage(makeString("Origin ", m_originString, " app bundle request: didCreateAppBundle - ", result == PushAppBundleCreationResult::Success ? "Created" : "Failed to create"));

    if (result == PushAppBundleCreationResult::Failure)
        return callCompletionHandlerAndCleanup(false);

    callCompletionHandlerAndCleanup(true);
}

// App bundle deletion

void AppBundleDeletionRequest::startInternal()
{
    m_appBundle->deleteExistingBundle();
}

void AppBundleDeletionRequest::didDeleteExistingBundleWithError(PushAppBundle& bundle, NSError *error)
{
    ASSERT_UNUSED(bundle, &bundle == m_appBundle.get());

    m_connection->broadcastDebugMessage(makeString("Origin ", m_originString, " app bundle request: didDeleteExistingBundleWithError"));

    if (error)
        m_connection->broadcastDebugMessage(makeString("Failed to delete app bundle: ", String([error description])));

    callCompletionHandlerAndCleanup(error ? String([error description]) : emptyString());
}

} // namespace WebPushD
