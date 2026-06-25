/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_lock.h"
#include "pas_status_reporter.h"
#include "pas_utils.h"
#include "iso_heap.h"
#include "iso_heap_config.h"
#include <cstdint>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

using namespace std;

namespace {

// Each of the following arguments can be set using -argumentName. For example, you can do:
//
//     ./Chaos -seed 42 -maxSmallObjectSize=20 -numThreads=1000
//
// Or somesuch. See HANDLE_ARG in main().

#define FOR_EACH_ARG(macro) \
    macro(uintptr_t,     seed,                           666,             "%lu") \
    macro(uintptr_t,     seedAdvance,                    1,               "%lu") \
    macro(uintptr_t,     maxSmallObjectSize,             1000,            "%lu") \
    macro(uintptr_t,     maxObjectSize,                  65536,           "%lu") \
    macro(double,        smallObjectProbability,         .95,             "%lf") \
    macro(double,        memalignProbability,            .05,             "%lf") \
    macro(uintptr_t,     maxMemalignShift,               14,              "%lu") \
    macro(uintptr_t,     numThreads,                     10,              "%lu") \
    macro(uintptr_t,     targetBytesPerPacket,           100000,          "%lu") \
    macro(uintptr_t,     targetBytesInGlobalPool,        100000000,       "%lu") \
    macro(uintptr_t,     maxPacketsPerThread,            10,              "%lu") \
    macro(uintptr_t,     reportPeriod,                   1000000,         "%lu") \
    macro(double,        deleteInActiveProbability,      .5,              "%lf") \
    macro(double,        massacreProbability,            .5,              "%lf") \
    macro(double,        massacreSurvivalRateTarget,     .5,              "%lf") \
    macro(double,        suicideProbability,             .001,            "%lf") \
    macro(unsigned,      enableBitFit,                   1,               "%u") \
    macro(unsigned,      enableSegregated,               1,               "%u") \
    macro(unsigned,      enableStatusReporter,           0,               "%u")

#define DECLARE_ARG(type, name, initialValue, format) \
    type name = initialValue;
FOR_EACH_ARG(DECLARE_ARG)
#undef DECLARE_ARG

pas_lock startLock;
pas_lock globalPoolLock;
pas_lock printLock;

struct Object {
    Object() = default;

    Object(void* ptr, uintptr_t size)
        : ptr(ptr)
        , size(size)
    {
    }

    void* ptr { nullptr };
    uintptr_t size { 0 };
};

struct Packet {
    Packet() = default;

    ~Packet()
    {
        PAS_ASSERT(objects.empty());
        PAS_ASSERT(!numBytes);
    }

    vector<Object> objects;
    uintptr_t numBytes { 0 };
};

vector<unique_ptr<Packet>> globalPool;
uintptr_t globalPoolNumBytes;

class Locker {
public:
    Locker(pas_lock& lock)
        : m_lock(lock)
    {
        pas_lock_lock(&lock);
    }

