/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
#include "pas_mte_config.h"

#include "stdlib.h"
#if PAS_OS(DARWIN)
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif
#if !PAS_OS(WINDOWS)
#include "unistd.h"
#include <pthread.h>
#endif

#include "pas_all_heaps.h"
#include "pas_heap.h"
#include "pas_mte.h"
#include "pas_scavenger.h"
#include "pas_segregated_heap.h"
#include "pas_system_heap.h"
#include "pas_utility_heap_config.h"
#include "pas_utils.h"
#include "pas_zero_memory.h"

#if PAS_ENABLE_BMALLOC
extern pas_heap bmalloc_common_primitive_heap;
extern const pas_heap_config bmalloc_heap_config;
extern pas_basic_heap_runtime_config bmalloc_flex_runtime_config;
extern pas_basic_heap_runtime_config bmalloc_intrinsic_runtime_config;
extern pas_basic_heap_runtime_config bmalloc_typed_runtime_config;
extern pas_basic_heap_runtime_config bmalloc_primitive_runtime_config;
#endif // PAS_ENABLE_BMALLOC
#if PAS_ENABLE_JIT
extern const pas_heap_config jit_heap_config;
extern pas_basic_heap_runtime_config jit_heap_runtime_config;
#endif // PAS_ENABLE_JIT
#if PAS_ENABLE_ISO
extern const pas_heap_config iso_heap_config;
#endif // PAS_ENABLE_ISO
extern const pas_heap_config pas_utility_heap_config;

#if defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#if PAS_ENABLE_MTE

static int is_env_false(const char* var)
{
    const char* value = getenv(var);
    if (!value)
        return 0;
    return !strcasecmp(value, "false") || !strcasecmp(value, "no") || !strcasecmp(value, "0");
}

static int is_env_true(const char* var)
{
    const char* value = getenv(var);
    if (!value)
        return 0;
    return !strcasecmp(value, "true") || !strcasecmp(value, "yes") || !strcasecmp(value, "1");
}

static bool get_value_if_available(unsigned* valuePtr, const char* var)
{
    const char* varStr = getenv(var);
    if (varStr) {
        unsigned value = 0;
        if (sscanf(varStr, "%u", &value) == 1) {
            *valuePtr = value;
            return true; // Found.
        }
    }
    return false; // Not found.
}

