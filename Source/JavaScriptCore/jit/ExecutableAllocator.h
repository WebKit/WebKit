/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#include <bit>
#include "ExecutableMemoryHandle.h"
#include "FastJITPermissions.h"
#include "JITCompilationEffort.h"
#include "JSCConfig.h"
#include "JSCPtrTag.h"
#include "Options.h"
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Gigacage.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

#if !ENABLE(LIBPAS_JIT_HEAP)
#include <wtf/MetaAllocator.h>
#endif

#if OS(DARWIN)
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#endif

#if ENABLE(MPROTECT_RX_TO_RWX)
#define EXECUTABLE_POOL_WRITABLE false
#else
#define EXECUTABLE_POOL_WRITABLE true
#endif

namespace JSC {

static constexpr unsigned jitAllocationGranule = 32;

class ExecutableAllocatorBase {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(ExecutableAllocatorBase);
public:
    bool isValid() const { return false; }

    static bool underMemoryPressure() { return false; }

    static double memoryPressureMultiplier(size_t) { return 1.0; }

    static void dumpProfile() { }

    RefPtr<ExecutableMemoryHandle> allocate(size_t, JITCompilationEffort) { return nullptr; }

    static void disableJIT() { };
    
    bool isValidExecutableMemory(const AbstractLocker&, void*) { return false; }

    static size_t committedByteCount() { return 0; }

    Lock& getLock() const WTF_RETURNS_LOCK(m_lock)
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

ALWAYS_INLINE bool isJITPC(void* pc)
{
    return g_jscConfig.startExecutableMemory <= pc && pc < g_jscConfig.endExecutableMemory;
}

JS_EXPORT_PRIVATE void dumpJITMemory(const void*, const void*, size_t);

#if ENABLE(MPROTECT_RX_TO_RWX)
JS_EXPORT_PRIVATE void* performJITMemcpyWithMProtect(void *dst, const void *src, size_t n);
#endif

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

#if ENABLE(JIT_SCAN_ASSEMBLER_BUFFER_FOR_ZEROES)
        auto checkForZeroes = [n] (const void* buffer_v) {
            // On x86-64, the maximum immediate size is 8B, no opcodes/prefixes have 0x00
            // On other architectures this could be smaller
            constexpr size_t maxZeroByteRunLength = 16;
            // This algorithm works because the number of 0-bytes which can fit into
            // one qword (8) is smaller than the limit on which we assert.
            constexpr size_t stride = sizeof(uint64_t);
            static_assert(stride <= maxZeroByteRunLength);

            const char* buffer = reinterpret_cast<const char*>(buffer_v);
            size_t runLength = 0;
            size_t i = 0;
            if (n > stride) {
                for (; (reinterpret_cast<uintptr_t>(buffer) + i) % stride; i++) {
                    if (!(buffer[i]))
                        runLength++;
                    else
                        runLength = 0;
                }
                for (; i + stride <= n; i += stride) {
                    uint64_t chunk = *reinterpret_cast<const uint64_t*>(buffer + i);
                    if (!chunk) {
                        runLength += sizeof(chunk);
                        RELEASE_ASSERT(runLength <= maxZeroByteRunLength, buffer);
                    } else {
                        runLength += (std::countr_zero(chunk) / 8);
                        RELEASE_ASSERT(runLength <= maxZeroByteRunLength, buffer);
                        runLength = std::countl_zero(chunk) / 8;
                    }
                }
                for (; i < n; i++) {
                    if (!(buffer[i])) {
                        runLength++;
                        RELEASE_ASSERT(runLength <= maxZeroByteRunLength, buffer);
                    }
                }
            }
        };
#endif

        if (UNLIKELY(Options::dumpJITMemoryPath()))
            dumpJITMemory(dst, src, n);

#if ENABLE(MPROTECT_RX_TO_RWX)
        auto ret = performJITMemcpyWithMProtect(dst, src, n);
#if ENABLE(JIT_SCAN_ASSEMBLER_BUFFER_FOR_ZEROES)
        checkForZeroes(dst);
#endif
        return ret;
#endif

        if (g_jscConfig.useFastJITPermissions) {
            threadSelfRestrict<MemoryRestriction::kRwxToRw>();
            memcpy(dst, src, n);
            threadSelfRestrict<MemoryRestriction::kRwxToRx>();
#if ENABLE(JIT_SCAN_ASSEMBLER_BUFFER_FOR_ZEROES)
            checkForZeroes(dst);
#endif
            return dst;
        }

#if ENABLE(SEPARATED_WX_HEAP)
        if (g_jscConfig.jitWriteSeparateHeaps) {
            // Use execute-only write thunk for writes inside the JIT region. This is a variant of
            // memcpy that takes an offset into the JIT region as its destination (first) parameter.
            off_t offset = (off_t)((uintptr_t)dst - startOfFixedExecutableMemoryPool<uintptr_t>());
            retagCodePtr<JITThunkPtrTag, CFunctionPtrTag>(g_jscConfig.jitWriteSeparateHeaps)(offset, src, n);
            RELEASE_ASSERT(!Gigacage::contains(src));
#if ENABLE(JIT_SCAN_ASSEMBLER_BUFFER_FOR_ZEROES)
            checkForZeroes(dst);
#endif
            return dst;
        }
#endif

        auto ret = memcpy(dst, src, n);
#if ENABLE(JIT_SCAN_ASSEMBLER_BUFFER_FOR_ZEROES)
        checkForZeroes(dst);
#endif
        return ret;
    }

    // Use regular memcpy for writes outside the JIT region.
    return memcpy(dst, src, n);
}

class ExecutableAllocator : private ExecutableAllocatorBase {
    WTF_MAKE_TZONE_ALLOCATED(ExecutableAllocator);
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
    
    JS_EXPORT_PRIVATE static void disableJIT();

    RefPtr<ExecutableMemoryHandle> allocate(size_t sizeInBytes, JITCompilationEffort);

    bool isValidExecutableMemory(const AbstractLocker&, void* address);

    static size_t committedByteCount();

    Lock& getLock() const;

#if ENABLE(MPROTECT_RX_TO_RWX)
    void startWriting(const void* start, size_t sizeInBytes);
    void finishWriting(const void* start, size_t sizeInBytes);
#endif

#if ENABLE(JUMP_ISLANDS)
    JS_EXPORT_PRIVATE void* getJumpIslandToUsingJITMemcpy(void* from, void* newDestination);
    JS_EXPORT_PRIVATE void* getJumpIslandToUsingMemcpy(void* from, void* newDestination);
    JS_EXPORT_PRIVATE void* getJumpIslandToConcurrently(void* from, void* newDestination);
#endif

private:
    ExecutableAllocator() = default;
    ~ExecutableAllocator() = default;
};

#else

class ExecutableAllocator : public ExecutableAllocatorBase {
    WTF_MAKE_TZONE_ALLOCATED(ExecutableAllocator);
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
