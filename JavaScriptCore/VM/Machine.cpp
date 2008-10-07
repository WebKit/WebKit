/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "Machine.h"

#include "Arguments.h"
#include "BatchedTransitionOptimizer.h"
#include "CodeBlock.h"
#include "DebuggerCallFrame.h"
#include "ExceptionHelpers.h"
#include "ExecState.h"
#include "GlobalEvalFunction.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSStaticScopeObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "Profiler.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "collector.h"
#include "debugger.h"
#include "operations.h"
#include "SamplingTool.h"
#include <stdio.h>

#if PLATFORM(DARWIN)
#include <mach/mach.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QDateTime>
#endif

using namespace std;

namespace JSC {

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

#if HAVE(COMPUTED_GOTO)
static void* op_throw_end_indirect;
static void* op_call_indirect;
#endif

#if ENABLE(CTI)

ALWAYS_INLINE static Instruction* vPCForPC(CodeBlock* codeBlock, void* pc)
{
    if (pc >= codeBlock->instructions.begin() && pc < codeBlock->instructions.end())
        return static_cast<Instruction*>(pc);

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(pc));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(pc);
    return codeBlock->instructions.begin() + vPCIndex;
}

#else // #ENABLE(CTI)

ALWAYS_INLINE static Instruction* vPCForPC(CodeBlock*, void* pc)
{
    return static_cast<Instruction*>(pc);
}

#endif // #ENABLE(CTI)

// Returns the depth of the scope chain within a given call frame.
static int depth(CodeBlock* codeBlock, ScopeChain& sc)
{
    if (!codeBlock->needsFullScopeChain)
        return 0;
    int scopeDepth = 0;
    ScopeChainIterator iter = sc.begin();
    ScopeChainIterator end = sc.end();
    while (!(*iter)->isObject(&JSActivation::info)) {
        ++iter;
        if (iter == end)
            break;
        ++scopeDepth;
    }
    return scopeDepth;
}

// FIXME: This operation should be called "getNumber", not "isNumber" (as it is in JSValue.h).
// FIXME: There's no need to have a "slow" version of this. All versions should be fast.
static bool fastIsNumber(JSValue* value, double& arg)
{
    if (JSImmediate::isNumber(value))
        arg = JSImmediate::getTruncatedInt32(value);
    else if (Heap::isNumber(static_cast<JSCell*>(value)))
        arg = static_cast<JSNumberCell*>(value)->value();
    else
        return false;
    return true;
}

// FIXME: Why doesn't JSValue::toInt32 have the Heap::isNumber optimization?
static bool fastToInt32(JSValue* value, int32_t& arg)
{
    if (JSImmediate::isNumber(value))
        arg = JSImmediate::getTruncatedInt32(value);
    else if (Heap::isNumber(static_cast<JSCell*>(value)))
        arg = static_cast<JSNumberCell*>(value)->toInt32();
    else
        return false;
    return true;
}

static ALWAYS_INLINE bool fastToUInt32(JSValue* value, uint32_t& arg)
{
    if (JSImmediate::isNumber(value)) {
        if (JSImmediate::getTruncatedUInt32(value, arg))
            return true;
        bool scratch;
        arg = JSValue::toUInt32SlowCase(JSImmediate::getTruncatedInt32(value), scratch);
        return true;
    } else if (Heap::isNumber(static_cast<JSCell*>(value)))
        arg = static_cast<JSNumberCell*>(value)->toUInt32();
    else
        return false;
    return true;
}

static inline bool jsLess(ExecState* exec, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) < JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    if (fastIsNumber(v1, n1) && fastIsNumber(v2, n2))
        return n1 < n2;

    Machine* machine = exec->machine();
    if (machine->isJSString(v1) && machine->isJSString(v2))
        return static_cast<const JSString*>(v1)->value() < static_cast<const JSString*>(v2)->value();

    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return static_cast<const JSString*>(p1)->value() < static_cast<const JSString*>(p2)->value();
}

static inline bool jsLessEq(ExecState* exec, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) <= JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    if (fastIsNumber(v1, n1) && fastIsNumber(v2, n2))
        return n1 <= n2;

    Machine* machine = exec->machine();
    if (machine->isJSString(v1) && machine->isJSString(v2))
        return !(static_cast<const JSString*>(v2)->value() < static_cast<const JSString*>(v1)->value());

    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(static_cast<const JSString*>(p2)->value() < static_cast<const JSString*>(p1)->value());
}

static NEVER_INLINE JSValue* jsAddSlowCase(ExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue* p1 = v1->toPrimitive(exec);
    JSValue* p2 = v2->toPrimitive(exec);

    if (p1->isString() || p2->isString()) {
        RefPtr<UString::Rep> value = concatenate(p1->toString(exec).rep(), p2->toString(exec).rep());
        if (!value)
            return throwOutOfMemoryError(exec);
        return jsString(exec, value.release());
    }

    return jsNumber(exec, p1->toNumber(exec) + p2->toNumber(exec));
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5626160 Add case: 3 3 (of these, 3637690 are for immediate values)
//    247412  Add case: 5 5
//    20900   Add case: 5 6
//    13962   Add case: 5 3
//    4000    Add case: 3 5

static ALWAYS_INLINE JSValue* jsAdd(ExecState* exec, JSValue* v1, JSValue* v2)
{
    double left;
    double right = 0.0;

    bool rightIsNumber = fastIsNumber(v2, right);
    if (rightIsNumber && fastIsNumber(v1, left))
        return jsNumber(exec, left + right);
    
    bool leftIsString = v1->isString();
    if (leftIsString && v2->isString()) {
        RefPtr<UString::Rep> value = concatenate(static_cast<JSString*>(v1)->value().rep(), static_cast<JSString*>(v2)->value().rep());
        if (!value)
            return throwOutOfMemoryError(exec);
        return jsString(exec, value.release());
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = JSImmediate::isImmediate(v2) ?
            concatenate(static_cast<JSString*>(v1)->value().rep(), JSImmediate::getTruncatedInt32(v2)) :
            concatenate(static_cast<JSString*>(v1)->value().rep(), right);

        if (!value)
            return throwOutOfMemoryError(exec);
        return jsString(exec, value.release());
    }

    // All other cases are pretty uncommon
    return jsAddSlowCase(exec, v1, v2);
}

static JSValue* jsTypeStringForValue(ExecState* exec, JSValue* v)
{
    if (v->isUndefined())
        return jsNontrivialString(exec, "undefined");
    if (v->isBoolean())
        return jsNontrivialString(exec, "boolean");
    if (v->isNumber())
        return jsNontrivialString(exec, "number");
    if (v->isString())
        return jsNontrivialString(exec, "string");
    if (v->isObject()) {
        // Return "undefined" for objects that should be treated
        // as null when doing comparisons.
        if (static_cast<JSObject*>(v)->structureID()->typeInfo().masqueradesAsUndefined())
            return jsNontrivialString(exec, "undefined");
        CallData callData;
        if (static_cast<JSObject*>(v)->getCallData(callData) != CallTypeNone)
            return jsNontrivialString(exec, "function");
    }
    return jsNontrivialString(exec, "object");
}

static bool jsIsObjectType(JSValue* v)
{
    if (JSImmediate::isImmediate(v))
        return v->isNull();

    JSType type = static_cast<JSCell*>(v)->structureID()->typeInfo().type();
    if (type == NumberType || type == StringType)
        return false;
    if (type == ObjectType) {
        if (static_cast<JSObject*>(v)->structureID()->typeInfo().masqueradesAsUndefined())
            return false;
        CallData callData;
        if (static_cast<JSObject*>(v)->getCallData(callData) != CallTypeNone)
            return false;
    }
    return true;
}

static bool jsIsFunctionType(JSValue* v)
{
    if (v->isObject()) {
        CallData callData;
        if (static_cast<JSObject*>(v)->getCallData(callData) != CallTypeNone)
            return true;
    }
    return false;
}

NEVER_INLINE bool Machine::resolve(ExecState* exec, Instruction* vPC, Register* r, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

    ScopeChainNode* scopeChain = this->scopeChain(r);
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    CodeBlock* codeBlock = this->codeBlock(r);
    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;
            r[dst] = result;
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

NEVER_INLINE bool Machine::resolveSkip(ExecState* exec, Instruction* vPC, Register* r, JSValue*& exceptionValue)
{
    CodeBlock* codeBlock = this->codeBlock(r);

    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    int skip = (vPC + 3)->u.operand + codeBlock->needsFullScopeChain;

    ScopeChainNode* scopeChain = this->scopeChain(r);
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;
            r[dst] = result;
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

NEVER_INLINE bool Machine::resolveGlobal(ExecState* exec, Instruction* vPC, Register* r, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>((vPC + 2)->u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int property = (vPC + 3)->u.operand;
    StructureID* structureID = (vPC + 4)->u.structureID;
    int offset = (vPC + 5)->u.operand;

    if (structureID == globalObject->structureID()) {
        r[dst] = globalObject->getDirectOffset(offset);
        return true;
    }

    CodeBlock* codeBlock = this->codeBlock(r);
    Identifier& ident = codeBlock->identifiers[property];
    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(exec, ident, slot)) {
        JSValue* result = slot.getValue(exec, ident);
        if (slot.isCacheable()) {
            if (vPC[4].u.structureID)
                vPC[4].u.structureID->deref();
            globalObject->structureID()->ref();
            vPC[4] = globalObject->structureID();
            vPC[5] = slot.cachedOffset();
            r[dst] = result;
            return true;
        }

        exceptionValue = exec->exception();
        if (exceptionValue)
            return false;
        r[dst] = result;
        return true;
    }

    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

ALWAYS_INLINE static JSValue* inlineResolveBase(ExecState* exec, Identifier& property, ScopeChainNode* scopeChain)
{
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator next = iter;
    ++next;
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    while (true) {
        base = *iter;
        if (next == end || base->getPropertySlot(exec, property, slot))
            return base;

        iter = next;
        ++next;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

NEVER_INLINE void Machine::resolveBase(ExecState* exec, Instruction* vPC, Register* r)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    CodeBlock* codeBlock = this->codeBlock(r);
    ScopeChainNode* scopeChain = this->scopeChain(r);
    r[dst] = inlineResolveBase(exec, codeBlock->identifiers[property], scopeChain);
}

NEVER_INLINE bool Machine::resolveBaseAndProperty(ExecState* exec, Instruction* vPC, Register* r, JSValue*& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int propDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

    ScopeChainNode* scopeChain = this->scopeChain(r);
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    CodeBlock* codeBlock = this->codeBlock(r);
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;
            r[propDst] = result;
            r[baseDst] = base;
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

NEVER_INLINE bool Machine::resolveBaseAndFunc(ExecState* exec, Instruction* vPC, Register* r, JSValue*& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int funcDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

    ScopeChainNode* scopeChain = this->scopeChain(r);
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    CodeBlock* codeBlock = this->codeBlock(r);
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(exec, ident, slot)) {            
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(exec);
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;

            r[baseDst] = thisObj;
            r[funcDst] = result;
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

ALWAYS_INLINE Register* slideRegisterWindowForCall(CodeBlock* newCodeBlock, RegisterFile* registerFile, Register* r, size_t registerOffset, int argc)
{
    Register* newEnd = r + registerOffset + newCodeBlock->numCalleeRegisters;

    if (LIKELY(argc == newCodeBlock->numParameters)) { // correct number of arguments
        if (UNLIKELY(!registerFile->grow(newEnd)))
            return 0;
        r += registerOffset;
    } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
        size_t omittedArgCount = newCodeBlock->numParameters - argc;
        registerOffset += omittedArgCount;
        newEnd += omittedArgCount;
        if (!registerFile->grow(newEnd))
            return 0;
        r += registerOffset;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();
    } else { // too many arguments -- copy expected arguments, leaving the extra arguments behind
        size_t numParameters = newCodeBlock->numParameters;
        registerOffset += numParameters;
        newEnd += numParameters;

        if (!registerFile->grow(newEnd))
            return 0;
        r += registerOffset;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argc;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argc] = argv[i];
    }

    return r;
}

static NEVER_INLINE bool isNotObject(ExecState* exec, bool forInstanceOf, CodeBlock* codeBlock, const Instruction* vPC, JSValue* value, JSValue*& exceptionData)
{
    if (value->isObject())
        return false;
    exceptionData = createInvalidParamError(exec, forInstanceOf ? "instanceof" : "in" , value, vPC, codeBlock);
    return true;
}

NEVER_INLINE JSValue* Machine::callEval(ExecState* exec, JSObject* thisObj, ScopeChainNode* scopeChain, RegisterFile* registerFile, Register* r, int argv, int argc, JSValue*& exceptionValue)
{
    if (argc < 2)
        return jsUndefined();

    JSValue* program = r[argv + 1].jsValue(exec);

    if (!program->isString())
        return program;

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, scopeChain->globalObject()->evalFunction());

    UString programSource = static_cast<JSString*>(program)->value();

    CodeBlock* codeBlock = this->codeBlock(r);
    RefPtr<EvalNode> evalNode = codeBlock->evalCodeCache.get(exec, programSource, scopeChain, exceptionValue);

    JSValue* result = 0;
    if (evalNode)
        result = exec->globalData().machine->execute(evalNode.get(), exec, thisObj, r - registerFile->start() + argv + 1 + RegisterFile::CallFrameHeaderSize, scopeChain, &exceptionValue);

    if (*profiler)
        (*profiler)->didExecute(exec, scopeChain->globalObject()->evalFunction());

    return result;
}

Machine::Machine()
    : m_sampler(0)
#if ENABLE(CTI)
    , m_ctiArrayLengthTrampoline(0)
    , m_ctiStringLengthTrampoline(0)
    , m_jitCodeBuffer(new JITCodeBuffer(1024 * 1024))
#endif
    , m_reentryDepth(0)
    , m_timeoutTime(0)
    , m_timeAtLastCheckTimeout(0)
    , m_timeExecuting(0)
    , m_timeoutCheckCount(0)
    , m_ticksUntilNextTimeoutCheck(initialTickCountThreshold)
{
    initTimeout();
    privateExecute(InitializeAndReturn, 0, 0, 0);
    
    // Bizarrely, calling fastMalloc here is faster than allocating space on the stack.
    void* storage = fastMalloc(sizeof(CollectorBlock));

    JSArray* jsArray = new (storage) JSArray(JSArray::createStructureID(jsNull()));
    m_jsArrayVptr = jsArray->vptr();
    static_cast<JSCell*>(jsArray)->~JSCell();

    JSString* jsString = new (storage) JSString(JSString::VPtrStealingHack);
    m_jsStringVptr = jsString->vptr();
    static_cast<JSCell*>(jsString)->~JSCell();

    JSFunction* jsFunction = new (storage) JSFunction(JSFunction::createStructureID(jsNull()));
    m_jsFunctionVptr = jsFunction->vptr();
    static_cast<JSCell*>(jsFunction)->~JSCell();
    
    fastFree(storage);
}

Machine::~Machine()
{
#if ENABLE(CTI)
    if (m_ctiArrayLengthTrampoline)
        fastFree(m_ctiArrayLengthTrampoline);
    if (m_ctiStringLengthTrampoline)
        fastFree(m_ctiStringLengthTrampoline);
#endif
}

#ifndef NDEBUG

void Machine::dumpCallFrame(const RegisterFile* registerFile, const Register* r)
{
    JSGlobalObject* globalObject = scopeChain(r)->globalObject();

    CodeBlock* codeBlock = this->codeBlock(r);
    codeBlock->dump(globalObject->globalExec());

    dumpRegisters(registerFile, r);
}

void Machine::dumpRegisters(const RegisterFile* registerFile, const Register* r)
{
    printf("Register frame: \n\n");
    printf("----------------------------------------------------\n");
    printf("            use            |   address  |   value   \n");
    printf("----------------------------------------------------\n");

    CodeBlock* codeBlock = this->codeBlock(r);
    const Register* it;
    const Register* end;

    if (codeBlock->codeType == GlobalCode) {
        it = registerFile->lastGlobal();
        end = it + registerFile->numGlobals();
        while (it != end) {
            printf("[global var]               | %10p | %10p \n", it, (*it).v());
            ++it;
        }
        printf("----------------------------------------------------\n");
    }
    
    it = r - RegisterFile::CallFrameHeaderSize - codeBlock->numParameters;
    printf("[this]                     | %10p | %10p \n", it, (*it).v()); ++it;
    end = it + max(codeBlock->numParameters - 1, 0); // - 1 to skip "this"
    if (it != end) {
        do {
            printf("[param]                    | %10p | %10p \n", it, (*it).v());
            ++it;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    printf("[CodeBlock]                | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ScopeChain]               | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[CallerRegisters]          | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ReturnPC]                 | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ReturnValueRegister]      | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ArgumentCount]            | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[Callee]                   | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[OptionalCalleeArguments]  | %10p | %10p \n", it, (*it).v()); ++it;
    printf("----------------------------------------------------\n");

    int registerCount = 0;

    end = it + codeBlock->numVars;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    end = it + codeBlock->numConstants;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    end = it + codeBlock->numCalleeRegisters - codeBlock->numConstants - codeBlock->numVars;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");
}

#endif

//#if !defined(NDEBUG) || ENABLE(SAMPLING_TOOL)

bool Machine::isOpcode(Opcode opcode)
{
#if HAVE(COMPUTED_GOTO)
    return opcode != HashTraits<Opcode>::emptyValue()
        && !HashTraits<Opcode>::isDeletedValue(opcode)
        && m_opcodeIDTable.contains(opcode);
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}

//#endif

NEVER_INLINE bool Machine::unwindCallFrame(ExecState*& exec, JSValue* exceptionValue, const Instruction*& vPC, CodeBlock*& codeBlock, Register*& r)
{
    CodeBlock* oldCodeBlock = codeBlock;
    ScopeChainNode* scopeChain = this->scopeChain(r);

    if (Debugger* debugger = exec->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(exec->dynamicGlobalObject(), codeBlock, scopeChain, r, exceptionValue);
        if (r[RegisterFile::Callee].jsValue(exec))
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->lastLine());
        else
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->lastLine());
    }

    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
        if (r[RegisterFile::Callee].jsValue(exec))
            profiler->didExecute(exec, static_cast<JSObject*>(r[RegisterFile::Callee].jsValue(exec)));
        else
            profiler->didExecute(exec, codeBlock->ownerNode->sourceURL(), codeBlock->ownerNode->lineNo());
    }

    // If this call frame created an activation or an 'arguments' object, tear it off.
    if (oldCodeBlock->codeType == FunctionCode && oldCodeBlock->needsFullScopeChain) {
        while (!scopeChain->object->isObject(&JSActivation::info))
            scopeChain = scopeChain->pop();
        JSActivation* activation = static_cast<JSActivation*>(scopeChain->object);
        ASSERT(activation->isObject(&JSActivation::info));

        Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue());
        ASSERT(!arguments || arguments->isObject(&Arguments::info));

        activation->copyRegisters(arguments);
    } else if (Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue())) {
        ASSERT(arguments->isObject(&Arguments::info));
        if (!arguments->isTornOff())
            arguments->copyRegisters();
    }

    if (oldCodeBlock->needsFullScopeChain)
        scopeChain->deref();

    void* returnPC = r[RegisterFile::ReturnPC].v();
    r = r[RegisterFile::CallerRegisters].r();
    exec = CallFrame::create(r);
    if (isHostCallFrame(r))
        return false;

    codeBlock = this->codeBlock(r);
    vPC = vPCForPC(codeBlock, returnPC);
    return true;
}

NEVER_INLINE Instruction* Machine::throwException(ExecState* exec, JSValue*& exceptionValue, const Instruction* vPC, Register*& r, bool explicitThrow)
{
    // Set up the exception object
    
    CodeBlock* codeBlock = this->codeBlock(r);
    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (exception->isNotAnObjectErrorStub()) {
            exception = createNotAnObjectError(exec, static_cast<JSNotAnObjectErrorStub*>(exception), vPC, codeBlock);
            exceptionValue = exception;
        } else {
            if (!exception->hasProperty(exec, Identifier(exec, "line")) && 
                !exception->hasProperty(exec, Identifier(exec, "sourceId")) && 
                !exception->hasProperty(exec, Identifier(exec, "sourceURL")) && 
                !exception->hasProperty(exec, Identifier(exec, expressionBeginOffsetPropertyName)) && 
                !exception->hasProperty(exec, Identifier(exec, expressionCaretOffsetPropertyName)) && 
                !exception->hasProperty(exec, Identifier(exec, expressionEndOffsetPropertyName))) {
                if (explicitThrow) {
                    int startOffset = 0;
                    int endOffset = 0;
                    int divotPoint = 0;
                    int line = codeBlock->expressionRangeForVPC(vPC, divotPoint, startOffset, endOffset);
                    exception->putWithAttributes(exec, Identifier(exec, "line"), jsNumber(exec, line), ReadOnly | DontDelete);
                    
                    // We only hit this path for error messages and throw statements, which don't have a specific failure position
                    // So we just give the full range of the error/throw statement.
                    exception->putWithAttributes(exec, Identifier(exec, expressionBeginOffsetPropertyName), jsNumber(exec, divotPoint - startOffset), ReadOnly | DontDelete);
                    exception->putWithAttributes(exec, Identifier(exec, expressionEndOffsetPropertyName), jsNumber(exec, divotPoint + endOffset), ReadOnly | DontDelete);
                } else
                    exception->putWithAttributes(exec, Identifier(exec, "line"), jsNumber(exec, codeBlock->lineNumberForVPC(vPC)), ReadOnly | DontDelete);
                exception->putWithAttributes(exec, Identifier(exec, "sourceId"), jsNumber(exec, codeBlock->ownerNode->sourceID()), ReadOnly | DontDelete);
                exception->putWithAttributes(exec, Identifier(exec, "sourceURL"), jsOwnedString(exec, codeBlock->ownerNode->sourceURL()), ReadOnly | DontDelete);
            }
            
            if (exception->isWatchdogException()) {
                while (unwindCallFrame(exec, exceptionValue, vPC, codeBlock, r)) {
                    // Don't need handler checks or anything, we just want to unroll all the JS callframes possible.
                }
                return 0;
            }
        }
    }

    if (Debugger* debugger = exec->dynamicGlobalObject()->debugger()) {
        ScopeChainNode* scopeChain = this->scopeChain(r);
        DebuggerCallFrame debuggerCallFrame(exec->dynamicGlobalObject(), codeBlock, scopeChain, r, exceptionValue);
        debugger->exception(debuggerCallFrame, codeBlock->ownerNode->sourceID(), codeBlock->lineNumberForVPC(vPC));
    }

    // Calculate an exception handler vPC, unwinding call frames as necessary.

    int scopeDepth;
    Instruction* handlerVPC;

    while (!codeBlock->getHandlerForVPC(vPC, handlerVPC, scopeDepth)) {
        if (!unwindCallFrame(exec, exceptionValue, vPC, codeBlock, r))
            return 0;
    }

    // Now unwind the scope chain within the exception handler's call frame.

    ScopeChain sc(this->scopeChain(r));
    int scopeDelta = depth(codeBlock, sc) - scopeDepth;
    ASSERT(scopeDelta >= 0);
    while (scopeDelta--)
        sc.pop();
    r[RegisterFile::ScopeChain] = sc.node();

    return handlerVPC;
}

