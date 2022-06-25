/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#import "config.h"
#import "WebMemorySampler.h"

#if ENABLE(MEMORY_SAMPLER)

#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/MemoryStatistics.h>
#import <JavaScriptCore/VM.h>
#import <WebCore/CommonVM.h>
#import <mach/mach.h>
#import <mach/mach_types.h>
#import <mach/task.h>
#import <malloc/malloc.h>
#import <notify.h>
#import <wtf/WallTime.h>

namespace WebKit {
    
struct SystemMallocStats {
    malloc_statistics_t defaultMallocZoneStats;
    malloc_statistics_t dispatchContinuationMallocZoneStats;
    malloc_statistics_t purgeableMallocZoneStats;
};

SystemMallocStats WebMemorySampler::sampleSystemMalloc() const
{
    static const char* defaultMallocZoneName = "DefaultMallocZone";
    static const char* dispatchContinuationMallocZoneName = "DispatchContinuations";
    static const char* purgeableMallocZoneName = "DefaultPurgeableMallocZone";
    SystemMallocStats mallocStats;
    vm_address_t* zones;
    unsigned count;
    
    // Zero out the structures in case a zone is missing
    malloc_statistics_t stats;
    stats.blocks_in_use = 0;
    stats.size_in_use = 0;
    stats.max_size_in_use = 0;
    stats.size_allocated = 0;
    mallocStats.defaultMallocZoneStats = stats;
    mallocStats.dispatchContinuationMallocZoneStats = stats;
    mallocStats.purgeableMallocZoneStats = stats;
    
    malloc_get_all_zones(mach_task_self(), 0, &zones, &count);
    for (unsigned i = 0; i < count; i++) {
        if (const char* name = malloc_get_zone_name(reinterpret_cast<malloc_zone_t*>(zones[i]))) {
            stats.blocks_in_use = 0;
            stats.size_in_use = 0;
            stats.max_size_in_use = 0;
            stats.size_allocated = 0;
            malloc_zone_statistics(reinterpret_cast<malloc_zone_t*>(zones[i]), &stats);
            if (!strcmp(name, defaultMallocZoneName))
                mallocStats.defaultMallocZoneStats = stats;
            else if (!strcmp(name, dispatchContinuationMallocZoneName))
                mallocStats.dispatchContinuationMallocZoneStats = stats;
            else if (!strcmp(name, purgeableMallocZoneName))
                mallocStats.purgeableMallocZoneStats = stats;
        }
    }
    return mallocStats;
}

size_t WebMemorySampler::sampleProcessCommittedBytes() const
{
    task_basic_info_64 taskInfo;
    mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
    task_info(mach_task_self(), TASK_BASIC_INFO_64, reinterpret_cast<task_info_t>(&taskInfo), &count);
    return taskInfo.resident_size;
}

String WebMemorySampler::processName() const
{
    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (!appName)
        appName = [[NSProcessInfo processInfo] processName];
    return String(appName);
}
  
WebMemoryStatistics WebMemorySampler::sampleWebKit() const
{
    size_t totalBytesInUse = 0, totalBytesCommitted = 0; 
    
    WebMemoryStatistics webKitMemoryStats;
    
    auto fastMallocStatistics = WTF::fastMallocStatistics();
    size_t fastMallocBytesInUse = fastMallocStatistics.committedVMBytes - fastMallocStatistics.freeListBytes;
    size_t fastMallocBytesCommitted = fastMallocStatistics.committedVMBytes;
    totalBytesInUse += fastMallocBytesInUse;
    totalBytesCommitted += fastMallocBytesCommitted;
    
    JSC::JSLockHolder lock(WebCore::commonVM());
    size_t jscHeapBytesInUse = WebCore::commonVM().heap.size();
    size_t jscHeapBytesCommitted = WebCore::commonVM().heap.capacity();
    totalBytesInUse += jscHeapBytesInUse;
    totalBytesCommitted += jscHeapBytesCommitted;
    
    JSC::GlobalMemoryStatistics globalMemoryStats = JSC::globalMemoryStatistics();
    totalBytesInUse += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;
    totalBytesCommitted += globalMemoryStats.stackBytes + globalMemoryStats.JITBytes;
    
    SystemMallocStats systemStats = sampleSystemMalloc();
    
    size_t defaultMallocZoneBytesInUse = systemStats.defaultMallocZoneStats.size_in_use;
    size_t dispatchContinuationMallocZoneBytesInUse = systemStats.dispatchContinuationMallocZoneStats.size_in_use;
    size_t purgeableMallocZoneBytesInUse = systemStats.purgeableMallocZoneStats.size_in_use;
    size_t defaultMallocZoneBytesCommitted = systemStats.defaultMallocZoneStats.size_allocated;
    size_t dispatchContinuationMallocZoneBytesCommitted = systemStats.dispatchContinuationMallocZoneStats.size_allocated;
    size_t purgeableMallocZoneBytesCommitted = systemStats.purgeableMallocZoneStats.size_allocated;
    totalBytesInUse += defaultMallocZoneBytesInUse + dispatchContinuationMallocZoneBytesInUse + purgeableMallocZoneBytesInUse;
    totalBytesCommitted += defaultMallocZoneBytesCommitted + dispatchContinuationMallocZoneBytesCommitted + purgeableMallocZoneBytesCommitted;
    
    size_t residentSize = sampleProcessCommittedBytes();

    WallTime now = WallTime::now();
        
    webKitMemoryStats.keys.append("Timestamp"_s);
    webKitMemoryStats.values.append(now.secondsSinceEpoch().seconds());
    webKitMemoryStats.keys.append("Total Bytes of Memory In Use"_s);
    webKitMemoryStats.values.append(totalBytesInUse);
    webKitMemoryStats.keys.append("Fast Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(fastMallocBytesInUse);
    webKitMemoryStats.keys.append("Default Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(defaultMallocZoneBytesInUse);
    webKitMemoryStats.keys.append("Dispatch Continuation Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(dispatchContinuationMallocZoneBytesInUse);
    webKitMemoryStats.keys.append("Purgeable Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(purgeableMallocZoneBytesInUse);
    webKitMemoryStats.keys.append("JavaScript Heap Bytes"_s);
    webKitMemoryStats.values.append(jscHeapBytesInUse);
    webKitMemoryStats.keys.append("Total Bytes of Committed Memory"_s);
    webKitMemoryStats.values.append(totalBytesCommitted);
    webKitMemoryStats.keys.append("Fast Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(fastMallocBytesCommitted);
    webKitMemoryStats.keys.append("Default Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(defaultMallocZoneBytesCommitted);
    webKitMemoryStats.keys.append("Dispatch Continuation Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(dispatchContinuationMallocZoneBytesCommitted);
    webKitMemoryStats.keys.append("Purgeable Malloc Zone Bytes"_s);
    webKitMemoryStats.values.append(purgeableMallocZoneBytesCommitted);
    webKitMemoryStats.keys.append("JavaScript Heap Bytes"_s);
    webKitMemoryStats.values.append(jscHeapBytesCommitted);
    webKitMemoryStats.keys.append("JavaScript Stack Bytes"_s);
    webKitMemoryStats.values.append(globalMemoryStats.stackBytes);
    webKitMemoryStats.keys.append("JavaScript JIT Bytes"_s);
    webKitMemoryStats.values.append(globalMemoryStats.JITBytes);
    webKitMemoryStats.keys.append("Resident Size"_s);
    webKitMemoryStats.values.append(residentSize);
    
    return webKitMemoryStats;
}
 
void WebMemorySampler::sendMemoryPressureEvent()
{
    // Free memory that could be released if we needed more.
    // We want to track memory that cannot.
    notify_post("org.WebKit.lowMemory");
}

}

#endif

