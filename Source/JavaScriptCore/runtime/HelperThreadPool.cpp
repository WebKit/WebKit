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
#include "HelperThreadPool.h"

#include "CPU.h"
#include "Options.h"
#include <wtf/SequesteredAutomaticThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HelperThreadPool);

static constexpr unsigned minJITThreads = 4;
static constexpr unsigned maxJITThreads = 6;
static constexpr unsigned maxJITThreadsWithSeveralVMs = 4;
static constexpr unsigned minCapacity = 4;
static constexpr unsigned gcWeight = 4;
static constexpr unsigned jitWeight = 2;
static constexpr unsigned wasmWeight = 2;
static constexpr Seconds idleTimeout = 10_s;

ASCIILiteral helperThreadClassName(HelperThreadClass helperClass)
{
    switch (helperClass) {
    case HelperThreadClass::GC:
        return "GC"_s;
    case HelperThreadClass::JIT:
        return "JIT"_s;
    case HelperThreadClass::Wasm:
        return "Wasm"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if USE(PROTECTED_JIT_STACKS)
// FIXME: With PROTECTED_JIT_STACKS, every worker allocates a sequestered stack when it is created and keeps it for
// the life of the process, though only JIT tasks need one. Allocate it when the thread starts instead.
using HelperThreadPoolWorkerBase = SequesteredAutomaticThread;
#else
using HelperThreadPoolWorkerBase = AutomaticThread;
#endif

class HelperThreadPool::Worker final : public HelperThreadPoolWorkerBase {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Worker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(Worker);
public:
    static Ref<Worker> create(const AbstractLocker& locker, HelperThreadPool& pool)
    {
        return adoptRef(*new Worker(locker, pool));
    }

    bool isAwaitingWork(const AbstractLocker& locker)
    {
        return hasUnderlyingThread(locker) && !isWaiting(locker) && !m_client;
    }

    ASCIILiteral name() const final
    {
#if OS(LINUX)
        return "HelperThread"_s;
#else
        return "JSC Helper Thread"_s;
#endif
    }

private:
#if USE(PROTECTED_JIT_STACKS)
    Worker(const AbstractLocker& locker, HelperThreadPool& pool)
        : HelperThreadPoolWorkerBase(locker, pool.m_lock, pool.m_workAvailable.copyRef(), idleTimeout)
        , m_pool(pool)
    {
    }
#else
    Worker(const AbstractLocker& locker, HelperThreadPool& pool)
        : HelperThreadPoolWorkerBase(locker, pool.m_lock, pool.m_workAvailable.copyRef(), ThreadType::Compiler, compilerThreadQOS(), idleTimeout)
        , m_pool(pool)
    {
    }
#endif

    PollResult poll(const AbstractLocker& locker) final
    {
        assertIsHeld(*m_pool.m_lock);
        m_pool.assertIsHolding(locker);
        HelperThreadClient* client = m_pool.selectClient(locker);
        if (!client)
            return PollResult::Wait;

        auto& state = m_pool.classState(client->m_class);
        client->m_running++;
        state.running++;
        state.peakRunning = std::max(state.peakRunning, state.running);
        state.tasks++;
        if (m_lastClass && *m_lastClass != client->m_class)
            state.migrations++;
        m_lastClass = client->m_class;
        m_pool.m_running++;
        m_client = client;
        return PollResult::Work;
    }

    WorkResult work() final
    {
        Thread::setCurrentHelperThreadQOS(*m_lastClass == HelperThreadClass::GC ? Thread::defaultQOS : compilerThreadQOS());
        auto result = m_client->runOneTask();
        ASSERT(!Thread::mayBeGCThread());

        Locker locker { *m_pool.m_lock };
        m_pool.assertIsHolding(locker);
        auto& state = m_pool.classState(m_client->m_class);
        ASSERT(m_client->m_running && state.running && m_pool.m_running);
        m_client->m_running--;
        state.running--;
        m_pool.m_running--;
        if (result == HelperThreadClient::TaskResult::NoWork)
            state.tasksWithNoWork++;
        if (!m_client->m_running)
            m_pool.m_clientBecameIdle.notifyAll();
        m_client = nullptr;
        m_pool.wakeWorkers(locker);
        return WorkResult::Continue;
    }

    HelperThreadPool& m_pool;
    HelperThreadClient* m_client { nullptr };
    std::optional<HelperThreadClass> m_lastClass;
};

HelperThreadPool& HelperThreadPool::singleton()
{
    static HelperThreadPool* pool;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        pool = new HelperThreadPool;
    });
    return *pool;
}

