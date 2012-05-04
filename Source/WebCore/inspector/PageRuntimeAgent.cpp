/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#if ENABLE(INSPECTOR)

#include "PageRuntimeAgent.h"

#include "Console.h"
#include "InspectorPageAgent.h"
#include "Page.h"
#include "ScriptState.h"

namespace WebCore {

PageRuntimeAgent::PageRuntimeAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, InjectedScriptManager* injectedScriptManager, Page* page, InspectorPageAgent* pageAgent)
    : InspectorRuntimeAgent(instrumentingAgents, state, injectedScriptManager)
    , m_inspectedPage(page)
    , m_pageAgent(pageAgent)
{
}

PageRuntimeAgent::~PageRuntimeAgent()
{
}

ScriptState* PageRuntimeAgent::scriptStateForEval(ErrorString* errorString, const String* frameId)
{
    if (!frameId)
        return mainWorldScriptState(m_inspectedPage->mainFrame());

    Frame* frame = m_pageAgent->frameForId(*frameId);
    if (!frame) {
        *errorString = "Frame with given id not found.";
        return 0;
    }
    return mainWorldScriptState(frame);
}

void PageRuntimeAgent::muteConsole()
{
    Console::mute();
}

void PageRuntimeAgent::unmuteConsole()
{
    Console::unmute();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
