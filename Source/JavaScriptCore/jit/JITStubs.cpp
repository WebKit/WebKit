/*
 * Copyright (C) 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 * Copyright (C) Research In Motion Limited 2010, 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(JIT)
#include "JITStubs.h"

#include "Arguments.h"
#include "ArrayConstructor.h"
#include "CallFrame.h"
#include "CallFrameInlines.h"
#include "CodeBlock.h"
#include "CodeProfiling.h"
#include "CommonSlowPaths.h"
#include "DFGCompilationMode.h"
#include "DFGDriver.h"
#include "DFGOSREntry.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "DeferGC.h"
#include "ErrorInstance.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "Heap.h"
#include <wtf/InlineASM.h>
#include "JIT.h"
#include "JITExceptions.h"
#include "JITToDFGDeferredCompilationCallback.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSNameScope.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LegacyProfiler.h"
#include "NameInstance.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include "Parser.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "RepatchBuffer.h"
#include "SamplingTool.h"
#include "SlowPathCall.h"
#include "Strong.h"
#include "StructureRareDataInlines.h"
#include <wtf/StdLibExtras.h>
#include <stdarg.h>
#include <stdio.h>

using namespace std;

#if CPU(ARM_TRADITIONAL)
#include "JITStubsARM.h"
#elif CPU(ARM_THUMB2)
#include "JITStubsARMv7.h"
#elif CPU(MIPS)
#include "JITStubsMIPS.h"
#elif CPU(SH4)
#include "JITStubsSH4.h"
#elif CPU(X86)
#include "JITStubsX86.h"
#elif CPU(X86_64)
#include "JITStubsX86_64.h"
#elif CPU(ARM64)
#include "JITStubsARM64.h"
#else
#error "JIT not supported on this platform."
#endif

namespace JSC {

#if ENABLE(OPCODE_SAMPLING)
    #define CTI_SAMPLER stackFrame.vm->interpreter->sampler()
#else
    #define CTI_SAMPLER 0
#endif

void performPlatformSpecificJITAssertions(VM* vm)
{
    if (!vm->canUseJIT())
        return;

#if CPU(ARM_THUMB2)
    performARMv7JITAssertions();
#elif CPU(ARM64)
    performARM64JITAssertions();
#elif CPU(ARM_TRADITIONAL)
    performARMJITAssertions();
#elif CPU(MIPS)
    performMIPSJITAssertions();
#elif CPU(SH4)
    performSH4JITAssertions();
#endif
}

#if !defined(NDEBUG)

extern "C" {

static void jscGeneratedNativeCode() 
{
    // When executing a JIT stub function (which might do an allocation), we hack the return address
    // to pretend to be executing this function, to keep stack logging tools from blowing out
    // memory.
}

}

struct StackHack {
    ALWAYS_INLINE StackHack(JITStackFrame& stackFrame) 
        : stackFrame(stackFrame)
        , savedReturnAddress(*stackFrame.returnAddressSlot())
    {
        if (!CodeProfiling::enabled())
            *stackFrame.returnAddressSlot() = ReturnAddressPtr(FunctionPtr(jscGeneratedNativeCode));
    }

    ALWAYS_INLINE ~StackHack() 
    { 
        *stackFrame.returnAddressSlot() = savedReturnAddress;
    }

    JITStackFrame& stackFrame;
    ReturnAddressPtr savedReturnAddress;
};

#define STUB_INIT_STACK_FRAME(stackFrame) JITStackFrame& stackFrame = *reinterpret_cast_ptr<JITStackFrame*>(STUB_ARGS); StackHack stackHack(stackFrame)
#define STUB_SET_RETURN_ADDRESS(returnAddress) stackHack.savedReturnAddress = ReturnAddressPtr(returnAddress)
#define STUB_RETURN_ADDRESS stackHack.savedReturnAddress

#else

#define STUB_INIT_STACK_FRAME(stackFrame) JITStackFrame& stackFrame = *reinterpret_cast_ptr<JITStackFrame*>(STUB_ARGS)
#define STUB_SET_RETURN_ADDRESS(returnAddress) *stackFrame.returnAddressSlot() = ReturnAddressPtr(returnAddress)
#define STUB_RETURN_ADDRESS *stackFrame.returnAddressSlot()

#endif

// The reason this is not inlined is to avoid having to do a PIC branch
// to get the address of the ctiVMThrowTrampoline function. It's also
// good to keep the code size down by leaving as much of the exception
// handling code out of line as possible.
static NEVER_INLINE void returnToThrowTrampoline(VM* vm, ReturnAddressPtr exceptionLocation, ReturnAddressPtr& returnAddressSlot)
{
    RELEASE_ASSERT(vm->exception());
    vm->exceptionLocation = exceptionLocation;
    returnAddressSlot = ReturnAddressPtr(FunctionPtr(ctiVMThrowTrampoline));
}

#define VM_THROW_EXCEPTION() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        return 0; \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    do {\
        returnToThrowTrampoline(stackFrame.vm, STUB_RETURN_ADDRESS, STUB_RETURN_ADDRESS);\
    } while (0)

#define CHECK_FOR_EXCEPTION() \
    do { \
        if (UNLIKELY(stackFrame.vm->exception())) \
            VM_THROW_EXCEPTION(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_AT_END() \
    do { \
        if (UNLIKELY(stackFrame.vm->exception())) \
            VM_THROW_EXCEPTION_AT_END(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_VOID() \
    do { \
        if (UNLIKELY(stackFrame.vm->exception())) { \
            VM_THROW_EXCEPTION_AT_END(); \
            return; \
        } \
    } while (0)

class ErrorFunctor {
public:
    virtual ~ErrorFunctor() { }
    virtual JSValue operator()(ExecState*) = 0;
};

class ErrorWithExecFunctor : public ErrorFunctor {
public:
    typedef JSObject* (*Factory)(ExecState* exec);
    
    ErrorWithExecFunctor(Factory factory)
        : m_factory(factory)
    {
    }

    virtual JSValue operator()(ExecState* exec) OVERRIDE
    {
        return m_factory(exec);
    }

private:
    Factory m_factory;
};

class ErrorWithExecAndCalleeFunctor : public ErrorFunctor {
public:
    typedef JSObject* (*Factory)(ExecState* exec, JSValue callee);

    ErrorWithExecAndCalleeFunctor(Factory factory, JSValue callee)
        : m_factory(factory), m_callee(callee)
    {
    }

    virtual JSValue operator()(ExecState* exec) OVERRIDE
    {
        return m_factory(exec, m_callee);
    }
private:
    Factory m_factory;
    JSValue m_callee;
};

// Helper function for JIT stubs that may throw an exception in the middle of
// processing a function call. This function rolls back the stack to
// our caller, so exception processing can proceed from a valid state.
template<typename T> static T throwExceptionFromOpCall(JITStackFrame& jitStackFrame, CallFrame* newCallFrame, ReturnAddressPtr& returnAddressSlot, ErrorFunctor* createError = 0)
{
    CallFrame* callFrame = newCallFrame->callerFrame()->removeHostCallFrameFlag();
    jitStackFrame.callFrame = callFrame;
    ASSERT(callFrame);
    callFrame->vm().topCallFrame = callFrame;
    if (createError)
        callFrame->vm().throwException(callFrame, (*createError)(callFrame));
    ASSERT(callFrame->vm().exception());
    returnToThrowTrampoline(&callFrame->vm(), ReturnAddressPtr(newCallFrame->returnPC()), returnAddressSlot);
    return T();
}

// If the CPU specific header does not provide an implementation, use the default one here.
#ifndef DEFINE_STUB_FUNCTION
#define DEFINE_STUB_FUNCTION(rtype, op) rtype JIT_STUB cti_##op(STUB_ARGS_DECLARATION)
#endif

// FIXME: This is still used by get_arguments_length, but other than that it's dead.
DEFINE_STUB_FUNCTION(EncodedJSValue, op_get_by_id_generic)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    Identifier& ident = stackFrame.args[1].identifier();

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

static JSValue getByVal(
    CallFrame* callFrame, JSValue baseValue, JSValue subscript, ReturnAddressPtr returnAddress)
{
    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        if (JSValue result = baseValue.asCell()->fastGetOwnProperty(callFrame, asString(subscript)->value(callFrame)))
            return result;
    }

    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i)) {
            ctiPatchCallByReturnAddress(callFrame->codeBlock(), returnAddress, FunctionPtr(cti_op_get_by_val_string));
            return asString(baseValue)->getIndex(callFrame, i);
        }
        return baseValue.get(callFrame, i);
    }

    if (isName(subscript))
        return baseValue.get(callFrame, jsCast<NameInstance*>(subscript.asCell())->privateName());

    Identifier property(callFrame, subscript.toString(callFrame)->value(callFrame));
    return baseValue.get(callFrame, property);
}

DEFINE_STUB_FUNCTION(EncodedJSValue, op_get_by_val)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    
    if (baseValue.isObject() && subscript.isInt32()) {
        // See if it's worth optimizing this at all.
        JSObject* object = asObject(baseValue);
        bool didOptimize = false;

        unsigned bytecodeOffset = callFrame->locationAsBytecodeOffset();
        ASSERT(bytecodeOffset);
        ByValInfo& byValInfo = callFrame->codeBlock()->getByValInfo(bytecodeOffset - 1);
        ASSERT(!byValInfo.stubRoutine);
        
        if (hasOptimizableIndexing(object->structure())) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(object->structure());
            if (arrayMode != byValInfo.arrayMode) {
                JIT::compileGetByVal(&callFrame->vm(), callFrame->codeBlock(), &byValInfo, STUB_RETURN_ADDRESS, arrayMode);
                didOptimize = true;
            }
        }
        
        if (!didOptimize) {
            // If we take slow path more than 10 times without patching then make sure we
            // never make that mistake again. Or, if we failed to patch and we have some object
            // that intercepts indexed get, then don't even wait until 10 times. For cases
            // where we see non-index-intercepting objects, this gives 10 iterations worth of
            // opportunity for us to observe that the get_by_val may be polymorphic.
            if (++byValInfo.slowPathCount >= 10
                || object->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
                // Don't ever try to optimize.
                RepatchBuffer repatchBuffer(callFrame->codeBlock());
                repatchBuffer.relinkCallerToFunction(STUB_RETURN_ADDRESS, FunctionPtr(cti_op_get_by_val_generic));
            }
        }
    }
    
    JSValue result = getByVal(callFrame, baseValue, subscript, STUB_RETURN_ADDRESS);
    CHECK_FOR_EXCEPTION();
    return JSValue::encode(result);
}
    
DEFINE_STUB_FUNCTION(EncodedJSValue, op_get_by_val_generic)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    
    JSValue result = getByVal(callFrame, baseValue, subscript, STUB_RETURN_ADDRESS);
    CHECK_FOR_EXCEPTION();
    return JSValue::encode(result);
}
    
DEFINE_STUB_FUNCTION(EncodedJSValue, op_get_by_val_string)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    
    CallFrame* callFrame = stackFrame.callFrame;
    
    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    
    JSValue result;
    
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            result = asString(baseValue)->getIndex(callFrame, i);
        else {
            result = baseValue.get(callFrame, i);
            if (!isJSString(baseValue))
                ctiPatchCallByReturnAddress(callFrame->codeBlock(), STUB_RETURN_ADDRESS, FunctionPtr(cti_op_get_by_val));
        }
    } else if (isName(subscript))
        result = baseValue.get(callFrame, jsCast<NameInstance*>(subscript.asCell())->privateName());
    else {
        Identifier property(callFrame, subscript.toString(callFrame)->value(callFrame));
        result = baseValue.get(callFrame, property);
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

static void putByVal(CallFrame* callFrame, JSValue baseValue, JSValue subscript, JSValue value)
{
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canSetIndexQuickly(i))
                object->setIndexQuickly(callFrame->vm(), i, value);
            else
                object->methodTable()->putByIndex(object, callFrame, i, value, callFrame->codeBlock()->isStrictMode());
        } else
            baseValue.putByIndex(callFrame, i, value, callFrame->codeBlock()->isStrictMode());
    } else if (isName(subscript)) {
        PutPropertySlot slot(callFrame->codeBlock()->isStrictMode());
        baseValue.put(callFrame, jsCast<NameInstance*>(subscript.asCell())->privateName(), value, slot);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame)->value(callFrame));
        if (!callFrame->vm().exception()) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot(callFrame->codeBlock()->isStrictMode());
            baseValue.put(callFrame, property, value, slot);
        }
    }
}

DEFINE_STUB_FUNCTION(void, op_put_by_val)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    JSValue value = stackFrame.args[2].jsValue();
    
    if (baseValue.isObject() && subscript.isInt32()) {
        // See if it's worth optimizing at all.
        JSObject* object = asObject(baseValue);
        bool didOptimize = false;

        unsigned bytecodeOffset = callFrame->locationAsBytecodeOffset();
        ASSERT(bytecodeOffset);
        ByValInfo& byValInfo = callFrame->codeBlock()->getByValInfo(bytecodeOffset - 1);
        ASSERT(!byValInfo.stubRoutine);
        
        if (hasOptimizableIndexing(object->structure())) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(object->structure());
            if (arrayMode != byValInfo.arrayMode) {
                JIT::compilePutByVal(&callFrame->vm(), callFrame->codeBlock(), &byValInfo, STUB_RETURN_ADDRESS, arrayMode);
                didOptimize = true;
            }
        }

        if (!didOptimize) {
            // If we take slow path more than 10 times without patching then make sure we
            // never make that mistake again. Or, if we failed to patch and we have some object
            // that intercepts indexed get, then don't even wait until 10 times. For cases
            // where we see non-index-intercepting objects, this gives 10 iterations worth of
            // opportunity for us to observe that the get_by_val may be polymorphic.
            if (++byValInfo.slowPathCount >= 10
                || object->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
                // Don't ever try to optimize.
                RepatchBuffer repatchBuffer(callFrame->codeBlock());
                repatchBuffer.relinkCallerToFunction(STUB_RETURN_ADDRESS, FunctionPtr(cti_op_put_by_val_generic));
            }
        }
    }
    
    putByVal(callFrame, baseValue, subscript, value);

    CHECK_FOR_EXCEPTION_AT_END();
}

DEFINE_STUB_FUNCTION(void, op_put_by_val_generic)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    JSValue value = stackFrame.args[2].jsValue();
    
    putByVal(callFrame, baseValue, subscript, value);

    CHECK_FOR_EXCEPTION_AT_END();
}

DEFINE_STUB_FUNCTION(void*, op_throw)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    stackFrame.vm->throwException(stackFrame.callFrame, stackFrame.args[0].jsValue()); 
    ExceptionHandler handler = genericUnwind(stackFrame.vm, stackFrame.callFrame, stackFrame.args[0].jsValue());
    STUB_SET_RETURN_ADDRESS(handler.catchRoutine);
    return handler.callFrame;
}

DEFINE_STUB_FUNCTION(void, op_throw_static_error)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    String message = errorDescriptionForValue(callFrame, stackFrame.args[0].jsValue())->value(callFrame);
    if (stackFrame.args[1].asInt32)
        stackFrame.vm->throwException(callFrame, createReferenceError(callFrame, message));
    else
        stackFrame.vm->throwException(callFrame, createTypeError(callFrame, message));
    VM_THROW_EXCEPTION_AT_END();
}

DEFINE_STUB_FUNCTION(void*, vm_throw)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    VM* vm = stackFrame.vm;
    ExceptionHandler handler = genericUnwind(vm, stackFrame.callFrame, vm->exception());
    STUB_SET_RETURN_ADDRESS(handler.catchRoutine);
    return handler.callFrame;
}

#if USE(JSVALUE32_64)
EncodedExceptionHandler JIT_STUB cti_vm_handle_exception(CallFrame* callFrame)
{
    ASSERT(!callFrame->hasHostCallFrameFlag());
    if (!callFrame) {
        // The entire stack has already been unwound. Nothing more to handle.
        return encode(uncaughtExceptionHandler());
    }

    VM* vm = callFrame->codeBlock()->vm();
    vm->topCallFrame = callFrame;
    return encode(genericUnwind(vm, callFrame, vm->exception()));
}
#else
ExceptionHandler JIT_STUB cti_vm_handle_exception(CallFrame* callFrame)
{
    ASSERT(!callFrame->hasHostCallFrameFlag());
    if (!callFrame) {
        // The entire stack has already been unwound. Nothing more to handle.
        return uncaughtExceptionHandler();
    }

    VM* vm = callFrame->codeBlock()->vm();
    vm->topCallFrame = callFrame;
    return genericUnwind(vm, callFrame, vm->exception());
}
#endif

} // namespace JSC

#endif // ENABLE(JIT)
