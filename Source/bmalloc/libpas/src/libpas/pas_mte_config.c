/*
 * Copyright (c) 2025-2026 Apple Inc. All rights reserved.
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
#include "pas_basic_heap_runtime_config.h"
#include "pas_heap.h"
#include "pas_internal_config.h"
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

static pas_basic_heap_runtime_config* all_bmalloc_runtime_configs[] = {
    &bmalloc_flex_runtime_config,
    &bmalloc_intrinsic_runtime_config,
    &bmalloc_typed_runtime_config,
    &bmalloc_primitive_runtime_config,
};
#endif // PAS_ENABLE_BMALLOC
#if PAS_ENABLE_JIT
extern const pas_heap_config jit_heap_config;
extern pas_heap_runtime_config jit_heap_runtime_config;
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
    pas_runtime_config* config = PAS_RUNTIME_CONFIG_PTR;

    struct proc_bsdinfo info;
    int rc = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    if (rc == sizeof(info) && info.pbi_flags & PAS_MTE_PROC_FLAG_SEC_ENABLED)
        config->enabled = true;

    if (is_env_true("MTE_overrideEnablementForJavaScriptCore")) {
        PAS_ASSERT(!is_env_false("MTE_overrideEnablementForJavaScriptCore"));
        config->enabled = false;
    }
    if (is_env_false("MTE_overrideEnablementForJavaScriptCore"))
        config->enabled = false;

    if (!config->enabled)
        return;

    uint64_t ldmState = 0;
    size_t sysCtlLen = sizeof(ldmState);
    const char* lockdownModeProcName = "com.apple.WebKit.WebContent.CaptivePortal";
    bool isLockdownModeWebContentProcess = !strncmp(getprogname(), lockdownModeProcName, strlen(lockdownModeProcName));
    if ((sysctlbyname("security.mac.lockdown_mode_state", &ldmState, &sysCtlLen, NULL, 0) >= 0 && ldmState == 1) || isLockdownModeWebContentProcess)
        config->is_lockdown_mode = true;
    else
        config->is_lockdown_mode = false;

    unsigned mode = 0;
    if (get_value_if_available(&mode, "MTE_libpasConfig")) {
        uint8_t mode_byte = (uint8_t)(mode & 0xFF);
        config->mode_bits.retag_on_scavenge = (mode_byte >> PAS_MTE_FEATURE_RETAG_ON_SCAVENGE) & 1;
        config->mode_bits.log_on_tag = (mode_byte >> PAS_MTE_FEATURE_LOG_ON_TAG) & 1;
        config->mode_bits.log_on_purify = (mode_byte >> PAS_MTE_FEATURE_LOG_ON_PURIFY) & 1;
        config->mode_bits.log_page_alloc = (mode_byte >> PAS_MTE_FEATURE_LOG_PAGE_ALLOC) & 1;
        config->mode_bits.zero_tag_all = (mode_byte >> PAS_MTE_FEATURE_ZERO_TAG_ALL) & 1;
        config->mode_bits.adjacent_tag_exclusion = (mode_byte >> PAS_MTE_FEATURE_ADJACENT_TAG_EXCLUSION) & 1;
        config->mode_bits.assert_adjacent_tags_are_disjoint = (mode_byte >> PAS_MTE_FEATURE_ASSERT_ADJACENT_TAGS_ARE_DISJOINT) & 1;
    }

    const char* name = getprogname();
    bool isWebContentProcess = !strncmp(name, "com.apple.WebKit.WebContent", 27) || !strncmp(name, "jsc", 3);

    if (isWebContentProcess) {
        // For a variety of reasons, a full MTE implementation in the WebContent process is not generally practical.
        // As such, by default, we disable MTE in the WebContent process while leaving it on in the privileged
        // processes. However, in certain "extra secure" contexts, this disablement is overridden such that we
        // treat WebContent like any other process for the purposes of MTE.

        bool wcp_is_hardened = false;
        bool isEnhancedSecurityWebContentProcess = !strncmp(name, "com.apple.WebKit.WebContent.EnhancedSecurity", 44);
        if (config->is_lockdown_mode || isEnhancedSecurityWebContentProcess)
            wcp_is_hardened = true;

        if (wcp_is_hardened) {
            config->medium_tagging_enabled = true;
            config->enabled = true;
            config->is_hardened = true;

            pas_mte_force_nontaggable_user_allocations_into_large_heap();
        } else {
            config->medium_tagging_enabled = false;
            config->is_hardened = false;
#if !PAS_USE_MTE_IN_WEBCONTENT
            // Disable tagging in libpas by default in WebContent process
            config->enabled = false;
#else
            config->enabled = true;
#endif
        }

#ifndef NDEBUG
        if (is_env_true("MTE_disableForWebContent")) {
            PAS_ASSERT(!is_env_true("MTE_overrideEnablementForWebContent"));
            config->enabled = false;
            config->medium_tagging_enabled = false;
        }
#endif
        if (is_env_true("MTE_overrideEnablementForWebContent")) {
            config->enabled = true;
            config->medium_tagging_enabled = true;
        } else if (is_env_false("MTE_overrideEnablementForWebContent")) {
            config->enabled = false;
            config->medium_tagging_enabled = false;
        }
    } else {
        config->medium_tagging_enabled = true; // Tag libpas medium objects in privileged processes
        config->is_hardened = true;
    }

    PAS_IGNORE_WARNINGS_BEGIN("unreachable-code");
    // Retag-on-scavenge functionally supports both segregated and bitfit heaps.
    // However, true to the name, for segregated heaps the retag operation only
    // takes place on the scavenging path.
    // For bitfit heaps, there is no such path: as such, freed bitfit objects are
    // immediately retagged. This closes the window where attackers could in
    // theory exploit a UAF.
    // As such, when retag-on-scavenge is enabled, we prefer to allocate from
    // bitfit heaps to exploit this property -- the exception of course being
    // isoheaps, due to the intrinsic type-unsafety of bitfit heaps.
    // In practice, for privileged processes this already takes place when WebCore
    // enables fastMiniMode(), but that enablement takes place later on and as such
    // can allow for some local-allocators to sneak by. This is not a problem for
    // mini-mode because a few stray allocations there won't hurt, but for a
    // security boundary those stray allocations are more of a problem. So we force
    // this here, prior to the creation of such segregated directories.
    if (PAS_MTE_FEATURE_ENABLED(PAS_MTE_FEATURE_RETAG_ON_SCAVENGE))
        pas_bmalloc_force_allocations_into_bitfit_heaps_where_available();
    PAS_IGNORE_WARNINGS_END;
}

static bool pas_mte_is_enabled(void)
{
    const pas_runtime_config* config = PAS_RUNTIME_CONFIG_PTR;
    struct proc_bsdinfo info;
    int rc = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    return (rc == sizeof(info) && (info.pbi_flags & PAS_MTE_PROC_FLAG_SEC_ENABLED) && config->enabled);
}

#else // !PAS_ENABLE_MTE

static PAS_UNUSED void pas_mte_do_initialization(void)
{
    pas_runtime_config* config = PAS_RUNTIME_CONFIG_PTR;
    config->enabled = false;
}

static PAS_UNUSED bool pas_mte_is_enabled(void)
{
    return false;
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

    const pas_runtime_config* config = PAS_RUNTIME_CONFIG_PTR;

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
#define LOG_FMT_VARS_FOR_BASIC_HEAP_RUNTIME_CONFIG(rcfg) \
    rcfg.base.max_segregated_object_size, rcfg.base.max_bitfit_object_size, rcfg.base.directory_size_bound_for_baseline_allocators, rcfg.base.directory_size_bound_for_no_view_cache
#define LOG_FMT_VARS_FOR_HEAP_RUNTIME_CONFIG(rcfg) \
    rcfg.max_segregated_object_size, rcfg.max_bitfit_object_size, rcfg.directory_size_bound_for_baseline_allocators, rcfg.directory_size_bound_for_no_view_cache

    fprintf(stderr,
        "%s(%d,0x%x) malloc: libpas config:"
        "\n\tDeallocation Log (Max Entries, Max Bytes): %zu, %zuB"
        "\n\tScavenger (Period, Deep-Sleep Timeout, Epoch-Delta): %.2fms, %.2fms, %llu"
        "\n\tMTE (Enabled/Medium-Enabled/Lockdown/Hardened/ATE/RoS/ZTA): (%u, %u, %u, %u, %u, %u, %u)"
#if PAS_ENABLE_BMALLOC
        "\n\tForwarding to System Heap: %u"
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
        config->enabled, config->medium_tagging_enabled, config->is_lockdown_mode, config->is_hardened,
        config->mode_bits.adjacent_tag_exclusion, config->mode_bits.retag_on_scavenge, config->mode_bits.zero_tag_all,
#if PAS_ENABLE_BMALLOC
        pas_system_heap_should_supplant_bmalloc(pas_heap_config_kind_bmalloc),
        LOG_FMT_VARS_FOR_HEAP_CONFIG(bmalloc_heap_config),
        LOG_FMT_VARS_FOR_BASIC_HEAP_RUNTIME_CONFIG(bmalloc_flex_runtime_config),
        LOG_FMT_VARS_FOR_BASIC_HEAP_RUNTIME_CONFIG(bmalloc_intrinsic_runtime_config),
        LOG_FMT_VARS_FOR_BASIC_HEAP_RUNTIME_CONFIG(bmalloc_typed_runtime_config),
        LOG_FMT_VARS_FOR_BASIC_HEAP_RUNTIME_CONFIG(bmalloc_primitive_runtime_config),
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

void pas_bmalloc_force_allocations_into_bitfit_heaps_where_available(void)
{
#if PAS_ENABLE_BMALLOC
    // Switch to bitfit allocation for anything that isn't isoheaped.
    bmalloc_intrinsic_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_primitive_runtime_config.base.max_segregated_object_size = 0;
    bmalloc_intrinsic_runtime_config.base.max_bitfit_object_size = UINT_MAX;
    bmalloc_primitive_runtime_config.base.max_bitfit_object_size = UINT_MAX;
#endif

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
}

bool pas_mte_is_mte_enabled(void)
{
    pas_mte_ensure_initialized();
#if PAS_ENABLE_MTE
    return PAS_RUNTIME_CONFIG_PTR->enabled;
#else
    return false;
#endif
}

void pas_mte_force_nontaggable_user_allocations_into_large_heap(void)
{
#if PAS_ENABLE_BMALLOC
    for (size_t i = 0; i < sizeof(all_bmalloc_runtime_configs) / sizeof(void*); i++) {
        pas_basic_heap_runtime_config* cfg = all_bmalloc_runtime_configs[i];
        cfg->base.max_segregated_object_size = (unsigned)PAS_MIN((size_t)cfg->base.max_segregated_object_size, PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE);
        cfg->base.max_bitfit_object_size = (unsigned)PAS_MIN((size_t)cfg->base.max_bitfit_object_size, PAS_MAX_MTE_TAGGABLE_OBJECT_SIZE);
    }
#endif
}
#endif // defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#endif // LIBPAS_ENABLED
