/*
 * Copyright (C) 2018 Igalia S.L.
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

#include "config.h"
#include "WebProcessProxy.h"

#include "UserMessage.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <signal.h>
#include <sys/types.h>
#include <wtf/FileSystem.h>


namespace WebKit {
using namespace WebCore;

void WebProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.extraInitializationData.set("enable-sandbox"_s, m_processPool->sandboxEnabled() ? "true"_s : "false"_s);

    if (m_processPool->sandboxEnabled()) {
        WebsiteDataStore* dataStore = websiteDataStore();
        if (!dataStore) {
            // Prewarmed processes don't have a WebsiteDataStore yet, so use the primary WebsiteDataStore from the WebProcessPool.
            // The process won't be used if current WebsiteDataStore is different than the WebProcessPool primary one.
            dataStore = WebsiteDataStore::defaultDataStore().ptr();
        }

        ASSERT(dataStore);
        dataStore->resolveDirectoriesIfNecessary();
        launchOptions.extraInitializationData.set("mediaKeysDirectory"_s, dataStore->resolvedMediaKeysDirectory());
        launchOptions.extraInitializationData.set("applicationCacheDirectory"_s, dataStore->resolvedApplicationCacheDirectory());

        launchOptions.extraSandboxPaths = m_processPool->sandboxPaths();
    }
}

void WebProcessProxy::sendMessageToWebContextWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    if (const auto& userMessageHandler = m_processPool->userMessageHandler())
        userMessageHandler(WTFMove(message), WTFMove(completionHandler));
}

void WebProcessProxy::sendMessageToWebContext(UserMessage&& message)
{
    sendMessageToWebContextWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebProcessProxy::platformSuspendProcess()
{
    auto id = processIdentifier();
    if (!id)
        return;

    RELEASE_LOG(Process, "%p - [PID=%i] WebProcessProxy::platformSuspendProcess", this, id);
    kill(id, SIGSTOP);
}

void WebProcessProxy::platformResumeProcess()
{
    auto id = processIdentifier();
    if (!id)
        return;

    RELEASE_LOG(Process, "%p - [PID=%i] WebProcessProxy::platformResumeProcess", this, id);
    kill(id, SIGCONT);
}

} // namespace WebKit
