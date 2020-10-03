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

#pragma once

#include "JSCPtrTag.h"
#include "Options.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace JSC {

class JITOperationList {
public:
    static JITOperationList& instance();
    static void initialize();

    void* contains(void* pointer) const
    {
        if constexpr(ASSERT_ENABLED)
            return m_validatedOperations.get(removeCodePtrTag(pointer));
        return pointer;
    }

    static void populatePointersInJavaScriptCore();

    JS_EXPORT_PRIVATE static void populatePointersInEmbedder(const uintptr_t* beginHost, const uintptr_t* endHost, const uintptr_t* beginOperations, const uintptr_t* endOperations);

    // FIXME: Currently, assertIsHostFunction and assertIsJITOperation are the same.
    // We will make them work in a subsequent patch.
    template<typename T> static void assertIsHostFunction(T function)
    {
        UNUSED_PARAM(function);
#if ENABLE(JIT_OPERATION_VALIDATION)
        ASSERT(function, !Options::useJIT() || JITOperationList::instance().contains(bitwise_cast<void*>(function)));
#endif
    }

    template<typename T> static void assertIsJITOperation(T function)
    {
        UNUSED_PARAM(function);
#if ENABLE(JIT_OPERATION_VALIDATION)
        ASSERT(!Options::useJIT() || JITOperationList::instance().contains(bitwise_cast<void*>(function)));
#endif
    }

private:
    HashMap<void*, void*> m_validatedOperations;
};

extern LazyNeverDestroyed<JITOperationList> jitOperationList;

inline JITOperationList& JITOperationList::instance()
{
    return jitOperationList.get();
}

} // namespace JSC
