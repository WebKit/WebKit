/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "pas_dyld_state.h"

#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include "pas_log.h"

/* This is copied from dyld_process_info_internal.h
 
   FIXME: Stop doing it this way. Dyld should give us the is_libsystem_initialized API
   somehow. */
typedef struct {
    uint32_t                version;
    uint32_t                infoArrayCount;
    uint64_t                infoArray;
    uint64_t                notification;
    bool                    processDetachedFromSharedRegion;
    bool                    libSystemInitialized;
    /* There are more things after this, but we don't care. */
} dyld_all_image_infos_64;

static dyld_all_image_infos_64* all_image_infos;

bool pas_dyld_is_libsystem_initialized(void)
{
    dyld_all_image_infos_64* infos;

    infos = all_image_infos;
    
    if (!infos) {
        task_dyld_info_data_t task_dyld_info;
        mach_msg_type_number_t count;
        kern_return_t result;

        count = TASK_DYLD_INFO_COUNT;
        result = task_info(mach_task_self(),
                           TASK_DYLD_INFO,
                           (task_info_t)&task_dyld_info,
                           &count);
        PAS_ASSERT(result == KERN_SUCCESS);

        if (!task_dyld_info.all_image_info_addr)
            return false;

        infos = (dyld_all_image_infos_64*)task_dyld_info.all_image_info_addr;

        pas_fence();
        
        all_image_infos = infos;
    }

    return infos->libSystemInitialized;
}

#endif /* LIBPAS_ENABLED */
