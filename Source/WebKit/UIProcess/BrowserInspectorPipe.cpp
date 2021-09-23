/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BrowserInspectorPipe.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "InspectorPlaywrightAgent.h"
#include "InspectorPlaywrightAgentClient.h"
#include "RemoteInspectorPipe.h"
#include "WebKit2Initialize.h"
#include <wtf/NeverDestroyed.h>

namespace WebKit {

void initializeBrowserInspectorPipe(std::unique_ptr<InspectorPlaywrightAgentClient> client)
{
    // Initialize main loop before creating inspecor agent and pipe queues.
    WebKit::InitializeWebKit2();

    class BrowserInspectorPipe {
    public:
        BrowserInspectorPipe(std::unique_ptr<InspectorPlaywrightAgentClient> client)
            : m_playwrightAgent(std::move(client))
            , m_remoteInspectorPipe(m_playwrightAgent)
        {
        }

        InspectorPlaywrightAgent m_playwrightAgent;
        RemoteInspectorPipe m_remoteInspectorPipe;
    };

    static NeverDestroyed<BrowserInspectorPipe> pipe(std::move(client));
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
