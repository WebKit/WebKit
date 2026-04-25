/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessActivityGroup.h"

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProcessActivityGroup);

Ref<ProcessActivityGroup> ProcessActivityGroup::create(ProcessActivityGroupContext& context, ASCIILiteral name, ProcessThrottlerActivityType type, std::optional<Seconds> timeout)
{
    return adoptRef(*new ProcessActivityGroup(context, name, type, timeout));
}

ProcessActivityGroup::ProcessActivityGroup(ProcessActivityGroupContext& context, ASCIILiteral name, ProcessThrottlerActivityType type, std::optional<Seconds> timeout)
    : m_context(context)
    , m_name(name)
    , m_type(type)
    , m_timer(RunLoop::Timer(RunLoop::mainSingleton(), "ProcessActivityGroup timer"_s, this, &ProcessActivityGroup::activityTimedOut))
    , m_timeout(timeout)
{
    auto processes = context.activityTargets();
    for (Ref process : processes)
        m_activities.add(process->coreProcessIdentifier(), createActivity(process));

    context.addProcessActivityGroup(*this);

    if (m_timeout)
        m_timer.startOneShot(*m_timeout);
}

ProcessActivityGroup::~ProcessActivityGroup()
{
    if (CheckedPtr context = m_context.get())
        context->removeProcessActivityGroup(*this);
}

Ref<ProcessThrottler::Activity> ProcessActivityGroup::createActivity(WebProcessProxy& process)
{
    if (m_type == ProcessThrottlerActivityType::Foreground)
        return protect(process.throttler())->foregroundActivity(m_name);
    return protect(process.throttler())->backgroundActivity(m_name);
}

void ProcessActivityGroup::addActivityForTarget(WebProcessProxy& target)
{
    if (m_timeout.has_value() && !m_timer.isActive()) {
        // The timer has fired, so we should not add new activities.
        return;
    }
    m_activities.set(target.coreProcessIdentifier(), createActivity(target));
}

void ProcessActivityGroup::removeActivityForTarget(WebProcessProxy& target)
{
    m_activities.remove(target.coreProcessIdentifier());
}

void ProcessActivityGroup::activityTimedOut()
{
    m_activities.clear();
}

Ref<ProcessActivityGroup> ProcessActivityGroupContext::foregroundProcessActivityGroup(ASCIILiteral name, std::optional<Seconds> timeout)
{
    return ProcessActivityGroup::create(*this, name, ProcessThrottlerActivityType::Foreground, timeout);
}

void ProcessActivityGroupContext::addProcessActivityGroup(ProcessActivityGroup& activityGroup)
{
    m_processActivityGroups.add(activityGroup);
}

void ProcessActivityGroupContext::removeProcessActivityGroup(ProcessActivityGroup& activityGroup)
{
    m_processActivityGroups.remove(activityGroup);
}

void ProcessActivityGroupContext::didAddActivityTarget(WebProcessProxy& target)
{
    m_processActivityGroups.forEach([&](auto& activityGroup) {
        activityGroup.addActivityForTarget(target);
    });
}

void ProcessActivityGroupContext::didRemoveActivityTarget(WebProcessProxy& target)
{
    m_processActivityGroups.forEach([&](auto& activityGroup) {
        activityGroup.removeActivityForTarget(target);
    });
}

}
