/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "malloc/malloc.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <unistd.h>
#include <vector>

#if USE_SYSPROF_CAPTURE
#include <sysprof-capture.h>
#endif

static std::string currentExecutablePath()
{
    // Adapted from FileSystemGlib.cpp.
    static char readLinkBuffer[PATH_MAX];
    ssize_t result = readlink("/proc/self/exe", readLinkBuffer, PATH_MAX);
    if (result == -1)
        return "";
    return readLinkBuffer;
}

class MallocZoneHeapManager {
public:
    static MallocZoneHeapManager& getInstance()
    {
        static MallocZoneHeapManager singleton;
        return singleton;
    }

    malloc_zone_t* defaultZone()
    {
        return &m_defaultZone;
    }
    malloc_zone_t* createZone()
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto zone = std::make_unique<malloc_zone_t>();
        auto* zonePtr = zone.get();
        m_zoneAllocations.emplace(zonePtr, std::map<void*, size_t>());
        m_zoneNames.emplace(zonePtr, "No name");
        m_zoneObjects.insert(std::move(zone));
        return zonePtr;
    }
    void renameZone(malloc_zone_t* zone, std::string newName)
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!zone || !m_zoneNames.count(zone))
            return;
        m_zoneNames[zone] = newName;
    }

    void* zoneMalloc(malloc_zone_t* zone, size_t size)
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!zone || !m_zoneNames.count(zone))
            return nullptr;
        void* memory = malloc(size);
        m_zoneAllocations[zone].emplace(memory, size);
        return memory;
    }
    void* zoneCalloc(malloc_zone_t* zone, size_t numItems, size_t size)
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!zone || !m_zoneNames.count(zone))
            return nullptr;
        void* memory = calloc(numItems, size);
        if (memory)
            m_zoneAllocations[zone].emplace(memory, numItems * size);
        return memory;
    }
    void* zoneRealloc(malloc_zone_t* zone, void* memory, size_t size)
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!zone || !m_zoneNames.count(zone))
            return nullptr;
        if (!memory)
            return zoneMalloc(zone, size);
        if (!size) {
            zoneFree(zone, memory);
            return nullptr;
        }
        void* ptr = realloc(memory, size);
        if (ptr) {
            if (ptr != memory)
                m_zoneAllocations[zone].erase(memory);
            m_zoneAllocations[zone][ptr] = size;
        }
        return ptr;
    }
    void* zoneMemalign(malloc_zone_t* zone, size_t alignment, size_t size)
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!zone || !m_zoneNames.count(zone))
            return nullptr;
        void* memory = aligned_alloc(alignment, size);
        m_zoneAllocations[zone].emplace(memory, size);
        return memory;
    }
    void zoneFree(malloc_zone_t* zone, void* memory)
    {
        const std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!zone || !m_zoneNames.count(zone) || !m_zoneAllocations[zone].count(memory))
            return;
        m_zoneAllocations[zone].erase(memory);
        free(memory);
    }

private:
    MallocZoneHeapManager()
        : m_monitorInterval(std::atoi(std::getenv("WEBKIT_MALLOC_HEAP_BREAKDOWN_LOG_INTERVAL") ?: "3"))
    {
        std::printf("MallocZoneHeapManager created for PID:%d(%s)\n", getpid(), currentExecutablePath().c_str());
        m_zoneAllocations.emplace(&m_defaultZone, std::map<void*, size_t>());
        m_zoneNames.emplace(&m_defaultZone, "Default Zone");

        if (m_monitorInterval > std::chrono::seconds(0))
            m_monitorThread = std::thread(&MallocZoneHeapManager::monitoringThreadMain, this);
    }

    ~MallocZoneHeapManager()
    {
        m_forceThreadExit = true;
        if (m_monitorThread)
            m_monitorThread->join();
    }

    struct SysprofContext {
        std::map<malloc_zone_t*, unsigned> zoneCounterIds;
        std::string processName;
    };

    void monitoringThreadMain()
    {
        srand((unsigned)time(0));
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 3000));

        std::optional<SysprofContext> sysprofContext;
#if USE_SYSPROF_CAPTURE
        if (getenv("SYSPROF_CONTROL_FD")) {
            sysprof_collector_init();

            // Temporary workaround for libsysprof-capture providing conflicting IDs to threads.
            sysprof_collector_request_counters(1000);

            sysprofContext = std::make_optional<SysprofContext>();
            const auto processName = currentExecutablePath();
            sysprofContext->processName = processName.find("WebProcess") != std::string::npos
                ? "WebKit (Web)"
                : (processName.find("NetworkProcess") != std::string::npos ? "WebKit (Net)" : "WebKit (UI)");
        }
