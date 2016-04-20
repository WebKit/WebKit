/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

// On Mac, you can build this like so:
// clang++ -o LockSpeedTest Source/WTF/benchmarks/LockSpeedTest.cpp -O3 -W -ISource/WTF -LWebKitBuild/Release -lWTF -framework Foundation -licucore -std=c++11

#include "config.h"

#include <thread>
#include <unistd.h>
#include <wtf/CurrentTime.h>
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ParkingLot.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>
#include <wtf/WordLock.h>
#include <wtf/text/CString.h>

namespace {

unsigned numThreadGroups;
unsigned numThreadsPerGroup;
unsigned workPerCriticalSection;
unsigned workBetweenCriticalSections;
unsigned spinLimit;
double secondsPerTest;
    
// This is the old WTF::SpinLock class, included here so that we can still compare our new locks to a
// spinlock baseline.
class YieldSpinLock {
public:
    YieldSpinLock()
    {
        m_lock.store(0, std::memory_order_relaxed);
    }

    void lock()
    {
        while (!m_lock.compareExchangeWeak(0, 1, std::memory_order_acquire))
            std::this_thread::yield();
    }

    void unlock()
    {
        m_lock.store(0, std::memory_order_release);
    }

    bool isLocked() const
    {
        return m_lock.load(std::memory_order_acquire);
    }

private:
    Atomic<unsigned> m_lock;
};

class PauseSpinLock {
public:
    PauseSpinLock()
    {
        m_lock.store(0, std::memory_order_relaxed);
    }

    void lock()
    {
        while (!m_lock.compareExchangeWeak(0, 1, std::memory_order_acquire))
            asm volatile ("pause");
    }

    void unlock()
    {
        m_lock.store(0, std::memory_order_release);
    }

    bool isLocked() const
    {
        return m_lock.load(std::memory_order_acquire);
    }

private:
    Atomic<unsigned> m_lock;
};

template<typename StateType>
class BargingLock {
public:
    BargingLock()
    {
        m_state.store(0);
    }
    
    void lock()
    {
        if (LIKELY(m_state.compareExchangeWeak(0, isLockedBit, std::memory_order_acquire)))
            return;
        
        lockSlow();
    }
    
    void unlock()
    {
        if (LIKELY(m_state.compareExchangeWeak(isLockedBit, 0, std::memory_order_release)))
            return;
        
        unlockSlow();
    }
    
    bool isLocked() const
    {
        return m_state.load(std::memory_order_acquire) & isLockedBit;
    }
    
private:
    NEVER_INLINE void lockSlow()
    {
        for (unsigned i = spinLimit; i--;) {
            StateType currentState = m_state.load();
            
            if (!(currentState & isLockedBit)
                && m_state.compareExchangeWeak(currentState, currentState | isLockedBit))
                return;
            
            if (currentState & hasParkedBit)
                break;
            
            std::this_thread::yield();
        }
        
        for (;;) {
            StateType currentState = m_state.load();
            
            if (!(currentState & isLockedBit)
                && m_state.compareExchangeWeak(currentState, currentState | isLockedBit))
                return;
            
            m_state.compareExchangeWeak(isLockedBit, isLockedBit | hasParkedBit);
            
            ParkingLot::compareAndPark(&m_state, isLockedBit | hasParkedBit);
        }
    }
    
    NEVER_INLINE void unlockSlow()
    {
        ParkingLot::unparkOne(
            &m_state,
            [this] (ParkingLot::UnparkResult result) {
                if (result.mayHaveMoreThreads)
                    m_state.store(hasParkedBit);
                else
                    m_state.store(0);
            });
    }
    
    static const StateType isLockedBit = 1;
    static const StateType hasParkedBit = 2;
    
    Atomic<StateType> m_state;
};

template<typename StateType>
class ThunderLock {
public:
    ThunderLock()
    {
        m_state.store(Unlocked);
    }
    
