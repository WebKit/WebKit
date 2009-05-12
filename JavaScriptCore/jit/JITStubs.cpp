/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
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
#include "JITStubs.h"

#if ENABLE(JIT)

#include "Arguments.h"
#include "CallFrame.h"
#include "CodeBlock.h"
#include "Collector.h"
#include "Debugger.h"
#include "ExceptionHelpers.h"
#include "GlobalEvalFunction.h"
#include "JIT.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSByteArray.h"
#include "JSFunction.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSStaticScopeObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include "Parser.h"
#include "Profiler.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "SamplingTool.h"
#include <stdio.h>

using namespace std;

namespace JSC {


#if COMPILER(GCC) && PLATFORM(X86)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines below to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, callFrame) == 0x38, JITStackFrame_callFrame_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) == 0x30, JITStackFrame_code_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedEBX) == 0x1c, JITStackFrame_stub_argument_space_matches_ctiTrampoline);

#if PLATFORM(DARWIN)
#define SYMBOL_STRING(name) "_" #name
#else
#define SYMBOL_STRING(name) #name
#endif

asm(
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "pushl %ebp" "\n"
    "movl %esp, %ebp" "\n"
    "pushl %esi" "\n"
    "pushl %edi" "\n"
    "pushl %ebx" "\n"
    "subl $0x1c, %esp" "\n"
    "movl $512, %esi" "\n"
    "movl 0x38(%esp), %edi" "\n"
    "call *0x30(%esp)" "\n"
    "addl $0x1c, %esp" "\n"
    "popl %ebx" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
);

asm(
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
#if USE(JIT_STUB_ARGUMENT_VA_LIST)
    "call " SYMBOL_STRING(_ZN3JSC8JITStubs12cti_vm_throwEPvz) "\n"
#else
#if USE(JIT_STUB_ARGUMENT_REGISTER)
    "movl %esp, %ecx" "\n"
#else // JIT_STUB_ARGUMENT_STACK
    "movl %esp, 0(%esp)" "\n"
#endif
    "call " SYMBOL_STRING(_ZN3JSC8JITStubs12cti_vm_throwEPPv) "\n"
#endif
    "addl $0x1c, %esp" "\n"
    "popl %ebx" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
);
    
#elif COMPILER(GCC) && PLATFORM(X86_64)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines below to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, callFrame) == 0x90, JITStackFrame_callFrame_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) == 0x80, JITStackFrame_code_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedRBX) == 0x48, JITStackFrame_stub_argument_space_matches_ctiTrampoline);

#if PLATFORM(DARWIN)
#define SYMBOL_STRING(name) "_" #name
#else
#define SYMBOL_STRING(name) #name
#endif

asm(
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "pushq %rbp" "\n"
    "movq %rsp, %rbp" "\n"
    "pushq %r12" "\n"
    "pushq %r13" "\n"
    "pushq %r14" "\n"
    "pushq %r15" "\n"
    "pushq %rbx" "\n"
    "subq $0x48, %rsp" "\n"
    "movq $512, %r12" "\n"
    "movq $0xFFFF000000000000, %r14" "\n"
    "movq $0xFFFF000000000002, %r15" "\n"
    "movq 0x90(%rsp), %r13" "\n"
    "call *0x80(%rsp)" "\n"
    "addq $0x48, %rsp" "\n"
    "popq %rbx" "\n"
    "popq %r15" "\n"
    "popq %r14" "\n"
    "popq %r13" "\n"
    "popq %r12" "\n"
    "popq %rbp" "\n"
    "ret" "\n"
);

asm(
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
#if USE(JIT_STUB_ARGUMENT_REGISTER)
    "movq %rsp, %rdi" "\n"
    "call " SYMBOL_STRING(_ZN3JSC8JITStubs12cti_vm_throwEPPv) "\n"
#else // JIT_STUB_ARGUMENT_VA_LIST or JIT_STUB_ARGUMENT_STACK
#error "JIT_STUB_ARGUMENT configuration not supported."
#endif
    "addq $0x48, %rsp" "\n"
    "popq %rbx" "\n"
    "popq %r15" "\n"
    "popq %r14" "\n"
    "popq %r13" "\n"
    "popq %r12" "\n"
    "popq %rbp" "\n"
    "ret" "\n"
);

#elif COMPILER(MSVC)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines below to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, callFrame) == 0x38, JITStackFrame_callFrame_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) == 0x30, JITStackFrame_code_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedEBX) == 0x1c, JITStackFrame_stub_argument_space_matches_ctiTrampoline);

extern "C" {
    
    __declspec(naked) EncodedJSValue ctiTrampoline(void* code, RegisterFile*, CallFrame*, JSValue* exception, Profiler**, JSGlobalData*)
    {
        __asm {
            push ebp;
            mov ebp, esp;
            push esi;
            push edi;
            push ebx;
            sub esp, 0x1c;
            mov esi, 512;
            mov ecx, esp;
            mov edi, [esp + 0x38];
            call [esp + 0x30];
            add esp, 0x1c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            ret;
        }
    }
    
    __declspec(naked) void ctiVMThrowTrampoline()
    {
        __asm {
#if USE(JIT_STUB_ARGUMENT_REGISTER)
            mov ecx, esp;
#else // JIT_STUB_ARGUMENT_VA_LIST or JIT_STUB_ARGUMENT_STACK
#error "JIT_STUB_ARGUMENT configuration not supported."
#endif
            call JSC::JITStubs::cti_vm_throw;
            add esp, 0x1c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            ret;
        }
    }
    
}

#endif

#if ENABLE(OPCODE_SAMPLING)
    #define CTI_SAMPLER stackFrame.globalData->interpreter->sampler()
#else
    #define CTI_SAMPLER 0
#endif

JITStubs::JITStubs(JSGlobalData* globalData)
    : m_ctiArrayLengthTrampoline(0)
    , m_ctiStringLengthTrampoline(0)
    , m_ctiVirtualCallPreLink(0)
    , m_ctiVirtualCallLink(0)
    , m_ctiVirtualCall(0)
    , m_ctiNativeCallThunk(0)
{
    JIT::compileCTIMachineTrampolines(globalData, &m_executablePool, &m_ctiArrayLengthTrampoline, &m_ctiStringLengthTrampoline, &m_ctiVirtualCallPreLink, &m_ctiVirtualCallLink, &m_ctiVirtualCall, &m_ctiNativeCallThunk);
}

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

NEVER_INLINE void JITStubs::tryCachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValue baseValue, const PutPropertySlot& slot)
{
    // The interpreter checks for recursion here; I do not believe this can occur in CTI.

    if (!baseValue.isCell())
        return;

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(JITStubs::cti_op_put_by_id_generic));
        return;
    }
    
    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isDictionary()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(JITStubs::cti_op_put_by_id_generic));
        return;
    }

    // If baseCell != base, then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(JITStubs::cti_op_put_by_id_generic));
        return;
    }

    StructureStubInfo* stubInfo = &codeBlock->getStubInfo(returnAddress);

    // Cache hit: Specialize instruction and ref Structures.

    // Structure transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        StructureChain* prototypeChain = structure->prototypeChain(callFrame);
        stubInfo->initPutByIdTransition(structure->previousID(), structure, prototypeChain);
        JIT::compilePutByIdTransition(callFrame->scopeChain()->globalData, codeBlock, stubInfo, structure->previousID(), structure, slot.cachedOffset(), prototypeChain, returnAddress);
        return;
    }
    
    stubInfo->initPutByIdReplace(structure);

    JIT::patchPutByIdReplace(stubInfo, structure, slot.cachedOffset(), returnAddress);
}

