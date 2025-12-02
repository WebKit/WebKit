/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include <wtf/DataLog.h>
#include <wtf/MallocSpan.h>
#include <wtf/MathExtras.h>
#include <wtf/MmapSpan.h>
#include <wtf/PageBlock.h>
#include <wtf/SafeStrerror.h>
#include <wtf/text/CString.h>

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

void* OSAllocator::tryReserveAndCommit(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned numGuardPagesToAddOnEachEnd)
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
#elif OS(LINUX) || OS(HAIKU)
    UNUSED_PARAM(jitCageEnabled);
    if (usage == OSAllocator::UncommittedPages)
        flags |= MAP_NORESERVE;
#else
    UNUSED_PARAM(jitCageEnabled);
#endif

#if OS(DARWIN)
    int fd = usage;
#else
    UNUSED_PARAM(usage);
    int fd = -1;
#endif

    size_t guardSize = 0;
    if (numGuardPagesToAddOnEachEnd) {
        guardSize = numGuardPagesToAddOnEachEnd * pageSize();
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        if (address)
            address = reinterpret_cast<uint8_t*>(address) - guardSize;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        bytes = bytes + 2 * guardSize;
    }
    auto result = MmapSpan<uint8_t>::mmap(address, bytes, protection, flags, fd);
    if (!result)
        return result.leakSpan().data();

    if (numGuardPagesToAddOnEachEnd) {
        // We use mmap to remap the guardpages rather than using mprotect as
        // mprotect results in multiple references to the code region. This
        // breaks the madvise based mechanism we use to return physical memory
        // to the OS.
        mmap(result.mutableSpan().first(guardSize).data(), guardSize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, fd, 0);
        mmap(result.mutableSpan().last(guardSize).data(), guardSize, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, fd, 0);
    }
    // Since we're only interested in the starting address, we don't have to specify the actual end of the
    // subspan to exclude the trailing guard pages.
    return result.leakSpan().subspan(guardSize).data();
}

void* OSAllocator::tryReserveUncommitted(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned numGuardPagesToAddOnEachEnd)
{
#if OS(LINUX) || OS(HAIKU)
    UNUSED_PARAM(usage);
    void* result = tryReserveAndCommit(bytes, OSAllocator::UncommittedPages, address, writable, executable, jitCageEnabled, numGuardPagesToAddOnEachEnd);
    if (result)
        while (madvise(result, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#else // not OS(LINUX) || OS(HAIKU)
    void* result = tryReserveAndCommit(bytes, usage, address, writable, executable, jitCageEnabled, numGuardPagesToAddOnEachEnd);
#if HAVE(MADV_FREE_REUSE)
    if (result) {
        // To support the "reserve then commit" model, we have to initially decommit.
        while (madvise(result, bytes, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
    }
#endif

#endif // not OS(LINUX) || OS(HAIKU)
    return result;
}

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned numGuardPagesToAddOnEachEnd)
{
    void* result = tryReserveUncommitted(bytes, usage, address, writable, executable, jitCageEnabled, numGuardPagesToAddOnEachEnd);
    RELEASE_ASSERT(result);
    return result;
}

void* OSAllocator::tryReserveUncommittedAligned(size_t bytes, size_t alignment, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned numGuardPagesToAddOnEachEnd)
{
    ASSERT(hasOneBitSet(alignment) && alignment >= pageSize());

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
    UNUSED_PARAM(usage); // Not supported for mach API.
    ASSERT_UNUSED(numGuardPagesToAddOnEachEnd, !numGuardPagesToAddOnEachEnd);
    ASSERT_UNUSED(jitCageEnabled, !jitCageEnabled); // Not supported for mach API.
    vm_prot_t protections = VM_PROT_READ;
    if (writable)
        protections |= VM_PROT_WRITE;
    if (executable)
        protections |= VM_PROT_EXECUTE;

    const vm_inherit_t childProcessInheritance = VM_INHERIT_DEFAULT;
    const bool copy = false;
    const int flags = VM_FLAGS_ANYWHERE;

    void* aligned = address;
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
    ASSERT_UNUSED(numGuardPagesToAddOnEachEnd, !numGuardPagesToAddOnEachEnd);
    int protection = PROT_READ;
    if (writable)
        protection |= PROT_WRITE;
    if (executable)
        protection |= PROT_EXEC;

    void* result = mmap(address, bytes, protection, MAP_NORESERVE | MAP_PRIVATE | MAP_ANON | MAP_ALIGNED(getLSBSet(alignment)), -1, 0);
    if (result == MAP_FAILED)
        return nullptr;
    if (result)
        while (madvise(result, bytes, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
    return result;
#else

    // Add the alignment so we can ensure enough mapped memory to get an aligned start.
    size_t mappedSize = bytes + alignment;
    auto* rawMapped = reinterpret_cast<uint8_t*>(tryReserveUncommitted(mappedSize, usage, address, writable, executable, jitCageEnabled, numGuardPagesToAddOnEachEnd));
    if (!rawMapped)
        return nullptr;
    auto mappedSpan = unsafeMakeSpan(rawMapped, mappedSize);

    auto* rawAligned = reinterpret_cast<uint8_t*>(roundUpToMultipleOf(alignment, reinterpret_cast<uintptr_t>(mappedSpan.data())));
    auto alignedSpan = mappedSpan.subspan(rawAligned - mappedSpan.data(), bytes);

    if (size_t leftExtra = alignedSpan.data() - mappedSpan.data())
        releaseDecommitted(mappedSpan.data(), leftExtra, 0);

    if (size_t rightExtra = std::to_address(mappedSpan.end()) - std::to_address(alignedSpan.end()))
        releaseDecommitted(std::to_address(alignedSpan.end()), rightExtra, 0);

    return alignedSpan.data();
#endif // HAVE(MAP_ALIGNED)
#endif // PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, void* address, bool writable, bool executable, bool jitCageEnabled, unsigned numGuardPagesToAddOnEachEnd)
{
    void* result = tryReserveAndCommit(bytes, usage, address, writable, executable, jitCageEnabled, numGuardPagesToAddOnEachEnd);
    RELEASE_ASSERT(result);
    return result;
}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
#if OS(LINUX) || OS(HAIKU)
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
#if OS(LINUX) || OS(HAIKU)
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

void OSAllocator::releaseDecommitted(void* address, size_t bytes, unsigned numberOfGuardPagesOnEachEnd = 0)
{
    void* base = address;
    size_t size = bytes;
    if (numberOfGuardPagesOnEachEnd) {
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        base = reinterpret_cast<uint8_t*>(base) - numberOfGuardPagesOnEachEnd * pageSize();
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        size += 2 * numberOfGuardPagesOnEachEnd * pageSize();
    }
    int result = munmap(base, size);
    if (result == -1)
        CRASH();
}

bool OSAllocator::tryProtect(void* address, size_t bytes, bool readable, bool writable)
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

void OSAllocator::protect(void* address, size_t bytes, bool readable, bool writable)
{
    if (bool result = tryProtect(address, bytes, readable, writable); !result) [[unlikely]] {
        dataLogLn("mprotect failed: ", safeStrerror(errno).data());
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace WTF
