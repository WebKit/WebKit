/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <unistd.h>
#include <wtf/CurrentTime.h>
#include <wtf/Lock.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/WordLock.h>

namespace {

// This is the old WTF::SpinLock class, included here so that we can still compare our new locks to a
// spinlock baseline.
class SpinLock {
public:
    SpinLock()
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

unsigned numThreadGroups;
unsigned numThreadsPerGroup;
unsigned workPerCriticalSection;
unsigned numNoiseThreads;
unsigned numIterations;
    
NO_RETURN void usage()
{
    printf("Usage: LockSpeedTest spinlock|wordlock|lock|mutex|all <num thread groups> <num threads per group> <work per critical section> <num noise threads> <num iterations>\n");
    exit(1);
}

template<typename LockType>
void runBenchmark(const char* name)
{
    std::unique_ptr<LockType[]> locks = std::make_unique<LockType[]>(numThreadGroups);
    std::unique_ptr<double[]> words = std::make_unique<double[]>(numThreadGroups);
    std::unique_ptr<ThreadIdentifier[]> threads = std::make_unique<ThreadIdentifier[]>(numThreadGroups * numThreadsPerGroup);
    std::unique_ptr<ThreadIdentifier[]> noiseThreads = std::make_unique<ThreadIdentifier[]>(numNoiseThreads);
    std::unique_ptr<double[]> noiseCounts = std::make_unique<double[]>(numNoiseThreads);

    volatile bool shouldStop = false;
    for (unsigned threadIndex = numNoiseThreads; threadIndex--;) {
        noiseCounts[threadIndex] = 0;
        noiseThreads[threadIndex] = createThread(
            "Noise Thread",
            [&shouldStop, &noiseCounts, threadIndex] () {
                while (!shouldStop)
                    noiseCounts[threadIndex]++;
            });
    }

    double before = monotonicallyIncreasingTimeMS();
    
    for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
        words[threadGroupIndex] = 0;

        for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
            threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = createThread(
                "Benchmark thread",
                [threadGroupIndex, &locks, &words] () {
                    for (unsigned i = numIterations; i--;) {
                        locks[threadGroupIndex].lock();
                        for (unsigned j = workPerCriticalSection; j--;) {
                            words[threadGroupIndex]++;
                            words[threadGroupIndex] *= 1.01;
                        }
                        locks[threadGroupIndex].unlock();
                    }
                });
        }
    }

    for (unsigned threadIndex = numThreadGroups * numThreadsPerGroup; threadIndex--;)
        waitForThreadCompletion(threads[threadIndex]);
    shouldStop = true;
    double noiseCount = 0;
    for (unsigned threadIndex = numNoiseThreads; threadIndex--;) {
        waitForThreadCompletion(noiseThreads[threadIndex]);
        noiseCount += noiseCounts[threadIndex];
    }

    double after = monotonicallyIncreasingTimeMS();

    printf("%s: %.3lf ms, %.0lf noise.\n", name, after - before, noiseCount);
}

} // anonymous namespace

int main(int argc, char** argv)
{
    WTF::initializeThreading();
    
    if (argc != 7
        || sscanf(argv[2], "%u", &numThreadGroups) != 1
        || sscanf(argv[3], "%u", &numThreadsPerGroup) != 1
        || sscanf(argv[4], "%u", &workPerCriticalSection) != 1
        || sscanf(argv[5], "%u", &numNoiseThreads) != 1
        || sscanf(argv[6], "%u", &numIterations) != 1)
        usage();

    bool didRun = false;
    if (!strcmp(argv[1], "spinlock") || !strcmp(argv[1], "all")) {
        runBenchmark<SpinLock>("SpinLock");
        didRun = true;
    }
    if (!strcmp(argv[1], "wordlock") || !strcmp(argv[1], "all")) {
        runBenchmark<WordLock>("WTF WordLock");
        didRun = true;
    }
    if (!strcmp(argv[1], "lock") || !strcmp(argv[1], "all")) {
        runBenchmark<Lock>("WTF Lock");
        didRun = true;
    }
    if (!strcmp(argv[1], "mutex") || !strcmp(argv[1], "all")) {
        runBenchmark<Mutex>("Platform Mutex");
        didRun = true;
    }

    if (!didRun)
        usage();

    return 0;
}
