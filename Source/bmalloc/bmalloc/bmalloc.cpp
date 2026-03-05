/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "bmalloc.h"

#include "Environment.h"
#include "ProcessCheck.h"
#include "SystemHeap.h"
#include "VMAllocate.h"

#if BENABLE(LIBPAS)
#include "bmalloc_heap_config.h"
#include "pas_page_sharing_pool.h"
#include "pas_platform.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_scavenger.h"
#include "pas_thread_local_cache.h"
#include "pas_mte_config.h"
#endif

namespace bmalloc { namespace api {

#if BUSE(LIBPAS)

#if BHAVE(36BIT_ADDRESS) && !PAS_HAVE(36BIT_ADDRESS)
#error BHAVE(36BIT_ADDRESS) is true, but PAS_HAVE(36BIT_ADDRESS) is false. They should match.
#elif !BHAVE(36BIT_ADDRESS) && PAS_HAVE(36BIT_ADDRESS)
#error BHAVE(36BIT_ADDRESS) is false, but PAS_HAVE(36BIT_ADDRESS) is true. They should match.
#endif

namespace {
static const bmalloc_type primitiveGigacageType = BMALLOC_TYPE_INITIALIZER(1, 1, "Primitive Gigacage");
} // anonymous namespace

pas_primitive_heap_ref gigacageHeaps[static_cast<size_t>(Gigacage::NumberOfKinds)] = {
    BMALLOC_AUXILIARY_HEAP_REF_INITIALIZER(&primitiveGigacageType, pas_bmalloc_heap_ref_kind_non_compact),
};
#endif

void* mallocOutOfLine(size_t size, CompactAllocationMode mode, HeapKind kind)
{
    return malloc(size, mode, kind);
}

void freeOutOfLine(void* object, HeapKind kind)
{
    free(object, kind);
}

void* tryLargeZeroedMemalignVirtual(size_t requiredAlignment, size_t requestedSize, CompactAllocationMode mode, HeapKind kind)
{
    RELEASE_BASSERT(isPowerOfTwo(requiredAlignment));

    size_t pageSize = vmPageSize();
    size_t alignment = roundUpToMultipleOf(pageSize, requiredAlignment);
    size_t size = roundUpToMultipleOf(pageSize, requestedSize);
    RELEASE_BASSERT(alignment >= requiredAlignment);
    RELEASE_BASSERT(size >= requestedSize);

    void* result;
    if (auto* systemHeap = SystemHeap::tryGetIfShouldSupplantBmalloc())
        result = systemHeap->memalignLarge(alignment, size);
    else
        result = tryMemalign(alignment, size, mode, kind);

    if (result)
        vmZeroAndPurge(result, size);

    return result;
}

void freeLargeVirtual(void* object, size_t size, HeapKind kind)
{
    if (auto* systemHeap = SystemHeap::tryGetIfShouldSupplantBmalloc()) {
        systemHeap->freeLarge(object);
        return;
    }
#if BUSE(LIBPAS)
    BUNUSED(size);
    BUNUSED(kind);
    bmalloc_deallocate_inline(object);
#elif BUSE(MIMALLOC)
    BUNUSED(size);
    BUNUSED(kind);
    mi_free(object);
#else
    BUNUSED(size);
    BUNUSED(kind);
    ::free(object);
#endif
}

void scavengeThisThread()
{
#if BENABLE(LIBPAS)
    pas_thread_local_cache_shrink(pas_thread_local_cache_try_get(),
                                  pas_lock_is_not_held);
#elif BUSE(MIMALLOC)
    mi_theap_collect(mi_theap_get_default(), /* force */ true);
#endif
}

void scavenge()
{
#if BENABLE(LIBPAS)
    pas_scavenger_run_synchronously_now();
    scavengeThisThread();
#elif BUSE(MIMALLOC)
    mi_collect(/* force */ true);
#endif
    if (SystemHeap* systemHeap = SystemHeap::tryGetIfShouldSupplantBmalloc()) {
        systemHeap->scavenge();
        return;
    }
}

bool isEnabled(HeapKind)
{
    return !Environment::get()->shouldBmallocAllocateThroughSystemHeap();
}

#if BOS(DARWIN)
void setScavengerThreadQOSClass(qos_class_t overrideClass)
{
#if BENABLE(LIBPAS)
    pas_scavenger_set_requested_qos_class(overrideClass);
#else
    BUNUSED(overrideClass);
#endif
}
#endif

void commitAlignedPhysical(void* object, size_t size, HeapKind)
{
    vmValidatePhysical(object, size);
    vmAllocatePhysicalPages(object, size);
}

void decommitAlignedPhysical(void* object, size_t size, HeapKind)
{
    vmValidatePhysical(object, size);
    vmDeallocatePhysicalPages(object, size);
}

void enableMiniMode(bool forceMiniMode)
{
#if BENABLE(LIBPAS)
    if (!forceMiniMode && !shouldAllowMiniMode())
        return;

    // Speed up the scavenger.
    pas_scavenger_period_in_milliseconds = 5.;
    pas_scavenger_max_epoch_delta = 5ll * 1000ll * 1000ll;

    // Do eager scavenging anytime pages are allocated or committed.
    pas_physical_page_sharing_pool_balancing_enabled = true;
    pas_physical_page_sharing_pool_balancing_enabled_for_utility = true;

    // Switch to bitfit allocation for anything that isn't isoheaped.
    bmalloc_intrinsic_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_primitive_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    bmalloc_primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;

    // If large-object delegation is enabled, we don't want to override that
    // just because we've entered mini-mode.
    // One way we could get around that would be to leave the bitfit object
    // size limits the way they are. However, in cases where the bitfit
    // size-limit had previously been constrained for performance, e.g. by
    // setting them to 0, we do want enableMiniMode to be able to re-expand
    // them.
    // So we take the object-delegation path to be a special case and make
    // sure to re-apply after performing the above expansion.
    PAS_IGNORE_WARNINGS_BEGIN("unreachable-code");
#if defined(PAS_MTE_USE_LARGE_OBJECT_DELEGATION)
    if (PAS_MTE_USE_LARGE_OBJECT_DELEGATION)
        pas_mte_force_nontaggable_user_allocations_into_large_heap();
#endif // defined(PAS_MTE_USE_LARGE_OBJECT_DELEGATION)
    PAS_IGNORE_WARNINGS_END;
#else
    BUNUSED_PARAM(forceMiniMode);
#endif // BENABLE(LIBPAS)
}

void disableScavenger()
{
#if BENABLE(LIBPAS)
    pas_scavenger_suspend();
#endif
}

void forceEnablePGM(uint16_t guardMallocRate)
{
#if BUSE(LIBPAS)
    pas_probabilistic_guard_malloc_initialize_pgm_as_enabled(guardMallocRate);
#else
    BUNUSED_PARAM(guardMallocRate);
#endif
}

} } // namespace bmalloc::api

