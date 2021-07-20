/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_baseline_allocator.h"

#include "pas_segregated_global_size_directory.h"

void pas_baseline_allocator_attach_directory(pas_baseline_allocator* allocator,
                                             pas_segregated_global_size_directory* directory)
{
    PAS_ASSERT(!allocator->u.allocator.view);
    
    PAS_ASSERT(
        PAS_BASELINE_LOCAL_ALLOCATOR_SIZE
        >= pas_segregated_global_size_directory_local_allocator_size(directory));
    
    pas_local_allocator_construct(&allocator->u.allocator, directory);
}

void pas_baseline_allocator_detach_directory(pas_baseline_allocator* allocator)
{
    PAS_ASSERT(allocator->u.allocator.view);
    pas_local_allocator_stop(
        &allocator->u.allocator,
        pas_lock_lock_mode_lock,
        pas_lock_is_not_held);
    pas_local_allocator_destruct(&allocator->u.allocator);
    pas_zero_memory(&allocator->u.allocator, sizeof(pas_local_allocator)); /* Does not zero the bits,
                                                                              which is OK. */
}

#endif /* LIBPAS_ENABLED */
