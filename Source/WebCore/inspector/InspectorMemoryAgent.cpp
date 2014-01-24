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

#include "InspectorMemoryAgent.h"

#include "CharacterData.h"
#include "Document.h"
#include "EventListenerMap.h"
#include "Frame.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "MemoryCache.h"
#include "Node.h"
#include "NodeTraversal.h"
#include "ScriptProfiler.h"
#include "StyledElement.h"
#include <inspector/InspectorValues.h>
#include <runtime/ArrayBufferView.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

// Use a type alias instead of 'using' here which would cause a conflict on Mac.
typedef Inspector::TypeBuilder::Memory::MemoryBlock InspectorMemoryBlock;
typedef Inspector::TypeBuilder::Array<InspectorMemoryBlock> InspectorMemoryBlocks;

using namespace Inspector;

namespace WebCore {

InspectorMemoryAgent::~InspectorMemoryAgent()
{
}

void InspectorMemoryAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, InspectorBackendDispatcher* backendDispatcher)
{
    m_backendDispatcher = InspectorMemoryBackendDispatcher::create(backendDispatcher, this);
}

void InspectorMemoryAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_backendDispatcher.clear();
}

void InspectorMemoryAgent::getDOMCounters(ErrorString*, int* documents, int* nodes, int* jsEventListeners)
{
    *documents = InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
    *nodes = InspectorCounters::counterValue(InspectorCounters::NodeCounter);
    *jsEventListeners = ThreadLocalInspectorCounters::current().counterValue(ThreadLocalInspectorCounters::JSEventListenerCounter);
}

InspectorMemoryAgent::InspectorMemoryAgent(InstrumentingAgents* instrumentingAgents)
    : InspectorAgentBase(ASCIILiteral("Memory"), instrumentingAgents)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
