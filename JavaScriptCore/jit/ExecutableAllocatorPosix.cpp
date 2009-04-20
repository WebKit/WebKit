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

#include "config.h"

#include "ExecutableAllocator.h"

#if ENABLE(ASSEMBLER) && !(PLATFORM(MAC) && PLATFORM(X86_64))

#include <sys/mman.h>
#include <unistd.h>

#if PLATFORM(DARWIN)
#include <mach/vm_statistics.h>
#endif

#if PLATFORM(DARWIN)
// On Mac OS X, the VM subsystem allows tagging memory requested from mmap and vm_map
// in order to aid tools that inspect system memory use. 
#if defined(VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR)
#define TAG_FOR_EXECUTABLEALLOCATOR_MEMORY VM_MAKE_TAG(VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR)
#else
#define TAG_FOR_EXECUTABLEALLOCATOR_MEMORY VM_MAKE_TAG(64)
#endif
#else
#define TAG_FOR_EXECUTABLEALLOCATOR_MEMORY -1
#endif

namespace JSC {

void ExecutableAllocator::intializePageSize()
{
    ExecutableAllocator::pageSize = getpagesize();
}

ExecutablePool::Allocation ExecutablePool::systemAlloc(size_t n)
{
    ExecutablePool::Allocation alloc = { reinterpret_cast<char*>(mmap(NULL, n, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, TAG_FOR_EXECUTABLEALLOCATOR_MEMORY, 0)), n };
    return alloc;
}

void ExecutablePool::systemRelease(const ExecutablePool::Allocation& alloc) 
{ 
    int result = munmap(alloc.pages, alloc.size);
    ASSERT_UNUSED(result, !result);
}

}

#undef TAG_FOR_EXECUTABLEALLOCATOR_MEMORY

#endif // HAVE(ASSEMBLER)