    void lock()
    {
        if (LIKELY(m_state.compareExchangeWeak(Unlocked, Locked, std::memory_order_acquire)))
            return;
        
        lockSlow();
    }
    
    void unlock()
    {
        if (LIKELY(m_state.compareExchangeWeak(Locked, Unlocked, std::memory_order_release)))
            return;
        
        unlockSlow();
    }
    
    bool isLocked() const
    {
        return m_state.load(std::memory_order_acquire) != Unlocked;
    }
    
private:
    NEVER_INLINE void lockSlow()
    {
        for (unsigned i = spinLimit; i--;) {
            State currentState = m_state.load();
            
            if (currentState == Unlocked
                && m_state.compareExchangeWeak(Unlocked, Locked))
                return;
            
            if (currentState == LockedAndParked)
                break;
            
            std::this_thread::yield();
        }
        
        for (;;) {
            if (m_state.compareExchangeWeak(Unlocked, Locked))
                return;
            
            m_state.compareExchangeWeak(Locked, LockedAndParked);
            ParkingLot::compareAndPark(&m_state, LockedAndParked);
        }
    }
    
    NEVER_INLINE void unlockSlow()
    {
        if (m_state.exchange(Unlocked) == LockedAndParked)
            ParkingLot::unparkAll(&m_state);
    }
    
    enum State : StateType {
        Unlocked,
        Locked,
        LockedAndParked
    };
    
    Atomic<State> m_state;
};

template<typename StateType>
class CascadeLock {
public:
    CascadeLock()
    {
        m_state.store(Unlocked);
    }
    
    void lock()
    {
        if (LIKELY(m_state.compareExchangeWeak(Unlocked, Locked, std::memory_order_acquire)))
            return;
        
        lockSlow();
    }
    
    void unlock()
    {
        if (LIKELY(m_state.compareExchangeWeak(Locked, Unlocked, std::memory_order_release)))
            return;
        
        unlockSlow();
    }
    
    bool isLocked() const
    {
        return m_state.load(std::memory_order_acquire) != Unlocked;
    }
    
private:
    NEVER_INLINE void lockSlow()
    {
        for (unsigned i = spinLimit; i--;) {
            State currentState = m_state.load();
            
            if (currentState == Unlocked
                && m_state.compareExchangeWeak(Unlocked, Locked))
                return;
            
            if (currentState == LockedAndParked)
                break;
            
            std::this_thread::yield();
        }
        
        State desiredState = Locked;
        for (;;) {
            if (m_state.compareExchangeWeak(Unlocked, desiredState))
                return;
            
            desiredState = LockedAndParked;
            m_state.compareExchangeWeak(Locked, LockedAndParked);
            ParkingLot::compareAndPark(&m_state, LockedAndParked);
        }
    }
    
    NEVER_INLINE void unlockSlow()
    {
        if (m_state.exchange(Unlocked) == LockedAndParked)
            ParkingLot::unparkOne(&m_state);
    }
    
    enum State : StateType {
        Unlocked,
        Locked,
        LockedAndParked
    };
    
    Atomic<State> m_state;
};

class HandoffLock {
public:
    HandoffLock()
    {
        m_state.store(0);
    }
    
    void lock()
    {
        if (LIKELY(m_state.compareExchangeWeak(0, isLockedBit, std::memory_order_acquire)))
            return;

        lockSlow();
    }

    void unlock()
    {
        if (LIKELY(m_state.compareExchangeWeak(isLockedBit, 0, std::memory_order_release)))
            return;

        unlockSlow();
    }

    bool isLocked() const
    {
        return m_state.load(std::memory_order_acquire) & isLockedBit;
    }
    
private:
    NEVER_INLINE void lockSlow()
    {
        for (;;) {
            unsigned state = m_state.load();
            
            if (!(state & isLockedBit)) {
                if (m_state.compareExchangeWeak(state, state | isLockedBit))
                    return;
                continue;
            }
            
            if (m_state.compareExchangeWeak(state, state + parkedCountUnit)) {
                bool result = ParkingLot::compareAndPark(&m_state, state + parkedCountUnit);
                m_state.exchangeAndAdd(-parkedCountUnit);
                if (result)
                    return;
            }
        }
    }
    
