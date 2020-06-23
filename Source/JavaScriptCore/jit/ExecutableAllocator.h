/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "FastJITPermissions.h"
#include "JITCompilationEffort.h"
#include "JSCConfig.h"
#include "JSCPtrTag.h"
#include "Options.h"
#include <stddef.h> // for ptrdiff_t
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/MetaAllocatorHandle.h>
#include <wtf/MetaAllocator.h>

#if OS(DARWIN)
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#endif

#if CPU(MIPS) && OS(LINUX)
#include <sys/cachectl.h>
#endif

#define JIT_ALLOCATOR_LARGE_ALLOC_SIZE (pageSize() * 4)

#define EXECUTABLE_POOL_WRITABLE true

namespace JSC {

static constexpr unsigned jitAllocationGranule = 32;

typedef WTF::MetaAllocatorHandle ExecutableMemoryHandle;

class ExecutableAllocatorBase {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ExecutableAllocatorBase);
public:
    bool isValid() const { return false; }

    static bool underMemoryPressure() { return false; }

    static double memoryPressureMultiplier(size_t) { return 1.0; }

    static void dumpProfile() { }

    RefPtr<ExecutableMemoryHandle> allocate(size_t, JITCompilationEffort) { return nullptr; }

    static void setJITEnabled(bool) { };
    
    bool isValidExecutableMemory(const AbstractLocker&, void*) { return false; }

    static size_t committedByteCount() { return 0; }

    Lock& getLock() const
    {
        return m_lock;
    }

protected:
    ExecutableAllocatorBase() = default;
    ~ExecutableAllocatorBase() = default;

private:
    mutable Lock m_lock;
};

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

JS_EXPORT_PRIVATE bool isJITPC(void* pc);

JS_EXPORT_PRIVATE void dumpJITMemory(const void*, const void*, size_t);

static ALWAYS_INLINE void* performJITMemcpy(void *dst, const void *src, size_t n)
{
#if CPU(ARM64)
    static constexpr size_t instructionSize = sizeof(unsigned);
    RELEASE_ASSERT(roundUpToMultipleOf<instructionSize>(dst) == dst);
    RELEASE_ASSERT(roundUpToMultipleOf<instructionSize>(src) == src);
#endif
    if (isJITPC(dst)) {
        RELEASE_ASSERT(!Gigacage::contains(src));
        RELEASE_ASSERT(reinterpret_cast<uint8_t*>(dst) + n <= endOfFixedExecutableMemoryPool());

        if (UNLIKELY(Options::dumpJITMemoryPath()))
            dumpJITMemory(dst, src, n);

        if (useFastJITPermissions()) {
            threadSelfRestrictRWXToRW();
            memcpy(dst, src, n);
            threadSelfRestrictRWXToRX();
            return dst;
        }

#if ENABLE(SEPARATED_WX_HEAP)
        if (g_jscConfig.jitWriteSeparateHeaps) {
            // Use execute-only write thunk for writes inside the JIT region. This is a variant of
            // memcpy that takes an offset into the JIT region as its destination (first) parameter.
            off_t offset = (off_t)((uintptr_t)dst - startOfFixedExecutableMemoryPool<uintptr_t>());
            retagCodePtr<JITThunkPtrTag, CFunctionPtrTag>(g_jscConfig.jitWriteSeparateHeaps)(offset, src, n);
            RELEASE_ASSERT(!Gigacage::contains(src));
            return dst;
        }
#endif
    }

    // Use regular memcpy for writes outside the JIT region.
    return memcpy(dst, src, n);
}

class ExecutableAllocator : private ExecutableAllocatorBase {
public:
    using Base = ExecutableAllocatorBase;

    JS_EXPORT_PRIVATE static ExecutableAllocator& singleton();
    static void initialize();
    static void initializeUnderlyingAllocator();

    bool isValid() const;

    static bool underMemoryPressure();
    
    static double memoryPressureMultiplier(size_t addedMemoryUsage);
    
#if ENABLE(META_ALLOCATOR_PROFILE)
    static void dumpProfile();
#else
    static void dumpProfile() { }
#endif
    
    JS_EXPORT_PRIVATE static void setJITEnabled(bool);

    RefPtr<ExecutableMemoryHandle> allocate(size_t sizeInBytes, JITCompilationEffort);

    bool isValidExecutableMemory(const AbstractLocker&, void* address);

    static size_t committedByteCount();

    Lock& getLock() const;

#if USE(JUMP_ISLANDS)
    JS_EXPORT_PRIVATE void* getJumpIslandTo(void* from, void* newDestination);
    JS_EXPORT_PRIVATE void* getJumpIslandToConcurrently(void* from, void* newDestination);
#endif

private:
    ExecutableAllocator() = default;
    ~ExecutableAllocator() = default;
};

#else

class ExecutableAllocator : public ExecutableAllocatorBase {
public:
    static ExecutableAllocator& singleton();
    static void initialize();
    static void initializeUnderlyingAllocator() { }

private:
    ExecutableAllocator() = default;
    ~ExecutableAllocator() = default;
};

static inline void* performJITMemcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

inline bool isJITPC(void*) { return false; }
#endif // ENABLE(JIT)


} // namespace JSC
