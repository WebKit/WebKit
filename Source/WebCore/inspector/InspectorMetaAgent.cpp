/*
* Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "InspectorMetaAgent.h"

#if ENABLE(INSPECTOR)

#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include <wtf/HashSet.h>

namespace WebCore {

InspectorMetaAgent::~InspectorMetaAgent()
{
}

void InspectorMetaAgent::getCapabilities(ErrorString*, const RefPtr<InspectorArray>& domainNames, RefPtr<InspectorArray>* capabilities)
{
    HashSet<String> requestedDomains;
    for (InspectorArray::iterator it = domainNames->begin(); it != domainNames->end(); ++it) {
        String value;
        if (it->get()->asString(&value))
            requestedDomains.add(value);
    }
    for (InspectorAgents::iterator it = m_agents->begin(); it != m_agents->end(); ++it) {
        InspectorBaseAgentInterface* agent = it->get();
        if (!requestedDomains.contains(agent->name()))
            continue;

        RefPtr<InspectorObject> domainDescriptor = InspectorObject::create();
        domainDescriptor->setString("domainName", agent->name());
        RefPtr<InspectorArray> agentCapabilities = InspectorArray::create();
        agent->getCapabilities(agentCapabilities.get());
        domainDescriptor->setArray("capabilities", agentCapabilities);
        (*capabilities)->pushObject(domainDescriptor);
    }
}

InspectorMetaAgent::InspectorMetaAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, InspectorAgents* agents)
    : InspectorBaseAgent<InspectorMetaAgent>("Meta", instrumentingAgents, state)
    , m_agents(agents)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