HelperThreadPool::HelperThreadPool()
    : m_lock(Box<Lock>::create())
    , m_workAvailable(AutomaticThreadCondition::create())
{
    auto configure = [&](HelperThreadClass helperClass, unsigned max, unsigned min, unsigned weight) {
        auto& classLimits = m_limits[static_cast<unsigned>(helperClass)];
        classLimits.max = max;
        // Without a minimum of one, a class could wait forever for threads held by another class that is
        // itself waiting on it, such as GC marking that needs a mutator blocked on a synchronous compile.
        classLimits.min = std::clamp(min, std::min(1U, classLimits.max), classLimits.max);
        classLimits.weight = weight;
    };
    unsigned cores = kernTCSMAwareNumberOfProcessorCores();
    unsigned gcMax = Options::numberOfGCMarkers() ? Options::numberOfGCMarkers() - 1 : 0;
    unsigned wasmMax = Options::useConcurrentJIT() ? Options::numberOfWasmCompilerThreads() : 1;
    configure(HelperThreadClass::GC, gcMax, 1, gcWeight);
    configure(HelperThreadClass::JIT, std::min(maxJITThreads, cores), minJITThreads, jitWeight);
    configure(HelperThreadClass::Wasm, wasmMax, 1, wasmWeight);

    m_capacity = Options::helperThreadPoolTargetCapacity();

    // Threads granted by a class minimum may exceed capacity, and JIT compiles blocked by a GC suspension
    // do not use any.
    unsigned numberOfWorkers = m_capacity;
    for (auto& classLimits : m_limits)
        numberOfWorkers += classLimits.min;
    numberOfWorkers += limits(HelperThreadClass::JIT).max;

    Locker locker { *m_lock };
    for (unsigned i = 0; i < numberOfWorkers; ++i)
        m_workers.append(Worker::create(locker, *this));
}

void HelperThreadPool::registerClient(HelperThreadClient& client, HelperThreadClass helperClass, unsigned maxConcurrency)
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    ASSERT(!m_clients.contains(&client));
    client.m_class = helperClass;
    client.m_maxConcurrency = maxConcurrency;
    client.m_demand = 0;
    client.m_running = 0;
    client.m_blocked = 0;
    client.m_isUnregistering = false;
    m_clients.append(&client);
}

void HelperThreadPool::unregisterClient(HelperThreadClient& client)
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    client.m_isUnregistering = true;
    client.m_demand = 0;
    while (client.m_running)
        m_clientBecameIdle.wait(*m_lock);
    m_clients.removeFirst(&client);
}

void HelperThreadPool::setDemand(HelperThreadClient& client, unsigned demand)
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    if (client.m_isUnregistering)
        return;
    if (std::exchange(client.m_demand, demand) < demand)
        wakeWorkers(locker);
}

void HelperThreadPool::waitUntilIdle(HelperThreadClient& client)
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    while (client.m_running)
        m_clientBecameIdle.wait(*m_lock);
}

unsigned HelperThreadPool::runnableTasks(const AbstractLocker&, HelperThreadClass helperClass) const
{
    auto& state = classState(helperClass);
    unsigned shortfall = 0;
    for (auto* client : m_clients) {
        if (client->m_class != helperClass || client->m_blocked)
            continue;
        unsigned wanted = std::min(client->m_demand, client->m_maxConcurrency);
        if (wanted > client->m_running)
            shortfall += wanted - client->m_running;
    }
    unsigned max = currentMax(helperClass);
    unsigned room = max > state.running ? max - state.running : 0;
    return std::min(shortfall, room);
}

HelperThreadClient* HelperThreadPool::eligibleClient(const AbstractLocker&, HelperThreadClass helperClass)
{
    auto& state = classState(helperClass);
    if (state.running >= currentMax(helperClass) || m_clients.isEmpty())
        return nullptr;
    for (unsigned i = 0; i < m_clients.size(); ++i) {
        unsigned index = (state.nextClientIndex + i) % m_clients.size();
        auto* client = m_clients[index];
        if (client->m_class != helperClass)
            continue;
        if (client->m_demand <= client->m_running || client->m_running >= client->m_maxConcurrency)
            continue;
        // While a compile waits out a GC suspension, another one from the same client would just wait too.
        if (client->m_blocked)
            continue;
        state.nextClientIndex = index + 1;
        return client;
    }
    return nullptr;
}

HelperThreadClient* HelperThreadPool::selectClient(const AbstractLocker& locker)
{
    for (unsigned i = 0; i < numberOfHelperThreadClasses; ++i) {
        if (m_classes[i].running >= m_limits[i].min)
            continue;
        if (auto* client = eligibleClient(locker, static_cast<HelperThreadClass>(i)))
            return client;
    }

    ASSERT(m_running >= m_blocked);
    if (m_running - m_blocked >= capacity())
        return nullptr;

    std::optional<unsigned> best;
    for (unsigned i = 0; i < numberOfHelperThreadClasses; ++i) {
        if (!runnableTasks(locker, static_cast<HelperThreadClass>(i)))
            continue;
        if (!best || m_classes[i].running * m_limits[*best].weight < m_classes[*best].running * m_limits[i].weight)
            best = i;
    }
    if (!best)
        return nullptr;
    return eligibleClient(locker, static_cast<HelperThreadClass>(*best));
}

