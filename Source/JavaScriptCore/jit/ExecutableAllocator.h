/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "JITCompilationEffort.h"
#include "JSCPtrTag.h"
#include <stddef.h> // for ptrdiff_t
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/Lock.h>
#include <wtf/MetaAllocatorHandle.h>
#include <wtf/MetaAllocator.h>

#if OS(IOS)
#include <libkern/OSCacheControl.h>
#endif

#if OS(IOS)
#include <sys/mman.h>
#endif

#if CPU(MIPS) && OS(LINUX)
#include <sys/cachectl.h>
#endif

#if ENABLE(FAST_JIT_PERMISSIONS)
#include <os/thread_self_restrict.h> 
#endif
#define JIT_ALLOCATOR_LARGE_ALLOC_SIZE (pageSize() * 4)

#define EXECUTABLE_POOL_WRITABLE true

namespace JSC {

static const unsigned jitAllocationGranule = 32;

typedef WTF::MetaAllocatorHandle ExecutableMemoryHandle;

#if ENABLE(JIT)

JS_EXPORT_PRIVATE void* startOfFixedExecutableMemoryPoolImpl();
JS_EXPORT_PRIVATE void* endOfFixedExecutableMemoryPoolImpl();

template<typename T = void*>
T startOfFixedExecutableMemoryPool()
{
    return bitwise_cast<T>(startOfFixedExecutableMemoryPoolImpl());
}

template<typename T = void*>
T endOfFixedExecutableMemoryPool()
{
    return bitwise_cast<T>(endOfFixedExecutableMemoryPoolImpl());
}

bool isJITPC(void* pc);

#if !ENABLE(FAST_JIT_PERMISSIONS) || !CPU(ARM64E)

typedef void (*JITWriteSeparateHeapsFunction)(off_t, const void*, size_t);
extern JS_EXPORT_PRIVATE JITWriteSeparateHeapsFunction jitWriteSeparateHeapsFunction;
extern JS_EXPORT_PRIVATE bool useFastPermisionsJITCopy;

#endif // !ENABLE(FAST_JIT_PERMISSIONS) || !CPU(ARM64E)

static inline void* performJITMemcpy(void *dst, const void *src, size_t n)
{
#if CPU(ARM64)
    static constexpr size_t instructionSize = sizeof(unsigned);
    RELEASE_ASSERT(roundUpToMultipleOf<instructionSize>(dst) == dst);
    RELEASE_ASSERT(roundUpToMultipleOf<instructionSize>(src) == src);
#endif
    if (isJITPC(dst)) {
        RELEASE_ASSERT(reinterpret_cast<uint8_t*>(dst) + n <= endOfFixedExecutableMemoryPool());
#if ENABLE(FAST_JIT_PERMISSIONS)
#if !CPU(ARM64E)
        if (useFastPermisionsJITCopy)
#endif
        {
            os_thread_self_restrict_rwx_to_rw();
            memcpy(dst, src, n);
            os_thread_self_restrict_rwx_to_rx();
            return dst;
        }
#endif // ENABLE(FAST_JIT_PERMISSIONS)

#if !ENABLE(FAST_JIT_PERMISSIONS) || !CPU(ARM64E)
        if (jitWriteSeparateHeapsFunction) {
            // Use execute-only write thunk for writes inside the JIT region. This is a variant of
            // memcpy that takes an offset into the JIT region as its destination (first) parameter.
            off_t offset = (off_t)((uintptr_t)dst - startOfFixedExecutableMemoryPool<uintptr_t>());
            retagCodePtr<JITThunkPtrTag, CFunctionPtrTag>(jitWriteSeparateHeapsFunction)(offset, src, n);
            return dst;
        }
#endif
    }

    // Use regular memcpy for writes outside the JIT region.
    return memcpy(dst, src, n);
}

class ExecutableAllocator {
    enum ProtectionSetting { Writable, Executable };

public:
    static ExecutableAllocator& singleton();
    static void initializeAllocator();

    bool isValid() const;

    static bool underMemoryPressure();
    
    static double memoryPressureMultiplier(size_t addedMemoryUsage);
    
#if ENABLE(META_ALLOCATOR_PROFILE)
    static void dumpProfile();
#else
    static void dumpProfile() { }
#endif

    RefPtr<ExecutableMemoryHandle> allocate(size_t sizeInBytes, void* ownerUID, JITCompilationEffort);

    bool isValidExecutableMemory(const AbstractLocker&, void* address);

    static size_t committedByteCount();

    Lock& getLock() const;
private:

    ExecutableAllocator();
    ~ExecutableAllocator();
};

#else

class ExecutableAllocator {
    enum ProtectionSetting { Writable, Executable };

public:
    static ExecutableAllocator& singleton();
    static void initializeAllocator();

    bool isValid() const { return false; }

    static bool underMemoryPressure() { return false; }

    static double memoryPressureMultiplier(size_t) { return 1.0; }

    static void dumpProfile() { }

    RefPtr<ExecutableMemoryHandle> allocate(size_t, void*, JITCompilationEffort) { return nullptr; }

    bool isValidExecutableMemory(const AbstractLocker&, void*) { return false; }

    static size_t committedByteCount() { return 0; }

    Lock& getLock() const
    {
        return m_lock;
    }

private:
    mutable Lock m_lock;
};

static inline void* performJITMemcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

inline bool isJITPC(void*) { return false; }
#endif // ENABLE(JIT)


} // namespace JSC
