/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CPU.h"

#if (CPU(X86) || CPU(X86_64) || CPU(ARM64)) && OS(DARWIN)
#include <array>
#include <mutex>
#include <optional>
#include <sys/sysctl.h>
#include <wtf/text/StringView.h>
#endif

#if ENABLE(ASSEMBLER)
#include "MacroAssembler.h"
#endif

namespace JSC {

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
bool isKernOpenSource()
{
    uint32_t val = 0;
    size_t valSize = sizeof(val);
    return !sysctlbyname("kern.opensource_kernel", &val, &valSize, nullptr, 0) && val;
}
#endif

#if (CPU(X86) || CPU(X86_64)) && OS(DARWIN)
bool isKernTCSMAvailable()
{
    if (!Options::useKernTCSM())
        return false;

    uint32_t val = 0;
    size_t valSize = sizeof(val);
    int rc = sysctlbyname("kern.tcsm_available", &val, &valSize, nullptr, 0);
    if (rc < 0)
        return false;
    return !!val;
}

bool enableKernTCSM()
{
    uint32_t val = 1;
    int rc = sysctlbyname("kern.tcsm_enable", nullptr, nullptr, &val, sizeof(val));
    if (rc < 0)
        return false;
    return true;
}

int kernTCSMAwareNumberOfProcessorCores()
{
    static std::once_flag onceFlag;
    static int result;
    std::call_once(onceFlag, [] {
        result = WTF::numberOfProcessorCores();
        if (result <= 1)
            return;
        if (isKernTCSMAvailable())
            --result;
    });
    return result;
}

int64_t hwL3CacheSize()
{
    int64_t val = 0;
    size_t valSize = sizeof(val);
    int rc = sysctlbyname("hw.l3cachesize", &val, &valSize, nullptr, 0);
    if (rc < 0)
        return 0;
    return val;
}

int32_t hwPhysicalCPUMax()
{
    int32_t val = 0;
    size_t valSize = sizeof(val);
    int rc = sysctlbyname("hw.physicalcpu_max", &val, &valSize, nullptr, 0);
    if (rc < 0)
        return 0;
    return val;
}

#endif // #if (CPU(X86) || CPU(X86_64)) && OS(DARWIN)

#if CPU(ARM64) && OS(DARWIN)
static int32_t sysctlInt32(const char* name)
{
    int32_t val = 0;
    size_t valSize = sizeof(val);
    int rc = sysctlbyname(name, &val, &valSize, nullptr, 0);
    if (rc < 0)
        return 0;
    return val;
}

struct PerfLevelSysctls {
    const ASCIILiteral physicalCPUMax;
    const ASCIILiteral name;
};

static constexpr std::array<PerfLevelSysctls, numberOfCoreCategories> perfLevelSysctls = { {
    { "hw.perflevel0.physicalcpu_max"_s, "hw.perflevel0.name"_s },
    { "hw.perflevel1.physicalcpu_max"_s, "hw.perflevel1.name"_s },
    { "hw.perflevel2.physicalcpu_max"_s, "hw.perflevel2.name"_s },
} };

static std::optional<CoreCategory> categoryFromPerfLevelName(unsigned level)
{
    std::array<char, 32> buffer { };
    size_t bufferSize = buffer.size();
    if (sysctlbyname(perfLevelSysctls[level].name, buffer.data(), &bufferSize, nullptr, 0) < 0)
        return std::nullopt;
    auto name = StringView::fromLatin1(buffer.data());
    if (name == "Super"_s)
        return CoreCategory::Super;
    if (name == "Performance"_s)
        return CoreCategory::Performance;
    if (name == "Efficiency"_s)
        return CoreCategory::Efficiency;
    // A chip whose cores are all alike reports one level named "Standard". Nothing slower exists to
    // contrast those cores with, so they are the performance cores.
    if (name == "Standard"_s)
        return CoreCategory::Performance;
    return std::nullopt;
}

// Fallback for a level whose name is missing or unfamiliar. The slowest level always holds the
// efficiency cores, and only a chip reporting all three levels has Super cores.
static CoreCategory categoryFromPerfLevelIndex(unsigned level, unsigned lastLevel)
{
    if (lastLevel && level == lastLevel)
        return CoreCategory::Efficiency;
    if (lastLevel > 1 && !level)
        return CoreCategory::Super;
    return CoreCategory::Performance;
}

int32_t hwNumberOfCores(CoreCategory category)
{
    static std::once_flag onceFlag;
    static std::array<int32_t, numberOfCoreCategories> coresPerCategory { };
    std::call_once(onceFlag, [] {
        constexpr unsigned maxLevelCount = perfLevelSysctls.size();
        std::array<int32_t, maxLevelCount> coresPerLevel { };
        std::array<std::optional<CoreCategory>, maxLevelCount> categoryPerLevel { };
        unsigned lastLevel = 0;
        // A level this chip does not have simply fails to answer, so ask about every level rather
        // than assuming the ones that answer are contiguous.
        for (unsigned level = 0; level < maxLevelCount; ++level) {
            int32_t cores = sysctlInt32(perfLevelSysctls[level].physicalCPUMax);
            if (cores <= 0)
                continue;
            coresPerLevel[level] = cores;
            categoryPerLevel[level] = categoryFromPerfLevelName(level);
            lastLevel = level;
        }
        for (unsigned level = 0; level < maxLevelCount; ++level) {
            if (coresPerLevel[level] <= 0)
                continue;
            CoreCategory levelCategory = categoryPerLevel[level].value_or(categoryFromPerfLevelIndex(level, lastLevel));
            coresPerCategory[static_cast<unsigned>(levelCategory)] += coresPerLevel[level];
        }
    });
    return coresPerCategory[static_cast<unsigned>(category)];
}
#endif // #if CPU(ARM64) && OS(DARWIN)

#if CPU(ARM64) && !(CPU(ARM64E) || OS(MACOS))
bool isARM64_LSE()
{
#if ENABLE(ASSEMBLER)
    return MacroAssembler::supportsLSE();
#else
    return false;
#endif
}
#endif

#if CPU(ARM64) && !OS(MACOS)
bool isARM64_SHA3()
{
#if ENABLE(ASSEMBLER)
    return MacroAssembler::supportsSHA3();
#else
    return false;
#endif
}
#endif

#if CPU(ARM64E)
bool isARM64E_FPAC()
{
#if OS(DARWIN)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        uint32_t val = 0;
        size_t valSize = sizeof(val);
        int rc = sysctlbyname("hw.optional.arm.FEAT_FPAC", &val, &valSize, nullptr, 0);
        g_jscConfig.canUseFPAC = rc >= 0 && val;
    });
    return g_jscConfig.canUseFPAC;
#else
    return false;
#endif
}
#endif // CPU(ARM64E)

#if CPU(X86_64)
bool isX86_64_AVX()
{
#if ENABLE(ASSEMBLER)
    return MacroAssembler::supportsAVX();
#else
    return false;
#endif
}
#endif

} // namespace JSC
