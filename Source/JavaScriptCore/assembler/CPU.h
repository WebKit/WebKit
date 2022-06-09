/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "Options.h"
#include <wtf/NumberOfCores.h>
#include <wtf/StdIntExtras.h>

namespace JSC {

using UCPUStrictInt32 = UCPURegister;

constexpr bool isARMv7IDIVSupported()
{
#if HAVE(ARM_IDIV_INSTRUCTIONS)
    return true;
#else
    return false;
#endif
}

constexpr bool isARM64()
{
#if CPU(ARM64)
    return true;
#else
    return false;
#endif
}

constexpr bool isARM64E()
{
#if CPU(ARM64E)
    return true;
#else
    return false;
#endif
}

constexpr bool isX86()
{
#if CPU(X86_64) || CPU(X86)
    return true;
#else
    return false;
#endif
}

constexpr bool isX86_64()
{
#if CPU(X86_64)
    return true;
#else
    return false;
#endif
}

constexpr bool is64Bit()
{
#if USE(JSVALUE64)
    return true;
#else
    return false;
#endif
}

constexpr bool is32Bit()
{
    return !is64Bit();
}

constexpr bool isAddress64Bit()
{
    return sizeof(void*) == 8;
}

constexpr bool isAddress32Bit()
{
    return !isAddress64Bit();
}

constexpr bool isMIPS()
{
#if CPU(MIPS)
    return true;
#else
    return false;
#endif
}

inline bool optimizeForARMv7IDIVSupported()
{
    return isARMv7IDIVSupported() && Options::useArchitectureSpecificOptimizations();
}

inline bool optimizeForARM64()
{
    return isARM64() && Options::useArchitectureSpecificOptimizations();
}

inline bool optimizeForX86()
{
    return isX86() && Options::useArchitectureSpecificOptimizations();
}

inline bool optimizeForX86_64()
{
    return isX86_64() && Options::useArchitectureSpecificOptimizations();
}

inline bool hasSensibleDoubleToInt()
{
    return optimizeForX86();
}

#if (CPU(X86) || CPU(X86_64)) && OS(DARWIN)
bool isKernTCSMAvailable();
bool enableKernTCSM();
int kernTCSMAwareNumberOfProcessorCores();
int64_t hwL3CacheSize();
int32_t hwPhysicalCPUMax();
#else
ALWAYS_INLINE bool isKernTCSMAvailable() { return false; }
ALWAYS_INLINE bool enableKernTCSM() { return false; }
ALWAYS_INLINE int kernTCSMAwareNumberOfProcessorCores() { return WTF::numberOfProcessorCores(); }
ALWAYS_INLINE int64_t hwL3CacheSize() { return 0; }
ALWAYS_INLINE int32_t hwPhysicalCPUMax() { return kernTCSMAwareNumberOfProcessorCores(); }
#endif

} // namespace JSC

