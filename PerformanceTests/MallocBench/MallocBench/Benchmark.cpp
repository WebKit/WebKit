/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "Benchmark.h"
#include "CPUCount.h"
#include "balloon.h"
#include "big.h"
#include "churn.h"
#include "facebook.h"
#include "flickr.h"
#include "fragment.h"
#include "list.h"
#include "medium.h"
#include "message.h"
#include "reddit.h"
#include "theverge.h"
#include "tree.h"
#include <dispatch/dispatch.h>
#include <iostream>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <map>
#include <string>
#include <sys/time.h>
#include <thread>
#include <unistd.h>

#include "mbmalloc.h"

using namespace std;

struct BenchmarkPair {
    const char* const name;
    const BenchmarkFunction function;
};

static const BenchmarkPair benchmarkPairs[] = {
    { "churn", benchmark_churn },
    { "list_allocate", benchmark_list_allocate },
    { "list_traverse", benchmark_list_traverse },
    { "tree_allocate", benchmark_tree_allocate },
    { "tree_traverse", benchmark_tree_traverse },
    { "tree_churn", benchmark_tree_churn },
    { "fragment", benchmark_fragment },
    { "fragment_iterate", benchmark_fragment_iterate },
    { "message_one", benchmark_message_one },
    { "message_many", benchmark_message_many },
    { "medium", benchmark_medium },
    { "big", benchmark_big },
    { "facebook", benchmark_facebook },
    { "balloon", benchmark_balloon },
    { "flickr", benchmark_flickr },
    { "reddit", benchmark_reddit },
    { "theverge", benchmark_theverge },
    { "flickr_memory_warning", benchmark_flickr_memory_warning },
    { "reddit_memory_warning", benchmark_reddit_memory_warning },
    { "theverge_memory_warning", benchmark_theverge_memory_warning },
};

static const size_t benchmarksPairsCount = sizeof(benchmarkPairs) / sizeof(BenchmarkPair);

static inline bool operator==(const BenchmarkPair& benchmarkPair, const string& string)
{
    return string == benchmarkPair.name;
}

static void*** allocateHeap(size_t heapSize, size_t chunkSize, size_t objectSize)
{
    if (!heapSize)
        return 0;

    size_t chunkCount = heapSize / chunkSize;
    size_t objectCount = chunkSize / objectSize;
    void*** chunks = (void***)mbmalloc(chunkCount * sizeof(void**));
    for (size_t i = 0; i < chunkCount; ++i) {
        chunks[i] = (void**)mbmalloc(objectCount * sizeof(void*));
        for (size_t j = 0; j < objectCount; ++j) {
            chunks[i][j] = (void*)mbmalloc(objectSize);
            bzero(chunks[i][j], objectSize);
        }
    }
    return chunks;
}

static void deallocateHeap(void*** chunks, size_t heapSize, size_t chunkSize, size_t objectSize)
{
    if (!heapSize)
        return;

    size_t chunkCount = heapSize / chunkSize;
    size_t objectCount = chunkSize / objectSize;
    for (size_t i = 0; i < chunkCount; ++i) {
        for (size_t j = 0; j < objectCount; ++j)
            mbfree(chunks[i][j], objectSize);
        mbfree(chunks[i], objectCount * sizeof(void*));
    }
    mbfree(chunks, chunkCount * sizeof(void**));
}

Benchmark::Benchmark(const string& benchmarkName, bool isParallel, bool measureHeap, size_t heapSize)
    : m_benchmarkPair()
    , m_elapsedTime()
    , m_isParallel(isParallel)
    , m_heapSize(heapSize)
    , m_measureHeap(measureHeap)
{
    const BenchmarkPair* benchmarkPair = std::find(
        benchmarkPairs, benchmarkPairs + benchmarksPairsCount, benchmarkName);
    if (benchmarkPair == benchmarkPairs + benchmarksPairsCount)
        return;
    
    m_benchmarkPair = benchmarkPair;
}
    
void Benchmark::printBenchmarks()
{
    cout << "Benchmarks: " << endl;
    for (size_t i = 0; i < benchmarksPairsCount; ++i)
        cout << "\t" << benchmarkPairs[i].name << endl;
}

void Benchmark::runOnce()
{
    if (!m_isParallel) {
        m_benchmarkPair->function(m_isParallel);
        return;
    }

    dispatch_group_t group = dispatch_group_create();

    for (size_t i = 0; i < cpuCount(); ++i) {
        dispatch_group_async(group, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            m_benchmarkPair->function(m_isParallel);
        });
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    dispatch_release(group);
}

void Benchmark::run()
{
    static const size_t count = 4;
    static const size_t objectSize = 32;
    static const size_t chunkSize = 1024 * 1024;
    
    void*** heap = allocateHeap(m_heapSize, chunkSize, objectSize);

    runOnce(); // Warmup run.

    for (size_t i = 0; i < count; ++i) {
        double start = currentTimeMS();
        runOnce();
        double end = currentTimeMS();
        double elapsed = end - start;
        m_elapsedTime += elapsed;
    }
    m_elapsedTime /= count;

    deallocateHeap(heap, m_heapSize, chunkSize, objectSize);
    
    if (!m_measureHeap)
        return;

    // Wait a bit for any async freeing to finish.
    size_t last;
    do {
        last = currentMemoryBytes().resident;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    } while (currentMemoryBytes().resident < last);

    m_memory = currentMemoryBytes();
}

void Benchmark::printReport()
{
    size_t kB = 1024;

    cout << "Time:       \t" << m_elapsedTime << "ms" << endl;
    if (!m_measureHeap)
        return;

    cout << "Memory:     \t" << m_memory.resident / kB << "kB" << endl;
    cout << "Peak Memory:\t" << m_memory.residentMax / kB << "kB" << endl;
}

double Benchmark::currentTimeMS()
{
    struct timeval now;
    gettimeofday(&now, 0);
    return (now.tv_sec * 1000.0) + now.tv_usec / 1000.0;
}

Benchmark::Memory Benchmark::currentMemoryBytes()
{
    Memory memory;

    task_vm_info_data_t vm_info;
    mach_msg_type_number_t vm_size = TASK_VM_INFO_COUNT;
    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_VM_INFO_PURGEABLE, (task_info_t)(&vm_info), &vm_size)) {
        cout << "Failed to get mach task info" << endl;
        exit(1);
    }

    memory.resident = vm_info.internal - vm_info.purgeable_volatile_pmap;
    memory.residentMax = vm_info.resident_size_peak;
    return memory;
}
