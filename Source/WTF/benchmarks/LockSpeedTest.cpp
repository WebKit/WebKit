/*
 * Copyright (C) 2015-2017, 2026 Apple Inc. All rights reserved.
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
// INTERNAL_SDK=$(xcrun --sdk macosx.internal --show-sdk-path) && \
// xcrun clang++ -o LockSpeedTest Source/WTF/benchmarks/LockSpeedTest.cpp \
//     -W -ISource/WTF -ISource/WTF/icu -ISource/WTF/benchmarks \
//     -I"$INTERNAL_SDK/usr/local/include" -IWebKitBuild/Release/usr/local/include \
//     -LWebKitBuild/Release -lWTF -lbmalloc \
//     -framework Foundation -framework Security -licucore \
//     -std=c++2b -fvisibility=hidden -DNDEBUG -O3 -arch arm64e
//
// For an OSS build (no internal SDK), drop the INTERNAL_SDK line and the
// -I"$INTERNAL_SDK/usr/local/include" flag, and use -arch arm64 instead of arm64e.

#include "config.h"

#include "ToyLocks.h"
#include <algorithm>
#include <thread>
#include <unistd.h>
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ParkingLot.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>
#include <wtf/WordLock.h>
#include <wtf/text/CString.h>

namespace {

unsigned numThreadGroups;
unsigned numThreadsPerGroup;
unsigned workPerCriticalSection;
unsigned workBetweenCriticalSections;
double secondsPerTest;
// Microseconds a writer sleeps between acquisitions. Zero means hammer the lock, which is the
// pathological case; a real workload with rare writes wants this nonzero.
unsigned writerSleepUsec;
    
[[noreturn]] void usage()
{
    printf("Usage: LockSpeedTest yieldspinlock|pausespinlock|wordlock|lock|barginglock|bargingwordlock|thunderlock|thunderwordlock|cascadelock|cascadewordlock|handofflock|unfairlock|mutex|all|readwritelock|pftlock|pftparkinglock|countedrwlock|sharedmutex|allrw <num thread groups> <num threads per group> <work per critical section> <work between critical sections> <spin limit> <seconds per test> [<num writers per group> [<writer sleep usec>]]\n");
    exit(1);
}

template<typename Type>
struct WithPadding {
    Type value;
    char buf[300]; // It's best if this isn't perfect to avoid false sharing.
};

HashMap<CString, Vector<double>> results;

void reportResult(const char* name, double value, const char* unit = "KHz")
{
    dataLogF("%s: %.3lf %s\n", name, value, unit);
    results.add(name, Vector<double>()).iterator->value.append(value);
}

struct Benchmark {
    template<typename LockType>
    static void run(const char* name)
    {
        Vector<WithPadding<LockType>> locks(numThreadGroups);
        Vector<WithPadding<double>> words(numThreadGroups);
        Vector<RefPtr<Thread>> threads(numThreadGroups * numThreadsPerGroup);

        std::atomic<bool> keepGoing = true;

        MonotonicTime before = MonotonicTime::now();
    
        Lock numIterationsLock;
        uint64_t numIterations = 0;
    
        for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
            words[threadGroupIndex].value = 0;

            for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
                threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = Thread::create(
                    "Benchmark thread"_s,
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
                        Locker locker { numIterationsLock };
                        numIterations += myNumIterations;
                    });
            }
        }

        sleep(Seconds { secondsPerTest });
        keepGoing = false;
    
        for (unsigned threadIndex = numThreadGroups * numThreadsPerGroup; threadIndex--;)
            threads[threadIndex]->waitForCompletion();

        MonotonicTime after = MonotonicTime::now();
    
        reportResult(name, numIterations / (after - before).seconds() / 1000);
    }
};

// Same shape as Benchmark, but each thread group is a mix of writers and readers, so that the
// read-write locks are measured on what they are for. Readers only read the shared word, which
// means a lock that lets readers run concurrently should scale with the reader count.
//
// Reader and writer throughput are reported separately as well as together, because the
// interesting differences between these locks are in how they split the two: a writer-preference
// lock can post a fine combined number while starving readers, and a reader-preference lock the
// reverse.
struct RWBenchmark {
    template<typename LockType>
    static void run(const char* name)
    {
        unsigned writersPerGroup = std::min(toyLockWritersPerGroup, numThreadsPerGroup);

        // These locks are neither copyable nor movable, so they cannot live in a Vector.
        auto locks = makeUniqueArray<WithPadding<LockType>>(numThreadGroups);
        Vector<WithPadding<double>> words(numThreadGroups);
        Vector<RefPtr<Thread>> threads(numThreadGroups * numThreadsPerGroup);

        std::atomic<bool> keepGoing = true;

        MonotonicTime before = MonotonicTime::now();

        Lock numIterationsLock;
        uint64_t numReadIterations = 0;
        uint64_t numWriteIterations = 0;
        // How long writers spent inside writeLock(). With rare writes, this is what matters about a
        // writer far more than its throughput does.
        double totalWriteAcquireSeconds = 0;

        for (unsigned threadGroupIndex = numThreadGroups; threadGroupIndex--;) {
            words[threadGroupIndex].value = 0;

            for (unsigned threadIndex = numThreadsPerGroup; threadIndex--;) {
                bool isWriter = threadIndex < writersPerGroup;
                threads[threadGroupIndex * numThreadsPerGroup + threadIndex] = Thread::create(
                    "Benchmark thread"_s,
                    [threadGroupIndex, isWriter, &locks, &words, &keepGoing, &numIterationsLock, &numReadIterations, &numWriteIterations, &totalWriteAcquireSeconds] () {
                        double localWord = 0;
                        double value = 1;
                        unsigned myNumIterations = 0;
                        double myAcquireSeconds = 0;
                        while (keepGoing) {
                            if (isWriter) {
                                MonotonicTime before = MonotonicTime::now();
                                locks[threadGroupIndex].value.writeLock();
                                myAcquireSeconds += (MonotonicTime::now() - before).seconds();
                                for (unsigned j = workPerCriticalSection; j--;) {
                                    words[threadGroupIndex].value += value;
                                    value = words[threadGroupIndex].value;
                                }
                                locks[threadGroupIndex].value.writeUnlock();
                                if (writerSleepUsec)
                                    usleep(writerSleepUsec);
                            } else {
                                locks[threadGroupIndex].value.readLock();
                                for (unsigned j = workPerCriticalSection; j--;)
                                    value += words[threadGroupIndex].value;
                                locks[threadGroupIndex].value.readUnlock();
                            }
                            for (unsigned j = workBetweenCriticalSections; j--;) {
                                localWord += value;
                                value = localWord;
                            }
                            myNumIterations++;
                        }
                        Locker locker { numIterationsLock };
                        if (isWriter) {
                            numWriteIterations += myNumIterations;
                            totalWriteAcquireSeconds += myAcquireSeconds;
                        } else
                            numReadIterations += myNumIterations;
                    });
            }
        }

        sleep(Seconds { secondsPerTest });
        keepGoing = false;

        for (unsigned threadIndex = numThreadGroups * numThreadsPerGroup; threadIndex--;)
            threads[threadIndex]->waitForCompletion();

        MonotonicTime after = MonotonicTime::now();

        double seconds = (after - before).seconds();
        char label[256];
        reportResult(name, (numReadIterations + numWriteIterations) / seconds / 1000);
        snprintf(label, sizeof(label), "%s reads", name);
        reportResult(label, numReadIterations / seconds / 1000);
        snprintf(label, sizeof(label), "%s writes", name);
        reportResult(label, numWriteIterations / seconds / 1000);
        snprintf(label, sizeof(label), "%s write acquire", name);
        reportResult(label, numWriteIterations ? totalWriteAcquireSeconds / numWriteIterations * 1000000 : 0, "usec");
    }
};

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

} // anonymous namespace

int main(int argc, char** argv)
{
    WTF::initialize();
    
    if ((argc != 8 && argc != 9 && argc != 10)
        || !parseValue(argv[2], &numThreadGroups)
        || !parseValue(argv[3], &numThreadsPerGroup)
        || !parseValue(argv[4], &workPerCriticalSection)
        || !parseValue(argv[5], &workBetweenCriticalSections)
        || !parseValue(argv[6], &toyLockSpinLimit)
        || sscanf(argv[7], "%lf", &secondsPerTest) != 1
        || (argc >= 9 && !parseValue(argv[8], &toyLockWritersPerGroup))
        || (argc >= 10 && !parseValue(argv[9], &writerSleepUsec)))
        usage();
    if (rangeVariable) {
        dataLog("Running with rangeMin = ", rangeMin, ", rangeMax = ", rangeMax, ", rangeStep = ", rangeStep, "\n");
        for (unsigned value = rangeMin; value <= rangeMax; value += rangeStep) {
            dataLog("Running with value = ", value, "\n");
            *rangeVariable = value;
            runEverything<Benchmark>(argv[1]);
            runEverythingRW<RWBenchmark>(argv[1]);
        }
    } else {
        runEverything<Benchmark>(argv[1]);
        runEverythingRW<RWBenchmark>(argv[1]);
    }
    
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