static void pas_mte_do_initialization(void)
{
    uint8_t* enabled_byte = &PAS_MTE_CONFIG_BYTE(PAS_MTE_ENABLE_FLAG);
    uint8_t* mode_byte = &PAS_MTE_CONFIG_BYTE(PAS_MTE_MODE_BITS);
    uint8_t* medium_byte = &PAS_MTE_CONFIG_BYTE(PAS_MTE_MEDIUM_TAGGING_ENABLE_FLAG);
    uint8_t* lockdown_mode_byte = &PAS_MTE_CONFIG_BYTE(PAS_MTE_LOCKDOWN_MODE_FLAG);
    uint8_t* hardened_byte = &PAS_MTE_CONFIG_BYTE(PAS_MTE_HARDENED_FLAG);

    struct proc_bsdinfo info;
    int rc = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    if (rc == sizeof(info) && info.pbi_flags & PAS_MTE_PROC_FLAG_SEC_ENABLED)
        *enabled_byte = 1;

    if (is_env_true("JSC_useAllocationProfiling") || is_env_true("MTE_overrideEnablementForJavaScriptCore")) {
        PAS_ASSERT(!(is_env_false("JSC_useAllocationProfiling") || is_env_false("MTE_overrideEnablementForJavaScriptCore")));
        *enabled_byte = 1;
    }
    if (is_env_false("JSC_useAllocationProfiling") || is_env_false("MTE_overrideEnablementForJavaScriptCore"))
        *enabled_byte = 0;

    if (!*enabled_byte)
        return;

    uint64_t ldmState = 0;
    size_t sysCtlLen = sizeof(ldmState);
    if (sysctlbyname("security.mac.lockdown_mode_state", &ldmState, &sysCtlLen, NULL, 0) >= 0 && ldmState == 1)
        *lockdown_mode_byte = 1;
    else
        *lockdown_mode_byte = 0;

    unsigned mode = 0;
    if (get_value_if_available(&mode, "JSC_allocationProfilingMode"))
        *mode_byte = (uint8_t)(mode & 0xFF);

    const char* name = getprogname();
    bool isWebContentProcess = !strncmp(name, "com.apple.WebKit.WebContent", 27) || !strncmp(name, "jsc", 3);

    unsigned taggingRate = 100;
    if (isWebContentProcess) {
        const uint8_t defaultWebContentTaggingRate = 33;
        taggingRate = defaultWebContentTaggingRate;

        // Debug option to override the WCP tagging rate.
        get_value_if_available(&taggingRate, "MTE_taggingRateForWebContent");
    }

    // Debug option to unconditionally override the tagging rate.
    get_value_if_available(&taggingRate, "MTE_taggingRate");

    PAS_MTE_CONFIG_BYTE(PAS_MTE_TAGGING_RATE) = taggingRate;

    if (isWebContentProcess) {
        // For a variety of reasons, a full MTE implementation in the WebContent process is not generally practical.
        // As such, by default, we disable MTE in the WebContent process while leaving it on in the privileged
        // processes. However, in certain "extra secure" contexts, this disablement is overridden such that we
        // treat WebContent like any other process for the purposes of MTE.

        bool wcp_is_hardened = false;
        bool isEnhancedSecurityWebContentProcess = !strncmp(name, "com.apple.WebKit.WebContent.EnhancedSecurity", 44);
        if (*lockdown_mode_byte || isEnhancedSecurityWebContentProcess)
            wcp_is_hardened = true;

        if (wcp_is_hardened) {
            *medium_byte = 1;
            *enabled_byte = 1;
            *hardened_byte = 1;
        } else {
            *medium_byte = 0;
#if !PAS_USE_MTE_IN_WEBCONTENT
            // Disable tagging in libpas by default in WebContent process
            *enabled_byte = 0;
#else
            *enabled_byte = 1;
#endif
            *hardened_byte = 0;
            // FIXME: rdar://159974195
            bmalloc_common_primitive_heap.is_non_compact_heap = false;
        }

#ifndef NDEBUG
        if (is_env_true("MTE_disableForWebContent")) {
            PAS_ASSERT(!is_env_true("MTE_overrideEnablementForWebContent"));
            *enabled_byte = 0;
            *medium_byte = 0;
        }
#endif
        if (is_env_true("MTE_overrideEnablementForWebContent")) {
            *enabled_byte = 1;
            *medium_byte = 1;
        } else if (is_env_false("MTE_overrideEnablementForWebContent")) {
            *enabled_byte = 0;
            *medium_byte = 0;
        }
    } else {
        *medium_byte = 1; // Tag libpas medium objects in privileged processes
        *hardened_byte = 1;
    }
}

static bool pas_mte_is_enabled(void)
{
    const uint8_t* enabledByte = ((const uint8_t*)(g_config + 2));
    struct proc_bsdinfo info;
    int rc = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    return (rc == sizeof(info) && (info.pbi_flags & PAS_MTE_PROC_FLAG_SEC_ENABLED) && !!*enabledByte);
}

static void pas_mte_get_config_bytes(uint8_t (*bytes_out)[6])
{
    (*bytes_out)[0] = PAS_MTE_CONFIG_BYTE(PAS_MTE_ENABLE_FLAG);
    (*bytes_out)[1] = PAS_MTE_CONFIG_BYTE(PAS_MTE_MODE_BITS);
    (*bytes_out)[2] = PAS_MTE_CONFIG_BYTE(PAS_MTE_TAGGING_RATE);
    (*bytes_out)[3] = PAS_MTE_CONFIG_BYTE(PAS_MTE_MEDIUM_TAGGING_ENABLE_FLAG);
    (*bytes_out)[4] = PAS_MTE_CONFIG_BYTE(PAS_MTE_LOCKDOWN_MODE_FLAG);
    (*bytes_out)[5] = PAS_MTE_CONFIG_BYTE(PAS_MTE_HARDENED_FLAG);
}

