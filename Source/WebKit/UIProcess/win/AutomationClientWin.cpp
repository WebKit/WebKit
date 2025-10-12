/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "AutomationClientWin.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "AutomationSessionClientWin.h"
#include "WebAutomationSession.h"
#include <wtf/RunLoop.h>

namespace WebKit {

AutomationClient::AutomationClient(WebProcessPool& processPool)
    : m_processPool(processPool)
{
    Inspector::RemoteInspector::singleton().setClient(this);
}

AutomationClient::~AutomationClient()
{
    Inspector::RemoteInspector::singleton().setClient(nullptr);
}

RefPtr<WebProcessPool> AutomationClient::protectedProcessPool() const
{
    if (RefPtr processPool = m_processPool.get())
        return processPool;

    return nullptr;
}

void AutomationClient::requestAutomationSession(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
{
    ASSERT(isMainRunLoop());

    auto session = adoptRef(new WebAutomationSession());
    session->setSessionIdentifier(sessionIdentifier);
    session->setClient(WTF::makeUnique<AutomationSessionClient>(sessionIdentifier, capabilities));
    m_processPool->setAutomationSession(WTFMove(session));
}

void AutomationClient::closeAutomationSession()
{
    RunLoop::mainSingleton().dispatch([this] {
        auto processPool = protectedProcessPool();
        if (!processPool || !processPool->automationSession())
            return;

        processPool->automationSession()->setClient(nullptr);
        processPool->setAutomationSession(nullptr);
        processPool->setPagesControlledByAutomation(false);
    });
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
