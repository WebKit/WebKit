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
#include "JITOperationList.h"

namespace JSC {

LazyNeverDestroyed<JITOperationList> jitOperationList;

#if ENABLE(JIT_OPERATION_VALIDATION)
extern const uintptr_t startOfHostFunctionsInJSC __asm("section$start$__DATA_CONST$__jsc_host");
extern const uintptr_t endOfHostFunctionsInJSC __asm("section$end$__DATA_CONST$__jsc_host");
extern const uintptr_t startOfJITOperationsInJSC __asm("section$start$__DATA_CONST$__jsc_ops");
extern const uintptr_t endOfJITOperationsInJSC __asm("section$end$__DATA_CONST$__jsc_ops");
#endif

void JITOperationList::initialize()
{
    jitOperationList.construct();
}

#if ENABLE(JIT_OPERATION_VALIDATION)
static SUPPRESS_ASAN ALWAYS_INLINE void addPointers(HashMap<void*, void*>& map, const uintptr_t* beginHost, const uintptr_t* endHost, const uintptr_t* beginOperations, const uintptr_t* endOperations)
{
    for (const uintptr_t* current = beginHost; current != endHost; ++current) {
        void* codePtr = removeCodePtrTag(bitwise_cast<void*>(*current));
        if (codePtr) {
            auto result = map.add(codePtr, tagCodePtr(codePtr, JSEntryPtrTag));
            ASSERT(result.isNewEntry);
        }
    }
    for (const uintptr_t* current = beginOperations; current != endOperations; ++current) {
        void* codePtr = removeCodePtrTag(bitwise_cast<void*>(*current));
        if (codePtr) {
            auto result = map.add(codePtr, tagCodePtr(codePtr, OperationPtrTag));
            ASSERT(result.isNewEntry);
        }
    }
}
#endif

void JITOperationList::populatePointersInJavaScriptCore()
{
#if ENABLE(JIT_OPERATION_VALIDATION)
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        if (Options::useJIT())
            addPointers(jitOperationList->m_validatedOperations, &startOfHostFunctionsInJSC, &endOfHostFunctionsInJSC, &startOfJITOperationsInJSC, &endOfJITOperationsInJSC);
    });
#endif
}

void JITOperationList::populatePointersInEmbedder(const uintptr_t* beginHost, const uintptr_t* endHost, const uintptr_t* beginOperations, const uintptr_t* endOperations)
{
    UNUSED_PARAM(beginHost);
    UNUSED_PARAM(endHost);
    UNUSED_PARAM(beginOperations);
    UNUSED_PARAM(endOperations);
#if ENABLE(JIT_OPERATION_VALIDATION)
    if (Options::useJIT())
        addPointers(jitOperationList->m_validatedOperations, beginHost, endHost, beginOperations, endOperations);
#endif
}

} // namespace JSC