class DynamicGlobalObjectScope {
public:
    DynamicGlobalObjectScope(ExecState* exec, JSGlobalObject* dynamicGlobalObject) 
        : m_exec(exec)
        , m_savedGlobalObject(exec->globalData().dynamicGlobalObject)
    {
        exec->globalData().dynamicGlobalObject = dynamicGlobalObject;
    }

    ~DynamicGlobalObjectScope()
    {
        m_exec->globalData().dynamicGlobalObject = m_savedGlobalObject;
    }

private:
    ExecState* m_exec;
    JSGlobalObject* m_savedGlobalObject;
};

JSValue* Machine::execute(ProgramNode* programNode, ExecState* exec, ScopeChainNode* scopeChain, JSObject* thisObj, JSValue** exception)
{
    ASSERT(!exec->hadException());

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return jsNull();
    }

    CodeBlock* codeBlock = &programNode->byteCode(scopeChain);

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = oldEnd + codeBlock->numParameters + RegisterFile::CallFrameHeaderSize + codeBlock->numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(exec);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(exec, scopeChain->globalObject());

    JSGlobalObject* lastGlobalObject = m_registerFile.globalObject();
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();
    globalObject->copyGlobalsTo(m_registerFile);

    Register* r = oldEnd + codeBlock->numParameters + RegisterFile::CallFrameHeaderSize;
    r[codeBlock->thisRegister] = thisObj;
    initializeCallFrame(r, codeBlock, 0, scopeChain, makeHostCallFramePointer(0), 0, 0, 0);

    if (codeBlock->needsFullScopeChain)
        scopeChain = scopeChain->copy();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, programNode->sourceURL(), programNode->lineNo());

    m_reentryDepth++;
#if ENABLE(CTI)
    if (!codeBlock->ctiCode)
        CTI::compile(this, exec, codeBlock);
    JSValue* result = CTI::execute(codeBlock->ctiCode, &m_registerFile, r, scopeChain->globalData, exception);
#else
    JSValue* result = privateExecute(Normal, &m_registerFile, r, exception);
#endif
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler)
        (*profiler)->didExecute(exec, programNode->sourceURL(), programNode->lineNo());

    if (m_reentryDepth && lastGlobalObject && globalObject != lastGlobalObject)
        lastGlobalObject->copyGlobalsTo(m_registerFile);

    m_registerFile.shrink(oldEnd);

    return result;
}

JSValue* Machine::execute(FunctionBodyNode* functionBodyNode, ExecState* exec, JSFunction* function, JSObject* thisObj, const ArgList& args, ScopeChainNode* scopeChain, JSValue** exception)
{
    ASSERT(!exec->hadException());

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return jsNull();
    }

    Register* oldEnd = m_registerFile.end();
    int argc = 1 + args.size(); // implicit "this" parameter

    if (!m_registerFile.grow(oldEnd + argc)) {
        *exception = createStackOverflowError(exec);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(exec, exec->globalData().dynamicGlobalObject ? exec->globalData().dynamicGlobalObject : scopeChain->globalObject());

    Register* argv = oldEnd;
    size_t dst = 0;
    argv[dst] = thisObj;

    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it)
        argv[++dst] = *it;

    CodeBlock* codeBlock = &functionBodyNode->byteCode(scopeChain);
    Register* r = slideRegisterWindowForCall(codeBlock, &m_registerFile, argv, argc + RegisterFile::CallFrameHeaderSize, argc);
    if (UNLIKELY(!r)) {
        *exception = createStackOverflowError(exec);
        m_registerFile.shrink(oldEnd);
        return jsNull();
    }
    // a 0 codeBlock indicates a built-in caller
    initializeCallFrame(r, codeBlock, 0, scopeChain, makeHostCallFramePointer(exec->registers()), 0, argc, function);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, function);

    m_reentryDepth++;
#if ENABLE(CTI)
    if (!codeBlock->ctiCode)
        CTI::compile(this, exec, codeBlock);
    JSValue* result = CTI::execute(codeBlock->ctiCode, &m_registerFile, r, scopeChain->globalData, exception);
#else
    JSValue* result = privateExecute(Normal, &m_registerFile, r, exception);
#endif
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    m_registerFile.shrink(oldEnd);
    return result;
}

JSValue* Machine::execute(EvalNode* evalNode, ExecState* exec, JSObject* thisObj, ScopeChainNode* scopeChain, JSValue** exception)
{
    return execute(evalNode, exec, thisObj, m_registerFile.size() + evalNode->byteCode(scopeChain).numParameters + RegisterFile::CallFrameHeaderSize, scopeChain, exception);
}

JSValue* Machine::execute(EvalNode* evalNode, ExecState* exec, JSObject* thisObj, int registerOffset, ScopeChainNode* scopeChain, JSValue** exception)
{
    ASSERT(!exec->hadException());

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(exec, exec->globalData().dynamicGlobalObject ? exec->globalData().dynamicGlobalObject : scopeChain->globalObject());

    EvalCodeBlock* codeBlock = &evalNode->byteCode(scopeChain);

    JSVariableObject* variableObject;
    for (ScopeChainNode* node = scopeChain; ; node = node->next) {
        ASSERT(node);
        if (node->object->isVariableObject()) {
            variableObject = static_cast<JSVariableObject*>(node->object);
            break;
        }
    }

    { // Scope for BatchedTransitionOptimizer

        BatchedTransitionOptimizer optimizer(variableObject);

        const Node::VarStack& varStack = codeBlock->ownerNode->varStack();
        Node::VarStack::const_iterator varStackEnd = varStack.end();
        for (Node::VarStack::const_iterator it = varStack.begin(); it != varStackEnd; ++it) {
            const Identifier& ident = (*it).first;
            if (!variableObject->hasProperty(exec, ident)) {
                PutPropertySlot slot;
                variableObject->put(exec, ident, jsUndefined(), slot);
            }
        }

        const Node::FunctionStack& functionStack = codeBlock->ownerNode->functionStack();
        Node::FunctionStack::const_iterator functionStackEnd = functionStack.end();
        for (Node::FunctionStack::const_iterator it = functionStack.begin(); it != functionStackEnd; ++it) {
            PutPropertySlot slot;
            variableObject->put(exec, (*it)->m_ident, (*it)->makeFunction(exec, scopeChain), slot);
        }

    }

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = m_registerFile.start() + registerOffset + codeBlock->numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(exec);
        return jsNull();
    }

    Register* r = m_registerFile.start() + registerOffset;

    // a 0 codeBlock indicates a built-in caller
    r[codeBlock->thisRegister] = thisObj;
    initializeCallFrame(r, codeBlock, 0, scopeChain, makeHostCallFramePointer(exec->registers()), 0, 0, 0);

    if (codeBlock->needsFullScopeChain)
        scopeChain = scopeChain->copy();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, evalNode->sourceURL(), evalNode->lineNo());

    m_reentryDepth++;
#if ENABLE(CTI)
    if (!codeBlock->ctiCode)
        CTI::compile(this, exec, codeBlock);
    JSValue* result = CTI::execute(codeBlock->ctiCode, &m_registerFile, r, scopeChain->globalData, exception);
#else
    JSValue* result = privateExecute(Normal, &m_registerFile, r, exception);
#endif
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler)
        (*profiler)->didExecute(exec, evalNode->sourceURL(), evalNode->lineNo());

    m_registerFile.shrink(oldEnd);
    return result;
}

NEVER_INLINE void Machine::debug(ExecState* exec, Register* r, DebugHookID debugHookID, int firstLine, int lastLine)
{
    Debugger* debugger = exec->dynamicGlobalObject()->debugger();
    if (!debugger)
        return;

    CodeBlock* codeBlock = this->codeBlock(r);
    ScopeChainNode* scopeChain = this->scopeChain(r);
    DebuggerCallFrame debuggerCallFrame(exec->dynamicGlobalObject(), codeBlock, scopeChain, r, 0);

    switch (debugHookID) {
        case DidEnterCallFrame:
            debugger->callEvent(debuggerCallFrame, codeBlock->ownerNode->sourceID(), firstLine);
            return;
        case WillLeaveCallFrame:
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerNode->sourceID(), lastLine);
            return;
        case WillExecuteStatement:
            debugger->atStatement(debuggerCallFrame, codeBlock->ownerNode->sourceID(), firstLine);
            return;
        case WillExecuteProgram:
            debugger->willExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceID(), firstLine);
            return;
        case DidExecuteProgram:
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceID(), lastLine);
            return;
        case DidReachBreakpoint:
            debugger->didReachBreakpoint(debuggerCallFrame, codeBlock->ownerNode->sourceID(), lastLine);
            return;
    }
}

void Machine::resetTimeoutCheck()
{
    m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    m_timeAtLastCheckTimeout = 0;
    m_timeExecuting = 0;
}

// Returns the time the current thread has spent executing, in milliseconds.
static inline unsigned getCPUTime()
{
#if PLATFORM(DARWIN)
    mach_msg_type_number_t infoCount = THREAD_BASIC_INFO_COUNT;
    thread_basic_info_data_t info;

    // Get thread information
    thread_info(mach_thread_self(), THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &infoCount);
    
    unsigned time = info.user_time.seconds * 1000 + info.user_time.microseconds / 1000;
    time += info.system_time.seconds * 1000 + info.system_time.microseconds / 1000;
    
    return time;
#elif HAVE(SYS_TIME_H)
    // FIXME: This should probably use getrusage with the RUSAGE_THREAD flag.
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    return t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    union {
        FILETIME fileTime;
        unsigned long long fileTimeAsLong;
    } userTime, kernelTime;
    
    // GetThreadTimes won't accept NULL arguments so we pass these even though
    // they're not used.
    FILETIME creationTime, exitTime;
    
    GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime.fileTime, &userTime.fileTime);
    
    return userTime.fileTimeAsLong / 10000 + kernelTime.fileTimeAsLong / 10000;
#else
#error Platform does not have getCurrentTime function
#endif
}

// We have to return a JSValue here, gcc seems to produce worse code if 
// we attempt to return a bool
ALWAYS_INLINE JSValue* Machine::checkTimeout(JSGlobalObject* globalObject)
{
    unsigned currentTime = getCPUTime();
    
    if (!m_timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        m_timeAtLastCheckTimeout = currentTime;
        return 0;
    }
    
    unsigned timeDiff = currentTime - m_timeAtLastCheckTimeout;
    
    if (timeDiff == 0)
        timeDiff = 1;
    
    m_timeExecuting += timeDiff;
    m_timeAtLastCheckTimeout = currentTime;
    
    // Adjust the tick threshold so we get the next checkTimeout call in the interval specified in 
    // preferredScriptCheckTimeInterval
    m_ticksUntilNextTimeoutCheck = static_cast<unsigned>((static_cast<float>(preferredScriptCheckTimeInterval) / timeDiff) * m_ticksUntilNextTimeoutCheck);
    // If the new threshold is 0 reset it to the default threshold. This can happen if the timeDiff is higher than the
    // preferred script check time interval.
    if (m_ticksUntilNextTimeoutCheck == 0)
        m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    
    if (m_timeoutTime && m_timeExecuting > m_timeoutTime) {
        if (globalObject->shouldInterruptScript())
            return jsNull(); // Appeasing GCC, all we need is a non-null js value.
        
        resetTimeoutCheck();
    }
    
    return 0;
}

