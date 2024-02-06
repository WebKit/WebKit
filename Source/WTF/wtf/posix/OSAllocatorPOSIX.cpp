/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/OSAllocator.h>

#include <errno.h>
#include <sys/mman.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/PageBlock.h>

#if ENABLE(JIT_CAGE)
#include <WebKitAdditions/JITCageAdditions.h>
#else // ENABLE(JIT_CAGE)
#if OS(DARWIN)
#define MAP_EXECUTABLE_FOR_JIT MAP_JIT
#define MAP_EXECUTABLE_FOR_JIT_WITH_JIT_CAGE MAP_JIT
#else // OS(DARWIN)
#define MAP_EXECUTABLE_FOR_JIT 0
#define MAP_EXECUTABLE_FOR_JIT_WITH_JIT_CAGE 0
#endif // OS(DARWIN)
#endif // ENABLE(JIT_CAGE)

#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/MachVMSPI.h>
#endif

namespace WTF {

void* OSAllocator::tryReserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    // All POSIX reservations start out logically committed.
    int protection = PROT_READ;
    if (writable)
        protection |= PROT_WRITE;
    if (executable)
        protection |= PROT_EXEC;

    int flags = MAP_PRIVATE | MAP_ANON;
#if OS(DARWIN)
    if (executable) {
        if (jitCageEnabled)
            flags |= MAP_EXECUTABLE_FOR_JIT_WITH_JIT_CAGE;
        else
            flags |= MAP_EXECUTABLE_FOR_JIT;
    }
#else
    UNUSED_PARAM(jitCageEnabled);
#endif

#if OS(DARWIN)
    int fd = usage;
#else
    UNUSED_PARAM(usage);
    int fd = -1;
#endif

    void* result = nullptr;
#if (OS(DARWIN) && CPU(X86_64))
    if (executable) {
        ASSERT(includesGuardPages);
        // Cook up an address to allocate at, using the following recipe:
        //   17 bits of zero, stay in userspace kids.
        //   26 bits of randomness for ASLR.
        //   21 bits of zero, at least stay aligned within one level of the pagetables.
        //
        // But! - as a temporary workaround for some plugin problems (rdar://problem/6812854),
        // for now instead of 2^26 bits of ASLR lets stick with 25 bits of randomization plus
        // 2^24, which should put up somewhere in the middle of userspace (in the address range
        // 0x200000000000 .. 0x5fffffffffff).
        intptr_t randomLocation = 0;
        randomLocation = arc4random() & ((1 << 25) - 1);
        randomLocation += (1 << 24);
        randomLocation <<= 21;
        result = reinterpret_cast<void*>(randomLocation);
    }
#endif

    result = mmap(result, bytes, protection, flags, fd, 0);
    if (result == MAP_FAILED)
        result = nullptr;
    if (result && includesGuardPages) {
        // We use mmap to remap the guardpages rather than using mprotect as
        // mprotect results in multiple references to the code region. This
        // breaks the madvise based mechanism we use to return physical memory
        // to the OS.
        mmap(result, pageSize(), PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, fd, 0);
        mmap(static_cast<char*>(result) + bytes - pageSize(), pageSize(), PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, fd, 0);
    }
    return result;
}