#else // !PAS_ENABLE_MTE

static PAS_UNUSED void pas_mte_do_initialization(void) { }

static PAS_UNUSED bool pas_mte_is_enabled(void)
{
    return false;
}

static PAS_UNUSED void pas_mte_get_config_bytes(uint8_t (*bytes_out)[6])
{
    for (int i = 0; i < 6; i++)
        (*bytes_out)[i] = 0;
}

#endif // PAS_ENABLE_MTE

#if PAS_OS(DARWIN)
static size_t max_object_size_for_page_config_sans_heap(const pas_page_base_config* page_config)
{
    if (!page_config->is_enabled)
        return 0;
    return pas_round_down_to_power_of_2(
        page_config->max_object_size,
        pas_page_base_config_min_align(*page_config));
}

static void pas_report_config(void)
{
    const char* progname = getprogname();
    const int pid = getpid();
    const mach_port_t threadno = pthread_mach_thread_np(pthread_self());

    uint8_t mte_conf[6];
    pas_mte_get_config_bytes(&mte_conf);

#define LOG_FMT_STR_FOR_HEAP_CONFIG(name) "\n\tHeap-Config " #name ":" \
                                   "\n\t\tPage Configs (Enabled/MTE Taggable, Static Max Obj Size):" \
                                   "\n\t\t\tSmall Segregated: %u/%u, %zuB"\
                                   "\n\t\t\tMedium Segregated: %u/%u, %zuB"\
                                   "\n\t\t\tSmall Bitfit: %u/%u, %zuB"\
                                   "\n\t\t\tMedium Bitfit : %u/%u, %zuB"\
                                   "\n\t\t\tMarge Bitfit : %u/%u, %zuB"
#define LOG_FMT_VARS_FOR_HEAP_CONFIG(cfg) \
    cfg.small_segregated_config.base.is_enabled, cfg.small_segregated_config.base.allow_mte_tagging, max_object_size_for_page_config_sans_heap(&cfg.small_segregated_config.base), \
    cfg.medium_segregated_config.base.is_enabled, cfg.medium_segregated_config.base.allow_mte_tagging, max_object_size_for_page_config_sans_heap(&cfg.medium_segregated_config.base), \
    cfg.small_bitfit_config.base.is_enabled, cfg.small_bitfit_config.base.allow_mte_tagging, max_object_size_for_page_config_sans_heap(&cfg.small_bitfit_config.base), \
    cfg.medium_bitfit_config.base.is_enabled, cfg.medium_bitfit_config.base.allow_mte_tagging, max_object_size_for_page_config_sans_heap(&cfg.medium_bitfit_config.base), \
    cfg.marge_bitfit_config.base.is_enabled, cfg.marge_bitfit_config.base.allow_mte_tagging, max_object_size_for_page_config_sans_heap(&cfg.marge_bitfit_config.base)
#define LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(rcfg) \
        rcfg.base.max_segregated_object_size, rcfg.base.max_bitfit_object_size, rcfg.base.directory_size_bound_for_baseline_allocators, rcfg.base.directory_size_bound_for_no_view_cache

    fprintf(stderr,
        "%s(%d,0x%x) malloc: libpas config:"
        "\n\tDeallocation Log (Max Entries, Max Bytes): %zu, %zuB"
        "\n\tScavenger (Period, Deep-Sleep Timeout, Epoch-Delta): %.2fms, %.2fms, %llu"
        "\n\tMTE (Enabled/Mode-Bits/Tagging-Rate/Medium-Enabled/Lockdown/Hardened): (%u, %u, %u, %u, %u, %u)"
#if PAS_ENABLE_BMALLOC
        "\n\tUsing System Heap: %u"
        LOG_FMT_STR_FOR_HEAP_CONFIG(bmalloc)
        "\n\t\tRuntime Heap Config Size-Maximums (Segregated, Bitfit, Baseline Dir, No-View-Cache Dir):"
        "\n\t\t\tFlex: %uB, %uB, %uB, %uB"
        "\n\t\t\tIntrinsic: %uB, %uB, %uB, %uB"
        "\n\t\t\tTyped: %uB, %uB, %uB, %uB"
        "\n\t\t\tPrimitive: %uB, %uB, %uB, %uB"
#endif
#if PAS_ENABLE_JIT
        LOG_FMT_STR_FOR_HEAP_CONFIG(jit)
        "\n\t\tRuntime Heap Config Size-Maximums (Segregated, Bitfit, Baseline Dir, No-View-Cache Dir):"
        "\n\t\t\tFlex: %uB, %uB, %uB, %uB"
#endif
#if PAS_ENABLE_ISO
        LOG_FMT_STR_FOR_HEAP_CONFIG(iso)
#endif
        LOG_FMT_STR_FOR_HEAP_CONFIG(utility)
        "\n",

        /* Begin the fmt vars */
        progname, pid, (int)threadno,
        (size_t)PAS_DEALLOCATION_LOG_SIZE, (size_t)PAS_DEALLOCATION_LOG_MAX_BYTES,
        pas_scavenger_period_in_milliseconds, pas_scavenger_deep_sleep_timeout_in_milliseconds, pas_scavenger_max_epoch_delta,
        mte_conf[0], mte_conf[1], mte_conf[2], mte_conf[3], mte_conf[4], mte_conf[5],
#if PAS_ENABLE_BMALLOC
        pas_system_heap_is_enabled(pas_heap_config_kind_bmalloc),
        LOG_FMT_VARS_FOR_HEAP_CONFIG(bmalloc_heap_config),
        LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(bmalloc_flex_runtime_config),
        LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(bmalloc_intrinsic_runtime_config),
        LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(bmalloc_typed_runtime_config),
        LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(bmalloc_primitive_runtime_config),
#endif
#if PAS_ENABLE_JIT
        LOG_FMT_VARS_FOR_HEAP_CONFIG(jit_heap_config),
        LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(jit_heap_runtime_config),
#endif
#if PAS_ENABLE_ISO
        LOG_FMT_VARS_FOR_HEAP_CONFIG(iso_heap_config),
#endif
        LOG_FMT_VARS_FOR_HEAP_CONFIG(pas_utility_heap_config));
}

