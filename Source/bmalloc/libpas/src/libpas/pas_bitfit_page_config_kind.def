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

/* This isn't really a header. It's a preprocessor for-each loop.
   
   To generate code for each page config kind, just do:
   
       #define PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(name, page_config_value) \
           ... the code you want for pas_bitfit_page_config_kind_##name \
               and (page_config_value) ...
       #include "pas_bitfit_page_config_kind.def"
       #undef PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND
   
   For example, this can be used to create switch statements as an alternative to adding virtual
   functions or fields to bitfit_page_config. Generally, we only use this when we have no
   other alternative (like the unified deallocation log).

   This is intentionally much like pas_segregated_page_config_kind.def, but it's not required that
   every heap config has every combo of small/medium segregated/bitfit. */

PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(
    null,
    ((pas_bitfit_page_config){
        .base = {
            .is_enabled = false,
            .heap_config_ptr = NULL,
            .page_config_ptr = NULL
        },
        .kind = pas_bitfit_page_config_kind_null
    }))

#if PAS_ENABLE_THINGY
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(thingy_small_bitfit,
                                   THINGY_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(thingy_medium_bitfit,
                                   THINGY_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(thingy_marge_bitfit,
                                   THINGY_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_ISO
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(iso_small_bitfit,
                                   ISO_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(iso_medium_bitfit,
                                   ISO_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(iso_marge_bitfit,
                                   ISO_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_ISO_TEST
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(iso_test_small_bitfit,
                                   ISO_TEST_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(iso_test_medium_bitfit,
                                   ISO_TEST_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(iso_test_marge_bitfit,
                                   ISO_TEST_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_MINALIGN32
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(minalign32_small_bitfit,
                                   MINALIGN32_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(minalign32_medium_bitfit,
                                   MINALIGN32_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(minalign32_marge_bitfit,
                                   MINALIGN32_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_PAGESIZE64K
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(pagesize64k_small_bitfit,
                                   PAGESIZE64K_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(pagesize64k_medium_bitfit,
                                   PAGESIZE64K_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(pagesize64k_marge_bitfit,
                                   PAGESIZE64K_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_BMALLOC
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(bmalloc_small_bitfit,
                                   BMALLOC_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(bmalloc_medium_bitfit,
                                   BMALLOC_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(bmalloc_marge_bitfit,
                                   BMALLOC_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_HOTBIT
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(hotbit_small_bitfit,
                                   HOTBIT_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(hotbit_medium_bitfit,
                                   HOTBIT_HEAP_CONFIG.medium_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(hotbit_marge_bitfit,
                                   HOTBIT_HEAP_CONFIG.marge_bitfit_config)
#endif

#if PAS_ENABLE_JIT
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(jit_small_bitfit,
                                   JIT_HEAP_CONFIG.small_bitfit_config)
PAS_DEFINE_BITFIT_PAGE_CONFIG_KIND(jit_medium_bitfit,
                                   JIT_HEAP_CONFIG.medium_bitfit_config)
#endif