void* OSAllocator::tryReserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
#if OS(LINUX)
    UNUSED_PARAM(usage);
    UNUSED_PARAM(jitCageEnabled);
    UNUSED_PARAM(includesGuardPages);
    int protection = PROT_READ;
    if (writable)
        protection |= PROT_WRITE;
    if (executable)
        protection |= PROT_EXEC;

    void* result = mmap(0, bytes, protection, MAP_NORESERVE | MAP_PRIVATE | MAP_ANON, -1, 0);
    if (result == MAP_FAILED)
        result = nullptr;
    if (result)
        while (madvise(result, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#else
    void* result = tryReserveAndCommit(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
#if HAVE(MADV_FREE_REUSE)
    if (result) {
        // To support the "reserve then commit" model, we have to initially decommit.
        while (madvise(result, bytes, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
    }
#endif

#endif

    return result;
}

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    void* result = tryReserveUncommitted(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
    RELEASE_ASSERT(result);
    return result;
}

void* OSAllocator::tryReserveUncommittedAligned(size_t bytes, size_t alignment, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    ASSERT(hasOneBitSet(alignment) && alignment >= pageSize());

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
    UNUSED_PARAM(usage); // Not supported for mach API.
    ASSERT_UNUSED(includesGuardPages, !includesGuardPages);
    ASSERT_UNUSED(jitCageEnabled, !jitCageEnabled); // Not supported for mach API.
    vm_prot_t protections = VM_PROT_READ;
    if (writable)
        protections |= VM_PROT_WRITE;
    if (executable)
        protections |= VM_PROT_EXECUTE;

    const vm_inherit_t childProcessInheritance = VM_INHERIT_DEFAULT;
    const bool copy = false;
    const int flags = VM_FLAGS_ANYWHERE;

    void* aligned = nullptr;
    kern_return_t result = mach_vm_map(mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&aligned), bytes, alignment - 1, flags, MEMORY_OBJECT_NULL, 0, copy, protections, protections, childProcessInheritance);
    ASSERT_UNUSED(result, result == KERN_SUCCESS || !aligned);
#if HAVE(MADV_FREE_REUSE)
    if (aligned) {
        // To support the "reserve then commit" model, we have to initially decommit.
        while (madvise(aligned, bytes, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
    }
#endif

    return aligned;
#else
#if HAVE(MAP_ALIGNED)
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
    UNUSED_PARAM(usage);
    UNUSED_PARAM(jitCageEnabled);
    UNUSED_PARAM(includesGuardPages);
    int protection = PROT_READ;
    if (writable)
        protection |= PROT_WRITE;
    if (executable)
        protection |= PROT_EXEC;

    void* result = mmap(0, bytes, protection, MAP_NORESERVE | MAP_PRIVATE | MAP_ANON | MAP_ALIGNED(getLSBSet(alignment)), -1, 0);
    if (result == MAP_FAILED)
        return nullptr;
    if (result)
        while (madvise(result, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
    return result;
#else

    // Add the alignment so we can ensure enough mapped memory to get an aligned start.
    size_t mappedSize = bytes + alignment;
    char* mapped = reinterpret_cast<char*>(tryReserveUncommitted(mappedSize, usage, writable, executable, jitCageEnabled, includesGuardPages));
    if (!mapped)
        return nullptr;
    char* mappedEnd = mapped + mappedSize;

    char* aligned = reinterpret_cast<char*>(roundUpToMultipleOf(alignment, reinterpret_cast<uintptr_t>(mapped)));
    char* alignedEnd = aligned + bytes;

    RELEASE_ASSERT(alignedEnd <= mappedEnd);

    if (size_t leftExtra = aligned - mapped)
        releaseDecommitted(mapped, leftExtra);

    if (size_t rightExtra = mappedEnd - alignedEnd)
        releaseDecommitted(alignedEnd, rightExtra);

    return aligned;
#endif // HAVE(MAP_ALIGNED)
#endif // PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    void* result = tryReserveAndCommit(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
    RELEASE_ASSERT(result);
    return result;
}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
#if OS(LINUX)
    UNUSED_PARAM(writable);
    UNUSED_PARAM(executable);
    while (madvise(address, bytes, MADV_WILLNEED) == -1 && errno == EAGAIN) { }
#elif HAVE(MADV_FREE_REUSE)
    UNUSED_PARAM(writable);
    UNUSED_PARAM(executable);
    while (madvise(address, bytes, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
#else
    // Non-MADV_FREE_REUSE reservations automatically commit on demand.
    UNUSED_PARAM(address);
    UNUSED_PARAM(bytes);
    UNUSED_PARAM(writable);
    UNUSED_PARAM(executable);
#endif
}

void OSAllocator::decommit(void* address, size_t bytes)
{
#if OS(LINUX)
    while (madvise(address, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#elif HAVE(MADV_FREE_REUSE)
    while (madvise(address, bytes, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
#elif HAVE(MADV_FREE)
    while (madvise(address, bytes, MADV_FREE) == -1 && errno == EAGAIN) { }
#elif HAVE(MADV_DONTNEED)
    while (madvise(address, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#else
    UNUSED_PARAM(address);
    UNUSED_PARAM(bytes);
#endif
}

void OSAllocator::hintMemoryNotNeededSoon(void* address, size_t bytes)
{
#if HAVE(MADV_DONTNEED)
    while (madvise(address, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#else
    UNUSED_PARAM(address);
    UNUSED_PARAM(bytes);
#endif
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
    int result = munmap(address, bytes);
    if (result == -1)
        CRASH();
}

bool OSAllocator::protect(void* address, size_t bytes, bool readable, bool writable)
{
    int protection = 0;
    if (readable) {
        if (writable)
            protection = PROT_READ | PROT_WRITE;
        else
            protection = PROT_READ;
    } else {
        ASSERT(!readable && !writable);
        protection = PROT_NONE;
    }
    return !mprotect(address, bytes, protection);
}

} // namespace WTF
