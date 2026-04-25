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

#include "pas_all_shared_page_directories.h"

#include "pas_heap_lock.h"
#include "pas_segregated_shared_page_directory.h"

pas_segregated_shared_page_directory* pas_first_shared_page_directory = NULL;

void pas_all_shared_page_directories_add(pas_segregated_shared_page_directory* directory)
{
    pas_heap_lock_assert_held();
    
    PAS_ASSERT(!directory->next);
    PAS_ASSERT(pas_first_shared_page_directory != directory);
    
    directory->next = pas_first_shared_page_directory;
    pas_first_shared_page_directory = directory;
}

bool pas_all_shared_page_directories_for_each(pas_all_shared_page_directories_callback callback,
                                              void *arg)
{
    pas_segregated_shared_page_directory* directory;
    
    pas_heap_lock_assert_held();

    for (directory = pas_first_shared_page_directory; directory; directory = directory->next) {
        if (!callback(directory, arg))
            return false;
    }

    return true;
}

#endif /* LIBPAS_ENABLED */
