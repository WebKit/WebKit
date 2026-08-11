/*
 * Copyright (c) 2021-2026 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_FLAGS_H
#define PAS_PAGE_FLAGS_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

typedef unsigned pas_page_flags;

enum {
    /* The default flag is 0, which indicates the memory is mmapped in some canonical way (anonymous, RW but
       not X, etc). Libpas may call mmap, mprotect and other such functions on this memory so long as it
       returns it to the canonical state before the memory is returned by some allocate function. Libpas could
       use this capability to:

       - Rapidly zero-fill the memory with a fixed mmap call.
       - Reset the commit state with a fixed mmap call.
       - Perform runtime checking that decommitted memory is not used by mprotecting it to PROT_NONE (and then
         returning it to normal permissions when recommitted).

       The default flags are good for performance and testing, but aren't necessary for libpas to work correctly,
       so it's not the end of the world to have to use pas_page_flag_client_owns_permissions instead. */
    pas_page_flags_none                    = 0,

    /* Libpas sometimes allocates its own memory from the OS, and sometimes is told to manage memory already
       allocated by someone else. Sometimes, clients don't want libpas to use certaint memory management functions
       on the memory that they ask libpas to manage. This flag indicates that libpas isn't allowed to call mmap,
       mprotect, or most other memory management function on the given memory. This is used for memory that was
       allocated by the client with some specific permissions or other special mmap arguments. Or, maybe it wasn't
       even allocated with mmap.

       Libpas may still call madvise() on memory with this capability, since madvise() doesn't change the
       permissions of the mapping. Currently, madvise() is the only syscall that libpas is allowed to use for
       memory that is for pas_page_flag_client_owns_permissions.

       Using pas_page_flag_client_owns_permissions may degrade peformance, affect the accuracy of OS memory usage accounting, or turn
       off some runtime checking. But it will not affect the correctness of the algorithm.

       Currently the primary client of this option is jit_heap_config, since the JIT heap is mmapped in a
       special way and with special permissions.*/
    pas_page_flag_client_owns_permissions  = 1u << 0,

    /* The memory holds executable code. Some platforms (Windows) requires executable protection information when committing a page,
       so freshly re-committed code pages remain executable. POSIX commit/decommit do not touch protection bits
       so this flag is effectively a no-op there. */
    pas_page_flag_executable               = 1u << 1,
};

static inline bool pas_page_flags_client_owns_permissions(pas_page_flags page_flags)
{
    return !!(page_flags & pas_page_flag_client_owns_permissions);
}

static inline bool pas_page_flags_is_executable(pas_page_flags page_flags)
{
    return !!(page_flags & pas_page_flag_executable);
}

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_FLAGS_H */
