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

#pragma once

#include "ProcessThrottler.h"
#include "WebProcessProxy.h"

#include <WebCore/ProcessIdentifier.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class ProcessActivityGroup;

class ProcessActivityGroupContext
: public CanMakeWeakPtr<ProcessActivityGroupContext>
, public CanMakeCheckedPtr<ProcessActivityGroupContext> {
    WTF_MAKE_TZONE_ALLOCATED(ProcessActivityGroupContext);
    WTF_MAKE_NONCOPYABLE(ProcessActivityGroupContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProcessActivityGroupContext);
public:
    ProcessActivityGroupContext() = default;
    virtual ~ProcessActivityGroupContext() { }

    Ref<ProcessActivityGroup> foregroundProcessActivityGroup(ASCIILiteral name, std::optional<Seconds> timeout = std::nullopt);

    virtual Vector<Ref<WebProcessProxy>> activityTargets() { return { }; }

    void didAddActivityTarget(WebProcessProxy&);
    void didRemoveActivityTarget(WebProcessProxy&);

    void addProcessActivityGroup(ProcessActivityGroup&);
    void removeProcessActivityGroup(ProcessActivityGroup&);

private:
    WeakHashSet<ProcessActivityGroup> m_processActivityGroups;
};

class ProcessActivityGroup : public RefCountedAndCanMakeWeakPtr<ProcessActivityGroup> {
    WTF_MAKE_TZONE_ALLOCATED(ProcessActivityGroup);
    WTF_MAKE_NONCOPYABLE(ProcessActivityGroup);
public:
    static Ref<ProcessActivityGroup> create(ProcessActivityGroupContext&, ASCIILiteral name, ProcessThrottlerActivityType, std::optional<Seconds> timeout);

    ~ProcessActivityGroup();

    void addActivityForTarget(WebProcessProxy&);
    void removeActivityForTarget(WebProcessProxy&);

    unsigned processActivityGroupSizeForTesting() const { return m_activities.size(); }

private:
    ProcessActivityGroup(ProcessActivityGroupContext&, ASCIILiteral name, ProcessThrottlerActivityType, std::optional<Seconds> timeout);

    Ref<ProcessThrottler::Activity> createActivity(WebProcessProxy&);
    void activityTimedOut();

    WeakPtr<ProcessActivityGroupContext> m_context;
    ASCIILiteral m_name;
    ProcessThrottlerActivityType m_type;
    HashMap<WebCore::ProcessIdentifier, Ref<ProcessThrottlerActivity>> m_activities;
    RunLoop::Timer m_timer;
    std::optional<Seconds> m_timeout;
};

}
