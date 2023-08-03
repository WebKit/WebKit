/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "InPlaceInterpreter.h"

#if ENABLE(WEBASSEMBLY)

#include "ArithProfile.h"
#include "CodeBlock.h"
#include "JSCConfig.h"
#include "LLIntPCRanges.h"
#include "LLIntSlowPaths.h"
#include "LLIntThunks.h"
#include "Opcode.h"
#include "WriteBarrier.h"

namespace JSC { namespace IPInt {

#define VALIDATE_IPINT_OPCODE(opcode, name) \
do { \
    void* base = reinterpret_cast<void*>(ipint_unreachable_validate); \
    void* ptr = reinterpret_cast<void*>(ipint_ ## name ## _validate); \
    void* untaggedBase = CodePtr<CFunctionPtrTag>::fromTaggedPtr(base).template untaggedPtr(); \
    void* untaggedPtr = CodePtr<CFunctionPtrTag>::fromTaggedPtr(ptr).template untaggedPtr(); \
    RELEASE_ASSERT((char*)(untaggedPtr) - (char*)(untaggedBase) == opcode * 256); \
} while (false);

#define VALIDATE_IPINT_0xFC_OPCODE(opcode, name) \
do { \
    void* base = reinterpret_cast<void*>(ipint_i32_trunc_sat_f32_s_validate); \
    void* ptr = reinterpret_cast<void*>(ipint_ ## name ## _validate); \
    void* untaggedBase = CodePtr<CFunctionPtrTag>::fromTaggedPtr(base).template untaggedPtr(); \
    void* untaggedPtr = CodePtr<CFunctionPtrTag>::fromTaggedPtr(ptr).template untaggedPtr(); \
    RELEASE_ASSERT((char*)(untaggedPtr) - (char*)(untaggedBase) == opcode * 256); \
} while (false);

void initialize()
{
#if !ENABLE(C_LOOP) && CPU(ADDRESS64) && (CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS)))
    FOR_EACH_IPINT_OPCODE(VALIDATE_IPINT_OPCODE);
    FOR_EACH_IPINT_0xFC_TRUNC_OPCODE(VALIDATE_IPINT_0xFC_OPCODE);
#else
    RELEASE_ASSERT_NOT_REACHED("IPInt only supports ARM64 and X86_64 (for now).");
#endif
}

} }

#endif // ENABLE(WEBASSEMBLY)