    NEVER_INLINE void unlockSlow()
    {
        for (;;) {
            unsigned state = m_state.load();
            
            if (!(state >> parkedCountShift)) {
                RELEASE_ASSERT(state == isLockedBit);
                if (m_state.compareExchangeWeak(isLockedBit, 0))
                    return;
                continue;
            }
            
            if (ParkingLot::unparkOne(&m_state).didUnparkThread) {
                // We unparked someone. There are now running and they hold the lock.
                return;
            }
            
            // Nobody unparked. Maybe there isn't anyone waiting. Just try again.
        }
    }
    
    static const unsigned isLockedBit = 1;
    static const unsigned parkedCountShift = 1;
    static const unsigned parkedCountUnit = 1 << parkedCountShift;
    
    Atomic<unsigned> m_state;
};

NO_RETURN void usage()
{
    printf("Usage: LockSpeedTest yieldspinlock|pausespinlock|wordlock|lock|barginglock|bargingwordlock|thunderlock|thunderwordlock|cascadelock|cascadewordlockhandofflock|mutex|all <num thread groups> <num threads per group> <work per critical section> <work between critical sections> <spin limit> <seconds per test>\n");
    exit(1);
}

template<typename Type>
struct WithPadding {
    Type value;
    char buf[300]; // It's best if this isn't perfect to avoid false sharing.
};

HashMap<CString, Vector<double>> results;

void reportResult(const char* name, double value)
{
    dataLogF("%s: %.3lf KHz\n", name, value);
    results.add(name, Vector<double>()).iterator->value.append(value);
}

bool didRun;

template<typename LockType>
void runBenchmark(const char* name)
{
    std::unique_ptr<WithPadding<LockType>[]> locks = std::make_unique<WithPadding<LockType>[]>(numThreadGroups);
    std::unique_ptr<WithPadding<double>[]> words = std::make_unique<WithPadding<double>[]>(numThreadGroups);
    std::unique_ptr<ThreadIdentifier[]> threads = std::make_unique<ThreadIdentifier[]>(numThreadGroups * numThreadsPerGroup);

    volatile bool keepGoing = true;

    double before = monotonicallyIncreasingTime();
    
    Lock numIterationsLock;
    uint64_t numIterations = 0;
    
    for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
        words[threadGroupIndex].value = 0;

        for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
            threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = createThread(
                "Benchmark thread",
                [threadGroupIndex, &locks, &words, &keepGoing, &numIterationsLock, &numIterations] () {
                    double localWord = 0;
                    double value = 1;
                    unsigned myNumIterations = 0;
                    while (keepGoing) {
                        locks[threadGroupIndex].value.lock();
                        for (unsigned j = workPerCriticalSection; j--;) {
                            words[threadGroupIndex].value += value;
                            words[threadGroupIndex].value *= 1.01;
                            value = words[threadGroupIndex].value;
                        }
                        locks[threadGroupIndex].value.unlock();
                        for (unsigned j = workBetweenCriticalSections; j--;) {
                            localWord += value;
                            localWord *= 1.01;
                            value = localWord;
                        }
                        myNumIterations++;
                    }
                    LockHolder locker(numIterationsLock);
                    numIterations += myNumIterations;
                });
        }
    }

    sleep(secondsPerTest);
    keepGoing = false;
    
    for (unsigned threadIndex = numThreadGroups * numThreadsPerGroup; threadIndex--;)
        waitForThreadCompletion(threads[threadIndex]);

    double after = monotonicallyIncreasingTime();
    
    reportResult(name, numIterations / (after - before) / 1000);
    didRun = true;
}

