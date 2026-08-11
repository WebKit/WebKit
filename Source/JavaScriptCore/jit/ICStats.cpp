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
#include <wtf/Hasher.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ICStats);

class ICHandlerChain {
public:
    ICHandlerChain() = default;

    ICHandlerChain(WTF::HashTableDeletedValueType)
        : m_isDeleted(true)
    {
    }

    void append(ICEvent::Kind kind) { m_chain.append(kind); }
    bool NODELETE isEmpty() const { return m_chain.isEmpty(); }
    void NODELETE clear()
    {
        m_chain.shrink(0);
        m_totalNumberOfHandlersInChain = 0;
    }
    unsigned NODELETE size() const { return m_chain.size(); }
    ICEvent::Kind operator[](unsigned i) const { return m_chain[i]; }

    void NODELETE setTotalNumberOfHandlersInChain(unsigned length) { m_totalNumberOfHandlersInChain = length; }
    unsigned NODELETE totalNumberOfHandlersInChain() const { return m_totalNumberOfHandlersInChain; }

    bool operator==(const ICHandlerChain& other) const
    {
        return m_chain == other.m_chain && m_totalNumberOfHandlersInChain == other.m_totalNumberOfHandlersInChain && m_isDeleted == other.m_isDeleted;
    }

    bool operator>(const ICHandlerChain& other) const
    {
        for (unsigned i = 0; i < std::min(m_chain.size(), other.m_chain.size()); i++) {
            if (m_chain[i] != other.m_chain[i])
                return m_chain[i] > other.m_chain[i];
        }
        if (m_chain.size() != other.m_chain.size())
            return m_chain.size() > other.m_chain.size();
        return m_totalNumberOfHandlersInChain > other.m_totalNumberOfHandlersInChain;
    }

    unsigned hash() const
    {
        return pairIntHash(computeHash(m_chain), WTF::IntHash<unsigned>::hash(m_totalNumberOfHandlersInChain));
    }

    bool isHashTableDeletedValue() const { return m_isDeleted; }

    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

private:
    Vector<ICEvent::Kind> m_chain;
    unsigned m_totalNumberOfHandlersInChain { 0 };
    bool m_isDeleted { false };
};

} // namespace JSC

namespace WTF {

template<> struct HashTraits<JSC::ICHandlerChain> : SimpleClassHashTraits<JSC::ICHandlerChain> {
    static constexpr bool emptyValueIsZero = true;
};

} // namespace WTF

namespace JSC {

static LazyNeverDestroyed<ICHandlerChain> s_currentChain;
static LazyNeverDestroyed<Spectrum<ICHandlerChain, uint64_t>> s_chainSpectrum;

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
    ASSERT(Options::useICStats());

    s_currentChain.construct();
    s_chainSpectrum.construct();

    std::atexit([] {
        ICStats& stats = singleton();
        dataLog("ICStats at exit:\n");
        Locker spectrumLocker { stats.m_spectrum.getLock() };
        auto list = stats.m_spectrum.buildList(spectrumLocker);
        uint64_t totalCount = 0;
        for (auto& entry : list)
            totalCount += entry.count;
        for (unsigned i = list.size(); i--;) {
            dataLog("    ", *list[i].key, ": ", list[i].count);
            if (totalCount)
                dataLogF(" (%.1f%%)", 100.0 * list[i].count / totalCount);
            dataLog("\n");
        }
        stats.dumpChains();
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
                    uint64_t totalCount = 0;
                    for (auto& entry : list)
                        totalCount += entry.count;
                    for (unsigned i = list.size(); i--;) {
                        dataLog("    ", *list[i].key, ": ", list[i].count);
                        if (totalCount)
                            dataLogF(" (%.1f%%)", 100.0 * list[i].count / totalCount);
                        dataLog("\n");
                    }
                }
                dumpChains();
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

static bool NODELETE shouldRecord()
{
    constexpr bool shouldMeasureOutsideSignpost = false;
    if (shouldMeasureOutsideSignpost)
        return true;
    return activeJSGlobalObjectSignpostIntervalCount.load();
}

void ICStats::add(const ICEvent& event)
{
    if (shouldRecord())
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

void ICStats::startNewChain(unsigned newChainLength)
{
    Locker locker { s_chainSpectrum->getLock() };
    if (!s_currentChain->isEmpty() && shouldRecord())
        s_chainSpectrum->add(locker, s_currentChain.get());
    s_currentChain->clear();
    s_currentChain->setTotalNumberOfHandlersInChain(newChainLength);
}

void ICStats::appendToCurrentChain(ICEvent::Kind kind)
{
    if (shouldRecord())
        s_currentChain->append(kind);
}

void ICStats::dumpChains()
{
    startNewChain(0);
    Locker locker { s_chainSpectrum->getLock() };
    auto list = s_chainSpectrum->buildList(locker);
    if (list.isEmpty())
        return;
    dataLog("IC Handler Chains:\n");
    for (unsigned i = list.size(); i--;) {
        auto& chain = *list[i].key;
        dataLog("    ", list[i].count, "x [");
        for (unsigned j = 0; j < chain.size(); j++) {
            if (j)
                dataLog(", ");
            printInternal(WTF::dataFile(), chain[j]);
        }
        dataLog("] (chain length: ", chain.totalNumberOfHandlersInChain(), ")\n");
    }

    auto dumpHistogram = [&](const char* title, auto keyFn) {
        UncheckedKeyHashMap<unsigned, uint64_t, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> histogram;
        uint64_t total = 0;
        for (unsigned i = 0; i < list.size(); i++) {
            histogram.add(keyFn(i), 0).iterator->value += list[i].count;
            total += list[i].count;
        }
        Vector<std::pair<unsigned, uint64_t>> entries;
        for (auto& [len, count] : histogram)
            entries.append({ len, count });
        std::sort(entries.begin(), entries.end());
        dataLog("\n", title, "\n");
        for (auto& [len, count] : entries) {
            dataLog("    ", len, ": ", count);
            if (total)
                dataLogF(" (%.1f%%)", 100.0 * count / total);
            dataLog("\n");
        }
    };
    dumpHistogram("IC Chain Length Histogram (total handlers in chain): ",
        [&](unsigned i) { return list[i].key->totalNumberOfHandlersInChain(); });
    dumpHistogram("IC Chain Length Histogram (executed handlers): ",
        [&](unsigned i) { return list[i].key->size(); });
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


