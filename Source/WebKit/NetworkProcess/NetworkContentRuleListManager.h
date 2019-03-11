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

#pragma once

#if ENABLE(CONTENT_EXTENSIONS)

#include "UserContentControllerIdentifier.h"
#include "WebCompiledContentRuleListData.h"
#include <WebCore/ContentExtensionsBackend.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class NetworkProcess;

class NetworkContentRuleListManager {
public:
    NetworkContentRuleListManager(NetworkProcess&);
    ~NetworkContentRuleListManager();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    using BackendCallback = CompletionHandler<void(WebCore::ContentExtensions::ContentExtensionsBackend&)>;
    void contentExtensionsBackend(UserContentControllerIdentifier, BackendCallback&&);

private:
    void addContentRuleLists(UserContentControllerIdentifier, Vector<std::pair<String, WebCompiledContentRuleListData>>&&);
    void removeContentRuleList(UserContentControllerIdentifier, const String& name);
    void removeAllContentRuleLists(UserContentControllerIdentifier);
    void remove(UserContentControllerIdentifier);

    HashMap<UserContentControllerIdentifier, std::unique_ptr<WebCore::ContentExtensions::ContentExtensionsBackend>> m_contentExtensionBackends;
    HashMap<UserContentControllerIdentifier, Vector<BackendCallback>> m_pendingCallbacks;
    NetworkProcess& m_networkProcess;
};

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
