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

COMPILE_ASSERT(STUB_ARGS_code == 0x0C, STUB_ARGS_code_is_0x0C);
COMPILE_ASSERT(STUB_ARGS_callFrame == 0x0E, STUB_ARGS_callFrame_is_0x0E);

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
    "movl 0x38(%esp), %edi" "\n" // Ox38 = 0x0E * 4, 0x0E = STUB_ARGS_callFrame (see assertion above)
    "call *0x30(%esp)" "\n" // Ox30 = 0x0C * 4, 0x0C = STUB_ARGS_code (see assertion above)
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

COMPILE_ASSERT(STUB_ARGS_code == 0x10, STUB_ARGS_code_is_0x10);
COMPILE_ASSERT(STUB_ARGS_callFrame == 0x12, STUB_ARGS_callFrame_is_0x12);

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
    "movq 0x90(%rsp), %r13" "\n" // Ox90 = 0x12 * 8, 0x12 = STUB_ARGS_callFrame (see assertion above)
    "call *0x80(%rsp)" "\n" // Ox80 = 0x10 * 8, 0x10 = STUB_ARGS_code (see assertion above)
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
            call [esp + 0x30]; // Ox30 = 0x0C * 4, 0x0C = STUB_ARGS_code (see assertion above)
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
    #define CTI_SAMPLER ARG_globalData->interpreter->sampler()
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

#if USE(CTI_REPATCH_PIC)
    JIT::patchPutByIdReplace(stubInfo, structure, slot.cachedOffset(), returnAddress);
#else
    JIT::compilePutByIdReplace(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, slot.cachedOffset(), returnAddress);
#endif
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
#if USE(CTI_REPATCH_PIC)
        JIT::compilePatchGetArrayLength(callFrame->scopeChain()->globalData, codeBlock, returnAddress);
#else
        ctiPatchCallByReturnAddress(returnAddress, globalData->jitStubs.ctiArrayLengthTrampoline());
#endif
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
        
#if USE(CTI_REPATCH_PIC)
        JIT::patchGetByIdSelf(stubInfo, structure, slot.cachedOffset(), returnAddress);
#else
        JIT::compileGetByIdSelf(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, slot.cachedOffset(), returnAddress);
#endif
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
    // When executing a CTI function (which might do an allocation), we hack the return address
    // to pretend to be executing this function, to keep stack logging tools from blowing out
    // memory.
}

}

struct StackHack {
    ALWAYS_INLINE StackHack(void** location) 
    { 
        returnAddressLocation = location;
        savedReturnAddress = *returnAddressLocation;
        ctiSetReturnAddress(returnAddressLocation, reinterpret_cast<void*>(jscGeneratedNativeCode));
    }
    ALWAYS_INLINE ~StackHack() 
    { 
        ctiSetReturnAddress(returnAddressLocation, savedReturnAddress);
    }

    void** returnAddressLocation;
    void* savedReturnAddress;
};

#define BEGIN_STUB_FUNCTION() SETUP_VA_LISTL_ARGS; StackHack stackHack(&STUB_RETURN_ADDRESS_SLOT)
#define STUB_SET_RETURN_ADDRESS(address) stackHack.savedReturnAddress = address
#define STUB_RETURN_ADDRESS stackHack.savedReturnAddress

#else

#define BEGIN_STUB_FUNCTION() SETUP_VA_LISTL_ARGS
#define STUB_SET_RETURN_ADDRESS(address) ctiSetReturnAddress(&STUB_RETURN_ADDRESS_SLOT, address);
#define STUB_RETURN_ADDRESS STUB_RETURN_ADDRESS_SLOT

#endif

// The reason this is not inlined is to avoid having to do a PIC branch
// to get the address of the ctiVMThrowTrampoline function. It's also
// good to keep the code size down by leaving as much of the exception
// handling code out of line as possible.
static NEVER_INLINE void returnToThrowTrampoline(JSGlobalData* globalData, void* exceptionLocation, void*& returnAddressSlot)
{
    ASSERT(globalData->exception);
    globalData->exceptionLocation = exceptionLocation;
    ctiSetReturnAddress(&returnAddressSlot, reinterpret_cast<void*>(ctiVMThrowTrampoline));
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
#define VM_THROW_EXCEPTION_2() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        RETURN_PAIR(0, 0); \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    returnToThrowTrampoline(ARG_globalData, STUB_RETURN_ADDRESS, STUB_RETURN_ADDRESS)

#define CHECK_FOR_EXCEPTION() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != JSValue())) \
            VM_THROW_EXCEPTION(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_AT_END() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != JSValue())) \
            VM_THROW_EXCEPTION_AT_END(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_VOID() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != JSValue())) { \
            VM_THROW_EXCEPTION_AT_END(); \
            return; \
        } \
    } while (0)