// rdar://164588924: We should refactor this to a more general mechanism
// for handling 'libpas setup' tasks, e.g. LibpasMallocReportConfig,
// probably in its own file with a hook back to this MTE setup work.
static void pas_mte_do_and_check_initialization(void* context)
{
    (void)context;
    pas_mte_do_initialization();
    const char* crashIfMTENotEnabled = getenv("MTE_crashIfNotEnabled");
    if (crashIfMTENotEnabled) {
        if (!strcasecmp(crashIfMTENotEnabled, "true")
            || !strcasecmp(crashIfMTENotEnabled, "yes")
            || !strcasecmp(crashIfMTENotEnabled, "1")) {
            PAS_ASSERT(pas_mte_is_enabled() && "MTE is not enabled, crashing");
        }
    }
    const char* logLibpasConfiguration = getenv("LibpasMallocReportConfig");
    if (logLibpasConfiguration) {
        if (!strcasecmp(logLibpasConfiguration, "true")
            || !strcasecmp(logLibpasConfiguration, "yes")
            || !strcasecmp(logLibpasConfiguration, "1"))
            pas_report_config();
    }
}

void pas_mte_ensure_initialized(void)
{
    static dispatch_once_t pred;
    dispatch_once_f(&pred, NULL, pas_mte_do_and_check_initialization);
}
#else // !PAS_OS(DARWIN)
#if PAS_ENABLE_MTE
#error "pas_mte_ensure_initialized does not support non-Darwin systems"
#endif
void pas_mte_ensure_initialized(void) { }
#endif // PAS_OS(DARWIN)
#endif // defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#endif // LIBPAS_ENABLED
