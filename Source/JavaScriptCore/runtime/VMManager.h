/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/DoublyLinkedList.h>
#include <wtf/IterationStatus.h>
#include <wtf/Lock.h>
#include <wtf/ScopedLambda.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

class VM;

class VMManager {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(VMManager);
public:
    enum class Error {
        None,
        TimedOut
    };

    static void add(VM*);
    static void remove(VM*);
    ALWAYS_INLINE static bool isValidVM(VM* vm)
    {
        return vm == s_recentVM ? true : isValidVMSlow(vm);
    }

    using IteratorCallback = IterationStatus(VM&);
    using TestCallback = bool(VM&);

    static VM* findMatchingVM(const Invocable<TestCallback> auto& test)
    {
        /* mlam */ SUPPRESS_FORWARD_DECL_ARG return findMatchingVMImpl(scopedLambda<TestCallback>(test));
    }

    static void forEachVM(const Invocable<IteratorCallback> auto& functor)
    {
        /* mlam */ SUPPRESS_FORWARD_DECL_ARG forEachVMImpl(scopedLambda<IteratorCallback>(functor));
    }

    static Error forEachVMWithTimeout(Seconds timeout, const Invocable<IteratorCallback> auto& functor)
    {
        /* mlam */ SUPPRESS_FORWARD_DECL_ARG return forEachVMWithTimeoutImpl(timeout, scopedLambda<IteratorCallback>(functor));
    }

    JS_EXPORT_PRIVATE static void dumpVMs();

private:
    JS_EXPORT_PRIVATE static bool isValidVMSlow(VM*);
    JS_EXPORT_PRIVATE static VM* findMatchingVMImpl(const ScopedLambda<TestCallback>&);
    JS_EXPORT_PRIVATE static void forEachVMImpl(const ScopedLambda<IteratorCallback>&);
    JS_EXPORT_PRIVATE static Error forEachVMWithTimeoutImpl(Seconds timeout, const ScopedLambda<IteratorCallback>&);

    JS_EXPORT_PRIVATE static VM* s_recentVM;
};

} // namespace JSC

