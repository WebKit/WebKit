/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebProcessManager.h"

#include "WebContext.h"

namespace WebKit {

WebProcessManager& WebProcessManager::shared()
{
    static WebProcessManager& manager = *new WebProcessManager;
    return manager;
}

WebProcessManager::WebProcessManager()
{
}

WebProcessProxy* WebProcessManager::getWebProcess(WebContext* context)
{
    switch (context->processModel()) {
        case ProcessModelSharedSecondaryProcess: {
            if (!m_sharedProcess)
                m_sharedProcess = WebProcessProxy::create(context);
            return m_sharedProcess.get();
        }
        case ProcessModelSharedSecondaryThread: {
            if (!m_sharedThread)
                m_sharedThread = WebProcessProxy::create(context);
            return m_sharedThread.get();
        }
        case ProcessModelSecondaryProcess: {
            std::pair<ProcessMap::iterator, bool> result = m_processMap.add(context, 0);
            if (result.second) {
                ASSERT(!result.first->second);
                result.first->second = WebProcessProxy::create(context);
            }

            ASSERT(result.first->second);
            return result.first->second.get();
        }
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

void WebProcessManager::processDidClose(WebProcessProxy* process, WebContext* context)
{
    if (process == m_sharedProcess) {
        ASSERT(context->processModel() == ProcessModelSharedSecondaryProcess);
        m_sharedProcess = 0;
        return;
    }

    ProcessMap::iterator it = m_processMap.find(context);
    if (it != m_processMap.end()) {
        ASSERT(it->second == process);
        m_processMap.remove(it);
        return;
    }

    // The shared thread connection should never be closed.
    ASSERT_NOT_REACHED();
}

void WebProcessManager::contextWasDestroyed(WebContext* context)
{
    m_processMap.remove(context);
}

} // namespace WebKit