NEVER_INLINE ScopeChainNode* Machine::createExceptionScope(ExecState* exec, const Instruction* vPC, Register* r)
{
    int dst = (++vPC)->u.operand;
    CodeBlock* codeBlock = this->codeBlock(r);
    Identifier& property = codeBlock->identifiers[(++vPC)->u.operand];
    JSValue* value = r[(++vPC)->u.operand].jsValue(exec);
    JSObject* scope = new (exec) JSStaticScopeObject(exec, property, value, DontDelete);
    r[dst] = scope;

    return scopeChain(r)->push(scope);
}

static StructureIDChain* cachePrototypeChain(ExecState* exec, StructureID* structureID)
{
    JSValue* prototype = structureID->prototypeForLookup(exec);
    if (JSImmediate::isImmediate(prototype))
        return 0;
    RefPtr<StructureIDChain> chain = StructureIDChain::create(static_cast<JSObject*>(prototype)->structureID());
    structureID->setCachedPrototypeChain(chain.release());
    return structureID->cachedPrototypeChain();
}

NEVER_INLINE void Machine::tryCachePutByID(ExecState* exec, CodeBlock* codeBlock, Instruction* vPC, JSValue* baseValue, const PutPropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_put_by_id))
        return;

    if (JSImmediate::isImmediate(baseValue))
        return;

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }
    
    JSCell* baseCell = static_cast<JSCell*>(baseValue);
    StructureID* structureID = baseCell->structureID();

    if (structureID->isDictionary()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Cache miss: record StructureID to compare against next time.
    StructureID* lastStructureID = vPC[4].u.structureID;
    if (structureID != lastStructureID) {
        // First miss: record StructureID to compare against next time.
        if (!lastStructureID) {
            vPC[4] = structureID;
            return;
        }

        // Second miss: give up.
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Cache hit: Specialize instruction and ref StructureIDs.

    // If baseCell != slot.base(), then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // StructureID transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        vPC[0] = getOpcode(op_put_by_id_transition);
        vPC[4] = structureID->previousID();
        vPC[5] = structureID;
        StructureIDChain* chain = structureID->cachedPrototypeChain();
        if (!chain) {
            chain = cachePrototypeChain(exec, structureID);
            if (!chain) {
                // This happens if someone has manually inserted null into the prototype chain
                vPC[0] = getOpcode(op_put_by_id_generic);
                return;
            }
        }
        vPC[6] = chain;
        vPC[7] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);
        return;
    }

    vPC[0] = getOpcode(op_put_by_id_replace);
    vPC[5] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);
}

NEVER_INLINE void Machine::uncachePutByID(CodeBlock* codeBlock, Instruction* vPC)
{
    codeBlock->derefStructureIDs(vPC);
    vPC[0] = getOpcode(op_put_by_id);
    vPC[4] = 0;
}