NEVER_INLINE void JITStubs::tryCacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(JITStubs::cti_op_get_by_id_generic));
        return;
    }
    
    JSGlobalData* globalData = &callFrame->globalData();

    if (isJSArray(globalData, baseValue) && propertyName == callFrame->propertyNames().length) {
        JIT::compilePatchGetArrayLength(callFrame->scopeChain()->globalData, codeBlock, returnAddress);
        return;
    }
    
    if (isJSString(globalData, baseValue) && propertyName == callFrame->propertyNames().length) {
        // The tradeoff of compiling an patched inline string length access routine does not seem
        // to pay off, so we currently only do this for arrays.
        ctiPatchCallByReturnAddress(returnAddress, globalData->jitStubs.ctiStringLengthTrampoline());
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(JITStubs::cti_op_get_by_id_generic));
        return;
    }

    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isDictionary()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(JITStubs::cti_op_get_by_id_generic));
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    StructureStubInfo* stubInfo = &codeBlock->getStubInfo(returnAddress);

    // Cache hit: Specialize instruction and ref Structures.

    if (slot.slotBase() == baseValue) {
        // set this up, so derefStructures can do it's job.
        stubInfo->initGetByIdSelf(structure);

        JIT::patchGetByIdSelf(stubInfo, structure, slot.cachedOffset(), returnAddress);
        return;
    }

    if (slot.slotBase() == structure->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase().isObject());

        JSObject* slotBaseObject = asObject(slot.slotBase());

        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (slotBaseObject->structure()->isDictionary())
            slotBaseObject->setStructure(Structure::fromDictionaryTransition(slotBaseObject->structure()));
        
        stubInfo->initGetByIdProto(structure, slotBaseObject->structure());

        JIT::compileGetByIdProto(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, slotBaseObject->structure(), slot.cachedOffset(), returnAddress);
        return;
    }

    size_t count = countPrototypeChainEntriesAndCheckForProxies(callFrame, baseValue, slot);
    if (!count) {
        stubInfo->opcodeID = op_get_by_id_generic;
        return;
    }

    StructureChain* prototypeChain = structure->prototypeChain(callFrame);
    stubInfo->initGetByIdChain(structure, prototypeChain);
    JIT::compileGetByIdChain(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, prototypeChain, count, slot.cachedOffset(), returnAddress);
}

#endif

#if USE(JIT_STUB_ARGUMENT_VA_LIST)
#define SETUP_VA_LISTL_ARGS va_list vl_args; va_start(vl_args, args)
#else // JIT_STUB_ARGUMENT_REGISTER or JIT_STUB_ARGUMENT_STACK
#define SETUP_VA_LISTL_ARGS
#endif

#ifndef NDEBUG

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
    {
        savedReturnAddress = *stackFrame.returnAddressSlot();
        *stackFrame.returnAddressSlot() = reinterpret_cast<void*>(jscGeneratedNativeCode);
    }

    ALWAYS_INLINE ~StackHack() 
    { 
        *stackFrame.returnAddressSlot() = savedReturnAddress;
    }

    JITStackFrame& stackFrame;
    void* savedReturnAddress;
};

#define STUB_INIT_STACK_FRAME(stackFrame) SETUP_VA_LISTL_ARGS; JITStackFrame& stackFrame = *reinterpret_cast<JITStackFrame*>(STUB_ARGS); StackHack stackHack(stackFrame);
#define STUB_SET_RETURN_ADDRESS(returnAddress) stackHack.savedReturnAddress = returnAddress
#define STUB_RETURN_ADDRESS stackHack.savedReturnAddress

#else

#define STUB_INIT_STACK_FRAME(stackFrame) SETUP_VA_LISTL_ARGS; JITStackFrame& stackFrame = *reinterpret_cast<JITStackFrame*>(STUB_ARGS);
#define STUB_SET_RETURN_ADDRESS(returnAddress) *stackFrame.returnAddressSlot() = returnAddress;
#define STUB_RETURN_ADDRESS *stackFrame.returnAddressSlot()

#endif

// The reason this is not inlined is to avoid having to do a PIC branch
// to get the address of the ctiVMThrowTrampoline function. It's also
// good to keep the code size down by leaving as much of the exception
// handling code out of line as possible.
static NEVER_INLINE void returnToThrowTrampoline(JSGlobalData* globalData, void* exceptionLocation, void*& returnAddressSlot)
{
    ASSERT(globalData->exception);
    globalData->exceptionLocation = exceptionLocation;
    returnAddressSlot = reinterpret_cast<void*>(ctiVMThrowTrampoline);
}

static NEVER_INLINE void throwStackOverflowError(CallFrame* callFrame, JSGlobalData* globalData, void* exceptionLocation, void*& returnAddressSlot)
{
    globalData->exception = createStackOverflowError(callFrame);
    returnToThrowTrampoline(globalData, exceptionLocation, returnAddressSlot);
}

#define VM_THROW_EXCEPTION() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        return 0; \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    returnToThrowTrampoline(stackFrame.globalData, STUB_RETURN_ADDRESS, STUB_RETURN_ADDRESS)

