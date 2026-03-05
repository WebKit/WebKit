/*
 * Copyright (C) 2025-2025 Apple Inc. All rights reserved.
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

#include "VMAllocate.h"

#if BUSE(LIBPAS)
#include "pas_mte_config.h"
#endif

namespace bmalloc {

#if BMALLOC_USE_MADV_ZERO
static pthread_once_t madvZeroOnceControl = PTHREAD_ONCE_INIT;
static bool madvZeroSupported = false;

static void zeroFillLatchIfMadvZeroIsSupported()
{
    // See libpas/pas_page_malloc.c for details on this approach
    // The logic is copied in both places, so if we change one we
    // should change the other as well.

    size_t pageSize = vmPageSize();
    void* base = mmap(NULL, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANON | BMALLOC_NORESERVE, static_cast<int>(VMTag::Malloc), 0);
    BASSERT(base);

    int rc = madvise(base, pageSize, MADV_ZERO);
    if (rc)
        madvZeroSupported = true;
    else
        madvZeroSupported = false;
    munmap(base, pageSize);
}

bool isMadvZeroSupported()
{
    pthread_once(&madvZeroOnceControl, zeroFillLatchIfMadvZeroIsSupported);
    return madvZeroSupported;
}
#endif

#if BENABLE(MTE) && BOS(DARWIN)
bool tryVmZeroAndPurgeMTECase(void* p, size_t vmSize, VMTag usage)
{
    if (!BMALLOC_USE_MTE)
        return false;
    const vm_inherit_t childProcessInheritance = VM_INHERIT_DEFAULT;
    const bool copy = false;
    const int tag = static_cast<int>(usage);
    /* We would much prefer to use mach_vm_behavior_set here, as it
       always preserves the page's current VM flags. However, it's
       currently blocked by an unknown security policy, so until that
       blocker is resolved we can use this instead without much loss. */
    kern_return_t vmMapResult = mach_vm_map(mach_task_self(),
        (mach_vm_address_t*)&p,
        vmSize,
        0,
        VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE | BMALLOC_VM_MTE | tag,
        MEMORY_OBJECT_NULL,
        0,
        copy,
        VM_PROT_DEFAULT,
        VM_PROT_ALL,
        childProcessInheritance);
    if (vmMapResult != KERN_SUCCESS)
        errno = 0;
    RELEASE_BASSERT(vmMapResult == KERN_SUCCESS);
    return true;
}
#endif // BENABLE(MTE) && BOS(DARWIN)

} // namespace bmalloc