NEVER_INLINE void Machine::tryCacheGetByID(ExecState* exec, CodeBlock* codeBlock, Instruction* vPC, JSValue* baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_get_by_id))
        return;

    // FIXME: Cache property access for immediates.
    if (JSImmediate::isImmediate(baseValue)) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    if (isJSArray(baseValue) && propertyName == exec->propertyNames().length) {
        vPC[0] = getOpcode(op_get_array_length);
        return;
    }

    if (isJSString(baseValue) && propertyName == exec->propertyNames().length) {
        vPC[0] = getOpcode(op_get_string_length);
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    StructureID* structureID = static_cast<JSCell*>(baseValue)->structureID();

    if (structureID->isDictionary()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    // Cache miss
    StructureID* lastStructureID = vPC[4].u.structureID;
    if (structureID != lastStructureID) {
        // First miss: record StructureID to compare against next time.
        if (!lastStructureID) {
            vPC[4] = structureID;
            return;
        }

        // Second miss: give up.
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    // Cache hit: Specialize instruction and ref StructureIDs.

    if (slot.slotBase() == baseValue) {
        vPC[0] = getOpcode(op_get_by_id_self);
        vPC[5] = slot.cachedOffset();

        codeBlock->refStructureIDs(vPC);
        return;
    }

    if (slot.slotBase() == structureID->prototypeForLookup(exec)) {
        ASSERT(slot.slotBase()->isObject());

        JSObject* baseObject = static_cast<JSObject*>(slot.slotBase());

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (baseObject->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(baseObject->structureID());
            baseObject->setStructureID(transition.release());
            static_cast<JSObject*>(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[5] = baseObject->structureID();
        vPC[6] = slot.cachedOffset();

        codeBlock->refStructureIDs(vPC);
        return;
    }

    size_t count = 0;
    JSObject* o = static_cast<JSObject*>(baseValue);
    while (slot.slotBase() != o) {
        JSValue* v = o->structureID()->prototypeForLookup(exec);

        // If we didn't find base in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.
        if (v->isNull()) {
            vPC[0] = getOpcode(op_get_by_id_generic);
            return;
        }

        o = static_cast<JSObject*>(v);

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (o->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(o->structureID());
            o->setStructureID(transition.release());
            static_cast<JSObject*>(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        ++count;
    }

    StructureIDChain* chain = structureID->cachedPrototypeChain();
    if (!chain)
        chain = cachePrototypeChain(exec, structureID);
    ASSERT(chain);

    vPC[0] = getOpcode(op_get_by_id_chain);
    vPC[4] = structureID;
    vPC[5] = chain;
    vPC[6] = count;
    vPC[7] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);
}

NEVER_INLINE void Machine::uncacheGetByID(CodeBlock* codeBlock, Instruction* vPC)
{
    codeBlock->derefStructureIDs(vPC);
    vPC[0] = getOpcode(op_get_by_id);
    vPC[4] = 0;
}

JSValue* Machine::privateExecute(ExecutionFlag flag, RegisterFile* registerFile, Register* r, JSValue** exception)
{
    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function.
    if (flag == InitializeAndReturn) {
        #if HAVE(COMPUTED_GOTO)
            #define ADD_OPCODE(id) m_opcodeTable[id] = &&id;
                FOR_EACH_OPCODE_ID(ADD_OPCODE);
            #undef ADD_OPCODE

            #define ADD_OPCODE_ID(id) m_opcodeIDTable.add(&&id, id);
                FOR_EACH_OPCODE_ID(ADD_OPCODE_ID);
            #undef ADD_OPCODE_ID
            ASSERT(m_opcodeIDTable.size() == numOpcodeIDs);
            op_throw_end_indirect = &&op_throw_end;
            op_call_indirect = &&op_call;
        #endif // HAVE(COMPUTED_GOTO)
        return 0;
    }

#if ENABLE(CTI)
    // Currently with CTI enabled we never interpret functions
    ASSERT_NOT_REACHED();
#endif

#define exec CallFrame::create(r)

    JSGlobalData* globalData = &exec->globalData();
    JSValue* exceptionValue = 0;
    Instruction* handlerVPC = 0;

    Instruction* vPC = this->codeBlock(r)->instructions.begin();
    Profiler** enabledProfilerReference = Profiler::enabledProfilerReference();
    unsigned tickCount = m_ticksUntilNextTimeoutCheck + 1;

#define VM_CHECK_EXCEPTION() \
    do { \
        if (UNLIKELY(globalData->exception != 0)) { \
            exceptionValue = globalData->exception; \
            goto vm_throw; \
        } \
    } while (0)

#if DUMP_OPCODE_STATS
    OpcodeStats::resetLastInstruction();
#endif

#define CHECK_FOR_TIMEOUT() \
    if (!--tickCount) { \
        if ((exceptionValue = checkTimeout(exec->dynamicGlobalObject()))) \
            goto vm_throw; \
        tickCount = m_ticksUntilNextTimeoutCheck; \
    }

#if HAVE(COMPUTED_GOTO)
    #define NEXT_OPCODE MACHINE_SAMPLING_sample(this->codeBlock(r), vPC); goto *vPC->u.opcode
#if DUMP_OPCODE_STATS
    #define BEGIN_OPCODE(opcode) opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define BEGIN_OPCODE(opcode) opcode:
#endif
    NEXT_OPCODE;
#else
    #define NEXT_OPCODE MACHINE_SAMPLING_sample(this->codeBlock(r), vPC); continue
#if DUMP_OPCODE_STATS
    #define BEGIN_OPCODE(opcode) case opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define BEGIN_OPCODE(opcode) case opcode:
#endif
    while (1) // iterator loop begins
    switch (vPC->u.opcode)
#endif
    {
    BEGIN_OPCODE(op_new_object) {
        /* new_object dst(r)

           Constructs a new empty Object instance using the original
           constructor, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        r[dst] = constructEmptyObject(exec);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_array) {
        /* new_array dst(r) firstArg(r) argCount(n)

           Constructs a new Array instance using the original
           constructor, and puts the result in register dst.
           The array will contain argCount elements with values
           taken from registers starting at register firstArg.
        */
        int dst = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;
        ArgList args(r + firstArg, argCount);
        r[dst] = constructArray(exec, args);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int regExp = (++vPC)->u.operand;
        r[dst] = new (globalData) RegExpObject(scopeChain(r)->globalObject()->regExpStructure(), codeBlock(r)->regexps[regExp]);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = r[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) == reinterpret_cast<intptr_t>(src2));
        else {
            JSValue* result = jsBoolean(equalSlowCase(exec, src1, src2));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_eq_null) {
        /* neq dst(r) src(r)

           Checks whether register src is null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = r[(++vPC)->u.operand].jsValue(exec);

        if (src->isUndefinedOrNull()) {
            r[dst] = jsBoolean(true);
            ++vPC;
            NEXT_OPCODE;
        }
        
        r[dst] = jsBoolean(!JSImmediate::isImmediate(src) && src->asCell()->structureID()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = jsBoolean(src1 != src2);
        else {
            JSValue* result = jsBoolean(!equalSlowCase(exec, src1, src2));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_neq_null) {
        /* neq dst(r) src(r)

           Checks whether register src is not null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = r[(++vPC)->u.operand].jsValue(exec);

        if (src->isUndefinedOrNull()) {
            r[dst] = jsBoolean(false);
            ++vPC;
            NEXT_OPCODE;
        }
        
        r[dst] = jsBoolean(JSImmediate::isImmediate(src) || !static_cast<JSCell*>(src)->asCell()->structureID()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediate(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) == reinterpret_cast<intptr_t>(src2));
        else if (JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate()))
            r[dst] = jsBoolean(false);
        else
            r[dst] = jsBoolean(strictEqualSlowCase(src1, src2));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);

        if (JSImmediate::areBothImmediate(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) != reinterpret_cast<intptr_t>(src2));
        else if (JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate()))
            r[dst] = jsBoolean(true);
        else
            r[dst] = jsBoolean(!strictEqualSlowCase(src1, src2));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* result = jsBoolean(jsLess(exec, src1, src2));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* result = jsBoolean(jsLessEq(exec, src1, src2));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            r[srcDst] = JSImmediate::incImmediateNumber(v);
        else {
            JSValue* result = jsNumber(exec, v->toNumber(exec) + 1);
            VM_CHECK_EXCEPTION();
            r[srcDst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            r[srcDst] = JSImmediate::decImmediateNumber(v);
        else {
            JSValue* result = jsNumber(exec, v->toNumber(exec) - 1);
            VM_CHECK_EXCEPTION();
            r[srcDst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            r[dst] = v;
            r[srcDst] = JSImmediate::incImmediateNumber(v);
        } else {
            JSValue* number = r[srcDst].jsValue(exec)->toJSNumber(exec);
            VM_CHECK_EXCEPTION();
            r[dst] = number;
            r[srcDst] = jsNumber(exec, number->uncheckedGetNumber() + 1);
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            r[dst] = v;
            r[srcDst] = JSImmediate::decImmediateNumber(v);
        } else {
            JSValue* number = r[srcDst].jsValue(exec)->toJSNumber(exec);
            VM_CHECK_EXCEPTION();
            r[dst] = number;
            r[srcDst] = jsNumber(exec, number->uncheckedGetNumber() - 1);
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = r[src].jsValue(exec)->toJSNumber(exec);
        VM_CHECK_EXCEPTION();

        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = r[(++vPC)->u.operand].jsValue(exec);
        double v;
        if (fastIsNumber(src, v))
            r[dst] = jsNumber(exec, -v);
        else {
            JSValue* result = jsNumber(exec, -src->toNumber(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            r[dst] = JSImmediate::addImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsAdd(exec, src1, src2);
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }
        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        double left;
        double right;
        if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
            r[dst] = jsNumber(exec, left * right);
        else {
            JSValue* result = jsNumber(exec, src1->toNumber(exec) * src2->toNumber(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_div) {
        /* div dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by the
           register divisor (converted to number), and puts the
           quotient in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* dividend = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* divisor = r[(++vPC)->u.operand].jsValue(exec);
        double left;
        double right;
        if (fastIsNumber(dividend, left) && fastIsNumber(divisor, right))
            r[dst] = jsNumber(exec, left / right);
        else {
            JSValue* result = jsNumber(exec, dividend->toNumber(exec) / divisor->toNumber(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mod) {
        /* mod dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by
           register divisor (converted to number), and puts the
           remainder in register dst.
        */
        int dst = (++vPC)->u.operand;
        int dividend = (++vPC)->u.operand;
        int divisor = (++vPC)->u.operand;

        JSValue* dividendValue = r[dividend].jsValue(exec);
        JSValue* divisorValue = r[divisor].jsValue(exec);

        if (JSImmediate::areBothImmediateNumbers(dividendValue, divisorValue) && divisorValue != JSImmediate::from(0)) {
            r[dst] = JSImmediate::from(JSImmediate::getTruncatedInt32(dividendValue) % JSImmediate::getTruncatedInt32(divisorValue));
            ++vPC;
            NEXT_OPCODE;
        }

        double d = dividendValue->toNumber(exec);
        JSValue* result = jsNumber(exec, fmod(d, divisorValue->toNumber(exec)));
        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sub) {
        /* sub dst(r) src1(r) src2(r)

           Subtracts register src2 (converted to number) from register
           src1 (converted to number), and puts the difference in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        double left;
        double right;
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            r[dst] = JSImmediate::subImmediateNumbers(src1, src2);
        else if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
            r[dst] = jsNumber(exec, left - right);
        else {
            JSValue* result = jsNumber(exec, src1->toNumber(exec) - src2->toNumber(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }
        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lshift) {
        /* lshift dst(r) val(r) shift(r)

           Performs left shift of register val (converted to int32) by
           register shift (converted to uint32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* shift = r[(++vPC)->u.operand].jsValue(exec);
        int32_t left;
        uint32_t right;
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            r[dst] = jsNumber(exec, JSImmediate::getTruncatedInt32(val) << (JSImmediate::getTruncatedUInt32(shift) & 0x1f));
        else if (fastToInt32(val, left) && fastToUInt32(shift, right))
            r[dst] = jsNumber(exec, left << (right & 0x1f));
        else {
            JSValue* result = jsNumber(exec, (val->toInt32(exec)) << (shift->toUInt32(exec) & 0x1f));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_rshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs arithmetic right shift of register val (converted
           to int32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* shift = r[(++vPC)->u.operand].jsValue(exec);
        int32_t left;
        uint32_t right;
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            r[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else if (fastToInt32(val, left) && fastToUInt32(shift, right))
            r[dst] = jsNumber(exec, left >> (right & 0x1f));
        else {
            JSValue* result = jsNumber(exec, (val->toInt32(exec)) >> (shift->toUInt32(exec) & 0x1f));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* shift = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
            r[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else {
            JSValue* result = jsNumber(exec, (val->toUInt32(exec)) >> (shift->toUInt32(exec) & 0x1f));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitand) {
        /* bitand dst(r) src1(r) src2(r)

           Computes bitwise AND of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int32_t left;
        int32_t right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = JSImmediate::andImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            r[dst] = jsNumber(exec, left & right);
        else {
            JSValue* result = jsNumber(exec, src1->toInt32(exec) & src2->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitxor) {
        /* bitxor dst(r) src1(r) src2(r)

           Computes bitwise XOR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int32_t left;
        int32_t right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = JSImmediate::xorImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            r[dst] = jsNumber(exec, left ^ right);
        else {
            JSValue* result = jsNumber(exec, src1->toInt32(exec) ^ src2->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitor) {
        /* bitor dst(r) src1(r) src2(r)

           Computes bitwise OR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int32_t left;
        int32_t right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = JSImmediate::orImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            r[dst] = jsNumber(exec, left | right);
        else {
            JSValue* result = jsNumber(exec, src1->toInt32(exec) | src2->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = r[(++vPC)->u.operand].jsValue(exec);
        int32_t value;
        if (fastToInt32(src, value))
            r[dst] = jsNumber(exec, ~value);
        else {
            JSValue* result = jsNumber(exec, ~src->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_not) {
        /* not dst(r) src(r)

           Computes logical NOT of register src (converted to
           boolean), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = jsBoolean(!r[src].jsValue(exec)->toBoolean(exec));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_instanceof) {
        /* instanceof dst(r) value(r) constructor(r) constructorProto(r)

           Tests whether register value is an instance of register
           constructor, and puts the boolean result in register
           dst. Register constructorProto must contain the "prototype"
           property (not the actual prototype) of the object in
           register constructor. This lookup is separated so that
           polymorphic inline caching can apply.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int baseProto = (++vPC)->u.operand;

        JSValue* baseVal = r[base].jsValue(exec);

        if (isNotObject(exec, true, codeBlock(r), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = static_cast<JSObject*>(baseVal);
        r[dst] = jsBoolean(baseObj->structureID()->typeInfo().implementsHasInstance() ? baseObj->hasInstance(exec, r[value].jsValue(exec), r[baseProto].jsValue(exec)) : false);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsTypeStringForValue(exec, r[src].jsValue(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_undefined) {
        /* is_undefined dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "undefined", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* v = r[src].jsValue(exec);
        r[dst] = jsBoolean(JSImmediate::isImmediate(v) ? v->isUndefined() : v->asCell()->structureID()->typeInfo().masqueradesAsUndefined());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_boolean) {
        /* is_boolean dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "boolean", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsBoolean(r[src].jsValue(exec)->isBoolean());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_number) {
        /* is_number dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "number", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsBoolean(r[src].jsValue(exec)->isNumber());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_string) {
        /* is_string dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "string", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsBoolean(r[src].jsValue(exec)->isString());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_object) {
        /* is_object dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "object", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsBoolean(jsIsObjectType(r[src].jsValue(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_function) {
        /* is_function dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "function", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsBoolean(jsIsFunctionType(r[src].jsValue(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_in) {
        /* in dst(r) property(r) base(r)

           Tests whether register base has a property named register
           property, and puts the boolean result in register dst.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        JSValue* baseVal = r[base].jsValue(exec);
        if (isNotObject(exec, false, codeBlock(r), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = static_cast<JSObject*>(baseVal);

        JSValue* propName = r[property].jsValue(exec);

        uint32_t i;
        if (propName->getUInt32(i))
            r[dst] = jsBoolean(baseObj->hasProperty(exec, i));
        else {
            Identifier property(exec, propName->toString(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = jsBoolean(baseObj->hasProperty(exec, property));
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve) {
        /* resolve dst(r) property(id)

           Looks up the property named by identifier property in the
           scope chain, and writes the resulting value to register
           dst. If the property is not found, raises an exception.
        */
        if (UNLIKELY(!resolve(exec, vPC, r, exceptionValue)))
            goto vm_throw;

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_skip) {
        /* resolve_skip dst(r) property(id) skip(n)

         Looks up the property named by identifier property in the
         scope chain skipping the top 'skip' levels, and writes the resulting
         value to register dst. If the property is not found, raises an exception.
         */
        if (UNLIKELY(!resolveSkip(exec, vPC, r, exceptionValue)))
            goto vm_throw;

        vPC += 4;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_global) {
        /* resolve_skip dst(r) globalObject(c) property(id) structureID(sID) offset(n)
         
           Performs a dynamic property lookup for the given property, on the provided
           global object.  If structureID matches the StructureID of the global then perform
           a fast lookup using the case offset, otherwise fall back to a full resolve and
           cache the new structureID and offset
         */
        if (UNLIKELY(!resolveGlobal(exec, vPC, r,  exceptionValue)))
            goto vm_throw;
        
        vPC += 6;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_global_var) {
        /* get_global_var dst(r) globalObject(c) index(n)

           Gets the global var at global slot index and places it in register dst.
         */
        int dst = (++vPC)->u.operand;
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;

        r[dst] = scope->registerAt(index);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_global_var) {
        /* put_global_var globalObject(c) index(n) value(r)
         
           Puts value into global slot index.
         */
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        
        scope->registerAt(index) = r[value].jsValue(exec);
        ++vPC;
        NEXT_OPCODE;
    }            
    BEGIN_OPCODE(op_get_scoped_var) {
        /* get_scoped_var dst(r) index(n) skip(n)

         Loads the contents of the index-th local from the scope skip nodes from
         the top of the scope chain, and places it in register dst
         */
        int dst = (++vPC)->u.operand;
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + codeBlock(r)->needsFullScopeChain;

        ScopeChainNode* scopeChain = this->scopeChain(r);
        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);
        while (skip--) {
            ++iter;
            ASSERT(iter != end);
        }

        ASSERT((*iter)->isVariableObject());
        JSVariableObject* scope = static_cast<JSVariableObject*>(*iter);
        r[dst] = scope->registerAt(index);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_scoped_var) {
        /* put_scoped_var index(n) skip(n) value(r)

         */
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + codeBlock(r)->needsFullScopeChain;
        int value = (++vPC)->u.operand;

        ScopeChainNode* scopeChain = this->scopeChain(r);
        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);
        while (skip--) {
            ++iter;
            ASSERT(iter != end);
        }

        ASSERT((*iter)->isVariableObject());
        JSVariableObject* scope = static_cast<JSVariableObject*>(*iter);
        scope->registerAt(index) = r[value].jsValue(exec);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base) {
        /* resolve_base dst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register dst. If none is found, the outermost scope (which
           will be the global object) is stored in register dst.
        */
        resolveBase(exec, vPC, r);

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_with_base) {
        /* resolve_with_base baseDst(r) propDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register srcDst, and the retrieved property value to register
           propDst. If the property is not found, raises an exception.

           This is more efficient than doing resolve_base followed by
           resolve, or resolve_base followed by get_by_id, as it
           avoids duplicate hash lookups.
        */
        if (UNLIKELY(!resolveBaseAndProperty(exec, vPC, r, exceptionValue)))
            goto vm_throw;

        vPC += 4;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_func) {
        /* resolve_func baseDst(r) funcDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes the
           appropriate object to use as "this" when calling its
           properties to register baseDst; and the retrieved property
           value to register propDst. If the property is not found,
           raises an exception.

           This differs from resolve_with_base, because the
           global this value will be substituted for activations or
           the global object, which is the right behavior for function
           calls but not for other property lookup.
        */
        if (UNLIKELY(!resolveBaseAndFunc(exec, vPC, r, exceptionValue)))
            goto vm_throw;

        vPC += 4;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id) {
        /* get_by_id dst(r) base(r) property(id) structureID(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        CodeBlock* codeBlock = this->codeBlock(r);
        Identifier& ident = codeBlock->identifiers[property];
        JSValue* baseValue = r[base].jsValue(exec);
        PropertySlot slot(baseValue);
        JSValue* result = baseValue->get(exec, ident, slot);
        VM_CHECK_EXCEPTION();

        tryCacheGetByID(exec, codeBlock, vPC, baseValue, ident, slot);

        r[dst] = result;
        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_self) {
        /* op_get_by_id_self dst(r) base(r) property(id) structureID(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base. If the cache misses, op_get_by_id_self reverts to
           op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = static_cast<JSCell*>(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = static_cast<JSObject*>(baseCell);
                int dst = vPC[1].u.operand;
                int offset = vPC[5].u.operand;

                ASSERT(baseObject->get(exec, codeBlock(r)->identifiers[vPC[3].u.operand]) == baseObject->getDirectOffset(offset));
                r[dst] = baseObject->getDirectOffset(offset);

                vPC += 8;
                NEXT_OPCODE;
            }
        }

        uncacheGetByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_proto) {
        /* op_get_by_id_proto dst(r) base(r) property(id) structureID(sID) protoStructureID(sID) offset(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype. If the cache misses, op_get_by_id_proto
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = static_cast<JSCell*>(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                ASSERT(structureID->prototypeForLookup(exec)->isObject());
                JSObject* protoObject = static_cast<JSObject*>(structureID->prototypeForLookup(exec));
                StructureID* protoStructureID = vPC[5].u.structureID;

                if (LIKELY(protoObject->structureID() == protoStructureID)) {
                    int dst = vPC[1].u.operand;
                    int offset = vPC[6].u.operand;

                    ASSERT(protoObject->get(exec, codeBlock(r)->identifiers[vPC[3].u.operand]) == protoObject->getDirectOffset(offset));
                    r[dst] = protoObject->getDirectOffset(offset);

                    vPC += 8;
                    NEXT_OPCODE;
                }
            }
        }

        uncacheGetByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_chain) {
        /* op_get_by_id_chain dst(r) base(r) property(id) structureID(sID) structureIDChain(sIDc) count(n) offset(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype chain. If the cache misses, op_get_by_id_chain
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = static_cast<JSCell*>(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                RefPtr<StructureID>* it = vPC[5].u.structureIDChain->head();
                size_t count = vPC[6].u.operand;
                RefPtr<StructureID>* end = it + count;

                JSObject* baseObject = static_cast<JSObject*>(baseCell);
                while (1) {
                    baseObject = static_cast<JSObject*>(baseObject->structureID()->prototypeForLookup(exec));
                    if (UNLIKELY(baseObject->structureID() != (*it).get()))
                        break;

                    if (++it == end) {
                        int dst = vPC[1].u.operand;
                        int offset = vPC[7].u.operand;

                        ASSERT(baseObject->get(exec, codeBlock(r)->identifiers[vPC[3].u.operand]) == baseObject->getDirectOffset(offset));
                        r[dst] = baseObject->getDirectOffset(offset);

                        vPC += 8;
                        NEXT_OPCODE;
                    }
                }
            }
        }

        uncacheGetByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_generic) {
        /* op_get_by_id_generic dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        Identifier& ident = codeBlock(r)->identifiers[property];
        JSValue* baseValue = r[base].jsValue(exec);
        PropertySlot slot(baseValue);
        JSValue* result = baseValue->get(exec, ident, slot);
        VM_CHECK_EXCEPTION();

        r[dst] = result;
        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_array_length) {
        /* op_get_array_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the array in register base,
           and puts the result in register dst. If register base does not hold
           an array, op_get_array_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);
        if (LIKELY(isJSArray(baseValue))) {
            int dst = vPC[1].u.operand;
            r[dst] = jsNumber(exec, static_cast<JSArray*>(baseValue)->length());
            vPC += 8;
            NEXT_OPCODE;
        }

        uncacheGetByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_string_length) {
        /* op_get_string_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the string in register base,
           and puts the result in register dst. If register base does not hold
           a string, op_get_string_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);
        if (LIKELY(isJSString(baseValue))) {
            int dst = vPC[1].u.operand;
            r[dst] = jsNumber(exec, static_cast<JSString*>(baseValue)->value().size());
            vPC += 8;
            NEXT_OPCODE;
        }

        uncacheGetByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id) {
        /* put_by_id base(r) property(id) value(r) nop(n) nop(n) nop(n) nop(n)

           Generic property access: Sets the property named by identifier
           property, belonging to register base, to register value.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */

        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int value = vPC[3].u.operand;

        CodeBlock* codeBlock = this->codeBlock(r);
        JSValue* baseValue = r[base].jsValue(exec);
        Identifier& ident = codeBlock->identifiers[property];
        PutPropertySlot slot;
        baseValue->put(exec, ident, r[value].jsValue(exec), slot);
        VM_CHECK_EXCEPTION();

        tryCachePutByID(exec, codeBlock, vPC, baseValue, slot);

        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id_transition) {
        /* op_put_by_id_transition base(r) property(id) value(r) oldStructureID(sID) newStructureID(sID) structureIDChain(sIDc) offset(n)
         
           Cached property access: Attempts to set a new property with a cached transition
           property named by identifier property, belonging to register base,
           to register value. If the cache misses, op_put_by_id_transition
           reverts to op_put_by_id_generic.
         
           Unlike many opcodes, this one does not write any output to
           the register file.
         */
        int base = vPC[1].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);
        
        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = static_cast<JSCell*>(baseValue);
            StructureID* oldStructureID = vPC[4].u.structureID;
            StructureID* newStructureID = vPC[5].u.structureID;
            
            if (LIKELY(baseCell->structureID() == oldStructureID)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = static_cast<JSObject*>(baseCell);

                RefPtr<StructureID>* it = vPC[6].u.structureIDChain->head();

                JSObject* proto = static_cast<JSObject*>(baseObject->structureID()->prototypeForLookup(exec));
                while (!proto->isNull()) {
                    if (UNLIKELY(proto->structureID() != (*it).get())) {
                        uncachePutByID(codeBlock(r), vPC);
                        NEXT_OPCODE;
                    }
                    ++it;
                    proto = static_cast<JSObject*>(proto->structureID()->prototypeForLookup(exec));
                }

                baseObject->transitionTo(newStructureID);

                int value = vPC[3].u.operand;
                unsigned offset = vPC[7].u.operand;
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(codeBlock(r)->identifiers[vPC[2].u.operand])) == offset);
                baseObject->putDirectOffset(offset, r[value].jsValue(exec));

                vPC += 8;
                NEXT_OPCODE;
            }
        }
        
        uncachePutByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id_replace) {
        /* op_put_by_id_replace base(r) property(id) value(r) structureID(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to set a pre-existing, cached
           property named by identifier property, belonging to register base,
           to register value. If the cache misses, op_put_by_id_replace
           reverts to op_put_by_id.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        JSValue* baseValue = r[base].jsValue(exec);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = static_cast<JSCell*>(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = static_cast<JSObject*>(baseCell);
                int value = vPC[3].u.operand;
                unsigned offset = vPC[5].u.operand;
                
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(codeBlock(r)->identifiers[vPC[2].u.operand])) == offset);
                baseObject->putDirectOffset(offset, r[value].jsValue(exec));

                vPC += 8;
                NEXT_OPCODE;
            }
        }

        uncachePutByID(codeBlock(r), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id_generic) {
        /* op_put_by_id_generic base(r) property(id) value(r) nop(n) nop(n) nop(n) nop(n)

           Generic property access: Sets the property named by identifier
           property, belonging to register base, to register value.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int value = vPC[3].u.operand;

        JSValue* baseValue = r[base].jsValue(exec);
        Identifier& ident = codeBlock(r)->identifiers[property];
        PutPropertySlot slot;
        baseValue->put(exec, ident, r[value].jsValue(exec), slot);
        VM_CHECK_EXCEPTION();

        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_id) {
        /* del_by_id dst(r) base(r) property(id)

           Converts register base to Object, deletes the property
           named by identifier property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].jsValue(exec)->toObject(exec);
        Identifier& ident = codeBlock(r)->identifiers[property];
        JSValue* result = jsBoolean(baseObj->deleteProperty(exec, ident));
        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_val) {
        /* get_by_val dst(r) base(r) property(r)

           Converts register base to Object, gets the property named
           by register property from the object, and puts the result
           in register dst. property is nominally converted to string
           but numbers are treated more efficiently.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        
        JSValue* baseValue = r[base].jsValue(exec);
        JSValue* subscript = r[property].jsValue(exec);

        JSValue* result;
        unsigned i;

        bool isUInt32 = JSImmediate::getUInt32(subscript, i);
        if (LIKELY(isUInt32)) {
            if (isJSArray(baseValue)) {
                JSArray* jsArray = static_cast<JSArray*>(baseValue);
                if (jsArray->canGetIndex(i))
                    result = jsArray->getIndex(i);
                else
                    result = jsArray->JSArray::get(exec, i);
            } else if (isJSString(baseValue) && static_cast<JSString*>(baseValue)->canGetIndex(i))
                result = static_cast<JSString*>(baseValue)->getIndex(&exec->globalData(), i);
            else
                result = baseValue->get(exec, i);
        } else {
            Identifier property(exec, subscript->toString(exec));
            result = baseValue->get(exec, property);
        }

        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_val) {
        /* put_by_val base(r) property(r) value(r)

           Sets register value on register base as the property named
           by register property. Base is converted to object
           first. register property is nominally converted to string
           but numbers are treated more efficiently.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        JSValue* baseValue = r[base].jsValue(exec);
        JSValue* subscript = r[property].jsValue(exec);

        unsigned i;

        bool isUInt32 = JSImmediate::getUInt32(subscript, i);
        if (LIKELY(isUInt32)) {
            if (isJSArray(baseValue)) {
                JSArray* jsArray = static_cast<JSArray*>(baseValue);
                if (jsArray->canSetIndex(i))
                    jsArray->setIndex(i, r[value].jsValue(exec));
                else
                    jsArray->JSArray::put(exec, i, r[value].jsValue(exec));
            } else
                baseValue->put(exec, i, r[value].jsValue(exec));
        } else {
            Identifier property(exec, subscript->toString(exec));
            if (!exec->hadException()) { // Don't put to an object if toString threw an exception.
                PutPropertySlot slot;
                baseValue->put(exec, property, r[value].jsValue(exec), slot);
            }
        }

        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_val) {
        /* del_by_val dst(r) base(r) property(r)

           Converts register base to Object, deletes the property
           named by register property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].jsValue(exec)->toObject(exec); // may throw

        JSValue* subscript = r[property].jsValue(exec);
        JSValue* result;
        uint32_t i;
        if (subscript->getUInt32(i))
            result = jsBoolean(baseObj->deleteProperty(exec, i));
        else {
            VM_CHECK_EXCEPTION();
            Identifier property(exec, subscript->toString(exec));
            VM_CHECK_EXCEPTION();
            result = jsBoolean(baseObj->deleteProperty(exec, property));
        }

        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_index) {
        /* put_by_index base(r) property(n) value(r)

           Sets register value on register base as the property named
           by the immediate number property. Base is converted to
           object first.

           Unlike many opcodes, this one does not write any output to
           the register file.

           This opcode is mainly used to initialize array literals.
        */
        int base = (++vPC)->u.operand;
        unsigned property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        r[base].jsValue(exec)->put(exec, property, r[value].jsValue(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop) {
        /* loop target(offset)
         
           Jumps unconditionally to offset target from the current
           instruction.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
#if DUMP_OPCODE_STATS
        OpcodeStats::resetLastInstruction();
#endif
        int target = (++vPC)->u.operand;
        CHECK_FOR_TIMEOUT();
        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp) {
        /* jmp target(offset)

           Jumps unconditionally to offset target from the current
           instruction.
        */
#if DUMP_OPCODE_STATS
        OpcodeStats::resetLastInstruction();
#endif
        int target = (++vPC)->u.operand;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_true) {
        /* loop_if_true cond(r) target(offset)
         
           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (r[cond].jsValue(exec)->toBoolean(exec)) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (r[cond].jsValue(exec)->toBoolean(exec)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (!r[cond].jsValue(exec)->toBoolean(exec)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_less) {
        /* loop_if_less src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int target = (++vPC)->u.operand;
        
        bool result = jsLess(exec, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_lesseq) {
        /* loop_if_lesseq src1(r) src2(r) target(offset)

           Checks whether register src1 is less than or equal to register
           src2, as with the ECMAScript '<=' operator, and then jumps to
           offset target from the current instruction, if and only if the 
           result of the comparison is true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
        */
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int target = (++vPC)->u.operand;
        
        bool result = jsLessEq(exec, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jnless) {
        /* jnless src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is false.
        */
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int target = (++vPC)->u.operand;

        bool result = jsLess(exec, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_imm) {
        /* switch_imm tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a range checked switch on the scrutinee value, using
           the tableIndex-th immediate switch jump table.  If the scrutinee value
           is an immediate number in the range covered by the referenced jump
           table, and the value at jumpTable[scrutinee value] is non-zero, then
           that value is used as the jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValue* scrutinee = r[(++vPC)->u.operand].jsValue(exec);
        if (!JSImmediate::isNumber(scrutinee))
            vPC += defaultOffset;
        else {
            int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
            vPC += codeBlock(r)->immediateSwitchJumpTables[tableIndex].offsetForValue(value, defaultOffset);
        }
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_char) {
        /* switch_char tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a range checked switch on the scrutinee value, using
           the tableIndex-th character switch jump table.  If the scrutinee value
           is a single character string in the range covered by the referenced jump
           table, and the value at jumpTable[scrutinee value] is non-zero, then
           that value is used as the jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValue* scrutinee = r[(++vPC)->u.operand].jsValue(exec);
        if (!scrutinee->isString())
            vPC += defaultOffset;
        else {
            UString::Rep* value = static_cast<JSString*>(scrutinee)->value().rep();
            if (value->size() != 1)
                vPC += defaultOffset;
            else
                vPC += codeBlock(r)->characterSwitchJumpTables[tableIndex].offsetForValue(value->data()[0], defaultOffset);
        }
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_string) {
        /* switch_string tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a sparse hashmap based switch on the value in the scrutinee
           register, using the tableIndex-th string switch jump table.  If the 
           scrutinee value is a string that exists as a key in the referenced 
           jump table, then the value associated with the string is used as the 
           jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValue* scrutinee = r[(++vPC)->u.operand].jsValue(exec);
        if (!scrutinee->isString())
            vPC += defaultOffset;
        else 
            vPC += codeBlock(r)->stringSwitchJumpTables[tableIndex].offsetForValue(static_cast<JSString*>(scrutinee)->value().rep(), defaultOffset);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func) {
        /* new_func dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function declarations, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        r[dst] = codeBlock(r)->functions[func]->makeFunction(exec, scopeChain(r));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func_exp) {
        /* new_func_exp dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function expressions, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        r[dst] = codeBlock(r)->functionExpressions[func]->makeFunction(exec, scopeChain(r));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_call_eval) {
        /* call_eval dst(r) func(r) thisVal(r) firstArg(r) argCount(n)

           Call a function named "eval" with no explicit "this" value
           (which may therefore be the eval operator). If register
           thisVal is the global object, and register func contains
           that global object's original global eval function, then
           perform the eval operator in local scope (interpreting
           the argument registers as for the "call"
           opcode). Otherwise, act exactly as the "call" opcode would.
         */

        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;
        int thisVal = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;
        ++vPC; // registerOffset

        JSValue* funcVal = r[func].jsValue(exec);
        JSValue* baseVal = r[thisVal].jsValue(exec);

        ScopeChainNode* scopeChain = this->scopeChain(r);
        if (baseVal == scopeChain->globalObject() && funcVal == scopeChain->globalObject()->evalFunction()) {
            JSObject* thisObject = static_cast<JSObject*>(r[codeBlock(r)->thisRegister].jsValue(exec));
            JSValue* result = callEval(exec, thisObject, scopeChain, registerFile, r, firstArg, argCount, exceptionValue);
            if (exceptionValue)
                goto vm_throw;

            r[dst] = result;

            ++vPC;
            NEXT_OPCODE;
        }

        // We didn't find the blessed version of eval, so reset vPC and process
        // this instruction as a normal function call, supplying the proper 'this'
        // value.
        vPC -= 6;
        r[thisVal] = baseVal->toThisObject(exec);

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_call_indirect; // indirect goto -> op_call
#endif
        // fall through to op_call
    }
    BEGIN_OPCODE(op_call) {
        /* call dst(r) func(r) thisVal(r) firstArg(r) argCount(n) registerOffset(n)

           Perform a function call. Specifically, call register func
           with a "this" value of register thisVal, and put the result
           in register dst.

           The arguments start at register firstArg and go up to
           argCount, but the "this" value is considered an implicit
           first argument, so the argCount should be one greater than
           the number of explicit arguments passed, and the register
           after firstArg should contain the actual first
           argument. This opcode will copy from the thisVal register
           to the firstArg register, unless the register index of
           thisVal is the special missing this object marker, which is
           2^31-1; in that case, the global object will be used as the
           "this" value.

           If func is a native code function, then this opcode calls
           it and returns the value immediately. 

           But if it is a JS function, then the current scope chain
           and code block is set to the function's, and we slide the
           register window so that the arguments would form the first
           few local registers of the called function's register
           window. In addition, a call frame header is written
           immediately before the arguments; see the call frame
           documentation for an explanation of how many registers a
           call frame takes and what they contain. That many registers
           before the firstArg register will be overwritten by the
           call. In addition, any registers higher than firstArg +
           argCount may be overwritten. Once this setup is complete,
           execution continues from the called function's first
           argument, and does not return until a "ret" opcode is
           encountered.
         */

        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;
        int thisVal = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;
        int registerOffset = (++vPC)->u.operand;

        JSValue* v = r[func].jsValue(exec);

        CallData callData;
        CallType callType = v->getCallData(callData);

        if (callType == CallTypeJS) {
            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = callData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

            r[firstArg] = thisVal == missingThisObjectMarker() ? exec->globalThisValue() : r[thisVal].jsValue(exec);
            
            Register* savedR = r;

            r = slideRegisterWindowForCall(newCodeBlock, registerFile, r, registerOffset, argCount);
            if (UNLIKELY(!r)) {
                r = savedR;
                exceptionValue = createStackOverflowError(CallFrame::create(r));
                goto vm_throw;
            }

            initializeCallFrame(r, newCodeBlock, vPC + 1, callDataScopeChain, savedR, dst, argCount, v);
    
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, static_cast<JSObject*>(v));

            vPC = newCodeBlock->instructions.begin();

#if DUMP_OPCODE_STATS
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_OPCODE;
        }

        if (callType == CallTypeHost) {
            JSValue* thisValue = thisVal == missingThisObjectMarker() ? exec->globalThisValue() : r[thisVal].jsValue(exec);
            ArgList args(r + firstArg + 1, argCount - 1);

            ScopeChainNode* scopeChain = this->scopeChain(r);
            initializeCallFrame(r + registerOffset, 0, vPC + 1, scopeChain, r, dst, argCount, v);
            ExecState* callFrame = CallFrame::create(r + registerOffset);

            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(callFrame, static_cast<JSObject*>(v));

            MACHINE_SAMPLING_callingHostFunction();

            JSValue* returnValue = callData.native.function(callFrame, static_cast<JSObject*>(v), thisValue, args);
            VM_CHECK_EXCEPTION();

            r[dst] = returnValue;

            if (*enabledProfilerReference)
                (*enabledProfilerReference)->didExecute(CallFrame::create(r), static_cast<JSObject*>(v));

            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(callType == CallTypeNone);

        exceptionValue = createNotAFunctionError(exec, v, vPC, codeBlock(r));
        goto vm_throw;
    }
    BEGIN_OPCODE(op_tear_off_activation) {
        int src = (++vPC)->u.operand;
        JSActivation* activation = static_cast<JSActivation*>(r[src].getJSValue());
        ASSERT(codeBlock(r)->needsFullScopeChain);
        ASSERT(activation->isObject(&JSActivation::info));

        Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue());
        ASSERT(!arguments || arguments->isObject(&Arguments::info));
        activation->copyRegisters(arguments);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_tear_off_arguments) {
        Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue());
        ASSERT(codeBlock(r)->usesArguments && !codeBlock(r)->needsFullScopeChain);
        ASSERT(arguments->isObject(&Arguments::info));

        arguments->copyRegisters();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_ret) {
        /* ret result(r)
           
           Return register result as the return value of the current
           function call, writing it into the caller's expected return
           value register. In addition, unwind one call frame and
           restore the scope chain, code block instruction pointer and
           register base to those of the calling function.
        */

        int result = (++vPC)->u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->didExecute(exec, static_cast<JSObject*>(r[RegisterFile::Callee].jsValue(exec)));

        if (codeBlock(r)->needsFullScopeChain)
            scopeChain(r)->deref();

        JSValue* returnValue = r[result].jsValue(exec);

        vPC = r[RegisterFile::ReturnPC].vPC();
        int dst = r[RegisterFile::ReturnValueRegister].i();
        r = r[RegisterFile::CallerRegisters].r();
        
        if (isHostCallFrame(r))
            return returnValue;

        r[dst] = returnValue;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_enter) {
        /* enter

           Initializes local variables to undefined and fills constant
           registers with their values. If the code block requires an
           activation, enter_with_activation should be used instead.

           This opcode should only be used at the beginning of a code
           block.
        */

        size_t i = 0;
        CodeBlock* codeBlock = this->codeBlock(r);
        
        for (size_t count = codeBlock->numVars; i < count; ++i)
            r[i] = jsUndefined();

        for (size_t count = codeBlock->constantRegisters.size(), j = 0; j < count; ++i, ++j)
            r[i] = codeBlock->constantRegisters[j];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_enter_with_activation) {
        /* enter_with_activation dst(r)

           Initializes local variables to undefined, fills constant
           registers with their values, creates an activation object,
           and places the new activation both in dst and at the top
           of the scope chain. If the code block does not require an
           activation, enter should be used instead.

           This opcode should only be used at the beginning of a code
           block.
        */

        size_t i = 0;
        CodeBlock* codeBlock = this->codeBlock(r);

        for (size_t count = codeBlock->numVars; i < count; ++i)
            r[i] = jsUndefined();

        for (size_t count = codeBlock->constantRegisters.size(), j = 0; j < count; ++i, ++j)
            r[i] = codeBlock->constantRegisters[j];

        int dst = (++vPC)->u.operand;
        JSActivation* activation = new (globalData) JSActivation(exec, static_cast<FunctionBodyNode*>(codeBlock->ownerNode), r);
        r[dst] = activation;
        r[RegisterFile::ScopeChain] = scopeChain(r)->copy()->push(activation);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_convert_this) {
        int thisRegister = (++vPC)->u.operand;
        JSValue* thisVal = r[thisRegister].getJSValue();
        if (thisVal->needsThisConversion())
            r[thisRegister] = thisVal->toThisObject(exec);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_create_arguments) {
        /* create_arguments

           Creates the 'arguments' object and places it in both the
           'arguments' call frame slot and the local 'arguments'
           register.

           This opcode should only be used at the beginning of a code
           block.
        */

        Arguments* arguments = new (globalData) Arguments(exec, r);
        r[RegisterFile::OptionalCalleeArguments] = arguments;
        r[RegisterFile::ArgumentsRegister] = arguments;
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_construct) {
        /* construct dst(r) constr(r) constrProto(r) firstArg(r) argCount(n) registerOffset(n)

           Invoke register "constr" as a constructor. For JS
           functions, the calling convention is exactly as for the
           "call" opcode, except that the "this" value is a newly
           created Object. For native constructors, a null "this"
           value is passed. In either case, the firstArg and argCount
           registers are interpreted as for the "call" opcode.

           Register constrProto must contain the prototype property of
           register constsr. This is to enable polymorphic inline
           caching of this lookup.
        */

        int dst = (++vPC)->u.operand;
        int constr = (++vPC)->u.operand;
        int constrProto = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;
        int registerOffset = (++vPC)->u.operand;

        JSValue* v = r[constr].jsValue(exec);

        ConstructData constructData;
        ConstructType constructType = v->getConstructData(constructData);

        if (constructType == ConstructTypeJS) {
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, static_cast<JSObject*>(v));

            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

            StructureID* structure;
            JSValue* prototype = r[constrProto].jsValue(exec);
            if (prototype->isObject())
                structure = static_cast<JSObject*>(prototype)->inheritorID();
            else
                structure = callDataScopeChain->globalObject()->emptyObjectStructure();
            JSObject* newObject = new (globalData) JSObject(structure);

            r[firstArg] = newObject; // "this" value

            Register* savedR = r;

            r = slideRegisterWindowForCall(newCodeBlock, registerFile, r, registerOffset, argCount);
            if (UNLIKELY(!r)) {
                r = savedR;
                exceptionValue = createStackOverflowError(CallFrame::create(r));
                goto vm_throw;
            }

            initializeCallFrame(r, newCodeBlock, vPC + 1, callDataScopeChain, savedR, dst, argCount, v);
    
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->didExecute(exec, static_cast<JSObject*>(v));

            vPC = newCodeBlock->instructions.begin();

#if DUMP_OPCODE_STATS
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_OPCODE;
        }

        if (constructType == ConstructTypeHost) {
            ArgList args(r + firstArg + 1, argCount - 1);

            ScopeChainNode* scopeChain = this->scopeChain(r);
            initializeCallFrame(r + registerOffset, 0, vPC + 1, scopeChain, r, dst, argCount, v);
            r += registerOffset;

            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, static_cast<JSObject*>(v));

            MACHINE_SAMPLING_callingHostFunction();

            JSValue* returnValue = constructData.native.function(exec, static_cast<JSObject*>(v), args);
            r -= registerOffset;

            VM_CHECK_EXCEPTION();
            r[dst] = returnValue;

            if (*enabledProfilerReference)
                (*enabledProfilerReference)->didExecute(exec, static_cast<JSObject*>(v));

            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(constructType == ConstructTypeNone);

        exceptionValue = createNotAConstructorError(exec, v, vPC, codeBlock(r));
        goto vm_throw;
    }
    BEGIN_OPCODE(op_construct_verify) {
        /* construct_verify dst(r) override(r)

           Verifies that register dst holds an object. If not, moves
           the object in register override to register dst.
        */

        int dst = vPC[1].u.operand;;
        if (LIKELY(r[dst].jsValue(exec)->isObject())) {
            vPC += 3;
            NEXT_OPCODE;
        }

        int override = vPC[2].u.operand;
        r[dst] = r[override];

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.
        */
        int scope = (++vPC)->u.operand;
        JSValue* v = r[scope].jsValue(exec);
        JSObject* o = v->toObject(exec);
        VM_CHECK_EXCEPTION();

        r[RegisterFile::ScopeChain] = scopeChain(r)->push(o);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        r[RegisterFile::ScopeChain] = scopeChain(r)->pop();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_pnames) {
        /* get_pnames dst(r) base(r)

           Creates a property name list for register base and puts it
           in register dst. This is not a true JavaScript value, just
           a synthetic value used to keep the iteration state in a
           register.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        r[dst] = JSPropertyNameIterator::create(exec, r[base].jsValue(exec));
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_next_pname) {
        /* next_pname dst(r) iter(r) target(offset)

           Tries to copies the next name from property name list in
           register iter. If there are names left, then copies one to
           register dst, and jumps to offset target. If there are none
           left, invalidates the iterator and continues to the next
           instruction.
        */
        int dst = (++vPC)->u.operand;
        int iter = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

        JSPropertyNameIterator* it = r[iter].jsPropertyNameIterator();
        if (JSValue* temp = it->next(exec)) {
            CHECK_FOR_TIMEOUT();
            r[dst] = temp;
            vPC += target;
            NEXT_OPCODE;
        }
        it->invalidate();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp_scopes) {
        /* jmp_scopes count(n) target(offset)

           Removes the a number of items from the current scope chain
           specified by immediate number count, then jumps to offset
           target.
        */
        int count = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

        ScopeChainNode* tmp = scopeChain(r);
        while (count--)
            tmp = tmp->pop();
        r[RegisterFile::ScopeChain] = tmp;

        vPC += target;
        NEXT_OPCODE;
    }
#if HAVE(COMPUTED_GOTO)
    // Appease GCC
    goto *(&&skip_new_scope);
#endif
    BEGIN_OPCODE(op_push_new_scope) {
        /* new_scope dst(r) property(id) value(r)
         
           Constructs a new StaticScopeObject with property set to value.  That scope
           object is then pushed onto the ScopeChain.  The scope object is then stored
           in dst for GC.
         */
        r[RegisterFile::ScopeChain] = createExceptionScope(exec, vPC, r);

        vPC += 4;
        NEXT_OPCODE;
    }
#if HAVE(COMPUTED_GOTO)
    skip_new_scope:
#endif
    BEGIN_OPCODE(op_catch) {
        /* catch ex(r)

           Retrieves the VMs current exception and puts it in register
           ex. This is only valid after an exception has been raised,
           and usually forms the beginning of an exception handler.
        */
        ASSERT(exceptionValue);
        ASSERT(!exec->hadException());
        int ex = (++vPC)->u.operand;
        r[ex] = exceptionValue;
        exceptionValue = 0;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_throw) {
        /* throw ex(r)

           Throws register ex as an exception. This involves three
           steps: first, it is set as the current exception in the
           VM's internal state, then the stack is unwound until an
           exception handler or a native code boundary is found, and
           then control resumes at the exception handler if any or
           else the script returns control to the nearest native caller.
        */

        int ex = (++vPC)->u.operand;
        exceptionValue = r[ex].jsValue(exec);

        handlerVPC = throwException(exec, exceptionValue, vPC, r, true);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_throw_end_indirect; // indirect goto -> op_throw_end
    }
    op_throw_end: {
#endif

        vPC = handlerVPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_unexpected_load) {
        /* unexpected_load load dst(r) src(k)

           Copies constant src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = codeBlock(r)->unexpectedConstants[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_error) {
        /* new_error dst(r) type(n) message(k)

           Constructs a new Error instance using the original
           constructor, using immediate number n as the type and
           constant message as the message string. The result is
           written to register dst.
        */
        int dst = (++vPC)->u.operand;
        int type = (++vPC)->u.operand;
        int message = (++vPC)->u.operand;

        CodeBlock* codeBlock = this->codeBlock(r);
        r[dst] = Error::create(exec, (ErrorType)type, codeBlock->unexpectedConstants[message]->toString(exec), codeBlock->lineNumberForVPC(vPC), codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->sourceURL());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_end) {
        /* end result(r)
           
           Return register result as the value of a global or eval
           program. Return control to the calling native code.
        */

        if (codeBlock(r)->needsFullScopeChain) {
            ScopeChainNode* scopeChain = this->scopeChain(r);
            ASSERT(scopeChain->refCount > 1);
            scopeChain->deref();
        }
        int result = (++vPC)->u.operand;
        return r[result].jsValue(exec);
    }
    BEGIN_OPCODE(op_put_getter) {
        /* put_getter base(r) property(id) function(r)

           Sets register function on register base as the getter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for getters defined
           in object literal form.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;

        ASSERT(r[base].jsValue(exec)->isObject());
        JSObject* baseObj = static_cast<JSObject*>(r[base].jsValue(exec));
        Identifier& ident = codeBlock(r)->identifiers[property];
        ASSERT(r[function].jsValue(exec)->isObject());
        baseObj->defineGetter(exec, ident, static_cast<JSObject*>(r[function].jsValue(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_setter) {
        /* put_setter base(r) property(id) function(r)

           Sets register function on register base as the setter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for setters defined
           in object literal form.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;

        ASSERT(r[base].jsValue(exec)->isObject());
        JSObject* baseObj = static_cast<JSObject*>(r[base].jsValue(exec));
        Identifier& ident = codeBlock(r)->identifiers[property];
        ASSERT(r[function].jsValue(exec)->isObject());
        baseObj->defineSetter(exec, ident, static_cast<JSObject*>(r[function].jsValue(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jsr) {
        /* jsr retAddrDst(r) target(offset)

           Places the address of the next instruction into the retAddrDst
           register and jumps to offset target from the current instruction.
        */
        int retAddrDst = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        r[retAddrDst] = vPC + 1;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sret) {
        /* sret retAddrSrc(r)

         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = (++vPC)->u.operand;
        vPC = r[retAddrSrc].vPC();
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_debug) {
        /* debug debugHookID(n) firstLine(n) lastLine(n)

         Notifies the debugger of the current state of execution. This opcode
         is only generated while the debugger is attached.
        */
        int debugHookID = (++vPC)->u.operand;
        int firstLine = (++vPC)->u.operand;
        int lastLine = (++vPC)->u.operand;

        debug(exec, r, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);

        ++vPC;
        NEXT_OPCODE;
    }
    vm_throw: {
        globalData->exception = 0;
        if (!tickCount) {
            // The exceptionValue is a lie! (GCC produces bad code for reasons I 
            // cannot fathom if we don't assign to the exceptionValue before branching)
            exceptionValue = createInterruptedExecutionException(globalData);
        }
        handlerVPC = throwException(exec, exceptionValue, vPC, r, false);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }
        vPC = handlerVPC;
        NEXT_OPCODE;
    }
    }
    #undef NEXT_OPCODE
    #undef BEGIN_OPCODE
    #undef VM_CHECK_EXCEPTION
    #undef CHECK_FOR_TIMEOUT
    #undef exec
}

