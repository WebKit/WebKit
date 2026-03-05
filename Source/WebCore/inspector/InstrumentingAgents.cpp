/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#include "InstrumentingAgents.h"
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMallocInlines.h>
namespace WebCore {

using namespace Inspector;

Ref<InstrumentingAgents> InstrumentingAgents::create(Inspector::InspectorEnvironment& environment)
{
    return adoptRef(*new InstrumentingAgents(environment, nullptr));
}

Ref<InstrumentingAgents> InstrumentingAgents::create(Inspector::InspectorEnvironment& environment, InstrumentingAgents& fallbackAgents)
{
    return adoptRef(*new InstrumentingAgents(environment, &fallbackAgents));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(InstrumentingAgents);

InstrumentingAgents::InstrumentingAgents(InspectorEnvironment& environment, InstrumentingAgents* fallbackAgents)
    : m_environment(environment)
    , m_fallbackAgents(fallbackAgents)
{
}

bool InstrumentingAgents::developerExtrasEnabled() const
{
    return protect(m_environment)->developerExtrasEnabled();
}

void InstrumentingAgents::reset()
{
#define RESET_MEMBER_VARIABLE_FOR_INSPECTOR_AGENT(Class, Name, Getter, Setter) \
    m_##Getter##Name = nullptr; \

FOR_EACH_INSPECTOR_AGENT(RESET_MEMBER_VARIABLE_FOR_INSPECTOR_AGENT)
#undef RESET_MEMBER_VARIABLE_FOR_INSPECTOR_AGENT
}

// FIXME: <https://webkit.org/b/300646> To ease the transition of agents and functionalities from page target
// to frame target, we added this fallback mechanism to let the frame use its page's agents as delegates
// for agents not yet supported. Remove this once we complete implementing/migrating the frame target's agents.
#define DEFINE_GETTER_SETTER_FOR_INSPECTOR_AGENT(Class, Name, Getter, Setter) \
Class* InstrumentingAgents::Getter##Name() const \
{ \
    if (m_##Getter##Name) \
        return m_##Getter##Name; \
    if (RefPtr fallbackAgents = m_fallbackAgents.get()) \
        return fallbackAgents->Getter##Name(); \
    return nullptr; \
} \
void InstrumentingAgents::set##Setter##Name(Class* agent) \
{ \
    m_##Getter##Name = agent; \
}

FOR_EACH_INSPECTOR_AGENT(DEFINE_GETTER_SETTER_FOR_INSPECTOR_AGENT)
#undef DEFINE_GETTER_SETTER_FOR_INSPECTOR_AGENT

} // namespace WebCore