    ~Locker()
    {
        pas_lock_unlock(&m_lock);
    }

private:
    pas_lock& m_lock;
};

void threadMain(uintptr_t threadIndex)
{
    {
        Locker locker(startLock);
        // Just flash this lock. This trick helps threads start more in unison than they otherwise
        // would.
    }

    uintptr_t mySeed = seed + threadIndex * seedAdvance;
    {
        Locker locker(printLock);
        cout << "Thread " << threadIndex << " starting with seed " << mySeed << "\n";
    }
    
    mt19937 randomGenerator(static_cast<unsigned>(mySeed));
    vector<unique_ptr<Packet>> localPool;
    uintptr_t numAllocationActions = 0;
    uintptr_t numDeallocationActions = 0;
    uintptr_t numAllocations = 0;
    uintptr_t numDeallocations = 0;

    auto givePacketToGlobalPool =
        [&] (unique_ptr<Packet> packet) {
            Locker locker(globalPoolLock);
            globalPoolNumBytes += packet->numBytes;
            globalPool.push_back(std::move(packet));
        };
    
    auto takePacketFromGlobalPool =
        [&] () {
            Locker locker(globalPoolLock);
            if (globalPool.empty())
                return unique_ptr<Packet>(nullptr);
            uintptr_t index =
                uniform_int_distribution<uintptr_t>(0, globalPool.size() - 1)(randomGenerator);
            unique_ptr<Packet> result = std::move(globalPool[index]);
            globalPool[index] = std::move(globalPool.back());
            PAS_ASSERT(result->numBytes <= globalPoolNumBytes);
            globalPoolNumBytes -= result->numBytes;
            globalPool.pop_back();
            return result;
        };

    auto allocate =
        [&] () {
            uintptr_t targetSize;
            uintptr_t targetMaxSize;

            if (bernoulli_distribution(smallObjectProbability)(randomGenerator))
                targetMaxSize = maxSmallObjectSize;
            else
                targetMaxSize = maxObjectSize;
            
            targetSize = uniform_int_distribution<uintptr_t>(0, targetMaxSize)(randomGenerator);

            numAllocations++;
            char* ptr;

            if (bernoulli_distribution(memalignProbability)(randomGenerator)) {
                ptr = static_cast<char*>(
                    iso_allocate_common_primitive_with_alignment(
                        targetSize,
                        static_cast<uintptr_t>(1) << uniform_int_distribution<uintptr_t>(
                            0, maxMemalignShift)(randomGenerator)));
            } else
                ptr = static_cast<char*>(iso_allocate_common_primitive(targetSize));
            
            for (uintptr_t i = targetSize; i--;)
                ptr[i] = static_cast<char>(i + 42);

            if (localPool.empty())
                localPool.push_back(make_unique<Packet>());

            localPool.back()->objects.push_back(Object(ptr, targetSize));
            localPool.back()->numBytes += targetSize;

            if (localPool.back()->numBytes > targetBytesPerPacket)
                localPool.push_back(make_unique<Packet>());
        };

    auto deleteAll =
        [&] (const auto& packetPtr) {
            for (Object object : packetPtr->objects) {
                iso_deallocate(object.ptr);
                numDeallocations++;
            }
            packetPtr->objects.clear();
            packetPtr->numBytes = 0;
        };

    auto allocationAction =
        [&] () {
            numAllocationActions++;
            
            if (globalPoolNumBytes > targetBytesInGlobalPool) {
                if (bernoulli_distribution(massacreProbability)(randomGenerator)) {
                    uintptr_t newTargetBytesInGlobalPool = static_cast<uintptr_t>(
                        targetBytesInGlobalPool * massacreSurvivalRateTarget);
                    PAS_ASSERT(newTargetBytesInGlobalPool >= 0);
                    PAS_ASSERT(newTargetBytesInGlobalPool <= targetBytesInGlobalPool);
                    while (globalPoolNumBytes > newTargetBytesInGlobalPool)
                        deleteAll(takePacketFromGlobalPool());
                } else {
                    unique_ptr<Packet> packet = takePacketFromGlobalPool();
                    if (packet)
                        localPool.push_back(std::move(packet));
                }
            } else
                allocate();

            if (localPool.size() > maxPacketsPerThread) {
                uintptr_t index = 
                    uniform_int_distribution<uintptr_t>(0, localPool.size() - 1)(randomGenerator);
                unique_ptr<Packet> result = std::move(localPool[index]);
                PAS_ASSERT(result);
                localPool.erase(localPool.begin() + index);
                if (!result->objects.empty())
                    givePacketToGlobalPool(std::move(result));
            }
        };

    auto deallocationAction =
        [&] () {
            numDeallocationActions++;

            if (bernoulli_distribution(suicideProbability)(randomGenerator)) {
                for (auto& entry : localPool)
                    deleteAll(entry);
                localPool.clear();
                return;
            }
            
            Packet* packet;
            if (localPool.empty())
                return;
            if (localPool.size() == 1
                || bernoulli_distribution(deleteInActiveProbability)(randomGenerator))
                packet = localPool.back().get();
            else {
                packet = localPool[
                    uniform_int_distribution<uintptr_t>(
                        0, localPool.size() - 2)(
                            randomGenerator)].get();
            }

            if (packet->objects.empty())
                return;
            
            uintptr_t index =
                uniform_int_distribution<uintptr_t>(0, packet->objects.size() - 1)(randomGenerator);
            Object object = packet->objects[index];
            packet->objects[index] = packet->objects.back();
            packet->objects.pop_back();

            numDeallocations++;

            for (uintptr_t i = object.size; i--;)
                PAS_ASSERT(static_cast<char*>(object.ptr)[i] == static_cast<char>(i + 42));
            
            iso_deallocate(object.ptr);
            PAS_ASSERT(object.size <= packet->numBytes);
            packet->numBytes -= object.size;
        };
    
    for (uintptr_t count = 1; ; ++count) {
        if (bernoulli_distribution(0.5)(randomGenerator))
            allocationAction();
        else
            deallocationAction();

        if (!(count % reportPeriod)) {
            uintptr_t localPoolNumBytes = 0;
            for (auto& entry : localPool)
                localPoolNumBytes += entry->numBytes;

            Locker locker(printLock);
            
            cout << "Thread " << threadIndex << ": "
                 << count << " iterations, "
                 << localPool.size() << " local packets, "
                 << localPoolNumBytes << " local bytes, "
                 << (localPool.empty() ? 0 : localPool.back()->numBytes) << " bytes in active packet, "
                 << globalPool.size() << " global packets, "
                 << globalPoolNumBytes << " global bytes, "
                 << numAllocationActions << " allocation actions, "
                 << numDeallocationActions << " deallocation actions, "
                 << numAllocations << " allocations, "
                 << numDeallocations << " deallocations.\n";
        }
    }

    PAS_ASSERT(!"Should not be reached");
}

} // anonymous namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    pas_lock_construct(&startLock);
    pas_lock_construct(&globalPoolLock);
    pas_lock_construct(&printLock);
    
    for (int argIndex = 1; argIndex < argc;) {
        char* argName = argv[argIndex++];
        
#define HANDLE_ARG(type, name, initialValue, format) \
        if (!strcmp(argName, "-" #name)) { \
            if (argIndex >= argc) { \
                cerr << "Need argument for -" #name "\n"; \
                return 1; \
            } \
            char* argValue = argv[argIndex++]; \
            if (sscanf(argValue, format, &name) != 1) { \
                cerr << "Badly formatted argument for -" #name ": " << argValue << "\n"; \
                return 1; \
            } \
            continue; \
        }
        FOR_EACH_ARG(HANDLE_ARG)
#undef HANDLE_ARG

        if (!strcmp(argName, "-help")) {
            cout << "Usage: chaos [options]\n";
            cout << "\n";
            cout << "Options:\n";
#define REPORT_ARG(type, name, initialValue, format) \
            printf("-%s <arg>     (type %s and initial value " format ")\n", #name, #type, type(initialValue));
            FOR_EACH_ARG(REPORT_ARG)
#undef REPORT_ARG
            return 2;
        }
        
        cerr << "Bad argument: " << argName << " (try -help)\n";
        return 1;
    }

    if (!enableBitFit)
        iso_intrinsic_runtime_config.base.max_bitfit_object_size = 0;

    if (!enableSegregated)
        iso_intrinsic_runtime_config.base.max_segregated_object_size = 0;

    pas_status_reporter_enabled = enableStatusReporter;

#define REPORT_ARG(type, name, initialValue, format) \
    printf("%s = " format "\n", #name, name);
    FOR_EACH_ARG(REPORT_ARG);
#undef REPORT_ARG

    vector<thread> threads;

    {
        Locker locker(startLock);
        for (uintptr_t i = numThreads; i--;)
            threads.push_back(thread(threadMain, i));
    }

    for (thread& someThread : threads) {
        someThread.join();
        if ((true)) {
            cerr << "ERROR: Thread exited.\n";
            return 1;
        }
    }

    cerr << "ERROR: Threads should not have exited.\n";
    return 1;
}