JSValue* Machine::retrieveArguments(ExecState* exec, JSFunction* function) const
{
    Register* r = this->callFrame(exec, function);
    if (!r)
        return jsNull();

    CodeBlock* codeBlock = Machine::codeBlock(r);
    if (codeBlock->usesArguments) {
        ASSERT(codeBlock->codeType == FunctionCode);
        SymbolTable& symbolTable = static_cast<FunctionBodyNode*>(codeBlock->ownerNode)->symbolTable();
        int argumentsIndex = symbolTable.get(exec->propertyNames().arguments.ustring().rep()).getIndex();
        return r[argumentsIndex].jsValue(exec);
    }

    Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue());
    if (!arguments) {
        arguments = new (exec) Arguments(exec, r);
        arguments->copyRegisters();
        r[RegisterFile::OptionalCalleeArguments] = arguments;
    }
    ASSERT(arguments->isObject(&Arguments::info));

    return arguments;
}

JSValue* Machine::retrieveCaller(ExecState* exec, InternalFunction* function) const
{
    Register* r = this->callFrame(exec, function);
    if (!r)
        return jsNull();

    Register* callerR = r[RegisterFile::CallerRegisters].r();
    if (isHostCallFrame(callerR))
        return jsNull();

    JSValue* caller = callerR[RegisterFile::Callee].jsValue(exec);
    if (!caller)
        return jsNull();

    return caller;
}

