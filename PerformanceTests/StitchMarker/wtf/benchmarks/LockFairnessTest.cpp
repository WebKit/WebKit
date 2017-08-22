/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
// xcrun clang++ -o LockFairnessTest Source/WTF/benchmarks/LockFairnessTest.cpp -O3 -W -ISource/WTF -ISource/WTF/icu -ISource/WTF/benchmarks -LWebKitBuild/Release -lWTF -framework Foundation -licucore -std=c++14 -fvisibility=hidden

#include "ToyLocks.h"
#include <thread>
#include <unistd.h>
#include <wtf/Compiler.h>
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

unsigned numThreads;
double secondsPerTest;
unsigned microsecondsInCriticalSection;

struct Benchmark {
    template<typename LockType>
    static void run(const char* name)
    {
        LockType lock;
        std::unique_ptr<unsigned[]> counts = std::make_unique<unsigned[]>(numThreads);
        std::unique_ptr<RefPtr<Thread>[]> threads = std::make_unique<RefPtr<Thread>[]>(numThreads);
    
        volatile bool keepGoing = true;
    
        lock.lock();
    
        for (unsigned threadIndex = numThreads; threadIndex--;) {
            counts[threadIndex] = 0;
            threads[threadIndex] = Thread::create(
                "Benchmark Thread",
                [&, threadIndex] () {
                    if (!microsecondsInCriticalSection) {
                        while (keepGoing) {
                            lock.lock();
                            counts[threadIndex]++;
                            lock.unlock();
                        }
                        return;
                    }
                    
                    while (keepGoing) {
                        lock.lock();
                        counts[threadIndex]++;
                        usleep(microsecondsInCriticalSection);
                        lock.unlock();
                    }
                });
        }
    
        sleepMS(100);
        lock.unlock();
    
        sleep(secondsPerTest);
    
        keepGoing = false;
        lock.lock();
    
        printf("%20s ", name);
        for (unsigned threadIndex = numThreads; threadIndex--;)
          printf("%s%6u", threadIndex != numThreads - 1 ? ", " : "", counts[threadIndex]);
        printf("\n");
    
        lock.unlock();
        for (unsigned threadIndex = numThreads; threadIndex--;)
            threads[threadIndex]->waitForCompletion();
    }
};

} // anonymous namespace

int main()
{
    WTF::initializeThreading();
    
    static constexpr struct {
        unsigned threads;
        double seconds;
        unsigned criticalSection;
    } tests[] = {
        {  1, 0.5, 100 },
        {  2, 0.5, 100 },
        {  4, 0.5, 100 },
        {  8, 0.5, 100 },
        { 16, 0.5, 100 },
    };

    for (auto&& t : tests) {
        numThreads = t.threads;
        secondsPerTest = t.seconds;
        microsecondsInCriticalSection = t.criticalSection;
        printf("%2u threads, %3.1f seconds of testing, %3u us in critical section:\n",
               numThreads, secondsPerTest, microsecondsInCriticalSection);
        runEverything<Benchmark>();
    }
    
    return 0;
}