JSObject* JITStubs::cti_op_convert_this(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v1 = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSObject* result = v1.toThisObject(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

void JITStubs::cti_op_end(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ScopeChainNode* scopeChain = ARG_callFrame->scopeChain();
    ASSERT(scopeChain->refCount > 1);
    scopeChain->deref();
}

EncodedJSValue JITStubs::cti_op_add(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v1 = ARG_src1;
    JSValue v2 = ARG_src2;

    double left;
    double right = 0.0;

    bool rightIsNumber = v2.getNumber(right);
    if (rightIsNumber && v1.getNumber(left))
        return JSValue::encode(jsNumber(ARG_globalData, left + right));
    
    CallFrame* callFrame = ARG_callFrame;

    bool leftIsString = v1.isString();
    if (leftIsString && v2.isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }

        return JSValue::encode(jsString(ARG_globalData, value.release()));
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = v2.isInt32Fast() ?
            concatenate(asString(v1)->value().rep(), v2.getInt32Fast()) :
            concatenate(asString(v1)->value().rep(), right);

        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }
        return JSValue::encode(jsString(ARG_globalData, value.release()));
    }

    // All other cases are pretty uncommon
    JSValue result = jsAddSlowCase(callFrame, v1, v2);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_pre_inc(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, v.toNumber(callFrame) + 1);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_timeout_check(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    
    JSGlobalData* globalData = ARG_globalData;
    TimeoutChecker& timeoutChecker = globalData->timeoutChecker;

    if (timeoutChecker.didTimeOut(ARG_callFrame)) {
        globalData->exception = createInterruptedExecutionException(globalData);
        VM_THROW_EXCEPTION_AT_END();
    }
    
    return timeoutChecker.ticksUntilNextCheck();
}

void JITStubs::cti_register_file_check(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    if (LIKELY(ARG_registerFile->grow(ARG_callFrame + ARG_callFrame->codeBlock()->m_numCalleeRegisters)))
        return;

    // Rewind to the previous call frame because op_call already optimistically
    // moved the call frame forward.
    CallFrame* oldCallFrame = ARG_callFrame->callerFrame();
    ARG_setCallFrame(oldCallFrame);
    throwStackOverflowError(oldCallFrame, ARG_globalData, oldCallFrame->returnPC(), STUB_RETURN_ADDRESS);
}

int JITStubs::cti_op_loop_if_less(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

int JITStubs::cti_op_loop_if_lesseq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLessEq(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

JSObject* JITStubs::cti_op_new_object(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return constructEmptyObject(ARG_callFrame);
}

void JITStubs::cti_op_put_by_id_generic(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    PutPropertySlot slot;
    ARG_src1.put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    CHECK_FOR_EXCEPTION_AT_END();
}

EncodedJSValue JITStubs::cti_op_get_by_id_generic(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

void JITStubs::cti_op_put_by_id(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1.put(callFrame, ident, ARG_src3, slot);

    ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_id_second));

    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_id_second(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    PutPropertySlot slot;
    ARG_src1.put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    tryCachePutByID(ARG_callFrame, ARG_callFrame->codeBlock(), STUB_RETURN_ADDRESS, ARG_src1, slot);
    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_id_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1.put(callFrame, ident, ARG_src3, slot);

    CHECK_FOR_EXCEPTION_AT_END();
}

EncodedJSValue JITStubs::cti_op_get_by_id(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_second));

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_second(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, ident, slot);

    tryCacheGetByID(callFrame, callFrame->codeBlock(), STUB_RETURN_ADDRESS, baseValue, ident, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_self_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue baseValue = ARG_src1;
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

EncodedJSValue JITStubs::cti_op_get_by_id_proto_list(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(callFrame, *ARG_id2, slot);

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

EncodedJSValue JITStubs::cti_op_get_by_id_proto_list_full(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_proto_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_array_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_get_by_id_string_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

#endif

EncodedJSValue JITStubs::cti_op_instanceof(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValue value = ARG_src1;
    JSValue baseVal = ARG_src2;
    JSValue proto = ARG_src3;

    // at least one of these checks must have failed to get to the slow case
    ASSERT(!value.isCell() || !baseVal.isCell() || !proto.isCell()
           || !value.isObject() || !baseVal.isObject() || !proto.isObject() 
           || (asObject(baseVal)->structure()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);

    if (!baseVal.isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "instanceof", baseVal, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSObject* baseObj = asObject(baseVal);
    TypeInfo typeInfo = baseObj->structure()->typeInfo();
    if (!typeInfo.implementsHasInstance())
        return JSValue::encode(jsBoolean(false));

    if (!typeInfo.overridesHasInstance()) {
        if (!proto.isObject()) {
            throwError(callFrame, TypeError, "instanceof called on an object with an invalid prototype property.");
            VM_THROW_EXCEPTION();
        }

        if (!value.isObject())
            return JSValue::encode(jsBoolean(false));
    }

    JSValue result = jsBoolean(baseObj->hasInstance(callFrame, value, proto));
    CHECK_FOR_EXCEPTION_AT_END();

    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_del_by_id(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    
    JSObject* baseObj = ARG_src1.toObject(callFrame);

    JSValue result = jsBoolean(baseObj->deleteProperty(callFrame, *ARG_id2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_mul(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValue::encode(jsNumber(ARG_globalData, left * right));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, src1.toNumber(callFrame) * src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

JSObject* JITStubs::cti_op_new_func(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return ARG_func1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

void* JITStubs::cti_op_call_JSFunction(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

#ifndef NDEBUG
    CallData callData;
    ASSERT(ARG_src1.getCallData(callData) == CallTypeJS);
#endif

    JSFunction* function = asFunction(ARG_src1);
    FunctionBodyNode* body = function->body();
    ScopeChainNode* callDataScopeChain = function->scope().node();
    body->jitCode(callDataScopeChain);

    return &(body->generatedBytecode());
}

VoidPtrPair JITStubs::cti_op_call_arityCheck(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* newCodeBlock = ARG_codeBlock4;
    int argCount = ARG_int3;

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
        if (!ARG_registerFile->grow(newEnd)) {
            // Rewind to the previous call frame because op_call already optimistically
            // moved the call frame forward.
            ARG_setCallFrame(oldCallFrame);
            throwStackOverflowError(oldCallFrame, ARG_globalData, ARG_returnAddress2, STUB_RETURN_ADDRESS);
            RETURN_PAIR(0, 0);
        }

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    }

    RETURN_PAIR(newCodeBlock, callFrame);
}

void* JITStubs::cti_vm_dontLazyLinkCall(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSGlobalData* globalData = ARG_globalData;
    JSFunction* callee = asFunction(ARG_src1);
    JITCode jitCode = callee->body()->generatedJITCode();
    ASSERT(jitCode);

    ctiPatchNearCallByReturnAddress(ARG_returnAddress2, globalData->jitStubs.ctiVirtualCallLink());

    return jitCode.addressForCall();
}

void* JITStubs::cti_vm_lazyLinkCall(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSFunction* callee = asFunction(ARG_src1);
    JITCode jitCode = callee->body()->generatedJITCode();
    ASSERT(jitCode);
    
    CodeBlock* codeBlock = 0;
    if (!callee->isHostFunction())
        codeBlock = &callee->body()->bytecode(callee->scope().node());

    CallLinkInfo* callLinkInfo = &ARG_callFrame->callerFrame()->codeBlock()->getCallLinkInfo(ARG_returnAddress2);
    JIT::linkCall(callee, codeBlock, jitCode, callLinkInfo, ARG_int3);

    return jitCode.addressForCall();
}

JSObject* JITStubs::cti_op_push_activation(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSActivation* activation = new (ARG_globalData) JSActivation(ARG_callFrame, static_cast<FunctionBodyNode*>(ARG_callFrame->codeBlock()->ownerNode()));
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->copy()->push(activation));
    return activation;
}

EncodedJSValue JITStubs::cti_op_call_NotJSFunction(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue funcVal = ARG_src1;

    CallData callData;
    CallType callType = funcVal.getCallData(callData);

    ASSERT(callType != CallTypeJS);

    if (callType == CallTypeHost) {
        int registerOffset = ARG_int2;
        int argCount = ARG_int3;
        CallFrame* previousCallFrame = ARG_callFrame;
        CallFrame* callFrame = CallFrame::create(previousCallFrame->registers() + registerOffset);

        callFrame->init(0, static_cast<Instruction*>(STUB_RETURN_ADDRESS), previousCallFrame->scopeChain(), previousCallFrame, 0, argCount, 0);
        ARG_setCallFrame(callFrame);

        Register* argv = ARG_callFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
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
        ARG_setCallFrame(previousCallFrame);
        CHECK_FOR_EXCEPTION();

        return JSValue::encode(returnValue);
    }

    ASSERT(callType == CallTypeNone);

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createNotAFunctionError(ARG_callFrame, funcVal, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

void JITStubs::cti_op_create_arguments(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void JITStubs::cti_op_create_arguments_no_params(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame, Arguments::NoParameters);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void JITStubs::cti_op_tear_off_activation(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain());
    asActivation(ARG_src1)->copyRegisters(ARG_callFrame->optionalCalleeArguments());
}

void JITStubs::cti_op_tear_off_arguments(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(ARG_callFrame->codeBlock()->usesArguments() && !ARG_callFrame->codeBlock()->needsFullScopeChain());
    ARG_callFrame->optionalCalleeArguments()->copyRegisters();
}

void JITStubs::cti_op_profile_will_call(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->willExecute(ARG_callFrame, ARG_src1);
}

void JITStubs::cti_op_profile_did_call(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->didExecute(ARG_callFrame, ARG_src1);
}

void JITStubs::cti_op_ret_scopeChain(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain());
    ARG_callFrame->scopeChain()->deref();
}

JSObject* JITStubs::cti_op_new_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ArgList argList(&ARG_callFrame->registers()[ARG_int1], ARG_int2);
    return constructArray(ARG_callFrame, argList);
}

EncodedJSValue JITStubs::cti_op_resolve(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
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
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSObject* JITStubs::cti_op_construct_JSConstruct(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSFunction* constructor = asFunction(ARG_src1);
    if (constructor->isHostFunction()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        ARG_globalData->exception = createNotAConstructorError(callFrame, constructor, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(constructor->getConstructData(constructData) == ConstructTypeJS);
#endif

    Structure* structure;
    if (ARG_src4.isObject())
        structure = asObject(ARG_src4)->inheritorID();
    else
        structure = constructor->scope().node()->globalObject()->emptyObjectStructure();
    return new (ARG_globalData) JSObject(structure);
}

EncodedJSValue JITStubs::cti_op_construct_NotJSConstruct(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValue constrVal = ARG_src1;
    int argCount = ARG_int3;
    int thisRegister = ARG_int5;

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
    ARG_globalData->exception = createNotAConstructorError(callFrame, constrVal, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

EncodedJSValue JITStubs::cti_op_get_by_val(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSGlobalData* globalData = ARG_globalData;

    JSValue baseValue = ARG_src1;
    JSValue subscript = ARG_src2;

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
            result = asString(baseValue)->getIndex(ARG_globalData, i);
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
    
    EncodedJSValue JITStubs::cti_op_get_by_val_string(STUB_ARGS)
    {
        BEGIN_STUB_FUNCTION();
        
        CallFrame* callFrame = ARG_callFrame;
        JSGlobalData* globalData = ARG_globalData;
        
        JSValue baseValue = ARG_src1;
        JSValue subscript = ARG_src2;
        
        JSValue result;
        
        if (LIKELY(subscript.isUInt32Fast())) {
            uint32_t i = subscript.getUInt32Fast();
            if (isJSString(globalData, baseValue) && asString(baseValue)->canGetIndex(i))
                result = asString(baseValue)->getIndex(ARG_globalData, i);
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
    

EncodedJSValue JITStubs::cti_op_get_by_val_byte_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    
    CallFrame* callFrame = ARG_callFrame;
    JSGlobalData* globalData = ARG_globalData;
    
    JSValue baseValue = ARG_src1;
    JSValue subscript = ARG_src2;
    
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

VoidPtrPair JITStubs::cti_op_resolve_func(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
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

            RETURN_PAIR(thisObj, JSValue::encode(result));
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

EncodedJSValue JITStubs::cti_op_sub(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValue::encode(jsNumber(ARG_globalData, left - right));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, src1.toNumber(callFrame) - src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

void JITStubs::cti_op_put_by_val(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSGlobalData* globalData = ARG_globalData;

    JSValue baseValue = ARG_src1;
    JSValue subscript = ARG_src2;
    JSValue value = ARG_src3;

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
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }

    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_val_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValue baseValue = ARG_src1;
    int i = ARG_int2;
    JSValue value = ARG_src3;

    ASSERT(isJSArray(ARG_globalData, baseValue));

    if (LIKELY(i >= 0))
        asArray(baseValue)->JSArray::put(callFrame, i, value);
    else {
        // This should work since we're re-boxing an immediate unboxed in JIT code.
        ASSERT(JSValue::makeInt32Fast(i));
        Identifier property(callFrame, JSValue::makeInt32Fast(i).toString(callFrame));
        // FIXME: can toString throw an exception here?
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }

    CHECK_FOR_EXCEPTION_AT_END();
}

void JITStubs::cti_op_put_by_val_byte_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    
    CallFrame* callFrame = ARG_callFrame;
    JSGlobalData* globalData = ARG_globalData;
    
    JSValue baseValue = ARG_src1;
    JSValue subscript = ARG_src2;
    JSValue value = ARG_src3;
    
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
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
}

EncodedJSValue JITStubs::cti_op_lesseq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsBoolean(jsLessEq(callFrame, ARG_src1, ARG_src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_op_loop_if_true(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1.toBoolean(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}
    
int JITStubs::cti_op_load_varargs(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    CallFrame* callFrame = ARG_callFrame;
    RegisterFile* registerFile = ARG_registerFile;
    int argsOffset = ARG_int1;
    JSValue arguments = callFrame[argsOffset].jsValue();
    uint32_t argCount = 0;
    if (!arguments.isUndefinedOrNull()) {
        if (!arguments.isObject()) {
            CodeBlock* codeBlock = callFrame->codeBlock();
            unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
            ARG_globalData->exception = createInvalidParamError(callFrame, "Function.prototype.apply", arguments, vPCIndex, codeBlock);
            VM_THROW_EXCEPTION();
        }
        if (asObject(arguments)->classInfo() == &Arguments::info) {
            Arguments* argsObject = asArguments(arguments);
            argCount = argsObject->numProvidedArguments(callFrame);
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                ARG_globalData->exception = createStackOverflowError(callFrame);
                VM_THROW_EXCEPTION();
            }
            argsObject->copyToRegisters(callFrame, callFrame->registers() + argsOffset, argCount);
        } else if (isJSArray(&callFrame->globalData(), arguments)) {
            JSArray* array = asArray(arguments);
            argCount = array->length();
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                ARG_globalData->exception = createStackOverflowError(callFrame);
                VM_THROW_EXCEPTION();
            }
            array->copyToRegisters(callFrame, callFrame->registers() + argsOffset, argCount);
        } else if (asObject(arguments)->inherits(&JSArray::info)) {
            JSObject* argObject = asObject(arguments);
            argCount = argObject->get(callFrame, callFrame->propertyNames().length).toUInt32(callFrame);
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                ARG_globalData->exception = createStackOverflowError(callFrame);
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
            ARG_globalData->exception = createInvalidParamError(callFrame, "Function.prototype.apply", arguments, vPCIndex, codeBlock);
            VM_THROW_EXCEPTION();
        }
    }
    CHECK_FOR_EXCEPTION_AT_END();
    return argCount + 1;
}

EncodedJSValue JITStubs::cti_op_negate(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src = ARG_src1;

    double v;
    if (src.getNumber(v))
        return JSValue::encode(jsNumber(ARG_globalData, -v));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, -src.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_resolve_base(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(JSC::resolveBase(ARG_callFrame, *ARG_id1, ARG_callFrame->scopeChain()));
}

EncodedJSValue JITStubs::cti_op_resolve_skip(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    int skip = ARG_int2;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = *ARG_id1;
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
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

EncodedJSValue JITStubs::cti_op_resolve_global(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSGlobalObject* globalObject = asGlobalObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    unsigned globalResolveInfoIndex = ARG_int3;
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
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

EncodedJSValue JITStubs::cti_op_div(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValue::encode(jsNumber(ARG_globalData, left / right));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, src1.toNumber(callFrame) / src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_pre_dec(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, v.toNumber(callFrame) - 1);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_op_jless(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

EncodedJSValue JITStubs::cti_op_not(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValue result = jsBoolean(!src.toBoolean(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

int JITStubs::cti_op_jtrue(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1.toBoolean(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair JITStubs::cti_op_post_inc(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValue number = v.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();

    RETURN_PAIR(JSValue::encode(number), JSValue::encode(jsNumber(ARG_globalData, number.uncheckedGetNumber() + 1)));
}

EncodedJSValue JITStubs::cti_op_eq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(!JSValue::areBothInt32Fast(src1, src2));
    JSValue result = jsBoolean(JSValue::equalSlowCaseInline(callFrame, src1, src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_lshift(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue val = ARG_src1;
    JSValue shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSValue::areBothInt32Fast(val, shift))
        return JSValue::encode(jsNumber(ARG_globalData, val.getInt32Fast() << (shift.getInt32Fast() & 0x1f)));
    if (val.numberToInt32(left) && shift.numberToUInt32(right))
        return JSValue::encode(jsNumber(ARG_globalData, left << (right & 0x1f)));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, (val.toInt32(callFrame)) << (shift.toUInt32(callFrame) & 0x1f));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_bitand(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    int32_t left;
    int32_t right;
    if (src1.numberToInt32(left) && src2.numberToInt32(right))
        return JSValue::encode(jsNumber(ARG_globalData, left & right));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, src1.toInt32(callFrame) & src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_rshift(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue val = ARG_src1;
    JSValue shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSFastMath::canDoFastRshift(val, shift))
        return JSValue::encode(JSFastMath::rightShiftImmediateNumbers(val, shift));
    if (val.numberToInt32(left) && shift.numberToUInt32(right))
        return JSValue::encode(jsNumber(ARG_globalData, left >> (right & 0x1f)));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, (val.toInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_bitnot(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src = ARG_src1;

    int value;
    if (src.numberToInt32(value))
        return JSValue::encode(jsNumber(ARG_globalData, ~value));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsNumber(ARG_globalData, ~src.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

VoidPtrPair JITStubs::cti_op_resolve_with_base(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();

            RETURN_PAIR(base, JSValue::encode(result));
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSObject* JITStubs::cti_op_new_func_exp(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return ARG_funcexp1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

EncodedJSValue JITStubs::cti_op_mod(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue dividendValue = ARG_src1;
    JSValue divisorValue = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;
    double d = dividendValue.toNumber(callFrame);
    JSValue result = jsNumber(ARG_globalData, fmod(d, divisorValue.toNumber(callFrame)));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_less(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsBoolean(jsLess(callFrame, ARG_src1, ARG_src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_neq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    ASSERT(!JSValue::areBothInt32Fast(src1, src2));

    CallFrame* callFrame = ARG_callFrame;
    JSValue result = jsBoolean(!JSValue::equalSlowCaseInline(callFrame, src1, src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

VoidPtrPair JITStubs::cti_op_post_dec(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValue number = v.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();

    RETURN_PAIR(JSValue::encode(number), JSValue::encode(jsNumber(ARG_globalData, number.uncheckedGetNumber() - 1)));
}

EncodedJSValue JITStubs::cti_op_urshift(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue val = ARG_src1;
    JSValue shift = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    if (JSFastMath::canDoFastUrshift(val, shift))
        return JSValue::encode(JSFastMath::rightShiftImmediateNumbers(val, shift));
    else {
        JSValue result = jsNumber(ARG_globalData, (val.toUInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
        CHECK_FOR_EXCEPTION_AT_END();
        return JSValue::encode(result);
    }
}

EncodedJSValue JITStubs::cti_op_bitxor(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValue result = jsNumber(ARG_globalData, src1.toInt32(callFrame) ^ src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

JSObject* JITStubs::cti_op_new_regexp(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return new (ARG_globalData) RegExpObject(ARG_callFrame->lexicalGlobalObject()->regExpStructure(), ARG_regexp1);
}

EncodedJSValue JITStubs::cti_op_bitor(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValue result = jsNumber(ARG_globalData, src1.toInt32(callFrame) | src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_call_eval(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    RegisterFile* registerFile = ARG_registerFile;

    Interpreter* interpreter = ARG_globalData->interpreter;
    
    JSValue funcVal = ARG_src1;
    int registerOffset = ARG_int2;
    int argCount = ARG_int3;

    Register* newCallFrame = callFrame->registers() + registerOffset;
    Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
    JSValue thisValue = argv[0].jsValue();
    JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

    if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
        JSValue exceptionValue;
        JSValue result = interpreter->callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
        if (UNLIKELY(exceptionValue != JSValue())) {
            ARG_globalData->exception = exceptionValue;
            VM_THROW_EXCEPTION_AT_END();
        }
        return JSValue::encode(result);
    }

    return JSValue::encode(JSValue());
}

EncodedJSValue JITStubs::cti_op_throw(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);

    JSValue exceptionValue = ARG_src1;
    ASSERT(exceptionValue);

    HandlerInfo* handler = ARG_globalData->interpreter->throwException(callFrame, exceptionValue, vPCIndex, true);

    if (!handler) {
        *ARG_exception = exceptionValue;
        return JSValue::encode(jsNull());
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = handler->nativeCode.addressForExceptionHandler();
    ASSERT(catchRoutine);
    STUB_SET_RETURN_ADDRESS(catchRoutine);
    return JSValue::encode(exceptionValue);
}

JSPropertyNameIterator* JITStubs::cti_op_get_pnames(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSPropertyNameIterator::create(ARG_callFrame, ARG_src1);
}

EncodedJSValue JITStubs::cti_op_next_pname(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSPropertyNameIterator* it = ARG_pni1;
    JSValue temp = it->next(ARG_callFrame);
    if (!temp)
        it->invalidate();
    return JSValue::encode(temp);
}

JSObject* JITStubs::cti_op_push_scope(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSObject* o = ARG_src1.toObject(ARG_callFrame);
    CHECK_FOR_EXCEPTION();
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->push(o));
    return o;
}

void JITStubs::cti_op_pop_scope(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->pop());
}

EncodedJSValue JITStubs::cti_op_typeof(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(jsTypeStringForValue(ARG_callFrame, ARG_src1));
}

EncodedJSValue JITStubs::cti_op_is_undefined(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue v = ARG_src1;
    return JSValue::encode(jsBoolean(v.isCell() ? v.asCell()->structure()->typeInfo().masqueradesAsUndefined() : v.isUndefined()));
}

EncodedJSValue JITStubs::cti_op_is_boolean(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(jsBoolean(ARG_src1.isBoolean()));
}

EncodedJSValue JITStubs::cti_op_is_number(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(jsBoolean(ARG_src1.isNumber()));
}

EncodedJSValue JITStubs::cti_op_is_string(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(jsBoolean(isJSString(ARG_globalData, ARG_src1)));
}

EncodedJSValue JITStubs::cti_op_is_object(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(jsBoolean(jsIsObjectType(ARG_src1)));
}

EncodedJSValue JITStubs::cti_op_is_function(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValue::encode(jsBoolean(jsIsFunctionType(ARG_src1)));
}

EncodedJSValue JITStubs::cti_op_stricteq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    return JSValue::encode(jsBoolean(JSValue::strictEqual(src1, src2)));
}

EncodedJSValue JITStubs::cti_op_nstricteq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src1 = ARG_src1;
    JSValue src2 = ARG_src2;

    return JSValue::encode(jsBoolean(!JSValue::strictEqual(src1, src2)));
}

EncodedJSValue JITStubs::cti_op_to_jsnumber(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue src = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSValue result = src.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValue::encode(result);
}

EncodedJSValue JITStubs::cti_op_in(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValue baseVal = ARG_src2;

    if (!baseVal.isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "in", baseVal, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSValue propName = ARG_src1;
    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName.getUInt32(i))
        return JSValue::encode(jsBoolean(baseObj->hasProperty(callFrame, i)));

    Identifier property(callFrame, propName.toString(callFrame));
    CHECK_FOR_EXCEPTION();
    return JSValue::encode(jsBoolean(baseObj->hasProperty(callFrame, property)));
}

JSObject* JITStubs::cti_op_push_new_scope(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSObject* scope = new (ARG_globalData) JSStaticScopeObject(ARG_callFrame, *ARG_id1, ARG_src2, DontDelete);

    CallFrame* callFrame = ARG_callFrame;
    callFrame->setScopeChain(callFrame->scopeChain()->push(scope));
    return scope;
}

void JITStubs::cti_op_jmp_scopes(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    unsigned count = ARG_int1;
    CallFrame* callFrame = ARG_callFrame;

    ScopeChainNode* tmp = callFrame->scopeChain();
    while (count--)
        tmp = tmp->pop();
    callFrame->setScopeChain(tmp);
}

void JITStubs::cti_op_put_by_index(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    unsigned property = ARG_int2;

    ARG_src1.put(callFrame, property, ARG_src3);
}

void* JITStubs::cti_op_switch_imm(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
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

void* JITStubs::cti_op_switch_char(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->characterSwitchJumpTable(tableIndex).ctiDefault.addressForSwitch();

    if (scrutinee.isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        if (value->size() == 1)
            result = codeBlock->characterSwitchJumpTable(tableIndex).ctiForValue(value->data()[0]).addressForSwitch();
    }

    return result;
}

void* JITStubs::cti_op_switch_string(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValue scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->stringSwitchJumpTable(tableIndex).ctiDefault.addressForSwitch();

    if (scrutinee.isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        result = codeBlock->stringSwitchJumpTable(tableIndex).ctiForValue(value).addressForSwitch();
    }

    return result;
}

EncodedJSValue JITStubs::cti_op_del_by_val(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValue baseValue = ARG_src1;
    JSObject* baseObj = baseValue.toObject(callFrame); // may throw

    JSValue subscript = ARG_src2;
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

void JITStubs::cti_op_put_getter(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1.isObject());
    JSObject* baseObj = asObject(ARG_src1);
    ASSERT(ARG_src3.isObject());
    baseObj->defineGetter(callFrame, *ARG_id2, asObject(ARG_src3));
}

void JITStubs::cti_op_put_setter(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1.isObject());
    JSObject* baseObj = asObject(ARG_src1);
    ASSERT(ARG_src3.isObject());
    baseObj->defineSetter(callFrame, *ARG_id2, asObject(ARG_src3));
}

JSObject* JITStubs::cti_op_new_error(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned type = ARG_int1;
    JSValue message = ARG_src2;
    unsigned bytecodeOffset = ARG_int3;

    unsigned lineNumber = codeBlock->lineNumberForBytecodeOffset(callFrame, bytecodeOffset);
    return Error::create(callFrame, static_cast<ErrorType>(type), message.toString(callFrame), lineNumber, codeBlock->ownerNode()->sourceID(), codeBlock->ownerNode()->sourceURL());
}

void JITStubs::cti_op_debug(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    int debugHookID = ARG_int1;
    int firstLine = ARG_int2;
    int lastLine = ARG_int3;

    ARG_globalData->interpreter->debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
}

EncodedJSValue JITStubs::cti_vm_throw(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalData* globalData = ARG_globalData;

    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, globalData->exceptionLocation);

    JSValue exceptionValue = globalData->exception;
    ASSERT(exceptionValue);
    globalData->exception = JSValue();

    HandlerInfo* handler = globalData->interpreter->throwException(callFrame, exceptionValue, vPCIndex, false);

    if (!handler) {
        *ARG_exception = exceptionValue;
        return JSValue::encode(jsNull());
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = handler->nativeCode.addressForExceptionHandler();
    ASSERT(catchRoutine);
    STUB_SET_RETURN_ADDRESS(catchRoutine);
    return JSValue::encode(exceptionValue);
}

} // namespace JSC

#endif // ENABLE(JIT)
