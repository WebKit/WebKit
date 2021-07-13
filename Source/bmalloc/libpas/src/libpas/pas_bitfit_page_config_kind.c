/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#include "pas_bitfit_page_config_kind.h"

#include "pas_all_heap_configs.h"

bool pas_small_bitfit_page_config_variant_is_enabled_override = PAS_USE_SMALL_BITFIT_OVERRIDE;
bool pas_medium_bitfit_page_config_variant_is_enabled_override = PAS_USE_MEDIUM_BITFIT_OVERRIDE;
bool pas_marge_bitfit_page_config_variant_is_enabled_override = PAS_USE_MARGE_BITFIT_OVERRIDE;

bool pas_bitfit_page_config_kind_for_each(
    pas_bitfit_page_config_kind_callback callback,
    void *arg)
{
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    if (!callback(pas_bitfit_page_config_kind_ ## name, \
                  pas_page_base_config_get_bitfit((value).base.page_config_ptr), \
                  arg)) \
        return false;
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
    return true;    
}

pas_bitfit_page_config* pas_bitfit_page_config_kind_for_config_table[
    0
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) + 1
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
    ] = {
#define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, value) \
    (pas_bitfit_page_config*)(value).base.page_config_ptr,
#include "pas_bitfit_page_config_kind.def"
#undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
};

#endif /* LIBPAS_ENABLED */
