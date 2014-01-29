/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef CommonSlowPaths_h
#define CommonSlowPaths_h

#include "CodeBlock.h"
#include "CodeSpecializationKind.h"
#include "ExceptionHelpers.h"
#include "JSStackInlines.h"
#include "NameInstance.h"
#include "StackAlignment.h"
#include "VM.h"
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(JIT) || ENABLE(LLINT)

namespace JSC {

// The purpose of this namespace is to include slow paths that are shared
// between the interpreter and baseline JIT. They are written to be agnostic
// with respect to the slow-path calling convention, but they do rely on the
// JS code being executed more-or-less directly from bytecode (so the call
// frame layout is unmodified, making it potentially awkward to use these
// from any optimizing JIT, like the DFG).

namespace CommonSlowPaths {

struct ArityCheckData {
    unsigned paddedStackSpace;
    void* thunkToCall;
    void* returnPC;
};

ALWAYS_INLINE int arityCheckFor(ExecState* exec, JSStack* stack, CodeSpecializationKind kind)
{
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    ASSERT(!callee->isHostFunction());
    CodeBlock* newCodeBlock = callee->jsExecutable()->codeBlockFor(kind);
    int argumentCountIncludingThis = exec->argumentCountIncludingThis();
    
    ASSERT(argumentCountIncludingThis < newCodeBlock->numParameters());
    int missingArgumentCount = newCodeBlock->numParameters() - argumentCountIncludingThis;
    int neededStackSpace = missingArgumentCount + 1; // Allow space to save the original return PC.
    int paddedStackSpace = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), neededStackSpace);

    if (!stack->ensureCapacityFor(exec->registers() - paddedStackSpace))
        return -1;
    return paddedStackSpace / stackAlignmentRegisters();
}

inline bool opIn(ExecState* exec, JSValue propName, JSValue baseVal)
{
    if (!baseVal.isObject()) {
        exec->vm().throwException(exec, createInvalidParameterError(exec, "in", baseVal));
        return false;
    }

    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName.getUInt32(i))
        return baseObj->hasProperty(exec, i);

    if (isName(propName))
        return baseObj->hasProperty(exec, jsCast<NameInstance*>(propName.asCell())->privateName());

    Identifier property(exec, propName.toString(exec)->value(exec));
    if (exec->vm().exception())
        return false;
    return baseObj->hasProperty(exec, property);
}

} // namespace CommonSlowPaths

class ExecState;
struct Instruction;

#if USE(JSVALUE64)
// According to C++ rules, a type used for the return signature of function with C linkage (i.e.
// 'extern "C"') needs to be POD; hence putting any constructors into it could cause either compiler
// warnings, or worse, a change in the ABI used to return these types.
struct SlowPathReturnType {
    void* a;
    void* b;
};

inline SlowPathReturnType encodeResult(void* a, void* b)
{
    SlowPathReturnType result;
    result.a = a;
    result.b = b;
    return result;
}

inline void decodeResult(SlowPathReturnType result, void*& a, void*& b)
{
    a = result.a;
    b = result.b;
}

#else // USE(JSVALUE32_64)
typedef int64_t SlowPathReturnType;

typedef union {
    struct {
        void* a;
        void* b;
    } pair;
    int64_t i;
} SlowPathReturnTypeEncoding;

inline SlowPathReturnType encodeResult(void* a, void* b)
{
    SlowPathReturnTypeEncoding u;
    u.pair.a = a;
    u.pair.b = b;
    return u.i;
}

inline void decodeResult(SlowPathReturnType result, void*& a, void*& b)
{
    SlowPathReturnTypeEncoding u;
    u.i = result;
    a = u.pair.a;
    b = u.pair.b;
}
#endif // USE(JSVALUE32_64)
    
#define SLOW_PATH
    
#define SLOW_PATH_DECL(name) \
extern "C" SlowPathReturnType SLOW_PATH name(ExecState* exec, Instruction* pc)
    
#define SLOW_PATH_HIDDEN_DECL(name) \
SLOW_PATH_DECL(name) WTF_INTERNAL
    
SLOW_PATH_HIDDEN_DECL(slow_path_call_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_construct_arityCheck);
SLOW_PATH_HIDDEN_DECL(slow_path_touch_entry);
SLOW_PATH_HIDDEN_DECL(slow_path_create_arguments);
SLOW_PATH_HIDDEN_DECL(slow_path_create_this);
SLOW_PATH_HIDDEN_DECL(slow_path_enter);
SLOW_PATH_HIDDEN_DECL(slow_path_get_callee);
SLOW_PATH_HIDDEN_DECL(slow_path_to_this);
SLOW_PATH_HIDDEN_DECL(slow_path_captured_mov);
SLOW_PATH_HIDDEN_DECL(slow_path_new_captured_func);
SLOW_PATH_HIDDEN_DECL(slow_path_not);
SLOW_PATH_HIDDEN_DECL(slow_path_eq);
SLOW_PATH_HIDDEN_DECL(slow_path_neq);
SLOW_PATH_HIDDEN_DECL(slow_path_stricteq);
SLOW_PATH_HIDDEN_DECL(slow_path_nstricteq);
SLOW_PATH_HIDDEN_DECL(slow_path_less);
SLOW_PATH_HIDDEN_DECL(slow_path_lesseq);
SLOW_PATH_HIDDEN_DECL(slow_path_greater);
SLOW_PATH_HIDDEN_DECL(slow_path_greatereq);
SLOW_PATH_HIDDEN_DECL(slow_path_inc);
SLOW_PATH_HIDDEN_DECL(slow_path_dec);
SLOW_PATH_HIDDEN_DECL(slow_path_to_number);
SLOW_PATH_HIDDEN_DECL(slow_path_negate);
SLOW_PATH_HIDDEN_DECL(slow_path_add);
SLOW_PATH_HIDDEN_DECL(slow_path_mul);
SLOW_PATH_HIDDEN_DECL(slow_path_sub);
SLOW_PATH_HIDDEN_DECL(slow_path_div);
SLOW_PATH_HIDDEN_DECL(slow_path_mod);
SLOW_PATH_HIDDEN_DECL(slow_path_lshift);
SLOW_PATH_HIDDEN_DECL(slow_path_rshift);
SLOW_PATH_HIDDEN_DECL(slow_path_urshift);
SLOW_PATH_HIDDEN_DECL(slow_path_unsigned);
SLOW_PATH_HIDDEN_DECL(slow_path_bitand);
SLOW_PATH_HIDDEN_DECL(slow_path_bitor);
SLOW_PATH_HIDDEN_DECL(slow_path_bitxor);
SLOW_PATH_HIDDEN_DECL(slow_path_typeof);
SLOW_PATH_HIDDEN_DECL(slow_path_is_object);
SLOW_PATH_HIDDEN_DECL(slow_path_is_function);
SLOW_PATH_HIDDEN_DECL(slow_path_in);
SLOW_PATH_HIDDEN_DECL(slow_path_del_by_val);
SLOW_PATH_HIDDEN_DECL(slow_path_strcat);
SLOW_PATH_HIDDEN_DECL(slow_path_to_primitive);

} // namespace JSC

#endif // ENABLE(JIT) || ENABLE(LLINT)

#endif // CommonSlowPaths_h