#endif

        while (!m_forceThreadExit) {
            {
                const std::lock_guard<std::recursive_mutex> lock(m_mutex);
                if (!sysprofContext) {
                    std::printf("%d Malloc Heap Breakdown: | PID | \"Zone name\" | Number of allocated chunks | Total bytes allocated | {\n", getpid());
                    size_t grandTotalBytesAllocated = 0;
                    for (const auto &[zonePtr, zoneName] : m_zoneNames) {
                        size_t totalBytesAllocated = 0;
                        for (const auto &[memoryPtr, bytesAllocated] : m_zoneAllocations[zonePtr])
                            totalBytesAllocated += bytesAllocated;
                        grandTotalBytesAllocated += totalBytesAllocated;
                        std::printf("%d \"%s\" %zu %zu\n", getpid(), zoneName.c_str(), m_zoneAllocations[zonePtr].size(), totalBytesAllocated);
                    }
                    std::printf("%d } Malloc Heap Breakdown: grand total bytes allocated: %zu\n", getpid(), grandTotalBytesAllocated);
                }
#if USE_SYSPROF_CAPTURE
                else {
                    std::vector<unsigned> counterIdsToSet;
                    std::vector<SysprofCaptureCounterValue> counterValuesToSet;
                    std::vector<SysprofCaptureCounter> countersToDefine;
                    auto setCounter = [&](malloc_zone_t* zone, const char* name, unsigned value) {
                        if (auto zoneCounterIdsIterator = sysprofContext->zoneCounterIds.find(zone); zoneCounterIdsIterator != sysprofContext->zoneCounterIds.end()) {
                            SysprofCaptureCounterValue counterValue;
                            counterValue.v64 = value;
                            unsigned id = zoneCounterIdsIterator->second;
                            counterIdsToSet.emplace_back(id);
                            counterValuesToSet.push_back(counterValue);
                        } else {
                            unsigned newId = sysprof_collector_request_counters(1);
                            SysprofCaptureCounter counter = { };
                            counter.id = newId;
                            counter.type = SYSPROF_CAPTURE_COUNTER_INT64;
                            counter.value.v64 = value;
                            sprintf(counter.category, "%s", sysprofContext->processName.c_str());
                            sprintf(counter.name, "%s", name);
                            countersToDefine.push_back(counter);
                            sysprofContext->zoneCounterIds.emplace(zone, newId);
                        }
                    };

                    size_t grandTotalBytesAllocated = 0;
                    for (const auto &[zonePtr, zoneName] : m_zoneNames) {
                        size_t totalBytesAllocated = 0;
                        for (const auto &[memoryPtr, bytesAllocated] : m_zoneAllocations[zonePtr])
                            totalBytesAllocated += bytesAllocated;
                        grandTotalBytesAllocated += totalBytesAllocated;
                        setCounter(zonePtr, zoneName.c_str(), totalBytesAllocated);
                    }
                    setCounter(nullptr, "Total bytes", grandTotalBytesAllocated);

                    if (!counterIdsToSet.empty())
                        sysprof_collector_set_counters(counterIdsToSet.data(), counterValuesToSet.data(), counterIdsToSet.size());
                    if (!countersToDefine.empty())
                        sysprof_collector_define_counters(countersToDefine.data(), countersToDefine.size());
                }
#endif
            }
            std::this_thread::sleep_for(m_monitorInterval);
        }
    }

    const std::chrono::seconds m_monitorInterval;
    malloc_zone_t m_defaultZone;
    std::map<malloc_zone_t*, std::map<void*, size_t>> m_zoneAllocations;
    std::map<malloc_zone_t*, std::string> m_zoneNames;
    std::set<std::unique_ptr<malloc_zone_t>> m_zoneObjects;
    std::recursive_mutex m_mutex;
    std::atomic<bool> m_forceThreadExit { false };
    std::optional<std::thread> m_monitorThread;
};

malloc_zone_t* malloc_default_zone()
{
    return MallocZoneHeapManager::getInstance().defaultZone();
}

malloc_zone_t* malloc_create_zone(vm_size_t, unsigned)
{
    return MallocZoneHeapManager::getInstance().createZone();
}

void* malloc_zone_malloc(malloc_zone_t* zone, size_t size)
{
    return MallocZoneHeapManager::getInstance().zoneMalloc(zone, size);
}

void* malloc_zone_calloc(malloc_zone_t* zone, size_t num_items, size_t size)
{
    return MallocZoneHeapManager::getInstance().zoneCalloc(zone, num_items, size);
}

void malloc_zone_free(malloc_zone_t* zone, void *ptr)
{
    MallocZoneHeapManager::getInstance().zoneFree(zone, ptr);
}

void* malloc_zone_realloc(malloc_zone_t* zone, void* ptr, size_t size)
{
    return MallocZoneHeapManager::getInstance().zoneRealloc(zone, ptr, size);
}

void* malloc_zone_memalign(malloc_zone_t* zone, size_t alignment, size_t size)
{
    return MallocZoneHeapManager::getInstance().zoneMemalign(zone, alignment, size);
}

void malloc_set_zone_name(malloc_zone_t* zone, const char* name)
{
    MallocZoneHeapManager::getInstance().renameZone(zone, name);
}

size_t malloc_zone_pressure_relief(malloc_zone_t*, size_t)
{
    return 0;
}

void malloc_zone_print(malloc_zone_t*, bool)
{
}
