/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "AssemblerBuffer.h"

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AssemblerData);

#if ENABLE(ASSEMBLER)

static ThreadSpecificAssemblerData* threadSpecificAssemblerDataPtr;

ThreadSpecificAssemblerData& threadSpecificAssemblerData()
{
    static std::once_flag flag;
    std::call_once(
        flag,
        [] () {
            threadSpecificAssemblerDataPtr = new ThreadSpecificAssemblerData();
        });
    return *threadSpecificAssemblerDataPtr;
}

#if CPU(ARM64E)
static ThreadSpecificAssemblerData* threadSpecificAssemblerHashesPtr;
ThreadSpecificAssemblerData& threadSpecificAssemblerHashes()
{
    static std::once_flag flag;
    std::call_once(
        flag,
        [] () {
            threadSpecificAssemblerHashesPtr = new ThreadSpecificAssemblerData();
        });
    return *threadSpecificAssemblerHashesPtr;
}
#endif // CPU(ARM64E)

#endif // ENABLE(ASSEMBLER)

} // namespace JSC
