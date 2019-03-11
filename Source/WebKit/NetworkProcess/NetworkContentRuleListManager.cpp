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

#include "config.h"
#include "NetworkContentRuleListManager.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "WebCompiledContentRuleList.h"

namespace WebKit {
using namespace WebCore;

NetworkContentRuleListManager::NetworkContentRuleListManager(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
{
}

NetworkContentRuleListManager::~NetworkContentRuleListManager()
{
    auto pendingCallbacks = WTFMove(m_pendingCallbacks);
    if (pendingCallbacks.isEmpty())
        return;

    WebCore::ContentExtensions::ContentExtensionsBackend backend;
    for (auto& callbacks : pendingCallbacks.values()) {
        for (auto& callback : callbacks)
            callback(backend);
    }
}

void NetworkContentRuleListManager::contentExtensionsBackend(UserContentControllerIdentifier identifier, BackendCallback&& callback)
{
    auto iterator = m_contentExtensionBackends.find(identifier);
    if (iterator != m_contentExtensionBackends.end()) {
        callback(*iterator->value);
        return;
    }
    m_pendingCallbacks.ensure(identifier, [] {
        return Vector<BackendCallback> { };
    }).iterator->value.append(WTFMove(callback));
    m_networkProcess.parentProcessConnection()->send(Messages::NetworkProcessProxy::ContentExtensionRules { identifier }, 0);
}

void NetworkContentRuleListManager::addContentRuleLists(UserContentControllerIdentifier identifier, const Vector<std::pair<String, WebCompiledContentRuleListData>>& contentRuleLists)
{
    auto& backend = *m_contentExtensionBackends.ensure(identifier, [] {
        return std::make_unique<WebCore::ContentExtensions::ContentExtensionsBackend>();
    }).iterator->value;

    for (const auto& contentRuleList : contentRuleLists) {
        WebCompiledContentRuleListData contentRuleListData = contentRuleList.second;
        auto compiledContentRuleList = WebCompiledContentRuleList::create(WTFMove(contentRuleListData));
        backend.addContentExtension(contentRuleList.first, WTFMove(compiledContentRuleList), ContentExtensions::ContentExtension::ShouldCompileCSS::No);
    }

    auto pendingCallbacks = m_pendingCallbacks.take(identifier);
    for (auto& callback : pendingCallbacks)
        callback(backend);

}

void NetworkContentRuleListManager::removeContentRuleList(UserContentControllerIdentifier identifier, const String& name)
{
    auto iterator = m_contentExtensionBackends.find(identifier);
    if (iterator == m_contentExtensionBackends.end())
        return;

    iterator->value->removeContentExtension(name);
}

void NetworkContentRuleListManager::removeAllContentRuleLists(UserContentControllerIdentifier identifier)
{
    auto iterator = m_contentExtensionBackends.find(identifier);
    if (iterator == m_contentExtensionBackends.end())
        return;

    iterator->value->removeAllContentExtensions();
}

void NetworkContentRuleListManager::remove(UserContentControllerIdentifier identifier)
{
    m_contentExtensionBackends.remove(identifier);
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