unsigned HelperThreadPool::liveThreads(const AbstractLocker& locker) const
{
    unsigned count = 0;
    for (auto& worker : m_workers) {
        if (worker->hasUnderlyingThread(locker))
            count++;
    }
    return count;
}

unsigned HelperThreadPool::capacity(unsigned numberOfEnteredVMs) const
{
    unsigned others = std::max(numberOfEnteredVMs, 1U) - 1;
    return std::max(m_capacity - std::min(others, m_capacity), std::min(minCapacity, m_capacity));
}

unsigned HelperThreadPool::currentMax(HelperThreadClass helperClass) const
{
    unsigned max = limits(helperClass).max;
    if (helperClass == HelperThreadClass::JIT && s_numberOfEnteredVMs.load(std::memory_order_relaxed) > 1)
        return std::min(max, maxJITThreadsWithSeveralVMs);
    return max;
}

void HelperThreadPool::didExitVM()
{
    unsigned numberOfEnteredVMs = s_numberOfEnteredVMs.fetch_sub(1, std::memory_order_relaxed);
    if (numberOfEnteredVMs <= 1)
        return;
    auto& pool = singleton();
    bool raisesJITMax = numberOfEnteredVMs == 2;
    if (!raisesJITMax && pool.capacity(numberOfEnteredVMs - 1) == pool.capacity(numberOfEnteredVMs))
        return;
    Locker locker { *pool.m_lock };
    pool.assertIsHolding(locker);
    pool.wakeWorkers(locker);
}

// FIXME: This and selectClient() walk every client and worker under the pool lock, on each poll, task
// completion and demand change. Keep per-HelperThreadClass shortfall and live/awaiting worker counts up
// to date instead.
void HelperThreadPool::wakeWorkers(const AbstractLocker& locker)
{
    unsigned guaranteed = 0;
    unsigned others = 0;
    for (unsigned i = 0; i < numberOfHelperThreadClasses; ++i) {
        unsigned running = m_classes[i].running;
        unsigned min = m_limits[i].min;
        unsigned runnable = runnableTasks(locker, static_cast<HelperThreadClass>(i));
        unsigned belowMin = min > running ? min - running : 0;
        unsigned fromMin = std::min(runnable, belowMin);
        guaranteed += fromMin;
        others += runnable - fromMin;
    }
    unsigned busy = m_running - m_blocked;
    unsigned currentCapacity = capacity();
    unsigned free = currentCapacity > busy + guaranteed ? currentCapacity - busy - guaranteed : 0;
    unsigned wanted = guaranteed + std::min(others, free);

    unsigned awaiting = 0;
    for (auto& worker : m_workers) {
        if (worker->isAwaitingWork(locker))
            awaiting++;
    }
    if (awaiting >= wanted)
        return;
    for (; awaiting < wanted; ++awaiting)
        m_workAvailable->notifyOne(locker);

    m_peakLiveThreads = std::max(m_peakLiveThreads, liveThreads(locker));
}

void HelperThreadPool::willBlock(HelperThreadClient& client)
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    ASSERT(client.m_running > client.m_blocked);
    m_blocked++;
    client.m_blocked++;
    m_blockingEvents++;
    wakeWorkers(locker);
}

void HelperThreadPool::didUnblock(HelperThreadClient& client)
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    ASSERT(m_blocked && client.m_blocked);
    m_blocked--;
    client.m_blocked--;
    wakeWorkers(locker);
}

HelperThreadPool::BlockingScope::BlockingScope(HelperThreadClient& client)
    : m_client(client)
{
    if (!Options::useSharedHelperThreadPool())
        return;
    m_pool = &singleton();
    m_pool->willBlock(m_client);
}

HelperThreadPool::BlockingScope::~BlockingScope()
{
    if (m_pool)
        m_pool->didUnblock(m_client);
}

auto HelperThreadPool::statistics() const -> Statistics
{
    Locker locker { *m_lock };
    assertIsHolding(locker);
    Statistics result;
    result.capacity = m_capacity;
    result.maxThreads = m_workers.size();
    result.liveThreads = liveThreads(locker);
    result.peakLiveThreads = std::max(m_peakLiveThreads, result.liveThreads);
    result.running = m_running;
    result.blocked = m_blocked;
    result.blockingEvents = m_blockingEvents;
    for (unsigned i = 0; i < numberOfHelperThreadClasses; ++i) {
        auto& state = m_classes[i];
        auto& classResult = result.classes[i];
        classResult.running = state.running;
        for (auto* client : m_clients) {
            if (client->m_class == static_cast<HelperThreadClass>(i))
                classResult.demand += client->m_demand;
        }
        classResult.peakRunning = state.peakRunning;
        classResult.tasks = state.tasks;
        classResult.tasksWithNoWork = state.tasksWithNoWork;
        classResult.migrations = state.migrations;
    }
    return result;
}

} // namespace JSC
