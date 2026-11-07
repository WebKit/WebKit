/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#ifndef PAS_MEMORY_FLAVOR_H
#define PAS_MEMORY_FLAVOR_H

#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

/* A memory flavor describes how a region of memory is mapped;
   this determines what libpas can/cannot do with it.
   Ranges of different flavors must never be coalesced, as that
   would necessarily overwrite their mappings with one flavor or
   the other.
   Dynamic protections are not included in the 'flavor' of an
   allocation. So while e.g. decommit can mprotect the page to PROT_NONE,
   that does not change the flavor, but is considered as a layer on top of it.
   Only the base protections for a page, i.e. those it was originally mapped with,
   are part of the flavor.
   */
enum pas_memory_flavor {
    /* Anonymous RW memory. */
    pas_memory_flavor_plain,

    /* Anonymous RW memory with backing MTE tag storage.
       Technically, this mapping is the same as _plain when MTE is disabled at the system level,
       but we shouldn't rely on that anywhere. */
    pas_memory_flavor_mte,

    /* Executable JIT memory. Libpas may not mmap, mprotect, or (generally) write into it:
       more specifically, it can zero this memory via madvise, but cannot write to it directly,
       meaning that metadata (e.g. page headers) cannot be located inline in these pages.
       FIXME: allocate JIT memory as a reservation from within libpas
       Currently, it's mapped outside of the library and donated to us. */
    pas_memory_flavor_rwx,
};

typedef enum pas_memory_flavor pas_memory_flavor;

#define PAS_NUM_MEMORY_FLAVORS 3u
/* pas_large_sharing_node stores the flavor in a bitfield of this many bits. */
#define PAS_MEMORY_FLAVOR_NUM_BITS 2u
#if PAS_COMPILER(CLANG)
_Static_assert(PAS_NUM_MEMORY_FLAVORS <= (1u << PAS_MEMORY_FLAVOR_NUM_BITS),
               "PAS_MEMORY_FLAVOR_NUM_BITS must be wide enough to hold every flavor");
#endif

/* The "how to map memory of each kind" table. These are static inline switches rather than a data table
   so that they constant-fold away whenever the flavor is known at compile time (e.g. from a heap config). */

/* Can libpas create fresh mappings of this flavor via pas_page_malloc? */
static PAS_ALWAYS_INLINE bool pas_memory_flavor_libpas_can_map(pas_memory_flavor flavor)
{
    switch (flavor) {
    case pas_memory_flavor_plain:
    case pas_memory_flavor_mte:
        return true;
    case pas_memory_flavor_rwx:
        return false;
    }
    PAS_ASSERT_NOT_REACHED();
    return false;
}

/* If the client owns this flavor, then we can't remap it even if we know how to
   do so. */
static PAS_ALWAYS_INLINE bool pas_memory_flavor_client_owns_permissions(pas_memory_flavor flavor)
{
    switch (flavor) {
    case pas_memory_flavor_plain:
    case pas_memory_flavor_mte:
        return false;
    case pas_memory_flavor_rwx:
        return true;
    }
    PAS_ASSERT_NOT_REACHED();
    return false;
}

static PAS_ALWAYS_INLINE bool pas_memory_flavor_is_executable(pas_memory_flavor flavor)
{
    switch (flavor) {
    case pas_memory_flavor_plain:
    case pas_memory_flavor_mte:
        return false;
    case pas_memory_flavor_rwx:
        return true;
    }
    PAS_ASSERT_NOT_REACHED();
    return false;
}

/* Does (re)mapping this memory require asking the VM for MTE tag storage (when MTE is enabled)? This is
   a macro so that it is a constant expression when flavor is, for use in static initializers (e.g. of
   heap configs); otherwise use pas_memory_flavor_needs_mte_tag_storage. */
#define PAS_MEMORY_FLAVOR_NEEDS_MTE_TAG_STORAGE(flavor) ((flavor) == pas_memory_flavor_mte)

static PAS_ALWAYS_INLINE bool pas_memory_flavor_needs_mte_tag_storage(pas_memory_flavor flavor)
{
    return PAS_MEMORY_FLAVOR_NEEDS_MTE_TAG_STORAGE(flavor);
}

static inline const char* pas_memory_flavor_get_string(pas_memory_flavor flavor)
{
    switch (flavor) {
    case pas_memory_flavor_plain:
        return "plain";
    case pas_memory_flavor_mte:
        return "mte";
    case pas_memory_flavor_rwx:
        return "rwx";
    }
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

PAS_END_EXTERN_C;

#endif /* PAS_MEMORY_FLAVOR_H */