unsigned rangeMin;
unsigned rangeMax;
unsigned rangeStep;
unsigned* rangeVariable;

bool parseValue(const char* string, unsigned* variable)
{
    unsigned myRangeMin;
    unsigned myRangeMax;
    unsigned myRangeStep;
    if (sscanf(string, "%u-%u:%u", &myRangeMin, &myRangeMax, &myRangeStep) == 3) {
        if (rangeVariable) {
            fprintf(stderr, "Can only have one variable with a range.\n");
            return false;
        }
        
        rangeMin = myRangeMin;
        rangeMax = myRangeMax;
        rangeStep = myRangeStep;
        rangeVariable = variable;
        return true;
    }
    
    if (sscanf(string, "%u", variable) == 1)
        return true;
    
    return false;
}

void runEverything(const char* what)
{
    if (!strcmp(what, "yieldspinlock") || !strcmp(what, "all"))
        runBenchmark<YieldSpinLock>("YieldSpinLock");
    if (!strcmp(what, "pausespinlock") || !strcmp(what, "all"))
        runBenchmark<PauseSpinLock>("PauseSpinLock");
    if (!strcmp(what, "wordlock") || !strcmp(what, "all"))
        runBenchmark<WordLock>("WTFWordLock");
    if (!strcmp(what, "lock") || !strcmp(what, "all"))
        runBenchmark<Lock>("WTFLock");
    if (!strcmp(what, "barginglock") || !strcmp(what, "all"))
        runBenchmark<BargingLock<uint8_t>>("ByteBargingLock");
    if (!strcmp(what, "bargingwordlock") || !strcmp(what, "all"))
        runBenchmark<BargingLock<uint32_t>>("WordBargingLock");
    if (!strcmp(what, "thunderlock") || !strcmp(what, "all"))
        runBenchmark<ThunderLock<uint8_t>>("ByteThunderLock");
    if (!strcmp(what, "thunderwordlock") || !strcmp(what, "all"))
        runBenchmark<ThunderLock<uint32_t>>("WordThunderLock");
    if (!strcmp(what, "cascadelock") || !strcmp(what, "all"))
        runBenchmark<CascadeLock<uint8_t>>("ByteCascadeLock");
    if (!strcmp(what, "cascadewordlock") || !strcmp(what, "all"))
        runBenchmark<CascadeLock<uint32_t>>("WordCascadeLock");
    if (!strcmp(what, "handofflock") || !strcmp(what, "all"))
        runBenchmark<HandoffLock>("HandoffLock");
    if (!strcmp(what, "mutex") || !strcmp(what, "all"))
        runBenchmark<Mutex>("PlatformMutex");

    if (!didRun)
        usage();
}

} // anonymous namespace

int main(int argc, char** argv)
{
    WTF::initializeThreading();
    
    if (argc != 8
        || !parseValue(argv[2], &numThreadGroups)
        || !parseValue(argv[3], &numThreadsPerGroup)
        || !parseValue(argv[4], &workPerCriticalSection)
        || !parseValue(argv[5], &workBetweenCriticalSections)
        || !parseValue(argv[6], &spinLimit)
        || sscanf(argv[7], "%lf", &secondsPerTest) != 1)
        usage();
    
    if (rangeVariable) {
        dataLog("Running with rangeMin = ", rangeMin, ", rangeMax = ", rangeMax, ", rangeStep = ", rangeStep, "\n");
        for (unsigned value = rangeMin; value <= rangeMax; value += rangeStep) {
            dataLog("Running with value = ", value, "\n");
            *rangeVariable = value;
            runEverything(argv[1]);
        }
    } else
        runEverything(argv[1]);
    
    for (auto& entry : results) {
        printf("%s = {", entry.key.data());
        bool first = true;
        for (double value : entry.value) {
            if (first)
                first = false;
            else
                printf(", ");
            printf("%.3lf", value);
        }
        printf("};\n");
    }

    return 0;
}
