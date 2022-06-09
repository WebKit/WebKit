/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PtrTag.h>

namespace JSC {

#if ENABLE(JIT_OPERATION_VALIDATION)

// This indirection is provided so that we can manually force on assertions for
// testing even on release builds.
#define JIT_OPERATION_VALIDATION_ASSERT_ENABLED ASSERT_ENABLED

struct JITOperationAnnotation;

class JITOperationList {
public:
    static JITOperationList& instance();
    static void initialize();

    template<typename PtrType>
    void* map(PtrType pointer) const
    {
        return m_validatedOperations.get(removeCodePtrTag(bitwise_cast<void*>(pointer)));
    }

#if JIT_OPERATION_VALIDATION_ASSERT_ENABLED
    template<typename PtrType>
    void* inverseMap(PtrType pointer) const
    {
        return m_validatedOperationsInverseMap.get(bitwise_cast<void*>(pointer));
    }
#endif

    static void populatePointersInJavaScriptCore();
    static void populatePointersInJavaScriptCoreForLLInt();

    JS_EXPORT_PRIVATE static void populatePointersInEmbedder(const JITOperationAnnotation* beginOperations, const JITOperationAnnotation* endOperations);

    template<typename T> static void assertIsJITOperation(T function)
    {
        UNUSED_PARAM(function);
#if JIT_OPERATION_VALIDATION_ASSERT_ENABLED
        RELEASE_ASSERT(!Options::useJIT() || JITOperationList::instance().map(function));
#endif
    }

    template<typename T> static void assertIsJITOperationWithValidation(T function)
    {
        UNUSED_PARAM(function);
#if JIT_OPERATION_VALIDATION_ASSERT_ENABLED
        RELEASE_ASSERT(!Options::useJIT() || JITOperationList::instance().inverseMap(function));
#endif
    }

private:
    ALWAYS_INLINE void addPointers(const JITOperationAnnotation* begin, const JITOperationAnnotation* end);

#if JIT_OPERATION_VALIDATION_ASSERT_ENABLED
    void addInverseMap(void* validationEntry, void* pointer);
#endif

    HashMap<void*, void*> m_validatedOperations;
#if JIT_OPERATION_VALIDATION_ASSERT_ENABLED
    HashMap<void*, void*> m_validatedOperationsInverseMap;
#endif
};

JS_EXPORT_PRIVATE extern LazyNeverDestroyed<JITOperationList> jitOperationList;

inline JITOperationList& JITOperationList::instance()
{
    return jitOperationList.get();
}

#else // not ENABLE(JIT_OPERATION_VALIDATION)

class JITOperationList {
public:
    static void initialize() { }

    static void populatePointersInJavaScriptCore() { }
    static void populatePointersInJavaScriptCoreForLLInt() { }

    template<typename T> static void assertIsJITOperation(T) { }
    template<typename T> static void assertIsJITOperationWithValidation(T) { }
};

#endif // ENABLE(JIT_OPERATION_VALIDATION)

} // namespace JSC
