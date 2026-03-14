/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "ICStats.h"

#include <cstdlib>
#include <mutex>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ICStats);

bool ICEvent::operator<(const ICEvent& other) const
{
    if (m_classInfo != other.m_classInfo) {
        if (!m_classInfo)
            return true;
        if (!other.m_classInfo)
            return false;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        return strcmp(m_classInfo->className, other.m_classInfo->className) < 0;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    if (m_kind != other.m_kind)
        return m_kind < other.m_kind;

    return m_propertyLocation < other.m_propertyLocation;
}

void ICEvent::dump(PrintStream& out) const
{
    out.print(m_kind, "(", m_classInfo ? m_classInfo->className : "<null>", ")");
    if (m_propertyLocation != Unknown)
        out.print(m_propertyLocation == BaseObject ? " self" : " proto lookup");
}

void ICEvent::log() const
{
    ICStats::singleton().add(*this);
}

Atomic<ICStats*> ICStats::s_instance;

ICStats::ICStats()
{
    std::atexit([] {
        ICStats* stats = s_instance.load();
        if (!stats)
            return;
        dataLog("ICStats at exit:\n");
        Locker spectrumLocker { stats->m_spectrum.getLock() };
        auto list = stats->m_spectrum.buildList(spectrumLocker);
        for (unsigned i = list.size(); i--;)
            dataLog("    ", *list[i].key, ": ", list[i].count, "\n");
    });

    m_thread = Thread::create(
        "JSC ICStats"_s,
        [this] () {
            Locker locker { m_lock };
            for (;;) {
                m_condition.waitFor(
                    m_lock, Seconds(1), [this] () -> bool { return m_shouldStop; });
                if (m_shouldStop)
                    break;
                
                dataLog("ICStats:\n");
                {
                    Locker spectrumLocker { m_spectrum.getLock() };
                    auto list = m_spectrum.buildList(spectrumLocker);
                    for (unsigned i = list.size(); i--;)
                        dataLog("    ", *list[i].key, ": ", list[i].count, "\n");
                }
            }
        });
}

ICStats::~ICStats()
{
    {
        Locker locker { m_lock };
        m_shouldStop = true;
        m_condition.notifyAll();
    }
    
    m_thread->waitForCompletion();
}

void ICStats::add(const ICEvent& event)
{
    if (JSC::activeJSGlobalObjectSignpostIntervalCount.load())
        m_spectrum.add(event);
}

ICStats& ICStats::singleton()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        s_instance.store(new ICStats());
    });
    return *s_instance.load();
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, ICEvent::Kind kind)
{
    switch (kind) {
#define ICEVENT_KIND_DUMP(name) case ICEvent::name: out.print(#name); return;
        FOR_EACH_ICEVENT_KIND(ICEVENT_KIND_DUMP);
#undef ICEVENT_KIND_DUMP
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF


