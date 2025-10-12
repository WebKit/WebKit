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
#endif
#if !PAS_OS(WINDOWS)
#include "unistd.h"
#endif

#include "pas_heap.h"
#include "pas_mte.h"
#include "pas_utils.h"
#include "pas_zero_memory.h"

#if defined(PAS_USE_OPENSOURCE_MTE) && PAS_USE_OPENSOURCE_MTE
#if PAS_ENABLE_MTE

extern pas_heap bmalloc_common_primitive_heap;

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
    uint8_t* is_wcp_byte = &PAS_MTE_CONFIG_BYTE(PAS_MTE_IS_WCP_FLAG);

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

    unsigned mode = 0;
    if (get_value_if_available(&mode, "JSC_allocationProfilingMode"))
        *mode_byte = (uint8_t)(mode & 0xFF);

    const char* name = info.pbi_name[0] ? info.pbi_name : info.pbi_comm;
    bool isWebContentProcess = !strncmp(name, "com.apple.WebKit.WebContent", 27) || !strncmp(name, "jsc", 3);
    *is_wcp_byte = isWebContentProcess;

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
        *medium_byte = 0;
#if !PAS_USE_MTE_IN_WEBCONTENT
        // Disable tagging in libpas by default in WebContent process
        *enabled_byte = 0;
#endif
        uint64_t ldmState = 0;
        size_t sysCtlLen = sizeof(ldmState);
        if (sysctlbyname("security.mac.lockdown_mode_state", &ldmState, &sysCtlLen, NULL, 0) >= 0 && ldmState == 1) {
            *enabled_byte = 1;
            *medium_byte = 1;
            *lockdown_mode_byte = 1;
        } else {
            *lockdown_mode_byte = 0;

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
    } else
        *medium_byte = 1; // Tag libpas medium objects in privileged processes.
}

static bool pas_mte_is_enabled(void)
{
    const uint8_t* enabledByte = ((const uint8_t*)(g_config + 2));
    struct proc_bsdinfo info;
    int rc = proc_pidinfo(getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info));
    return (rc == sizeof(info) && (info.pbi_flags & PAS_MTE_PROC_FLAG_SEC_ENABLED) && !!*enabledByte);
}

#else // !PAS_ENABLE_MTE

static PAS_UNUSED void pas_mte_do_initialization(void) { }

static PAS_UNUSED bool pas_mte_is_enabled(void)
{
    return false;
}

#endif // PAS_ENABLE_MTE

#if PAS_OS(DARWIN)
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