#define CHECK_FOR_EXCEPTION() \
    do { \
        if (UNLIKELY(stackFrame.globalData->exception != JSValue())) \
            VM_THROW_EXCEPTION(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_AT_END() \
    do { \
        if (UNLIKELY(stackFrame.globalData->exception != JSValue())) \
            VM_THROW_EXCEPTION_AT_END(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_VOID() \
    do { \
        if (UNLIKELY(stackFrame.globalData->exception != JSValue())) { \
            VM_THROW_EXCEPTION_AT_END(); \
            return; \
        } \
    } while (0)


JSObject* JITStubs::cti_op_convert_this(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v1 = stackFrame.args[0].jsValue();
    CallFrame* callFrame = stackFrame.callFrame;

    JSObject* result = v1.toThisObject(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

void JITStubs::cti_op_end(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ScopeChainNode* scopeChain = stackFrame.callFrame->scopeChain();
    ASSERT(scopeChain->refCount > 1);
    scopeChain->deref();
}

EncodedJSValue JITStubs::cti_op_add(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v1 = stackFrame.args[0].jsValue();
    JSValue v2 = stackFrame.args[1].jsValue();

    double left;
    double right = 0.0;

    bool rightIsNumber = v2.getNumber(right);
    if (rightIsNumber && v1.getNumber(left))
        return JSValue::encode(jsNumber(stackFrame.globalData, left + right));
    
    CallFrame* callFrame = stackFrame.callFrame;

    bool leftIsString = v1.isString();
    if (leftIsString && v2.isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }

        return JSValue::encode(jsString(stackFrame.globalData, value.release()));
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = v2.isInt32Fast() ?
            concatenate(asString(v1)->value().rep(), v2.getInt32Fast()) :
            concatenate(asString(v1)->value().rep(), right);

        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }
        return JSValue::encode(jsString(stackFrame.globalData, value.release()));
    }

    // All other cases are pretty uncommon
    JSValue result = jsAddSlowCase(callFrame, v1, v2);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_pre_inc(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, v.toNumber(callFrame) + 1);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_timeout_check(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    
    JSGlobalData* globalData = stackFrame.globalData;
    TimeoutChecker& timeoutChecker = globalData->timeoutChecker;

    if (timeoutChecker.didTimeOut(stackFrame.callFrame)) {
        globalData->exception = createInterruptedExecutionException(globalData);
        VM_THROW_EXCEPTION_AT_END();
    }
    
    return timeoutChecker.ticksUntilNextCheck();
}

void JITStubs::cti_register_file_check(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    if (LIKELY(stackFrame.registerFile->grow(stackFrame.callFrame + stackFrame.callFrame->codeBlock()->m_numCalleeRegisters)))
        return;

    // Rewind to the previous call frame because op_call already optimistically
    // moved the call frame forward.
    CallFrame* oldCallFrame = stackFrame.callFrame->callerFrame();
    stackFrame.callFrame = oldCallFrame;
    throwStackOverflowError(oldCallFrame, stackFrame.globalData, oldCallFrame->returnPC(), STUB_RETURN_ADDRESS);
}

int JITStubs::cti_op_loop_if_less(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();
    CallFrame* callFrame = stackFrame.callFrame;

    bool result = jsLess(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

int JITStubs::cti_op_loop_if_lesseq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();
    CallFrame* callFrame = stackFrame.callFrame;

    bool result = jsLessEq(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

JSObject* JITStubs::cti_op_new_object(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return constructEmptyObject(stackFrame.callFrame);
}

void JITStubs::cti_op_put_by_id_generic(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    PutPropertySlot slot;
    stackFrame.args[0].jsValue().put(stackFrame.callFrame, stackFrame.args[1].identifier(), stackFrame.args[2].jsValue(), slot);
    CHECK_FOR_EXCEPTION_AT_END();
}

EncodedJSValue JITStubs::cti_op_get_by_id_generic(STUB_ARGS_DECLARATION)
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

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

void JITStubs::cti_op_put_by_id(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    Identifier& ident = stackFrame.args[1].identifier();

    PutPropertySlot slot;
    stackFrame.args[0].jsValue().put(callFrame, ident, stackFrame.args[2].jsValue(), slot);

    ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_id_second));

    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_id_second(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    PutPropertySlot slot;
    stackFrame.args[0].jsValue().put(stackFrame.callFrame, stackFrame.args[1].identifier(), stackFrame.args[2].jsValue(), slot);
    tryCachePutByID(stackFrame.callFrame, stackFrame.callFrame->codeBlock(), STUB_RETURN_ADDRESS, stackFrame.args[0].jsValue(), slot);
    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_id_fail(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    Identifier& ident = stackFrame.args[1].identifier();

    PutPropertySlot slot;
    stackFrame.args[0].jsValue().put(callFrame, ident, stackFrame.args[2].jsValue(), slot);

    CHECK_FOR_EXCEPTION_AT_END();
}

EncodedJSValue JITStubs::cti_op_get_by_id(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    Identifier& ident = stackFrame.args[1].identifier();

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_second));

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_second(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    Identifier& ident = stackFrame.args[1].identifier();

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    tryCacheGetByID(callFrame, callFrame->codeBlock(), STUB_RETURN_ADDRESS, baseValue, ident, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_self_fail(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    Identifier& ident = stackFrame.args[1].identifier();

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    CHECK_FOR_EXCEPTION();

    if (baseValue.isCell()
        && slot.isCacheable()
        && !asCell(baseValue)->structure()->isDictionary()
        && slot.slotBase() == baseValue) {

        CodeBlock* codeBlock = callFrame->codeBlock();
        StructureStubInfo* stubInfo = &codeBlock->getStubInfo(STUB_RETURN_ADDRESS);

        ASSERT(slot.slotBase().isObject());

        PolymorphicAccessStructureList* polymorphicStructureList;
        int listIndex = 1;

        if (stubInfo->opcodeID == op_get_by_id_self) {
            ASSERT(!stubInfo->stubRoutine);
            polymorphicStructureList = new PolymorphicAccessStructureList(MacroAssembler::CodeLocationLabel(), stubInfo->u.getByIdSelf.baseObjectStructure);
            stubInfo->initGetByIdSelfList(polymorphicStructureList, 2);
        } else {
            polymorphicStructureList = stubInfo->u.getByIdSelfList.structureList;
            listIndex = stubInfo->u.getByIdSelfList.listSize;
            stubInfo->u.getByIdSelfList.listSize++;
        }

        JIT::compileGetByIdSelfList(callFrame->scopeChain()->globalData, codeBlock, stubInfo, polymorphicStructureList, listIndex, asCell(baseValue)->structure(), slot.cachedOffset());

        if (listIndex == (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_generic));
    } else {
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_generic));
    }
    return JSValue::encode(result);
}

static PolymorphicAccessStructureList* getPolymorphicAccessStructureListSlot(StructureStubInfo* stubInfo, int& listIndex)
{
    PolymorphicAccessStructureList* prototypeStructureList = 0;
    listIndex = 1;

    switch (stubInfo->opcodeID) {
    case op_get_by_id_proto:
        prototypeStructureList = new PolymorphicAccessStructureList(stubInfo->stubRoutine, stubInfo->u.getByIdProto.baseObjectStructure, stubInfo->u.getByIdProto.prototypeStructure);
        stubInfo->stubRoutine.reset();
        stubInfo->initGetByIdProtoList(prototypeStructureList, 2);
        break;
    case op_get_by_id_chain:
        prototypeStructureList = new PolymorphicAccessStructureList(stubInfo->stubRoutine, stubInfo->u.getByIdChain.baseObjectStructure, stubInfo->u.getByIdChain.chain);
        stubInfo->stubRoutine.reset();
        stubInfo->initGetByIdProtoList(prototypeStructureList, 2);
        break;
    case op_get_by_id_proto_list:
        prototypeStructureList = stubInfo->u.getByIdProtoList.structureList;
        listIndex = stubInfo->u.getByIdProtoList.listSize;
        stubInfo->u.getByIdProtoList.listSize++;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    ASSERT(listIndex < POLYMORPHIC_LIST_CACHE_SIZE);
    return prototypeStructureList;
}

EncodedJSValue JITStubs::cti_op_get_by_id_proto_list(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, stackFrame.args[1].identifier(), slot);

    CHECK_FOR_EXCEPTION();

    if (!baseValue.isCell() || !slot.isCacheable() || asCell(baseValue)->structure()->isDictionary()) {
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_fail));
        return JSValue::encode(result);
    }

    Structure* structure = asCell(baseValue)->structure();
    CodeBlock* codeBlock = callFrame->codeBlock();
    StructureStubInfo* stubInfo = &codeBlock->getStubInfo(STUB_RETURN_ADDRESS);

    ASSERT(slot.slotBase().isObject());
    JSObject* slotBaseObject = asObject(slot.slotBase());

    if (slot.slotBase() == baseValue)
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_fail));
    else if (slot.slotBase() == asCell(baseValue)->structure()->prototypeForLookup(callFrame)) {
        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (slotBaseObject->structure()->isDictionary())
            slotBaseObject->setStructure(Structure::fromDictionaryTransition(slotBaseObject->structure()));

        int listIndex;
        PolymorphicAccessStructureList* prototypeStructureList = getPolymorphicAccessStructureListSlot(stubInfo, listIndex);

        JIT::compileGetByIdProtoList(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, prototypeStructureList, listIndex, structure, slotBaseObject->structure(), slot.cachedOffset());

        if (listIndex == (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_list_full));
    } else if (size_t count = countPrototypeChainEntriesAndCheckForProxies(callFrame, baseValue, slot)) {
        int listIndex;
        PolymorphicAccessStructureList* prototypeStructureList = getPolymorphicAccessStructureListSlot(stubInfo, listIndex);
        JIT::compileGetByIdChainList(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, prototypeStructureList, listIndex, structure, structure->prototypeChain(callFrame), count, slot.cachedOffset());

        if (listIndex == (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_list_full));
    } else
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_fail));

    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_proto_list_full(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(stackFrame.callFrame, stackFrame.args[1].identifier(), slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_proto_fail(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(stackFrame.callFrame, stackFrame.args[1].identifier(), slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_array_fail(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(stackFrame.callFrame, stackFrame.args[1].identifier(), slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_string_fail(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue baseValue = stackFrame.args[0].jsValue();
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(stackFrame.callFrame, stackFrame.args[1].identifier(), slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

#endif

EncodedJSValue JITStubs::cti_op_instanceof(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue value = stackFrame.args[0].jsValue();
    JSValue baseVal = stackFrame.args[1].jsValue();
    JSValue proto = stackFrame.args[2].jsValue();

    // at least one of these checks must have failed to get to the slow case
    ASSERT(!value.isCell() || !baseVal.isCell() || !proto.isCell()
           || !value.isObject() || !baseVal.isObject() || !proto.isObject() 
           || (asObject(baseVal)->structure()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);

    if (!baseVal.isObject()) {
        CallFrame* callFrame = stackFrame.callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        stackFrame.globalData->exception = createInvalidParamError(callFrame, "instanceof", baseVal, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSObject* baseObj = asObject(baseVal);
    TypeInfo typeInfo = baseObj->structure()->typeInfo();
    if (!typeInfo.implementsHasInstance())
        return JSValue::encode(jsBoolean(false));

    if (!typeInfo.overridesHasInstance()) {
        if (!value.isObject())
            return JSValue::encode(jsBoolean(false));

        if (!proto.isObject()) {
            throwError(callFrame, TypeError, "instanceof called on an object with an invalid prototype property.");
            VM_THROW_EXCEPTION();
        }
    }

    JSValue result = jsBoolean(baseObj->hasInstance(callFrame, value, proto));
    CHECK_FOR_EXCEPTION_AT_END();

    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_del_by_id(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    
    JSObject* baseObj = stackFrame.args[0].jsValue().toObject(callFrame);

    JSValue result = jsBoolean(baseObj->deleteProperty(callFrame, stackFrame.args[1].identifier()));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_mul(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValue::encode(jsNumber(stackFrame.globalData, left * right));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, src1.toNumber(callFrame) * src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

JSObject* JITStubs::cti_op_new_func(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return stackFrame.args[0].funcDeclNode()->makeFunction(stackFrame.callFrame, stackFrame.callFrame->scopeChain());
}

void* JITStubs::cti_op_call_JSFunction(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

#ifndef NDEBUG
    CallData callData;
    ASSERT(stackFrame.args[0].jsValue().getCallData(callData) == CallTypeJS);
#endif

    JSFunction* function = asFunction(stackFrame.args[0].jsValue());
    FunctionBodyNode* body = function->body();
    ScopeChainNode* callDataScopeChain = function->scope().node();
    body->jitCode(callDataScopeChain);

    return &(body->generatedBytecode());
}

VoidPtrPair JITStubs::cti_op_call_arityCheck(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* newCodeBlock = stackFrame.args[3].codeBlock();
    int argCount = stackFrame.args[2].int32();

    ASSERT(argCount != newCodeBlock->m_numParameters);

    CallFrame* oldCallFrame = callFrame->callerFrame();

    if (argCount > newCodeBlock->m_numParameters) {
        size_t numParameters = newCodeBlock->m_numParameters;
        Register* r = callFrame->registers() + numParameters;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argCount;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argCount] = argv[i];

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    } else {
        size_t omittedArgCount = newCodeBlock->m_numParameters - argCount;
        Register* r = callFrame->registers() + omittedArgCount;
        Register* newEnd = r + newCodeBlock->m_numCalleeRegisters;
        if (!stackFrame.registerFile->grow(newEnd)) {
            // Rewind to the previous call frame because op_call already optimistically
            // moved the call frame forward.
            stackFrame.callFrame = oldCallFrame;
            throwStackOverflowError(oldCallFrame, stackFrame.globalData, stackFrame.args[1].returnAddress(), STUB_RETURN_ADDRESS);
            RETURN_POINTER_PAIR(0, 0);
        }

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    }

    RETURN_POINTER_PAIR(newCodeBlock, callFrame);
}

void* JITStubs::cti_vm_dontLazyLinkCall(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSGlobalData* globalData = stackFrame.globalData;
    JSFunction* callee = asFunction(stackFrame.args[0].jsValue());
    JITCode jitCode = callee->body()->generatedJITCode();
    ASSERT(jitCode);

    ctiPatchNearCallByReturnAddress(stackFrame.args[1].returnAddress(), globalData->jitStubs.ctiVirtualCallLink());

    return jitCode.addressForCall();
}

void* JITStubs::cti_vm_lazyLinkCall(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSFunction* callee = asFunction(stackFrame.args[0].jsValue());
    JITCode jitCode = callee->body()->generatedJITCode();
    ASSERT(jitCode);
    
    CodeBlock* codeBlock = 0;
    if (!callee->isHostFunction())
        codeBlock = &callee->body()->bytecode(callee->scope().node());

    CallLinkInfo* callLinkInfo = &stackFrame.callFrame->callerFrame()->codeBlock()->getCallLinkInfo(stackFrame.args[1].returnAddress());
    JIT::linkCall(callee, codeBlock, jitCode, callLinkInfo, stackFrame.args[2].int32());

    return jitCode.addressForCall();
}

JSObject* JITStubs::cti_op_push_activation(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSActivation* activation = new (stackFrame.globalData) JSActivation(stackFrame.callFrame, static_cast<FunctionBodyNode*>(stackFrame.callFrame->codeBlock()->ownerNode()));
    stackFrame.callFrame->setScopeChain(stackFrame.callFrame->scopeChain()->copy()->push(activation));
    return activation;
}

EncodedJSValue JITStubs::cti_op_call_NotJSFunction(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue funcVal = stackFrame.args[0].jsValue();

    CallData callData;
    CallType callType = funcVal.getCallData(callData);

    ASSERT(callType != CallTypeJS);

    if (callType == CallTypeHost) {
        int registerOffset = stackFrame.args[1].int32();
        int argCount = stackFrame.args[2].int32();
        CallFrame* previousCallFrame = stackFrame.callFrame;
        CallFrame* callFrame = CallFrame::create(previousCallFrame->registers() + registerOffset);

        callFrame->init(0, static_cast<Instruction*>(STUB_RETURN_ADDRESS), previousCallFrame->scopeChain(), previousCallFrame, 0, argCount, 0);
        stackFrame.callFrame = callFrame;

        Register* argv = stackFrame.callFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
        ArgList argList(argv + 1, argCount - 1);

        JSValue returnValue;
        {
            SamplingTool::HostCallRecord callRecord(CTI_SAMPLER);

            // FIXME: All host methods should be calling toThisObject, but this is not presently the case.
            JSValue thisValue = argv[0].jsValue();
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();

            returnValue = callData.native.function(callFrame, asObject(funcVal), thisValue, argList);
        }
        stackFrame.callFrame = previousCallFrame;
        CHECK_FOR_EXCEPTION();

        return JSValue::encode(returnValue);
    }

    ASSERT(callType == CallTypeNone);

    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createNotAFunctionError(stackFrame.callFrame, funcVal, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

void JITStubs::cti_op_create_arguments(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    Arguments* arguments = new (stackFrame.globalData) Arguments(stackFrame.callFrame);
    stackFrame.callFrame->setCalleeArguments(arguments);
    stackFrame.callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void JITStubs::cti_op_create_arguments_no_params(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    Arguments* arguments = new (stackFrame.globalData) Arguments(stackFrame.callFrame, Arguments::NoParameters);
    stackFrame.callFrame->setCalleeArguments(arguments);
    stackFrame.callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void JITStubs::cti_op_tear_off_activation(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ASSERT(stackFrame.callFrame->codeBlock()->needsFullScopeChain());
    asActivation(stackFrame.args[0].jsValue())->copyRegisters(stackFrame.callFrame->optionalCalleeArguments());
}

void JITStubs::cti_op_tear_off_arguments(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ASSERT(stackFrame.callFrame->codeBlock()->usesArguments() && !stackFrame.callFrame->codeBlock()->needsFullScopeChain());
    stackFrame.callFrame->optionalCalleeArguments()->copyRegisters();
}

void JITStubs::cti_op_profile_will_call(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ASSERT(*stackFrame.enabledProfilerReference);
    (*stackFrame.enabledProfilerReference)->willExecute(stackFrame.callFrame, stackFrame.args[0].jsValue());
}

void JITStubs::cti_op_profile_did_call(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ASSERT(*stackFrame.enabledProfilerReference);
    (*stackFrame.enabledProfilerReference)->didExecute(stackFrame.callFrame, stackFrame.args[0].jsValue());
}

void JITStubs::cti_op_ret_scopeChain(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ASSERT(stackFrame.callFrame->codeBlock()->needsFullScopeChain());
    stackFrame.callFrame->scopeChain()->deref();
}

JSObject* JITStubs::cti_op_new_array(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    ArgList argList(&stackFrame.callFrame->registers()[stackFrame.args[0].int32()], stackFrame.args[1].int32());
    return constructArray(stackFrame.callFrame, argList);
}

EncodedJSValue JITStubs::cti_op_resolve(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    Identifier& ident = stackFrame.args[0].identifier();
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();
            return JSValue::encode(result);
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSObject* JITStubs::cti_op_construct_JSConstruct(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSFunction* constructor = asFunction(stackFrame.args[0].jsValue());
    if (constructor->isHostFunction()) {
        CallFrame* callFrame = stackFrame.callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        stackFrame.globalData->exception = createNotAConstructorError(callFrame, constructor, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(constructor->getConstructData(constructData) == ConstructTypeJS);
#endif

    Structure* structure;
    if (stackFrame.args[3].jsValue().isObject())
        structure = asObject(stackFrame.args[3].jsValue())->inheritorID();
    else
        structure = constructor->scope().node()->globalObject()->emptyObjectStructure();
    return new (stackFrame.globalData) JSObject(structure);
}

EncodedJSValue JITStubs::cti_op_construct_NotJSConstruct(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue constrVal = stackFrame.args[0].jsValue();
    int argCount = stackFrame.args[2].int32();
    int thisRegister = stackFrame.args[4].int32();

    ConstructData constructData;
    ConstructType constructType = constrVal.getConstructData(constructData);

    if (constructType == ConstructTypeHost) {
        ArgList argList(callFrame->registers() + thisRegister + 1, argCount - 1);

        JSValue returnValue;
        {
            SamplingTool::HostCallRecord callRecord(CTI_SAMPLER);
            returnValue = constructData.native.function(callFrame, asObject(constrVal), argList);
        }
        CHECK_FOR_EXCEPTION();

        return JSValue::encode(returnValue);
    }

    ASSERT(constructType == ConstructTypeNone);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createNotAConstructorError(callFrame, constrVal, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

EncodedJSValue JITStubs::cti_op_get_by_val(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSGlobalData* globalData = stackFrame.globalData;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();

    JSValue result;

    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (isJSArray(globalData, baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canGetIndex(i))
                result = jsArray->getIndex(i);
            else
                result = jsArray->JSArray::get(callFrame, i);
        } else if (isJSString(globalData, baseValue) && asString(baseValue)->canGetIndex(i)) {
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_val_string));
            result = asString(baseValue)->getIndex(stackFrame.globalData, i);
        } else if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_val_byte_array));
            return JSValue::encode(asByteArray(baseValue)->getIndex(callFrame, i));
        } else
            result = baseValue.get(callFrame, i);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        result = baseValue.get(callFrame, property);
    }

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}
    
EncodedJSValue JITStubs::cti_op_get_by_val_string(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    
    CallFrame* callFrame = stackFrame.callFrame;
    JSGlobalData* globalData = stackFrame.globalData;
    
    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    
    JSValue result;
    
    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (isJSString(globalData, baseValue) && asString(baseValue)->canGetIndex(i))
            result = asString(baseValue)->getIndex(stackFrame.globalData, i);
        else {
            result = baseValue.get(callFrame, i);
            if (!isJSString(globalData, baseValue))
                ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_val));
        }
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        result = baseValue.get(callFrame, property);
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}
    

EncodedJSValue JITStubs::cti_op_get_by_val_byte_array(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    
    CallFrame* callFrame = stackFrame.callFrame;
    JSGlobalData* globalData = stackFrame.globalData;
    
    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    
    JSValue result;

    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            return JSValue::encode(asByteArray(baseValue)->getIndex(callFrame, i));
        }

        result = baseValue.get(callFrame, i);
        if (!isJSByteArray(globalData, baseValue))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_val));
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        result = baseValue.get(callFrame, property);
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_resolve_func(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = stackFrame.args[0].identifier();
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {            
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(callFrame);
            JSValue result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();

            callFrame->registers()[stackFrame.args[1].int32()] = JSValue(thisObj);
            return JSValue::encode(result);
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_AT_END();
    return JSValue::encode(JSValue());
}

EncodedJSValue JITStubs::cti_op_sub(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValue::encode(jsNumber(stackFrame.globalData, left - right));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, src1.toNumber(callFrame) - src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

void JITStubs::cti_op_put_by_val(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSGlobalData* globalData = stackFrame.globalData;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    JSValue value = stackFrame.args[2].jsValue();

    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (isJSArray(globalData, baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canSetIndex(i))
                jsArray->setIndex(i, value);
            else
                jsArray->JSArray::put(callFrame, i, value);
        } else if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            JSByteArray* jsByteArray = asByteArray(baseValue);
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_val_byte_array));
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            if (value.isInt32Fast()) {
                jsByteArray->setIndex(i, value.getInt32Fast());
                return;
            } else {
                double dValue = 0;
                if (value.getNumber(dValue)) {
                    jsByteArray->setIndex(i, dValue);
                    return;
                }
            }

            baseValue.put(callFrame, i, value);
        } else
            baseValue.put(callFrame, i, value);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        if (!stackFrame.globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }

    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_val_array(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue baseValue = stackFrame.args[0].jsValue();
    int i = stackFrame.args[1].int32();
    JSValue value = stackFrame.args[2].jsValue();

    ASSERT(isJSArray(stackFrame.globalData, baseValue));

    if (LIKELY(i >= 0))
        asArray(baseValue)->JSArray::put(callFrame, i, value);
    else {
        // This should work since we're re-boxing an immediate unboxed in JIT code.
        ASSERT(JSValue::makeInt32Fast(i));
        Identifier property(callFrame, JSValue::makeInt32Fast(i).toString(callFrame));
        // FIXME: can toString throw an exception here?
        if (!stackFrame.globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }

    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_val_byte_array(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    
    CallFrame* callFrame = stackFrame.callFrame;
    JSGlobalData* globalData = stackFrame.globalData;
    
    JSValue baseValue = stackFrame.args[0].jsValue();
    JSValue subscript = stackFrame.args[1].jsValue();
    JSValue value = stackFrame.args[2].jsValue();
    
    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            JSByteArray* jsByteArray = asByteArray(baseValue);
            
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            if (value.isInt32Fast()) {
                jsByteArray->setIndex(i, value.getInt32Fast());
                return;
            } else {
                double dValue = 0;                
                if (value.getNumber(dValue)) {
                    jsByteArray->setIndex(i, dValue);
                    return;
                }
            }
        }

        if (!isJSByteArray(globalData, baseValue))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_val));
        baseValue.put(callFrame, i, value);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        if (!stackFrame.globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
}

EncodedJSValue JITStubs::cti_op_lesseq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsBoolean(jsLessEq(callFrame, stackFrame.args[0].jsValue(), stackFrame.args[1].jsValue()));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_op_loop_if_true(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    bool result = src1.toBoolean(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}
    
int JITStubs::cti_op_load_varargs(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);
    CallFrame* callFrame = stackFrame.callFrame;
    RegisterFile* registerFile = stackFrame.registerFile;
    int argsOffset = stackFrame.args[0].int32();
    JSValue arguments = callFrame[argsOffset].jsValue();
    uint32_t argCount = 0;
    if (!arguments.isUndefinedOrNull()) {
        if (!arguments.isObject()) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
            stackFrame.globalData->exception = createInvalidParamError(callFrame, "Function.prototype.apply", arguments, vPCIndex, codeBlock);
            VM_THROW_EXCEPTION();
        }
        if (asObject(arguments)->classInfo() == &Arguments::info) {
            Arguments* argsObject = asArguments(arguments);
            argCount = argsObject->numProvidedArguments(callFrame);
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                stackFrame.globalData->exception = createStackOverflowError(callFrame);
                VM_THROW_EXCEPTION();
            }
            argsObject->copyToRegisters(callFrame, callFrame->registers() + argsOffset, argCount);
        } else if (isJSArray(&callFrame->globalData(), arguments)) {
            JSArray* array = asArray(arguments);
            argCount = array->length();
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                stackFrame.globalData->exception = createStackOverflowError(callFrame);
                VM_THROW_EXCEPTION();
            }
            array->copyToRegisters(callFrame, callFrame->registers() + argsOffset, argCount);
        } else if (asObject(arguments)->inherits(&JSArray::info)) {
            JSObject* argObject = asObject(arguments);
            argCount = argObject->get(callFrame, callFrame->propertyNames().length).toUInt32(callFrame);
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                stackFrame.globalData->exception = createStackOverflowError(callFrame);
                VM_THROW_EXCEPTION();
            }
            Register* argsBuffer = callFrame->registers() + argsOffset;
            for (unsigned i = 0; i < argCount; ++i) {
                argsBuffer[i] = asObject(arguments)->get(callFrame, i);
                CHECK_FOR_EXCEPTION();
            }
        } else {
            CodeBlock* codeBlock = callFrame->codeBlock();
            unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
            stackFrame.globalData->exception = createInvalidParamError(callFrame, "Function.prototype.apply", arguments, vPCIndex, codeBlock);
            VM_THROW_EXCEPTION();
        }
    }
    CHECK_FOR_EXCEPTION_AT_END();
    return argCount + 1;
}

EncodedJSValue JITStubs::cti_op_negate(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src = stackFrame.args[0].jsValue();

    double v;
    if (src.getNumber(v))
        return JSValue::encode(jsNumber(stackFrame.globalData, -v));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, -src.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_resolve_base(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(JSC::resolveBase(stackFrame.callFrame, stackFrame.args[0].identifier(), stackFrame.callFrame->scopeChain()));
}

EncodedJSValue JITStubs::cti_op_resolve_skip(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    int skip = stackFrame.args[1].int32();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = stackFrame.args[0].identifier();
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();
            return JSValue::encode(result);
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

EncodedJSValue JITStubs::cti_op_resolve_global(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSGlobalObject* globalObject = asGlobalObject(stackFrame.args[0].jsValue());
    Identifier& ident = stackFrame.args[1].identifier();
    unsigned globalResolveInfoIndex = stackFrame.args[2].int32();
    ASSERT(globalObject->isGlobalObject());

    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValue result = slot.getValue(callFrame, ident);
        if (slot.isCacheable() && !globalObject->structure()->isDictionary()) {
            GlobalResolveInfo& globalResolveInfo = callFrame->codeBlock()->globalResolveInfo(globalResolveInfoIndex);
            if (globalResolveInfo.structure)
                globalResolveInfo.structure->deref();
            globalObject->structure()->ref();
            globalResolveInfo.structure = globalObject->structure();
            globalResolveInfo.offset = slot.cachedOffset();
            return JSValue::encode(result);
        }

        CHECK_FOR_EXCEPTION_AT_END();
        return JSValue::encode(result);
    }

    unsigned vPCIndex = callFrame->codeBlock()->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

EncodedJSValue JITStubs::cti_op_div(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValue::encode(jsNumber(stackFrame.globalData, left / right));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, src1.toNumber(callFrame) / src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_pre_dec(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, v.toNumber(callFrame) - 1);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_op_jless(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();
    CallFrame* callFrame = stackFrame.callFrame;

    bool result = jsLess(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

int JITStubs::cti_op_jlesseq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();
    CallFrame* callFrame = stackFrame.callFrame;

    bool result = jsLessEq(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

EncodedJSValue JITStubs::cti_op_not(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue result = jsBoolean(!src.toBoolean(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_op_jtrue(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    bool result = src1.toBoolean(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

EncodedJSValue JITStubs::cti_op_post_inc(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue number = v.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();

    callFrame->registers()[stackFrame.args[1].int32()] = jsNumber(stackFrame.globalData, number.uncheckedGetNumber() + 1);
    return JSValue::encode(number);
}

EncodedJSValue JITStubs::cti_op_eq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    ASSERT(!JSValue::areBothInt32Fast(src1, src2));
    JSValue result = jsBoolean(JSValue::equalSlowCaseInline(callFrame, src1, src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_lshift(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue val = stackFrame.args[0].jsValue();
    JSValue shift = stackFrame.args[1].jsValue();

    int32_t left;
    uint32_t right;
    if (JSValue::areBothInt32Fast(val, shift))
        return JSValue::encode(jsNumber(stackFrame.globalData, val.getInt32Fast() << (shift.getInt32Fast() & 0x1f)));
    if (val.numberToInt32(left) && shift.numberToUInt32(right))
        return JSValue::encode(jsNumber(stackFrame.globalData, left << (right & 0x1f)));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, (val.toInt32(callFrame)) << (shift.toUInt32(callFrame) & 0x1f));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_bitand(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    int32_t left;
    int32_t right;
    if (src1.numberToInt32(left) && src2.numberToInt32(right))
        return JSValue::encode(jsNumber(stackFrame.globalData, left & right));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, src1.toInt32(callFrame) & src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_rshift(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue val = stackFrame.args[0].jsValue();
    JSValue shift = stackFrame.args[1].jsValue();

    int32_t left;
    uint32_t right;
    if (JSFastMath::canDoFastRshift(val, shift))
        return JSValue::encode(JSFastMath::rightShiftImmediateNumbers(val, shift));
    if (val.numberToInt32(left) && shift.numberToUInt32(right))
        return JSValue::encode(jsNumber(stackFrame.globalData, left >> (right & 0x1f)));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, (val.toInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_bitnot(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src = stackFrame.args[0].jsValue();

    int value;
    if (src.numberToInt32(value))
        return JSValue::encode(jsNumber(stackFrame.globalData, ~value));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsNumber(stackFrame.globalData, ~src.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_resolve_with_base(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = stackFrame.args[0].identifier();
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();

            callFrame->registers()[stackFrame.args[1].int32()] = JSValue(base);
            return JSValue::encode(result);
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    stackFrame.globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_AT_END();
    return JSValue::encode(JSValue());
}

JSObject* JITStubs::cti_op_new_func_exp(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return stackFrame.args[0].funcExprNode()->makeFunction(stackFrame.callFrame, stackFrame.callFrame->scopeChain());
}

EncodedJSValue JITStubs::cti_op_mod(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue dividendValue = stackFrame.args[0].jsValue();
    JSValue divisorValue = stackFrame.args[1].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;
    double d = dividendValue.toNumber(callFrame);
    JSValue result = jsNumber(stackFrame.globalData, fmod(d, divisorValue.toNumber(callFrame)));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_less(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsBoolean(jsLess(callFrame, stackFrame.args[0].jsValue(), stackFrame.args[1].jsValue()));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_neq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    ASSERT(!JSValue::areBothInt32Fast(src1, src2));

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue result = jsBoolean(!JSValue::equalSlowCaseInline(callFrame, src1, src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_post_dec(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v = stackFrame.args[0].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue number = v.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();

    callFrame->registers()[stackFrame.args[1].int32()] = jsNumber(stackFrame.globalData, number.uncheckedGetNumber() - 1);
    return JSValue::encode(number);
}

EncodedJSValue JITStubs::cti_op_urshift(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue val = stackFrame.args[0].jsValue();
    JSValue shift = stackFrame.args[1].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    if (JSFastMath::canDoFastUrshift(val, shift))
        return JSValue::encode(JSFastMath::rightShiftImmediateNumbers(val, shift));
    else {
        JSValue result = jsNumber(stackFrame.globalData, (val.toUInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
        CHECK_FOR_EXCEPTION_AT_END();
        return JSValue::encode(result);
    }
}

EncodedJSValue JITStubs::cti_op_bitxor(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue result = jsNumber(stackFrame.globalData, src1.toInt32(callFrame) ^ src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

JSObject* JITStubs::cti_op_new_regexp(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return new (stackFrame.globalData) RegExpObject(stackFrame.callFrame->lexicalGlobalObject()->regExpStructure(), stackFrame.args[0].regExp());
}

EncodedJSValue JITStubs::cti_op_bitor(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue result = jsNumber(stackFrame.globalData, src1.toInt32(callFrame) | src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_call_eval(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    RegisterFile* registerFile = stackFrame.registerFile;

    Interpreter* interpreter = stackFrame.globalData->interpreter;
    
    JSValue funcVal = stackFrame.args[0].jsValue();
    int registerOffset = stackFrame.args[1].int32();
    int argCount = stackFrame.args[2].int32();

    Register* newCallFrame = callFrame->registers() + registerOffset;
    Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
    JSValue thisValue = argv[0].jsValue();
    JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

    if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
        JSValue exceptionValue;
        JSValue result = interpreter->callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
        if (UNLIKELY(exceptionValue != JSValue())) {
            stackFrame.globalData->exception = exceptionValue;
            VM_THROW_EXCEPTION_AT_END();
        }
        return JSValue::encode(result);
    }

    return JSValue::encode(JSValue());
}

EncodedJSValue JITStubs::cti_op_throw(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);

    JSValue exceptionValue = stackFrame.args[0].jsValue();
    ASSERT(exceptionValue);

    HandlerInfo* handler = stackFrame.globalData->interpreter->throwException(callFrame, exceptionValue, vPCIndex, true);

    if (!handler) {
        *stackFrame.exception = exceptionValue;
        return JSValue::encode(jsNull());
    }

    stackFrame.callFrame = callFrame;
    void* catchRoutine = handler->nativeCode.addressForExceptionHandler();
    ASSERT(catchRoutine);
    STUB_SET_RETURN_ADDRESS(catchRoutine);
    return JSValue::encode(exceptionValue);
}

JSPropertyNameIterator* JITStubs::cti_op_get_pnames(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSPropertyNameIterator::create(stackFrame.callFrame, stackFrame.args[0].jsValue());
}

EncodedJSValue JITStubs::cti_op_next_pname(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSPropertyNameIterator* it = stackFrame.args[0].propertyNameIterator();
    JSValue temp = it->next(stackFrame.callFrame);
    if (!temp)
        it->invalidate();
    return JSValue::encode(temp);
}

JSObject* JITStubs::cti_op_push_scope(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSObject* o = stackFrame.args[0].jsValue().toObject(stackFrame.callFrame);
    CHECK_FOR_EXCEPTION();
    stackFrame.callFrame->setScopeChain(stackFrame.callFrame->scopeChain()->push(o));
    return o;
}

void JITStubs::cti_op_pop_scope(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    stackFrame.callFrame->setScopeChain(stackFrame.callFrame->scopeChain()->pop());
}

EncodedJSValue JITStubs::cti_op_typeof(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(jsTypeStringForValue(stackFrame.callFrame, stackFrame.args[0].jsValue()));
}

EncodedJSValue JITStubs::cti_op_is_undefined(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue v = stackFrame.args[0].jsValue();
    return JSValue::encode(jsBoolean(v.isCell() ? v.asCell()->structure()->typeInfo().masqueradesAsUndefined() : v.isUndefined()));
}

EncodedJSValue JITStubs::cti_op_is_boolean(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(jsBoolean(stackFrame.args[0].jsValue().isBoolean()));
}

EncodedJSValue JITStubs::cti_op_is_number(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(jsBoolean(stackFrame.args[0].jsValue().isNumber()));
}

EncodedJSValue JITStubs::cti_op_is_string(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(jsBoolean(isJSString(stackFrame.globalData, stackFrame.args[0].jsValue())));
}

EncodedJSValue JITStubs::cti_op_is_object(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(jsBoolean(jsIsObjectType(stackFrame.args[0].jsValue())));
}

EncodedJSValue JITStubs::cti_op_is_function(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(jsBoolean(jsIsFunctionType(stackFrame.args[0].jsValue())));
}

EncodedJSValue JITStubs::cti_op_stricteq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    return JSValue::encode(jsBoolean(JSValue::strictEqual(src1, src2)));
}

EncodedJSValue JITStubs::cti_op_to_primitive(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(stackFrame.args[0].jsValue().toPrimitive(stackFrame.callFrame));
}

EncodedJSValue JITStubs::cti_op_strcat(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    return JSValue::encode(concatenateStrings(stackFrame.callFrame, &stackFrame.callFrame->registers()[stackFrame.args[0].int32()], stackFrame.args[1].int32()));
}

EncodedJSValue JITStubs::cti_op_nstricteq(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src1 = stackFrame.args[0].jsValue();
    JSValue src2 = stackFrame.args[1].jsValue();

    return JSValue::encode(jsBoolean(!JSValue::strictEqual(src1, src2)));
}

EncodedJSValue JITStubs::cti_op_to_jsnumber(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue src = stackFrame.args[0].jsValue();
    CallFrame* callFrame = stackFrame.callFrame;

    JSValue result = src.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_in(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    JSValue baseVal = stackFrame.args[1].jsValue();

    if (!baseVal.isObject()) {
        CallFrame* callFrame = stackFrame.callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        stackFrame.globalData->exception = createInvalidParamError(callFrame, "in", baseVal, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSValue propName = stackFrame.args[0].jsValue();
    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName.getUInt32(i))
        return JSValue::encode(jsBoolean(baseObj->hasProperty(callFrame, i)));

    Identifier property(callFrame, propName.toString(callFrame));
    CHECK_FOR_EXCEPTION();
    return JSValue::encode(jsBoolean(baseObj->hasProperty(callFrame, property)));
}

JSObject* JITStubs::cti_op_push_new_scope(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSObject* scope = new (stackFrame.globalData) JSStaticScopeObject(stackFrame.callFrame, stackFrame.args[0].identifier(), stackFrame.args[1].jsValue(), DontDelete);

    CallFrame* callFrame = stackFrame.callFrame;
    callFrame->setScopeChain(callFrame->scopeChain()->push(scope));
    return scope;
}

void JITStubs::cti_op_jmp_scopes(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    unsigned count = stackFrame.args[0].int32();
    CallFrame* callFrame = stackFrame.callFrame;

    ScopeChainNode* tmp = callFrame->scopeChain();
    while (count--)
        tmp = tmp->pop();
    callFrame->setScopeChain(tmp);
}

void JITStubs::cti_op_put_by_index(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    unsigned property = stackFrame.args[1].int32();

    stackFrame.args[0].jsValue().put(callFrame, property, stackFrame.args[2].jsValue());
}

void* JITStubs::cti_op_switch_imm(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue scrutinee = stackFrame.args[0].jsValue();
    unsigned tableIndex = stackFrame.args[1].int32();
    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    if (scrutinee.isInt32Fast())
        return codeBlock->immediateSwitchJumpTable(tableIndex).ctiForValue(scrutinee.getInt32Fast()).addressForSwitch();
    else {
        double value;
        int32_t intValue;
        if (scrutinee.getNumber(value) && ((intValue = static_cast<int32_t>(value)) == value))
            return codeBlock->immediateSwitchJumpTable(tableIndex).ctiForValue(intValue).addressForSwitch();
        else
            return codeBlock->immediateSwitchJumpTable(tableIndex).ctiDefault.addressForSwitch();
    }
}

void* JITStubs::cti_op_switch_char(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue scrutinee = stackFrame.args[0].jsValue();
    unsigned tableIndex = stackFrame.args[1].int32();
    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->characterSwitchJumpTable(tableIndex).ctiDefault.addressForSwitch();

    if (scrutinee.isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        if (value->size() == 1)
            result = codeBlock->characterSwitchJumpTable(tableIndex).ctiForValue(value->data()[0]).addressForSwitch();
    }

    return result;
}

void* JITStubs::cti_op_switch_string(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    JSValue scrutinee = stackFrame.args[0].jsValue();
    unsigned tableIndex = stackFrame.args[1].int32();
    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->stringSwitchJumpTable(tableIndex).ctiDefault.addressForSwitch();

    if (scrutinee.isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        result = codeBlock->stringSwitchJumpTable(tableIndex).ctiForValue(value).addressForSwitch();
    }

    return result;
}

EncodedJSValue JITStubs::cti_op_del_by_val(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    JSValue baseValue = stackFrame.args[0].jsValue();
    JSObject* baseObj = baseValue.toObject(callFrame); // may throw

    JSValue subscript = stackFrame.args[1].jsValue();
    JSValue result;
    uint32_t i;
    if (subscript.getUInt32(i))
        result = jsBoolean(baseObj->deleteProperty(callFrame, i));
    else {
        CHECK_FOR_EXCEPTION();
        Identifier property(callFrame, subscript.toString(callFrame));
        CHECK_FOR_EXCEPTION();
        result = jsBoolean(baseObj->deleteProperty(callFrame, property));
    }

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

void JITStubs::cti_op_put_getter(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    ASSERT(stackFrame.args[0].jsValue().isObject());
    JSObject* baseObj = asObject(stackFrame.args[0].jsValue());
    ASSERT(stackFrame.args[2].jsValue().isObject());
    baseObj->defineGetter(callFrame, stackFrame.args[1].identifier(), asObject(stackFrame.args[2].jsValue()));
}

void JITStubs::cti_op_put_setter(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    ASSERT(stackFrame.args[0].jsValue().isObject());
    JSObject* baseObj = asObject(stackFrame.args[0].jsValue());
    ASSERT(stackFrame.args[2].jsValue().isObject());
    baseObj->defineSetter(callFrame, stackFrame.args[1].identifier(), asObject(stackFrame.args[2].jsValue()));
}

JSObject* JITStubs::cti_op_new_error(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned type = stackFrame.args[0].int32();
    JSValue message = stackFrame.args[1].jsValue();
    unsigned bytecodeOffset = stackFrame.args[2].int32();

    unsigned lineNumber = codeBlock->lineNumberForBytecodeOffset(callFrame, bytecodeOffset);
    return Error::create(callFrame, static_cast<ErrorType>(type), message.toString(callFrame), lineNumber, codeBlock->ownerNode()->sourceID(), codeBlock->ownerNode()->sourceURL());
}

void JITStubs::cti_op_debug(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;

    int debugHookID = stackFrame.args[0].int32();
    int firstLine = stackFrame.args[1].int32();
    int lastLine = stackFrame.args[2].int32();

    stackFrame.globalData->interpreter->debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
}

EncodedJSValue JITStubs::cti_vm_throw(STUB_ARGS_DECLARATION)
{
    STUB_INIT_STACK_FRAME(stackFrame);

    CallFrame* callFrame = stackFrame.callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalData* globalData = stackFrame.globalData;

    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, globalData->exceptionLocation);

    JSValue exceptionValue = globalData->exception;
    ASSERT(exceptionValue);
    globalData->exception = JSValue();

    HandlerInfo* handler = globalData->interpreter->throwException(callFrame, exceptionValue, vPCIndex, false);

    if (!handler) {
        *stackFrame.exception = exceptionValue;
        return JSValue::encode(jsNull());
    }

    stackFrame.callFrame = callFrame;
    void* catchRoutine = handler->nativeCode.addressForExceptionHandler();
    ASSERT(catchRoutine);
    STUB_SET_RETURN_ADDRESS(catchRoutine);
    return JSValue::encode(exceptionValue);
}

} // namespace JSC

#endif // ENABLE(JIT)
