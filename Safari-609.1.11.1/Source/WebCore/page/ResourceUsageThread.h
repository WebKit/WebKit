/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(RESOURCE_USAGE)

#include "ResourceUsageData.h"
#include <array>
#include <functional>
#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>

#if OS(DARWIN)
#include <mach/mach.h>
#endif

namespace JSC {
class VM;
}

namespace WebCore {

enum ResourceUsageCollectionMode {
    None = 0,
    CPU = 1 << 0,
    Memory = 1 << 1,
    All = CPU | Memory,
};

class ResourceUsageThread {
    WTF_MAKE_NONCOPYABLE(ResourceUsageThread);

public:
    static void addObserver(void* key, ResourceUsageCollectionMode, std::function<void (const ResourceUsageData&)>);
    static void removeObserver(void* key);

private:
    friend NeverDestroyed<ResourceUsageThread>;
    ResourceUsageThread();
    static ResourceUsageThread& singleton();

    void waitUntilObservers();
    void notifyObservers(ResourceUsageData&&);

    void recomputeCollectionMode();

    void createThreadIfNeeded();
    NO_RETURN void threadBody();

    void platformSaveStateBeforeStarting();
    void platformCollectCPUData(JSC::VM*, ResourceUsageData&);
    void platformCollectMemoryData(JSC::VM*, ResourceUsageData&);

    RefPtr<Thread> m_thread;
    Lock m_lock;
    Condition m_condition;
    HashMap<void*, std::pair<ResourceUsageCollectionMode, std::function<void (const ResourceUsageData&)>>> m_observers;
    ResourceUsageCollectionMode m_collectionMode { None };

    // Platforms may need to access some data from the common VM.
    // They should ensure their use of the VM is thread safe.
    JSC::VM* m_vm { nullptr };

#if ENABLE(SAMPLING_PROFILER) && OS(DARWIN)
    mach_port_t m_samplingProfilerMachThread { MACH_PORT_NULL };
#endif

};

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