void Machine::retrieveLastCaller(ExecState* exec, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValue*& function) const
{
    function = 0;
    lineNumber = -1;
    sourceURL = UString();

    Register* r = exec->registers();
    Register* callerR = r[RegisterFile::CallerRegisters].r();
    if (isHostCallFrame(callerR))
        return;

    CodeBlock* callerCodeBlock = codeBlock(callerR);
    if (!callerCodeBlock)
        return;

    Instruction* vPC = vPCForPC(callerCodeBlock, r[RegisterFile::ReturnPC].v());
    lineNumber = callerCodeBlock->lineNumberForVPC(vPC - 1);
    sourceID = callerCodeBlock->ownerNode->sourceID();
    sourceURL = callerCodeBlock->ownerNode->sourceURL();

    JSValue* caller = callerR[RegisterFile::Callee].getJSValue();
    if (!caller)
        return;

    function = caller;
}

Register* Machine::callFrame(ExecState* exec, InternalFunction* function) const
{
    for (Register* r = exec->registers(); r; r = stripHostCallFrameBit(r[RegisterFile::CallerRegisters].r()))
        if (r[RegisterFile::Callee].getJSValue() == function)
            return r;
    return 0;
}

void Machine::getArgumentsData(Register* callFrame, JSFunction*& function, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc)
{
    function = static_cast<JSFunction*>(callFrame[RegisterFile::Callee].getJSValue());
    ASSERT(function->inherits(&JSFunction::info));
    
    CodeBlock* codeBlock = &function->m_body->generatedByteCode();
    int numParameters = codeBlock->numParameters;
    argc = callFrame[RegisterFile::ArgumentCount].i();

    if (argc <= numParameters)
        argv = callFrame - RegisterFile::CallFrameHeaderSize - numParameters + 1; // + 1 to skip "this"
    else
        argv = callFrame - RegisterFile::CallFrameHeaderSize - numParameters - argc + 1; // + 1 to skip "this"

    argc -= 1; // - 1 to skip "this"
    firstParameterIndex = -RegisterFile::CallFrameHeaderSize - numParameters + 1; // + 1 to skip "this"
}

#if ENABLE(CTI)

NEVER_INLINE static void doSetReturnAddressVMThrowTrampoline(void** returnAddress)
{
    ctiSetReturnAddress(returnAddress, reinterpret_cast<void*>(ctiVMThrowTrampoline));
}

NEVER_INLINE void Machine::tryCTICachePutByID(ExecState* exec, CodeBlock* codeBlock, void* returnAddress, JSValue* baseValue, const PutPropertySlot& slot)
{
    // The interpreter checks for recursion here; I do not believe this can occur in CTI.

    if (JSImmediate::isImmediate(baseValue))
        return;

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiRepatchCallByReturnAddress(returnAddress, (void*)cti_op_put_by_id_generic);
        return;
    }
    
    JSCell* baseCell = static_cast<JSCell*>(baseValue);
    StructureID* structureID = baseCell->structureID();

    if (structureID->isDictionary()) {
        ctiRepatchCallByReturnAddress(returnAddress, (void*)cti_op_put_by_id_generic);
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(returnAddress);
    Instruction* vPC = codeBlock->instructions.begin() + vPCIndex;

    // Cache hit: Specialize instruction and ref StructureIDs.

    // If baseCell != base, then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        ctiRepatchCallByReturnAddress(returnAddress, (void*)cti_op_put_by_id_generic);
        return;
    }

    // StructureID transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        vPC[0] = getOpcode(op_put_by_id_transition);
        vPC[4] = structureID->previousID();
        vPC[5] = structureID;
        StructureIDChain* chain = structureID->cachedPrototypeChain();
        if (!chain) {
            chain = cachePrototypeChain(exec, structureID);
            if (!chain) {
                // This happens if someone has manually inserted null into the prototype chain
                vPC[0] = getOpcode(op_put_by_id_generic);
                return;
            }
        }
        vPC[6] = chain;
        vPC[7] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);
        CTI::compilePutByIdTransition(this, exec, codeBlock, structureID->previousID(), structureID, slot.cachedOffset(), chain, returnAddress);
        return;
    }
    
    vPC[0] = getOpcode(op_put_by_id_replace);
    vPC[4] = structureID;
    vPC[5] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);

#if USE(CTI_REPATCH_PIC)
    UNUSED_PARAM(exec);
    CTI::patchPutByIdReplace(codeBlock, structureID, slot.cachedOffset(), returnAddress);
#else
    CTI::compilePutByIdReplace(this, exec, codeBlock, structureID, slot.cachedOffset(), returnAddress);
#endif
}

void* Machine::getCTIArrayLengthTrampoline(ExecState* exec, CodeBlock* codeBlock)
{
    if (!m_ctiArrayLengthTrampoline)
        m_ctiArrayLengthTrampoline = CTI::compileArrayLengthTrampoline(this, exec, codeBlock);
        
    return m_ctiArrayLengthTrampoline;
}

void* Machine::getCTIStringLengthTrampoline(ExecState* exec, CodeBlock* codeBlock)
{
    if (!m_ctiStringLengthTrampoline)
        m_ctiStringLengthTrampoline = CTI::compileStringLengthTrampoline(this, exec, codeBlock);
        
    return m_ctiStringLengthTrampoline;
}

NEVER_INLINE void Machine::tryCTICacheGetByID(ExecState* exec, CodeBlock* codeBlock, void* returnAddress, JSValue* baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    // FIXME: Cache property access for immediates.
    if (JSImmediate::isImmediate(baseValue)) {
        ctiRepatchCallByReturnAddress(returnAddress, (void*)cti_op_get_by_id_generic);
        return;
    }

    if (isJSArray(baseValue) && propertyName == exec->propertyNames().length) {
#if USE(CTI_REPATCH_PIC)
        CTI::compilePatchGetArrayLength(this, exec, codeBlock, returnAddress);
#else
        ctiRepatchCallByReturnAddress(returnAddress, getCTIArrayLengthTrampoline(exec, codeBlock));
#endif
        return;
    }
    if (isJSString(baseValue) && propertyName == exec->propertyNames().length) {
        // The tradeoff of compiling an repatched inline string length access routine does not seem
        // to pay off, so we currently only do this for arrays.
        ctiRepatchCallByReturnAddress(returnAddress, getCTIStringLengthTrampoline(exec, codeBlock));
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiRepatchCallByReturnAddress(returnAddress, (void*)cti_op_get_by_id_generic);
        return;
    }

    JSCell* baseCell = static_cast<JSCell*>(baseValue);
    StructureID* structureID = baseCell->structureID();

    if (structureID->isDictionary()) {
        ctiRepatchCallByReturnAddress(returnAddress, (void*)cti_op_get_by_id_generic);
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(returnAddress);
    Instruction* vPC = codeBlock->instructions.begin() + vPCIndex;

    // Cache hit: Specialize instruction and ref StructureIDs.

    if (slot.slotBase() == baseValue) {
        // set this up, so derefStructureIDs can do it's job.
        vPC[0] = getOpcode(op_get_by_id_self);
        vPC[4] = structureID;
        vPC[5] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);
        
#if USE(CTI_REPATCH_PIC)
        CTI::patchGetByIdSelf(codeBlock, structureID, slot.cachedOffset(), returnAddress);
#else
        CTI::compileGetByIdSelf(this, exec, codeBlock, structureID, slot.cachedOffset(), returnAddress);
#endif
        return;
    }

    if (slot.slotBase() == structureID->prototypeForLookup(exec)) {
        ASSERT(slot.slotBase()->isObject());

        JSObject* slotBaseObject = static_cast<JSObject*>(slot.slotBase());

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (slotBaseObject->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(slotBaseObject->structureID());
            slotBaseObject->setStructureID(transition.release());
            static_cast<JSObject*>(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[4] = structureID;
        vPC[5] = slotBaseObject->structureID();
        vPC[6] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);

        CTI::compileGetByIdProto(this, exec, codeBlock, structureID, slotBaseObject->structureID(), slot.cachedOffset(), returnAddress);
        return;
    }

    size_t count = 0;
    JSObject* o = static_cast<JSObject*>(baseValue);
    while (slot.slotBase() != o) {
        JSValue* v = o->structureID()->prototypeForLookup(exec);

        // If we didn't find slotBase in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.

        if (v->isNull()) {
            vPC[0] = getOpcode(op_get_by_id_generic);
            return;
        }

        o = static_cast<JSObject*>(v);

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (o->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(o->structureID());
            o->setStructureID(transition.release());
            static_cast<JSObject*>(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        ++count;
    }

    StructureIDChain* chain = structureID->cachedPrototypeChain();
    if (!chain)
        chain = cachePrototypeChain(exec, structureID);

    ASSERT(chain);
    vPC[0] = getOpcode(op_get_by_id_chain);
    vPC[4] = structureID;
    vPC[5] = chain;
    vPC[6] = count;
    vPC[7] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);

    CTI::compileGetByIdChain(this, exec, codeBlock, structureID, chain, count, slot.cachedOffset(), returnAddress);
}

#define VM_THROW_EXCEPTION() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        return 0; \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    do { \
        ASSERT(ARG_globalData->exception); \
        ARG_globalData->throwReturnAddress = CTI_RETURN_ADDRESS; \
        doSetReturnAddressVMThrowTrampoline(&CTI_RETURN_ADDRESS); \
    } while (0)

#define VM_CHECK_EXCEPTION() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != 0)) \
            VM_THROW_EXCEPTION(); \
    } while (0)
#define VM_CHECK_EXCEPTION_ARG(exceptionValue) \
    do { \
        if (UNLIKELY((exceptionValue) != 0)) { \
            ARG_globalData->exception = (exceptionValue); \
            VM_THROW_EXCEPTION(); \
        } \
    } while (0)
#define VM_CHECK_EXCEPTION_AT_END() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != 0)) \
            VM_THROW_EXCEPTION_AT_END(); \
    } while (0)
#define VM_CHECK_EXCEPTION_VOID() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != 0)) { \
            VM_THROW_EXCEPTION_AT_END(); \
            return; \
        } \
    } while (0)

