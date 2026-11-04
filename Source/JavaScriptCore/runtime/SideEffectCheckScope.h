/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "CodeSpecializationKind.h"
#include "JSCJSValue.h"
#include "NativeFunction.h"
#include <wtf/HashMap.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

class ArgList;
class CodeBlock;
class FunctionExecutable;
class JSFunction;
class JSGlobalObject;
class JSObject;
class ScriptExecutable;
class SourceCode;
class SourceProvider;
class VM;

// Functions that are allowed to run while a SideEffectCheckScope is active, even though they were not
// defined by the code being evaluated. Every entry must not modify state that is observable by the page.
class SideEffectFreeFunctions {
    WTF_MAKE_TZONE_ALLOCATED(SideEffectFreeFunctions);
    WTF_MAKE_NONCOPYABLE(SideEffectFreeFunctions);
public:
    enum class Guard : uint8_t {
        // Natives only call into other code through the VM entry points, which are themselves checked.
        None,
        // The receiver and every argument must be primitives.
        PrimitiveArguments,
        // The receiver must be an unmodified array, and every callable argument must also be allowed to run,
        // since a JS builtin calls its callback (and reads properties of its receiver) without going through C++.
        ArrayReceiver,
    };

    SideEffectFreeFunctions();
    ~SideEffectFreeFunctions();

    void addHostFunction(NativeFunction, Guard = Guard::None);

    // Lazily resolves the JSC builtins in the allowlist from a pristine realm, so that functions that were
    // replaced or moved around by the page are never mistaken for the original.
    void ensureBuiltinsAdded(VM&);

    std::optional<Guard> guardForHostFunction(TaggedNativeFunction) const;
    std::optional<Guard> guardForBuiltin(const FunctionExecutable&) const;

private:
    void add(JSValue, Guard);

    UncheckedKeyHashMap<const void*, Guard> m_hostFunctions;
    UncheckedKeyHashMap<unsigned, Guard, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_builtins;
    RefPtr<SourceProvider> m_builtinSourceProvider;
    bool m_didAddBuiltins { false };
};

// While active, the only code that may run is code defined by `source` and the functions in
// SideEffectFreeFunctions. Anything else throws an EvalError instead of running. Code defined by `source`
// is the caller's responsibility (e.g. the Web Inspector frontend statically rejects assignments).
class SideEffectCheckScope {
    WTF_FORBID_HEAP_ALLOCATION;
    WTF_MAKE_NONCOPYABLE(SideEffectCheckScope);
public:
    // Code is only checked while it runs in the LLInt, where every call it makes (explicitly or e.g. by calling a getter)
    // goes through a slow path. Callers must not evaluate anything that should be checked if this is false.
    static bool isSupported();

    SideEffectCheckScope(VM&, const SourceCode&);
    ~SideEffectCheckScope();

    bool isEvaluatedCode(const ScriptExecutable*) const;
    bool isEvaluatedCode(const CodeBlock*) const;
    bool isEvaluatedSource(const SourceProvider* provider) const { return provider == m_sourceProvider.get(); }

    // Returns false, after throwing, if `callee` must not run.
    bool checkCall(JSGlobalObject*, JSObject* callee, CodeSpecializationKind, JSValue thisValue, const ArgList& arguments);
    bool checkCachedCall(JSGlobalObject*, JSFunction*);

private:
    bool throwUnlessAllowed(JSGlobalObject*, bool isAllowed);
    bool isAllowedToRun(JSObject* callee, CodeSpecializationKind, JSValue thisValue, const ArgList& arguments);
    bool isAllowedCallback(JSValue);
    bool passesGuard(SideEffectFreeFunctions::Guard, JSValue thisValue, const ArgList& arguments);

    VM& m_vm;
    SideEffectCheckScope* m_previousScope;
    const RefPtr<SourceProvider> m_sourceProvider;
    MonotonicTime m_deadline;
};

} // namespace JSC
