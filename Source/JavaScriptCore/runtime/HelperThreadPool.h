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

#include <wtf/AutomaticThread.h>
#include <wtf/Box.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

enum class HelperThreadClass : uint8_t { GC, JIT, Wasm };
static constexpr unsigned numberOfHelperThreadClasses = 3;

ASCIILiteral helperThreadClassName(HelperThreadClass);

// Holding the HelperThreadPool's lock, which also guards its clients' scheduling state. A proxy because
// HelperThreadClient can't name that lock. Exists only for thread-safety analysis; the variable is never defined.
class WTF_CAPABILITY("helper thread pool lock") HelperThreadPoolLock { };
extern HelperThreadPoolLock& helperThreadPoolLock;

class HelperThreadClient {
public:
    enum class TaskResult : uint8_t { Ran, NoWork };

    virtual ~HelperThreadClient() = default;

    // A client that returns NoWork must republish its demand.
    virtual TaskResult runOneTask() WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock) = 0;

private:
    friend class HelperThreadPool;

    HelperThreadClass m_class WTF_GUARDED_BY_CAPABILITY(helperThreadPoolLock) { HelperThreadClass::GC };
    unsigned m_maxConcurrency WTF_GUARDED_BY_CAPABILITY(helperThreadPoolLock) { 0 };
    unsigned m_demand WTF_GUARDED_BY_CAPABILITY(helperThreadPoolLock) { 0 };
    unsigned m_running WTF_GUARDED_BY_CAPABILITY(helperThreadPoolLock) { 0 };
    unsigned m_blocked WTF_GUARDED_BY_CAPABILITY(helperThreadPoolLock) { 0 };
    bool m_isUnregistering WTF_GUARDED_BY_CAPABILITY(helperThreadPoolLock) { false };
};

// One process-wide pool of helper threads that is shared by GC marking, JS JIT compilation and Wasm
// compilation. Each class gets at least its minimum number of threads while it has work, even when
// the pool is at capacity, so no class can starve another one into a deadlock. The rest of the
// capacity is split by weight.
//
// The pool lock is a leaf lock: clients may call into the pool while holding their own locks, and the
// pool never calls into a client while holding its lock.
class HelperThreadPool {
    WTF_MAKE_NONCOPYABLE(HelperThreadPool);
    WTF_MAKE_TZONE_ALLOCATED(HelperThreadPool);
public:
    static HelperThreadPool& singleton();

    unsigned maxConcurrency(HelperThreadClass helperClass) const { return limits(helperClass).max; }

    void registerClient(HelperThreadClient&, HelperThreadClass, unsigned maxConcurrency) WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);
    void unregisterClient(HelperThreadClient&) WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);

    void setDemand(HelperThreadClient&, unsigned demand) WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);
    void waitUntilIdle(HelperThreadClient&) WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);

    static void didEnterVM() { s_numberOfEnteredVMs.fetch_add(1, std::memory_order_relaxed); }
    static void didExitVM();

    class BlockingScope {
        WTF_MAKE_NONCOPYABLE(BlockingScope);
    public:
        explicit BlockingScope(HelperThreadClient&);
        ~BlockingScope();
    private:
        HelperThreadPool* m_pool { nullptr };
        HelperThreadClient& m_client;
    };

    struct ClassStatistics {
        unsigned running { 0 };
        unsigned demand { 0 };
        unsigned peakRunning { 0 };
        uint64_t tasks { 0 };
        uint64_t tasksWithNoWork { 0 };
        uint64_t migrations { 0 };
    };

    struct Statistics {
        unsigned capacity { 0 };
        unsigned maxThreads { 0 };
        unsigned liveThreads { 0 };
        unsigned peakLiveThreads { 0 };
        unsigned running { 0 };
        unsigned blocked { 0 };
        uint64_t blockingEvents { 0 };
        std::array<ClassStatistics, numberOfHelperThreadClasses> classes;
    };
    Statistics statistics() const WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);

private:
    class Worker;
    friend class Worker;

    // Fixed when the pool is created.
    struct ClassLimits {
        unsigned min { 0 };
        unsigned max { 0 };
        unsigned weight { 1 };
    };

    struct ClassState {
        unsigned running { 0 };
        unsigned peakRunning { 0 };
        unsigned nextClientIndex { 0 };
        uint64_t tasks { 0 };
        uint64_t tasksWithNoWork { 0 };
        uint64_t migrations { 0 };
    };

    HelperThreadPool();

    const ClassLimits& limits(HelperThreadClass helperClass) const { return m_limits[static_cast<unsigned>(helperClass)]; }
    ClassState& classState(HelperThreadClass helperClass) WTF_REQUIRES_LOCK(*m_lock) { return m_classes[static_cast<unsigned>(helperClass)]; }
    const ClassState& classState(HelperThreadClass helperClass) const WTF_REQUIRES_LOCK(*m_lock) { return m_classes[static_cast<unsigned>(helperClass)]; }

    // Taking m_lock also gives helperThreadPoolLock, but the analysis can't see that.
    void assertIsHolding(const AbstractLocker&) const WTF_REQUIRES_LOCK(*m_lock) WTF_ASSERTS_ACQUIRED_CAPABILITY(helperThreadPoolLock) { }

    unsigned runnableTasks(const AbstractLocker&, HelperThreadClass) const WTF_REQUIRES_LOCK(*m_lock) WTF_REQUIRES_CAPABILITY(helperThreadPoolLock);
    HelperThreadClient* eligibleClient(const AbstractLocker&, HelperThreadClass) WTF_REQUIRES_LOCK(*m_lock) WTF_REQUIRES_CAPABILITY(helperThreadPoolLock);
    HelperThreadClient* selectClient(const AbstractLocker&) WTF_REQUIRES_LOCK(*m_lock) WTF_REQUIRES_CAPABILITY(helperThreadPoolLock);
    void wakeWorkers(const AbstractLocker&) WTF_REQUIRES_LOCK(*m_lock) WTF_REQUIRES_CAPABILITY(helperThreadPoolLock);
    unsigned liveThreads(const AbstractLocker&) const WTF_REQUIRES_LOCK(*m_lock);
    unsigned capacity() const { return capacity(s_numberOfEnteredVMs.load(std::memory_order_relaxed)); }
    unsigned capacity(unsigned numberOfEnteredVMs) const;
    unsigned currentMax(HelperThreadClass) const;

    void willBlock(HelperThreadClient&) WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);
    void didUnblock(HelperThreadClient&) WTF_EXCLUDES_CAPABILITY(helperThreadPoolLock);

    Box<Lock> m_lock;
    const Ref<AutomaticThreadCondition> m_workAvailable;
    Condition m_clientBecameIdle;
    // Created with the pool and never changed.
    Vector<Ref<Worker>> m_workers;
    Vector<HelperThreadClient*> m_clients WTF_GUARDED_BY_LOCK(*m_lock);
    std::array<ClassLimits, numberOfHelperThreadClasses> m_limits;
    std::array<ClassState, numberOfHelperThreadClasses> m_classes WTF_GUARDED_BY_LOCK(*m_lock);
    unsigned m_capacity { 0 };
    unsigned m_running WTF_GUARDED_BY_LOCK(*m_lock) { 0 };
    unsigned m_blocked WTF_GUARDED_BY_LOCK(*m_lock) { 0 };
    unsigned m_peakLiveThreads WTF_GUARDED_BY_LOCK(*m_lock) { 0 };
    uint64_t m_blockingEvents WTF_GUARDED_BY_LOCK(*m_lock) { 0 };

    static inline std::atomic<unsigned> s_numberOfEnteredVMs;
};

} // namespace JSC
