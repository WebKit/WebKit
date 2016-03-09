/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ExecutableAllocator_h
#define ExecutableAllocator_h
#include "JITCompilationEffort.h"
#include <stddef.h> // for ptrdiff_t
#include <limits>
#include <wtf/Assertions.h>
#include <wtf/Lock.h>
#include <wtf/MetaAllocatorHandle.h>
#include <wtf/MetaAllocator.h>
#include <wtf/PageAllocation.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if OS(IOS)
#include <libkern/OSCacheControl.h>
#endif

#if OS(IOS)
#include <sys/mman.h>
#endif

#if CPU(MIPS) && OS(LINUX)
#include <sys/cachectl.h>
#endif

#if CPU(SH4) && OS(LINUX)
#include <asm/cachectl.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#define JIT_ALLOCATOR_LARGE_ALLOC_SIZE (pageSize() * 4)

#define EXECUTABLE_POOL_WRITABLE true

namespace JSC {

class VM;

static const unsigned jitAllocationGranule = 32;

typedef WTF::MetaAllocatorHandle ExecutableMemoryHandle;

#if ENABLE(ASSEMBLER)

#if ENABLE(EXECUTABLE_ALLOCATOR_DEMAND)
class DemandExecutableAllocator;
#endif

#if ENABLE(EXECUTABLE_ALLOCATOR_FIXED)
#if CPU(ARM)
static const size_t fixedExecutableMemoryPoolSize = 16 * 1024 * 1024;
#elif CPU(ARM64)
static const size_t fixedExecutableMemoryPoolSize = 32 * 1024 * 1024;
#elif CPU(X86_64)
static const size_t fixedExecutableMemoryPoolSize = 1024 * 1024 * 1024;
#else
static const size_t fixedExecutableMemoryPoolSize = 32 * 1024 * 1024;
#endif
#if CPU(ARM)
static const double executablePoolReservationFraction = 0.15;
#else
static const double executablePoolReservationFraction = 0.25;
#endif

extern uintptr_t startOfFixedExecutableMemoryPool;
extern uintptr_t endOfFixedExecutableMemoryPool;

#if ENABLE(SEPARATED_WX_HEAP)
extern uintptr_t jitWriteFunctionAddress;
#endif
#endif // ENABLE(EXECUTABLE_ALLOCATOR_FIXED)

static inline void* performJITMemcpy(void *dst, const void *src, size_t n)
{
#if ENABLE(SEPARATED_WX_HEAP)
    // Use execute-only write thunk for writes inside the JIT region. This is a variant of
    // memcpy that takes an offset into the JIT region as its destination (first) parameter.
    if (jitWriteFunctionAddress && (uintptr_t)dst >= startOfFixedExecutableMemoryPool && (uintptr_t)dst <= endOfFixedExecutableMemoryPool) {
        using JITWriteFunction = void (*)(off_t, const void*, size_t);
        JITWriteFunction func = (JITWriteFunction)jitWriteFunctionAddress;
        off_t offset = (off_t)((uintptr_t)dst - startOfFixedExecutableMemoryPool);
        func(offset, src, n);
        return dst;
    }
#endif

    // Use regular memcpy for writes outside the JIT region.
    return memcpy(dst, src, n);
}

class ExecutableAllocator {
    enum ProtectionSetting { Writable, Executable };

public:
    ExecutableAllocator(VM&);
    ~ExecutableAllocator();
    
    static void initializeAllocator();

    bool isValid() const;

    static bool underMemoryPressure();
    
    static double memoryPressureMultiplier(size_t addedMemoryUsage);
    
#if ENABLE(META_ALLOCATOR_PROFILE)
    static void dumpProfile();
#else
    static void dumpProfile() { }
#endif

    RefPtr<ExecutableMemoryHandle> allocate(VM&, size_t sizeInBytes, void* ownerUID, JITCompilationEffort);

    bool isValidExecutableMemory(const LockHolder&, void* address);

    static size_t committedByteCount();

    Lock& getLock() const;
};

#endif // ENABLE(JIT) && ENABLE(ASSEMBLER)

} // namespace JSC

#endif // !defined(ExecutableAllocator)