JSValue* Machine::cti_op_convert_this(CTI_ARGS)
{
    JSValue* v1 = ARG_src1;
    ExecState* exec = ARG_exec;

    JSObject* result = v1->toThisObject(exec);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Machine::cti_op_end(CTI_ARGS)
{
    Register* r = ARG_r;
    ScopeChainNode* scopeChain = Machine::scopeChain(r);
    ASSERT(scopeChain->refCount > 1);
    scopeChain->deref();
}

JSValue* Machine::cti_op_add(CTI_ARGS)
{
    JSValue* v1 = ARG_src1;
    JSValue* v2 = ARG_src2;

    double left;
    double right = 0.0;

    bool rightIsNumber = fastIsNumber(v2, right);
    if (rightIsNumber && fastIsNumber(v1, left))
        return jsNumber(ARG_globalData, left + right);
    
    ExecState* exec = ARG_exec;

    bool leftIsString = v1->isString();
    if (leftIsString && v2->isString()) {
        RefPtr<UString::Rep> value = concatenate(static_cast<JSString*>(v1)->value().rep(), static_cast<JSString*>(v2)->value().rep());
        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(exec);
            VM_THROW_EXCEPTION();
        }

        return jsString(ARG_globalData, value.release());
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = JSImmediate::isImmediate(v2) ?
            concatenate(static_cast<JSString*>(v1)->value().rep(), JSImmediate::getTruncatedInt32(v2)) :
            concatenate(static_cast<JSString*>(v1)->value().rep(), right);

        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(exec);
            VM_THROW_EXCEPTION();
        }
        return jsString(ARG_globalData, value.release());
    }

    // All other cases are pretty uncommon
    JSValue* result = jsAddSlowCase(exec, v1, v2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_pre_inc(CTI_ARGS)
{
    JSValue* v = ARG_src1;

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, v->toNumber(exec) + 1);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Machine::cti_timeout_check(CTI_ARGS)
{
    if (ARG_globalData->machine->checkTimeout(ARG_exec->dynamicGlobalObject())) {
        ARG_globalData->exception = createInterruptedExecutionException(ARG_globalData);
        VM_THROW_EXCEPTION_AT_END();
    }
}

int Machine::cti_op_loop_if_less(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;
    ExecState* exec = ARG_exec;

    bool result = jsLess(exec, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Machine::cti_op_loop_if_lesseq(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;
    ExecState* exec = ARG_exec;

    bool result = jsLessEq(exec, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_new_object(CTI_ARGS)
{
    return constructEmptyObject(ARG_exec);;
}

void Machine::cti_op_put_by_id(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1->put(exec, ident, ARG_src3, slot);

    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, (void*)cti_op_put_by_id_second);

    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_id_second(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PutPropertySlot slot;
    baseValue->put(exec, ident, ARG_src3, slot);

    Register* r = ARG_r;
    ARG_globalData->machine->tryCTICachePutByID(exec, codeBlock(r), CTI_RETURN_ADDRESS, baseValue, slot);

    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_id_generic(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1->put(exec, ident, ARG_src3, slot);

    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_id_fail(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1->put(exec, ident, ARG_src3, slot);

    // should probably uncachePutByID() ... this would mean doing a vPC lookup - might be worth just bleeding this until the end.
    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, (void*)cti_op_put_by_id_generic);

    VM_CHECK_EXCEPTION_AT_END();
}

JSValue* Machine::cti_op_get_by_id(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(exec, ident, slot);

    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, (void*)cti_op_get_by_id_second);

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_get_by_id_second(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(exec, ident, slot);

    Register* r = ARG_r;
    ARG_globalData->machine->tryCTICacheGetByID(exec, codeBlock(r), CTI_RETURN_ADDRESS, baseValue, ident, slot);

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_get_by_id_generic(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(exec, ident, slot);

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_get_by_id_fail(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(exec, ident, slot);

    // should probably uncacheGetByID() ... this would mean doing a vPC lookup - might be worth just bleeding this until the end.
    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, (void*)cti_op_get_by_id_generic);

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_instanceof(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    JSValue* value = ARG_src1;
    JSValue* baseVal = ARG_src2;
    JSValue* proto = ARG_src3;
    JSCell* valueCell = static_cast<JSCell*>(value);
    JSCell* baseCell = static_cast<JSCell*>(baseVal);
    JSCell* protoCell = static_cast<JSCell*>(proto);

    // at least one of these checks must have failed to get to the slow case
    ASSERT(JSImmediate::isAnyImmediate(valueCell, baseCell, protoCell) 
           || !valueCell->isObject() || !baseCell->isObject() || !protoCell->isObject() 
           || (baseCell->structureID()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);

    if (!baseVal->isObject()) {
        Register* r = ARG_r;
        CodeBlock* codeBlock = Machine::codeBlock(r);
        ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
        unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(exec, "instanceof", baseVal, codeBlock->instructions.begin() + vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    if (!baseCell->structureID()->typeInfo().implementsHasInstance())
        return jsBoolean(false);

    if (!proto->isObject()) {
        throwError(exec, TypeError, "instanceof called on an object with an invalid prototype property.");
        VM_THROW_EXCEPTION();
    }
        
    if (!value->isObject())
        return jsBoolean(false);

    JSValue* result = jsBoolean(static_cast<JSObject*>(baseCell)->hasInstance(exec, valueCell, protoCell));
    VM_CHECK_EXCEPTION_AT_END();

    return result;
}

JSValue* Machine::cti_op_del_by_id(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Identifier& ident = *ARG_id2;
    
    JSObject* baseObj = ARG_src1->toObject(exec);

    JSValue* result = jsBoolean(baseObj->deleteProperty(exec, ident));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_mul(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left * right);

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, src1->toNumber(exec) * src2->toNumber(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_new_func(CTI_ARGS)
{
    return ARG_func1->makeFunction(ARG_exec, Machine::scopeChain(ARG_r));
}

void* Machine::cti_op_call_JSFunction(CTI_ARGS)
{
#ifndef NDEBUG
    CallData callData;
    ASSERT(ARG_src1->getCallData(callData) == CallTypeJS);
#endif

    if (UNLIKELY(*ARG_profilerReference != 0))
        (*ARG_profilerReference)->willExecute(CallFrame::create(ARG_r), static_cast<JSObject*>(ARG_src1));

    ScopeChainNode* callDataScopeChain = static_cast<JSFunction*>(ARG_src1)->m_scopeChain.node();
    CodeBlock* newCodeBlock = &static_cast<JSFunction*>(ARG_src1)->m_body->byteCode(callDataScopeChain);

    Register* r = slideRegisterWindowForCall(newCodeBlock, ARG_registerFile, ARG_r, ARG_int2, ARG_int3);
    if (UNLIKELY(!r)) {
        ARG_globalData->exception = createStackOverflowError(CallFrame::create(ARG_r));
        VM_THROW_EXCEPTION();
    }

    r[RegisterFile::CodeBlock] = newCodeBlock;
    r[RegisterFile::ScopeChain] = callDataScopeChain;
    r[RegisterFile::CallerRegisters] = ARG_r;
    // RegisterFile::ReturnPC is set by callee
    // RegisterFile::ReturnValueRegister is set by caller
    r[RegisterFile::ArgumentCount] = ARG_int3; // original argument count (for the sake of the "arguments" object)
    r[RegisterFile::Callee] = ARG_src1;
    r[RegisterFile::OptionalCalleeArguments] = nullJSValue;

    ARG_setR(r);

    return newCodeBlock->ctiCode;
}

void* Machine::cti_vm_compile(CTI_ARGS)
{
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);

    if (!codeBlock->ctiCode)
        CTI::compile(ARG_globalData->machine, CallFrame::create(r), codeBlock);

    return codeBlock->ctiCode;
}

JSValue* Machine::cti_op_push_activation(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);
    ScopeChainNode* scopeChain = Machine::scopeChain(r);

    JSActivation* activation = new (ARG_globalData) JSActivation(exec, static_cast<FunctionBodyNode*>(codeBlock->ownerNode), r);
    r[RegisterFile::ScopeChain] = scopeChain->copy()->push(activation);
    return activation;
}

JSValue* Machine::cti_op_call_NotJSFunction(CTI_ARGS)
{
    JSValue* funcVal = ARG_src1;

    CallData callData;
    CallType callType = funcVal->getCallData(callData);

    ASSERT(callType != CallTypeJS);

    if (callType == CallTypeHost) {
        int registerOffset = ARG_int2;
        int argCount = ARG_int3;
        Register* savedR = ARG_r;
        Register* r = savedR + registerOffset;

        initializeCallFrame(r, 0, ARG_instr4 + 1, scopeChain(savedR), savedR, 0, argCount, funcVal);
        ARG_setR(r);

        if (*ARG_profilerReference)
            (*ARG_profilerReference)->willExecute(CallFrame::create(r), static_cast<JSObject*>(funcVal));

        Register* argv = r - RegisterFile::CallFrameHeaderSize - argCount;
        ArgList argList(argv + 1, argCount - 1);

        CTI_MACHINE_SAMPLING_callingHostFunction();

        JSValue* returnValue = callData.native.function(CallFrame::create(r), static_cast<JSObject*>(funcVal), argv[0].jsValue(CallFrame::create(r)), argList);
        ARG_setR(savedR);
        VM_CHECK_EXCEPTION();

        if (*ARG_profilerReference)
            (*ARG_profilerReference)->didExecute(CallFrame::create(savedR), static_cast<JSObject*>(funcVal));

        return returnValue;
    }

    ASSERT(callType == CallTypeNone);

    ARG_globalData->exception = createNotAFunctionError(CallFrame::create(ARG_r), funcVal, ARG_instr4, codeBlock(ARG_r));
    VM_THROW_EXCEPTION();
}

void Machine::cti_op_create_arguments(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;

    Arguments* arguments = new (ARG_globalData) Arguments(exec, r);
    r[RegisterFile::OptionalCalleeArguments] = arguments;
    r[RegisterFile::ArgumentsRegister] = arguments;
}

void Machine::cti_op_tear_off_activation(CTI_ARGS)
{
    Register* r = ARG_r;

    JSActivation* activation = static_cast<JSActivation*>(ARG_src1);
    ASSERT(codeBlock(r)->needsFullScopeChain);
    ASSERT(activation->isObject(&JSActivation::info));

    Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue());
    ASSERT(!arguments || arguments->isObject(&Arguments::info));
    activation->copyRegisters(arguments);
}

void Machine::cti_op_tear_off_arguments(CTI_ARGS)
{
    Register* r = ARG_r;

    Arguments* arguments = static_cast<Arguments*>(r[RegisterFile::OptionalCalleeArguments].getJSValue());
    ASSERT(codeBlock(r)->usesArguments && !codeBlock(r)->needsFullScopeChain);
    ASSERT(arguments->isObject(&Arguments::info));

    arguments->copyRegisters();
}

void Machine::cti_op_ret_profiler(CTI_ARGS)
{
    ExecState* exec = ARG_exec;

    Register* r = ARG_r;
    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->didExecute(exec, static_cast<JSObject*>(r[RegisterFile::Callee].jsValue(exec)));
}

void Machine::cti_op_ret_scopeChain(CTI_ARGS)
{
    Register* r = ARG_r;
    ASSERT(codeBlock(r)->needsFullScopeChain);
    scopeChain(r)->deref();
}

JSValue* Machine::cti_op_new_array(CTI_ARGS)
{
    ArgList argsList(ARG_registers1, ARG_int2);
    return constructArray(ARG_exec, argsList);
}

JSValue* Machine::cti_op_resolve(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    ScopeChainNode* scopeChain = Machine::scopeChain(r);

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            VM_CHECK_EXCEPTION_AT_END();
            return result;
        }
    } while (++iter != end);

    CodeBlock* codeBlock = Machine::codeBlock(r);
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    exec->setException(createUndefinedVariableError(exec, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock));
    VM_THROW_EXCEPTION();
}

void* Machine::cti_op_construct_JSConstruct(CTI_ARGS)
{
    RegisterFile* registerFile = ARG_registerFile;
    Register* r = ARG_r;

    JSValue* constrVal = ARG_src1;
    JSValue* constrProtoVal = ARG_src2;
    int firstArg = ARG_int3;
    int registerOffset = ARG_int4;
    int argCount = ARG_int5;

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(constrVal->getConstructData(constructData) == ConstructTypeJS);
#endif

    JSFunction* constructor = static_cast<JSFunction*>(constrVal);

    if (*ARG_profilerReference)
        (*ARG_profilerReference)->willExecute(CallFrame::create(r), constructor);

    ScopeChainNode* callDataScopeChain = constructor->m_scopeChain.node();
    FunctionBodyNode* functionBodyNode = constructor->m_body.get();
    CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

    StructureID* structure;
    if (constrProtoVal->isObject())
        structure = static_cast<JSObject*>(constrProtoVal)->inheritorID();
    else
        structure = callDataScopeChain->globalObject()->emptyObjectStructure();
    JSObject* newObject = new (ARG_globalData) JSObject(structure);

    r[firstArg] = newObject; // "this" value

    r = slideRegisterWindowForCall(newCodeBlock, registerFile, r, registerOffset, argCount);
    if (UNLIKELY(!r)) {
        ARG_globalData->exception = createStackOverflowError(CallFrame::create(ARG_r));
        VM_THROW_EXCEPTION();
    }

    r[RegisterFile::CodeBlock] = newCodeBlock;
    r[RegisterFile::ScopeChain] = callDataScopeChain;
    r[RegisterFile::CallerRegisters] = ARG_r;
    // RegisterFile::ReturnPC is set by callee
    // RegisterFile::ReturnValueRegister is set by caller
    r[RegisterFile::ArgumentCount] = argCount; // original argument count (for the sake of the "arguments" object)
    r[RegisterFile::Callee] = constructor;
    r[RegisterFile::OptionalCalleeArguments] = nullJSValue;

    ARG_setR(r);
    return newCodeBlock->ctiCode;
}

JSValue* Machine::cti_op_construct_NotJSConstruct(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;

    JSValue* constrVal = ARG_src1;
    int firstArg = ARG_int3;
    int argCount = ARG_int5;

    ConstructData constructData;
    ConstructType constructType = constrVal->getConstructData(constructData);

    JSObject* constructor = static_cast<JSObject*>(constrVal);

    if (constructType == ConstructTypeHost) {
        if (*ARG_profilerReference)
            (*ARG_profilerReference)->willExecute(exec, constructor);

        ArgList argList(r + firstArg + 1, argCount - 1);

        CTI_MACHINE_SAMPLING_callingHostFunction();

        JSValue* returnValue = constructData.native.function(exec, constructor, argList);
        VM_CHECK_EXCEPTION();

        if (*ARG_profilerReference)
            (*ARG_profilerReference)->didExecute(exec, constructor);

        return returnValue;
    }

    ASSERT(constructType == ConstructTypeNone);

    exec->setException(createNotAConstructorError(exec, constrVal, ARG_instr6, codeBlock(r)));
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_get_by_val(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Machine* machine = ARG_globalData->machine;

    JSValue* baseValue = ARG_src1;
    JSValue* subscript = ARG_src2;

    JSValue* result;
    unsigned i;

    bool isUInt32 = JSImmediate::getUInt32(subscript, i);
    if (LIKELY(isUInt32)) {
        if (machine->isJSArray(baseValue)) {
            JSArray* jsArray = static_cast<JSArray*>(baseValue);
            if (jsArray->canGetIndex(i))
                result = jsArray->getIndex(i);
            else
                result = jsArray->JSArray::get(exec, i);
        } else if (machine->isJSString(baseValue) && static_cast<JSString*>(baseValue)->canGetIndex(i))
            result = static_cast<JSString*>(baseValue)->getIndex(ARG_globalData, i);
        else
            result = baseValue->get(exec, i);
    } else {
        Identifier property(exec, subscript->toString(exec));
        result = baseValue->get(exec, property);
    }

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_resolve_func(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    ScopeChainNode* scopeChain = Machine::scopeChain(r);

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(exec, ident, slot)) {            
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(exec);
            JSValue* result = slot.getValue(exec, ident);
            VM_CHECK_EXCEPTION_AT_END();

            ARG_set2ndResult(result);
            return thisObj;
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = Machine::codeBlock(r);
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    exec->setException(createUndefinedVariableError(exec, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock));
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_sub(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left - right);

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, src1->toNumber(exec) - src2->toNumber(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Machine::cti_op_put_by_val(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Machine* machine = ARG_globalData->machine;

    JSValue* baseValue = ARG_src1;
    JSValue* subscript = ARG_src2;
    JSValue* value = ARG_src3;

    unsigned i;

    bool isUInt32 = JSImmediate::getUInt32(subscript, i);
    if (LIKELY(isUInt32)) {
        if (machine->isJSArray(baseValue)) {
            JSArray* jsArray = static_cast<JSArray*>(baseValue);
            if (jsArray->canSetIndex(i))
                jsArray->setIndex(i, value);
            else
                jsArray->JSArray::put(exec, i, value);
        } else
            baseValue->put(exec, i, value);
    } else {
        Identifier property(exec, subscript->toString(exec));
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue->put(exec, property, value, slot);
        }
    }

    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_val_array(CTI_ARGS)
{
    ExecState* exec = ARG_exec;

    JSValue* baseValue = ARG_src1;
    int i = ARG_int2;
    JSValue* value = ARG_src3;

    ASSERT(ARG_globalData->machine->isJSArray(baseValue));

    if (LIKELY(i >= 0))
        static_cast<JSArray*>(baseValue)->JSArray::put(exec, i, value);
    else {
        Identifier property(exec, JSImmediate::from(i)->toString(exec));
        // FIXME: can toString throw an exception here?
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue->put(exec, property, value, slot);
        }
    }

    VM_CHECK_EXCEPTION_AT_END();
}

JSValue* Machine::cti_op_lesseq(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    JSValue* result = jsBoolean(jsLessEq(exec, ARG_src1, ARG_src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Machine::cti_op_loop_if_true(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;

    ExecState* exec = ARG_exec;

    bool result = src1->toBoolean(exec);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_negate(CTI_ARGS)
{
    JSValue* src = ARG_src1;

    double v;
    if (fastIsNumber(src, v))
        return jsNumber(ARG_globalData, -v);

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, -src->toNumber(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_resolve_base(CTI_ARGS)
{
    return inlineResolveBase(ARG_exec, *ARG_id1, scopeChain(ARG_r));
}

JSValue* Machine::cti_op_resolve_skip(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    ScopeChainNode* scopeChain = Machine::scopeChain(r);

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
        if (o->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            VM_CHECK_EXCEPTION_AT_END();
            return result;
        }
    } while (++iter != end);

    CodeBlock* codeBlock = Machine::codeBlock(r);
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    exec->setException(createUndefinedVariableError(exec, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock));
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_resolve_global(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(ARG_src1);
    Identifier& ident = *ARG_id2;
    Instruction* vPC = ARG_instr3;
    ASSERT(globalObject->isGlobalObject());

    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(exec, ident, slot)) {
        JSValue* result = slot.getValue(exec, ident);
        if (slot.isCacheable()) {
            if (vPC[4].u.structureID)
                vPC[4].u.structureID->deref();
            globalObject->structureID()->ref();
            vPC[4] = globalObject->structureID();
            vPC[5] = slot.cachedOffset();
            return result;
        }

        VM_CHECK_EXCEPTION_AT_END();
        return result;
    }
    
    Register* r = ARG_r;
    exec->setException(createUndefinedVariableError(exec, ident, vPC, codeBlock(r)));    
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_div(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left / right);

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, src1->toNumber(exec) / src2->toNumber(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_pre_dec(CTI_ARGS)
{
    JSValue* v = ARG_src1;

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, v->toNumber(exec) - 1);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Machine::cti_op_jless(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;
    ExecState* exec = ARG_exec;

    bool result = jsLess(exec, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_not(CTI_ARGS)
{
    JSValue* src = ARG_src1;

    ExecState* exec = ARG_exec;

    JSValue* result = jsBoolean(!src->toBoolean(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int SFX_CALL Machine::cti_op_jtrue(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;

    ExecState* exec = ARG_exec;

    bool result = src1->toBoolean(exec);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_post_inc(CTI_ARGS)
{
    JSValue* v = ARG_src1;

    ExecState* exec = ARG_exec;

    JSValue* number = v->toJSNumber(exec);
    VM_CHECK_EXCEPTION();
    ARG_set2ndResult(jsNumber(ARG_globalData, number->uncheckedGetNumber() + 1));
    return number;
}

JSValue* Machine::cti_op_eq(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    ExecState* exec = ARG_exec;

    ASSERT(!JSImmediate::areBothImmediateNumbers(src1, src2));
    JSValue* result = jsBoolean(equalSlowCaseInline(exec, src1, src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_lshift(CTI_ARGS)
{
    JSValue* val = ARG_src1;
    JSValue* shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSImmediate::areBothImmediateNumbers(val, shift))
        return jsNumber(ARG_globalData, JSImmediate::getTruncatedInt32(val) << (JSImmediate::getTruncatedUInt32(shift) & 0x1f));
    if (fastToInt32(val, left) && fastToUInt32(shift, right))
        return jsNumber(ARG_globalData, left << (right & 0x1f));

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, (val->toInt32(exec)) << (shift->toUInt32(exec) & 0x1f));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_bitand(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    int32_t left;
    int32_t right;
    if (fastToInt32(src1, left) && fastToInt32(src2, right))
        return jsNumber(ARG_globalData, left & right);

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, src1->toInt32(exec) & src2->toInt32(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_rshift(CTI_ARGS)
{
    JSValue* val = ARG_src1;
    JSValue* shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSImmediate::areBothImmediateNumbers(val, shift))
        return JSImmediate::rightShiftImmediateNumbers(val, shift);
    if (fastToInt32(val, left) && fastToUInt32(shift, right))
        return jsNumber(ARG_globalData, left >> (right & 0x1f));

    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, (val->toInt32(exec)) >> (shift->toUInt32(exec) & 0x1f));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_bitnot(CTI_ARGS)
{
    JSValue* src = ARG_src1;

    int value;
    if (fastToInt32(src, value))
        return jsNumber(ARG_globalData, ~value);
            
    ExecState* exec = ARG_exec;
    JSValue* result = jsNumber(ARG_globalData, ~src->toInt32(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_resolve_with_base(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    ScopeChainNode* scopeChain = Machine::scopeChain(r);

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            VM_CHECK_EXCEPTION_AT_END();
            ARG_set2ndResult(result);
            return base;
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = Machine::codeBlock(r);
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    exec->setException(createUndefinedVariableError(exec, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock));
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_new_func_exp(CTI_ARGS)
{
    return ARG_funcexp1->makeFunction(ARG_exec, scopeChain(ARG_r));
}

JSValue* Machine::cti_op_mod(CTI_ARGS)
{
    JSValue* dividendValue = ARG_src1;
    JSValue* divisorValue = ARG_src2;

    ExecState* exec = ARG_exec;
    double d = dividendValue->toNumber(exec);
    JSValue* result = jsNumber(ARG_globalData, fmod(d, divisorValue->toNumber(exec)));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_less(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    JSValue* result = jsBoolean(jsLess(exec, ARG_src1, ARG_src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_neq(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    ASSERT(!JSImmediate::areBothImmediateNumbers(src1, src2));

    ExecState* exec = ARG_exec;
    JSValue* result = jsBoolean(!equalSlowCaseInline(exec, src1, src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_post_dec(CTI_ARGS)
{
    JSValue* v = ARG_src1;

    ExecState* exec = ARG_exec;

    JSValue* number = v->toJSNumber(exec);
    VM_CHECK_EXCEPTION();

    ARG_set2ndResult(jsNumber(ARG_globalData, number->uncheckedGetNumber() - 1));
    return number;
}

JSValue* Machine::cti_op_urshift(CTI_ARGS)
{
    JSValue* val = ARG_src1;
    JSValue* shift = ARG_src2;

    ExecState* exec = ARG_exec;

    if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
        return JSImmediate::rightShiftImmediateNumbers(val, shift);
    else {
        JSValue* result = jsNumber(ARG_globalData, (val->toUInt32(exec)) >> (shift->toUInt32(exec) & 0x1f));
        VM_CHECK_EXCEPTION_AT_END();
        return result;
    }
}

JSValue* Machine::cti_op_bitxor(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    ExecState* exec = ARG_exec;

    JSValue* result = jsNumber(ARG_globalData, src1->toInt32(exec) ^ src2->toInt32(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_new_regexp(CTI_ARGS)
{
    return new (ARG_globalData) RegExpObject(scopeChain(ARG_r)->globalObject()->regExpStructure(), ARG_regexp1);
}

JSValue* Machine::cti_op_bitor(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    ExecState* exec = ARG_exec;

    JSValue* result = jsNumber(ARG_globalData, src1->toInt32(exec) | src2->toInt32(exec));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_call_eval(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    RegisterFile* registerFile = ARG_registerFile;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);
    ScopeChainNode* scopeChain = Machine::scopeChain(r);

    Machine* machine = ARG_globalData->machine;
    
    JSValue* funcVal = ARG_src1;
    int registerOffset = ARG_int2;
    int argCount = ARG_int3;
    JSValue* baseVal = ARG_src5;

    if (baseVal == scopeChain->globalObject() && funcVal == scopeChain->globalObject()->evalFunction()) {
        JSObject* thisObject = static_cast<JSObject*>(r[codeBlock->thisRegister].jsValue(exec));
        JSValue* exceptionValue = 0;
        JSValue* result = machine->callEval(exec, thisObject, scopeChain, registerFile,  r, registerOffset - RegisterFile::CallFrameHeaderSize - argCount, argCount, exceptionValue);
        VM_CHECK_EXCEPTION_ARG(exceptionValue);
        return result;
    }

    return JSImmediate::impossibleValue();
}

void* Machine::cti_op_throw(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);

    JSValue* exceptionValue = ARG_src1;
    ASSERT(exceptionValue);

    Instruction* handlerVPC = ARG_globalData->machine->throwException(exec, exceptionValue, codeBlock->instructions.begin() + vPCIndex, r, true);

    if (!handlerVPC) {
        *ARG_exception = exceptionValue;
        return JSImmediate::nullImmediate();
    }

    ARG_setR(r);
    void* catchRoutine = Machine::codeBlock(r)->nativeExceptionCodeForHandlerVPC(handlerVPC);
    ASSERT(catchRoutine);
    ctiSetReturnAddress(&CTI_RETURN_ADDRESS, catchRoutine);
    return exceptionValue;
}

JSPropertyNameIterator* Machine::cti_op_get_pnames(CTI_ARGS)
{
    return JSPropertyNameIterator::create(ARG_exec, ARG_src1);
}

JSValue* Machine::cti_op_next_pname(CTI_ARGS)
{
    JSPropertyNameIterator* it = ARG_pni1;
    JSValue* temp = it->next(ARG_exec);
    if (!temp)
        it->invalidate();
    return temp;
}

void Machine::cti_op_push_scope(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    JSValue* v = ARG_src1;

    JSObject* o = v->toObject(exec);
    VM_CHECK_EXCEPTION_VOID();

    Register* r = ARG_r;
    r[RegisterFile::ScopeChain] = scopeChain(r)->push(o);
}

void Machine::cti_op_pop_scope(CTI_ARGS)
{
    Register* r = ARG_r;
    r[RegisterFile::ScopeChain] = scopeChain(r)->pop();
}

JSValue* Machine::cti_op_typeof(CTI_ARGS)
{
    return jsTypeStringForValue(ARG_exec, ARG_src1);
}

JSValue* Machine::cti_op_is_undefined(CTI_ARGS)
{
    JSValue* v = ARG_src1;
    return jsBoolean(JSImmediate::isImmediate(v) ? v->isUndefined() : v->asCell()->structureID()->typeInfo().masqueradesAsUndefined());
}

JSValue* Machine::cti_op_is_boolean(CTI_ARGS)
{
    return jsBoolean(ARG_src1->isBoolean());
}

JSValue* Machine::cti_op_is_number(CTI_ARGS)
{
    return jsBoolean(ARG_src1->isNumber());
}

JSValue* Machine::cti_op_is_string(CTI_ARGS)
{
    return jsBoolean(ARG_globalData->machine->isJSString(ARG_src1));
}

JSValue* Machine::cti_op_is_object(CTI_ARGS)
{
    return jsBoolean(jsIsObjectType(ARG_src1));
}

JSValue* Machine::cti_op_is_function(CTI_ARGS)
{
    return jsBoolean(jsIsFunctionType(ARG_src1));
}

JSValue* Machine::cti_op_stricteq(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    // handled inline as fast cases
    ASSERT(!JSImmediate::areBothImmediate(src1, src2));
    ASSERT(!(JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate())));

    return jsBoolean(strictEqualSlowCaseInline(src1, src2));
}

JSValue* Machine::cti_op_nstricteq(CTI_ARGS)
{
    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    // handled inline as fast cases
    ASSERT(!JSImmediate::areBothImmediate(src1, src2));
    ASSERT(!(JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate())));
    
    return jsBoolean(!strictEqualSlowCaseInline(src1, src2));
}

JSValue* Machine::cti_op_to_jsnumber(CTI_ARGS)
{
    JSValue* src = ARG_src1;
    ExecState* exec = ARG_exec;

    JSValue* result = src->toJSNumber(exec);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_in(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    JSValue* baseVal = ARG_src2;

    if (!baseVal->isObject()) {
        Register* r = ARG_r;
        CodeBlock* codeBlock = Machine::codeBlock(r);
        ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
        unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
        exec->setException(createInvalidParamError(exec, "in", baseVal, codeBlock->instructions.begin() + vPCIndex, codeBlock));
        VM_THROW_EXCEPTION();
    }

    JSValue* propName = ARG_src1;
    JSObject* baseObj = static_cast<JSObject*>(baseVal);

    uint32_t i;
    if (propName->getUInt32(i))
        return jsBoolean(baseObj->hasProperty(exec, i));

    Identifier property(exec, propName->toString(exec));
    VM_CHECK_EXCEPTION();
    return jsBoolean(baseObj->hasProperty(exec, property));
}

JSValue* Machine::cti_op_push_new_scope(CTI_ARGS)
{
    JSObject* scope = new (ARG_globalData) JSStaticScopeObject(ARG_exec, *ARG_id1, ARG_src2, DontDelete);

    Register* r = ARG_r;
    r[RegisterFile::ScopeChain] = scopeChain(r)->push(scope);
    return scope;
}

void Machine::cti_op_jmp_scopes(CTI_ARGS)
{
    unsigned count = ARG_int1;
    Register* r = ARG_r;

    ScopeChainNode* tmp = scopeChain(r);
    while (count--)
        tmp = tmp->pop();
    r[RegisterFile::ScopeChain] = tmp;
}

void Machine::cti_op_put_by_index(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    unsigned property = ARG_int2;

    ARG_src1->put(exec, property, ARG_src3);
}

void* Machine::cti_op_switch_imm(CTI_ARGS)
{
    JSValue* scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);

    if (JSImmediate::isNumber(scrutinee)) {
        int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
        return codeBlock->immediateSwitchJumpTables[tableIndex].ctiForValue(value);
    }

    return codeBlock->immediateSwitchJumpTables[tableIndex].ctiDefault;
}

void* Machine::cti_op_switch_char(CTI_ARGS)
{
    JSValue* scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);

    void* result = codeBlock->characterSwitchJumpTables[tableIndex].ctiDefault;

    if (scrutinee->isString()) {
        UString::Rep* value = static_cast<JSString*>(scrutinee)->value().rep();
        if (value->size() == 1)
            result = codeBlock->characterSwitchJumpTables[tableIndex].ctiForValue(value->data()[0]);
    }

    return result;
}

void* Machine::cti_op_switch_string(CTI_ARGS)
{
    JSValue* scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);

    void* result = codeBlock->stringSwitchJumpTables[tableIndex].ctiDefault;

    if (scrutinee->isString()) {
        UString::Rep* value = static_cast<JSString*>(scrutinee)->value().rep();
        result = codeBlock->stringSwitchJumpTables[tableIndex].ctiForValue(value);
    }

    return result;
}

JSValue* Machine::cti_op_del_by_val(CTI_ARGS)
{
    ExecState* exec = ARG_exec;

    JSValue* baseValue = ARG_src1;
    JSObject* baseObj = baseValue->toObject(exec); // may throw

    JSValue* subscript = ARG_src2;
    JSValue* result;
    uint32_t i;
    if (subscript->getUInt32(i))
        result = jsBoolean(baseObj->deleteProperty(exec, i));
    else {
        VM_CHECK_EXCEPTION();
        Identifier property(exec, subscript->toString(exec));
        VM_CHECK_EXCEPTION();
        result = jsBoolean(baseObj->deleteProperty(exec, property));
    }

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Machine::cti_op_put_getter(CTI_ARGS)
{
    ExecState* exec = ARG_exec;

    ASSERT(ARG_src1->isObject());
    JSObject* baseObj = static_cast<JSObject*>(ARG_src1);
    Identifier& ident = *ARG_id2;
    ASSERT(ARG_src3->isObject());
    baseObj->defineGetter(exec, ident, static_cast<JSObject*>(ARG_src3));
}

void Machine::cti_op_put_setter(CTI_ARGS)
{
    ExecState* exec = ARG_exec;

    ASSERT(ARG_src1->isObject());
    JSObject* baseObj = static_cast<JSObject*>(ARG_src1);
    Identifier& ident = *ARG_id2;
    ASSERT(ARG_src3->isObject());
    baseObj->defineSetter(exec, ident, static_cast<JSObject*>(ARG_src3));
}

JSValue* Machine::cti_op_new_error(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);
    unsigned type = ARG_int1;
    JSValue* message = ARG_src2;
    unsigned lineNumber = ARG_int3;

    return Error::create(exec, static_cast<ErrorType>(type), message->toString(exec), lineNumber, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->sourceURL());
}

void Machine::cti_op_debug(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;

    int debugHookID = ARG_int1;
    int firstLine = ARG_int2;
    int lastLine = ARG_int3;

    ARG_globalData->machine->debug(exec, r, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
}

void* Machine::cti_vm_throw(CTI_ARGS)
{
    ExecState* exec = ARG_exec;
    Register* r = ARG_r;
    CodeBlock* codeBlock = Machine::codeBlock(r);

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(ARG_globalData->throwReturnAddress));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(ARG_globalData->throwReturnAddress);

    JSValue* exceptionValue = ARG_globalData->exception;
    ASSERT(exceptionValue);
    ARG_globalData->exception = 0;

    Instruction* handlerVPC = ARG_globalData->machine->throwException(exec, exceptionValue, codeBlock->instructions.begin() + vPCIndex, r, false);

    if (!handlerVPC) {
        *ARG_exception = exceptionValue;
        return JSImmediate::nullImmediate();
    }

    ARG_setR(r);
    void* catchRoutine = Machine::codeBlock(r)->nativeExceptionCodeForHandlerVPC(handlerVPC);
    ASSERT(catchRoutine);
    ctiSetReturnAddress(&CTI_RETURN_ADDRESS, catchRoutine);
    return exceptionValue;
}

#undef VM_CHECK_EXCEPTION
#undef VM_CHECK_EXCEPTION_ARG
#undef VM_CHECK_EXCEPTION_AT_END
#undef VM_CHECK_EXCEPTION_VOID

#endif // ENABLE(CTI)

} // namespace JSC
