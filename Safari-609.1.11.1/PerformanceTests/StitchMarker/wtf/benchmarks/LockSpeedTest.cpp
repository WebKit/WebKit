/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
// xcrun clang++ -o LockSpeedTest Source/WTF/benchmarks/LockSpeedTest.cpp -O3 -W -ISource/WTF -ISource/WTF/icu -ISource/WTF/benchmarks -LWebKitBuild/Release -lWTF -framework Foundation -licucore -std=c++14 -fvisibility=hidden

#include "ToyLocks.h"
#include <thread>
#include <unistd.h>
#include <wtf/CurrentTime.h>
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
double secondsPerTest;
    
template<typename Type>
struct WithPadding {
    Type value;
    char buf[300]; // It's best if this isn't perfect to avoid false sharing.
};

HashMap<CString, Vector<double>> results;

void reportResult(const char* name, double value)
{
    printf("%20s: %9.3f KHz\n", name, value);
    results.add(name, Vector<double>()).iterator->value.append(value);
}

struct Benchmark {
    template<typename LockType>
    static void run(const char* name)
    {
        std::unique_ptr<WithPadding<LockType>[]> locks = std::make_unique<WithPadding<LockType>[]>(numThreadGroups);
        std::unique_ptr<WithPadding<double>[]> words = std::make_unique<WithPadding<double>[]>(numThreadGroups);
        std::unique_ptr<RefPtr<Thread>[]> threads = std::make_unique<RefPtr<Thread>[]>(numThreadGroups * numThreadsPerGroup);

        volatile bool keepGoing = true;

        double before = monotonicallyIncreasingTime();
    
        Lock numIterationsLock;
        uint64_t numIterations = 0;
    
        for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
            words[threadGroupIndex].value = 0;

            for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
                threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = Thread::create(
                    "Benchmark thread",
                    [threadGroupIndex, &locks, &words, &keepGoing, &numIterationsLock, &numIterations] () {
                        double localWord = 0;
                        double value = 1;
                        unsigned myNumIterations = 0;
                        while (keepGoing) {
                            locks[threadGroupIndex].value.lock();
                            for (unsigned j = workPerCriticalSection; j--;) {
                                words[threadGroupIndex].value += value;
                                value = words[threadGroupIndex].value;
                            }
                            locks[threadGroupIndex].value.unlock();
                            for (unsigned j = workBetweenCriticalSections; j--;) {
                                localWord += value;
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
            threads[threadIndex]->waitForCompletion();

        double after = monotonicallyIncreasingTime();
    
        reportResult(name, numIterations / (after - before) / 1000);
    }
};

} // anonymous namespace

int main()
{
    WTF::initializeThreading();

    static constexpr struct {
        unsigned threadGroups, threadsPerGroup, perCriticalSection, betweenCriticalSections;
        double seconds;
    } tests[] = {
        { 1,  2, 500, 300, 0.5 },
        { 2,  4, 500, 300, 0.5 },
        { 2,  8, 500, 300, 0.5 },
        { 2, 16, 500, 300, 0.5 },
        { 4, 16, 500, 300, 0.5 },
    };

    for (auto&& t : tests) {
        numThreadGroups = t.threadGroups;
        numThreadsPerGroup = t.threadsPerGroup;
        workPerCriticalSection = t.perCriticalSection;
        workBetweenCriticalSections = t.betweenCriticalSections;
        secondsPerTest = t.seconds;

        printf("%2u groups, %2u threads per group, %5u work per critical section, %5u work between critical sections, %6.3f seconds per test\n",
               numThreadGroups, numThreadsPerGroup, workPerCriticalSection, workBetweenCriticalSections, secondsPerTest);

        runEverything<Benchmark>();

        for (auto& entry : results) {
            printf("%20s = { ", entry.key.data());
            bool first = true;
            for (double value : entry.value) {
                if (first)
                    first = false;
                else
                    printf(", ");
                printf("%9.3f", value);
            }
            printf(" };\n");
        }
    }

    return 0;
}
