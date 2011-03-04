/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich (cwzwarich@uwaterloo.ca)
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
#include "JSGlobalObject.h"

#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"

#include "Arguments.h"
#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "BooleanConstructor.h"
#include "BooleanPrototype.h"
#include "CodeBlock.h"
#include "DateConstructor.h"
#include "DatePrototype.h"
#include "ErrorConstructor.h"
#include "ErrorPrototype.h"
#include "FunctionConstructor.h"
#include "FunctionPrototype.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "Interpreter.h"
#include "MathObject.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Profiler.h"
#include "RegExpConstructor.h"
#include "RegExpMatchesArray.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "ScopeChainMark.h"
#include "StringConstructor.h"
#include "StringPrototype.h"
#include "Debugger.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSGlobalObject);

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

template <typename T> static inline void markIfNeeded(MarkStack& markStack, WriteBarrier<T>* v)
{
    if (*v)
        markStack.append(v);
}

static inline void markIfNeeded(MarkStack& markStack, const RefPtr<Structure>& s)
{
    if (s && s->storedPrototype())
        markStack.append(s->storedPrototypeSlot());
}

JSGlobalObject::~JSGlobalObject()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (d()->debugger)
        d()->debugger->detach(this);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (UNLIKELY(*profiler != 0)) {
        (*profiler)->stopProfiling(globalExec(), UString());
    }

    d()->destructor(d());
}

void JSGlobalObject::init(JSObject* thisValue)
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    
    structure()->disableSpecificFunctionTracking();

    d()->globalData = Heap::heap(this)->globalData();
    d()->globalScopeChain.set(*d()->globalData, this, new (d()->globalData.get()) ScopeChainNode(0, this, d()->globalData.get(), this, thisValue));

    JSGlobalObject::globalExec()->init(0, 0, d()->globalScopeChain.get(), CallFrame::noCaller(), 0, 0);

    d()->debugger = 0;

    d()->profileGroup = 0;

    reset(prototype());
}

void JSGlobalObject::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (symbolTablePut(exec->globalData(), propertyName, value))
        return;
    JSVariableObject::put(exec, propertyName, value, slot);
}

void JSGlobalObject::putWithAttributes(ExecState* exec, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (symbolTablePutWithAttributes(exec->globalData(), propertyName, value, attributes))
        return;

    JSValue valueBefore = getDirect(propertyName);
    PutPropertySlot slot;
    JSVariableObject::put(exec, propertyName, value, slot);
    if (!valueBefore) {
        JSValue valueAfter = getDirect(propertyName);
        if (valueAfter)
            JSObject::putWithAttributes(exec, propertyName, valueAfter, attributes);
    }
}

void JSGlobalObject::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunc, unsigned attributes)
{
    PropertySlot slot;
    if (!symbolTableGet(propertyName, slot))
        JSVariableObject::defineGetter(exec, propertyName, getterFunc, attributes);
}

void JSGlobalObject::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunc, unsigned attributes)
{
    PropertySlot slot;
    if (!symbolTableGet(propertyName, slot))
        JSVariableObject::defineSetter(exec, propertyName, setterFunc, attributes);
}

static inline JSObject* lastInPrototypeChain(JSObject* object)
{
    JSObject* o = object;
    while (o->prototype().isObject())
        o = asObject(o->prototype());
    return o;
}

void JSGlobalObject::reset(JSValue prototype)
{
    ExecState* exec = JSGlobalObject::globalExec();

    // Prototypes

    d()->functionPrototype.set(exec->globalData(), this, new (exec) FunctionPrototype(exec, this, FunctionPrototype::createStructure(jsNull()))); // The real prototype will be set once ObjectPrototype is created.
    d()->functionStructure = JSFunction::createStructure(d()->functionPrototype.get());
    d()->internalFunctionStructure = InternalFunction::createStructure(d()->functionPrototype.get());
    JSFunction* callFunction = 0;
    JSFunction* applyFunction = 0;
    d()->functionPrototype->addFunctionProperties(exec, this, d()->functionStructure.get(), &callFunction, &applyFunction);
    d()->callFunction.set(exec->globalData(), this, callFunction);
    d()->applyFunction.set(exec->globalData(), this, applyFunction);
    d()->objectPrototype.set(exec->globalData(), this, new (exec) ObjectPrototype(exec, this, ObjectPrototype::createStructure(jsNull()), d()->functionStructure.get()));
    d()->functionPrototype->structure()->setPrototypeWithoutTransition(d()->objectPrototype.get());

    d()->emptyObjectStructure = d()->objectPrototype->inheritorID();

    d()->callbackFunctionStructure = JSCallbackFunction::createStructure(d()->functionPrototype.get());
    d()->argumentsStructure = Arguments::createStructure(d()->objectPrototype.get());
    d()->callbackConstructorStructure = JSCallbackConstructor::createStructure(d()->objectPrototype.get());
    d()->callbackObjectStructure = JSCallbackObject<JSObjectWithGlobalObject>::createStructure(d()->objectPrototype.get());

    d()->arrayPrototype.set(exec->globalData(), this, new (exec) ArrayPrototype(this, ArrayPrototype::createStructure(d()->objectPrototype.get())));
    d()->arrayStructure = JSArray::createStructure(d()->arrayPrototype.get());
    d()->regExpMatchesArrayStructure = RegExpMatchesArray::createStructure(d()->arrayPrototype.get());

    d()->stringPrototype.set(exec->globalData(), this, new (exec) StringPrototype(exec, this, StringPrototype::createStructure(d()->objectPrototype.get())));
    d()->stringObjectStructure = StringObject::createStructure(d()->stringPrototype.get());

    d()->booleanPrototype.set(exec->globalData(), this, new (exec) BooleanPrototype(exec, this, BooleanPrototype::createStructure(d()->objectPrototype.get()), d()->functionStructure.get()));
    d()->booleanObjectStructure = BooleanObject::createStructure(d()->booleanPrototype.get());

    d()->numberPrototype.set(exec->globalData(), this, new (exec) NumberPrototype(exec, this, NumberPrototype::createStructure(d()->objectPrototype.get()), d()->functionStructure.get()));
    d()->numberObjectStructure = NumberObject::createStructure(d()->numberPrototype.get());

    d()->datePrototype.set(exec->globalData(), this, new (exec) DatePrototype(exec, this, DatePrototype::createStructure(d()->objectPrototype.get())));
    d()->dateStructure = DateInstance::createStructure(d()->datePrototype.get());

    d()->regExpPrototype.set(exec->globalData(), this, new (exec) RegExpPrototype(exec, this, RegExpPrototype::createStructure(d()->objectPrototype.get()), d()->functionStructure.get()));
    d()->regExpStructure = RegExpObject::createStructure(d()->regExpPrototype.get());

    d()->methodCallDummy.set(exec->globalData(), this, constructEmptyObject(exec));

    ErrorPrototype* errorPrototype = new (exec) ErrorPrototype(exec, this, ErrorPrototype::createStructure(d()->objectPrototype.get()), d()->functionStructure.get());
    d()->errorStructure = ErrorInstance::createStructure(errorPrototype);

    // Constructors

    JSCell* objectConstructor = new (exec) ObjectConstructor(exec, this, ObjectConstructor::createStructure(d()->functionPrototype.get()), d()->objectPrototype.get(), d()->functionStructure.get());
    JSCell* functionConstructor = new (exec) FunctionConstructor(exec, this, FunctionConstructor::createStructure(d()->functionPrototype.get()), d()->functionPrototype.get());
    JSCell* arrayConstructor = new (exec) ArrayConstructor(exec, this, ArrayConstructor::createStructure(d()->functionPrototype.get()), d()->arrayPrototype.get(), d()->functionStructure.get());
    JSCell* stringConstructor = new (exec) StringConstructor(exec, this, StringConstructor::createStructure(d()->functionPrototype.get()), d()->functionStructure.get(), d()->stringPrototype.get());
    JSCell* booleanConstructor = new (exec) BooleanConstructor(exec, this, BooleanConstructor::createStructure(d()->functionPrototype.get()), d()->booleanPrototype.get());
    JSCell* numberConstructor = new (exec) NumberConstructor(exec, this, NumberConstructor::createStructure(d()->functionPrototype.get()), d()->numberPrototype.get());
    JSCell* dateConstructor = new (exec) DateConstructor(exec, this, DateConstructor::createStructure(d()->functionPrototype.get()), d()->functionStructure.get(), d()->datePrototype.get());

    d()->regExpConstructor.set(exec->globalData(), this, new (exec) RegExpConstructor(exec, this, RegExpConstructor::createStructure(d()->functionPrototype.get()), d()->regExpPrototype.get()));

    d()->errorConstructor.set(exec->globalData(), this, new (exec) ErrorConstructor(exec, this, ErrorConstructor::createStructure(d()->functionPrototype.get()), errorPrototype));

    RefPtr<Structure> nativeErrorPrototypeStructure = NativeErrorPrototype::createStructure(errorPrototype);
    RefPtr<Structure> nativeErrorStructure = NativeErrorConstructor::createStructure(d()->functionPrototype.get());
    d()->evalErrorConstructor.set(exec->globalData(), this, new (exec) NativeErrorConstructor(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "EvalError"));
    d()->rangeErrorConstructor.set(exec->globalData(), this, new (exec) NativeErrorConstructor(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "RangeError"));
    d()->referenceErrorConstructor.set(exec->globalData(), this, new (exec) NativeErrorConstructor(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "ReferenceError"));
    d()->syntaxErrorConstructor.set(exec->globalData(), this, new (exec) NativeErrorConstructor(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "SyntaxError"));
    d()->typeErrorConstructor.set(exec->globalData(), this, new (exec) NativeErrorConstructor(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "TypeError"));
    d()->URIErrorConstructor.set(exec->globalData(), this, new (exec) NativeErrorConstructor(exec, this, nativeErrorStructure, nativeErrorPrototypeStructure, "URIError"));

    d()->objectPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, objectConstructor, DontEnum);
    d()->functionPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, functionConstructor, DontEnum);
    d()->arrayPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, arrayConstructor, DontEnum);
    d()->booleanPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, booleanConstructor, DontEnum);
    d()->stringPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, stringConstructor, DontEnum);
    d()->numberPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, numberConstructor, DontEnum);
    d()->datePrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, dateConstructor, DontEnum);
    d()->regExpPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, d()->regExpConstructor.get(), DontEnum);
    errorPrototype->putDirectFunctionWithoutTransition(exec->globalData(), exec->propertyNames().constructor, d()->errorConstructor.get(), DontEnum);

    // Set global constructors

    // FIXME: These properties could be handled by a static hash table.

    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Object"), objectConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Function"), functionConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Array"), arrayConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Boolean"), booleanConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "String"), stringConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Number"), numberConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Date"), dateConstructor, DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "RegExp"), d()->regExpConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "Error"), d()->errorConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "EvalError"), d()->evalErrorConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "RangeError"), d()->rangeErrorConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "ReferenceError"), d()->referenceErrorConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "SyntaxError"), d()->syntaxErrorConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "TypeError"), d()->typeErrorConstructor.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec->globalData(), Identifier(exec, "URIError"), d()->URIErrorConstructor.get(), DontEnum);

    // Set global values.
    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(exec, "Math"), new (exec) MathObject(exec, this, MathObject::createStructure(d()->objectPrototype.get())), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "NaN"), jsNaN(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "Infinity"), jsNumber(Inf), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "undefined"), jsUndefined(), DontEnum | DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(exec, "JSON"), new (exec) JSONObject(this, JSONObject::createStructure(d()->objectPrototype.get())), DontEnum | DontDelete)
    };

    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));

    // Set global functions.

    d()->evalFunction.set(exec->globalData(), this, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, exec->propertyNames().eval, globalFuncEval));
    putDirectFunctionWithoutTransition(exec, d()->evalFunction.get(), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 2, Identifier(exec, "parseInt"), globalFuncParseInt), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "parseFloat"), globalFuncParseFloat), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "isNaN"), globalFuncIsNaN), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "isFinite"), globalFuncIsFinite), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "escape"), globalFuncEscape), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "unescape"), globalFuncUnescape), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "decodeURI"), globalFuncDecodeURI), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "decodeURIComponent"), globalFuncDecodeURIComponent), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "encodeURI"), globalFuncEncodeURI), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "encodeURIComponent"), globalFuncEncodeURIComponent), DontEnum);
#ifndef NDEBUG
    putDirectFunctionWithoutTransition(exec, new (exec) JSFunction(exec, this, d()->functionStructure.get(), 1, Identifier(exec, "jscprint"), globalFuncJSCPrint), DontEnum);
#endif

    resetPrototype(prototype);
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(JSValue prototype)
{
    setPrototype(prototype);

    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(this);
    JSObject* objectPrototype = d()->objectPrototype.get();
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototype(objectPrototype);
}

void JSGlobalObject::markChildren(MarkStack& markStack)
{
    JSVariableObject::markChildren(markStack);
    
    markIfNeeded(markStack, &d()->globalScopeChain);

    markIfNeeded(markStack, &d()->regExpConstructor);
    markIfNeeded(markStack, &d()->errorConstructor);
    markIfNeeded(markStack, &d()->evalErrorConstructor);
    markIfNeeded(markStack, &d()->rangeErrorConstructor);
    markIfNeeded(markStack, &d()->referenceErrorConstructor);
    markIfNeeded(markStack, &d()->syntaxErrorConstructor);
    markIfNeeded(markStack, &d()->typeErrorConstructor);
    markIfNeeded(markStack, &d()->URIErrorConstructor);

    markIfNeeded(markStack, &d()->evalFunction);
    markIfNeeded(markStack, &d()->callFunction);
    markIfNeeded(markStack, &d()->applyFunction);

    markIfNeeded(markStack, &d()->objectPrototype);
    markIfNeeded(markStack, &d()->functionPrototype);
    markIfNeeded(markStack, &d()->arrayPrototype);
    markIfNeeded(markStack, &d()->booleanPrototype);
    markIfNeeded(markStack, &d()->stringPrototype);
    markIfNeeded(markStack, &d()->numberPrototype);
    markIfNeeded(markStack, &d()->datePrototype);
    markIfNeeded(markStack, &d()->regExpPrototype);

    markIfNeeded(markStack, &d()->methodCallDummy);

    markIfNeeded(markStack, d()->errorStructure);
    markIfNeeded(markStack, d()->argumentsStructure);
    markIfNeeded(markStack, d()->arrayStructure);
    markIfNeeded(markStack, d()->booleanObjectStructure);
    markIfNeeded(markStack, d()->callbackConstructorStructure);
    markIfNeeded(markStack, d()->callbackFunctionStructure);
    markIfNeeded(markStack, d()->callbackObjectStructure);
    markIfNeeded(markStack, d()->dateStructure);
    markIfNeeded(markStack, d()->emptyObjectStructure);
    markIfNeeded(markStack, d()->errorStructure);
    markIfNeeded(markStack, d()->functionStructure);
    markIfNeeded(markStack, d()->numberObjectStructure);
    markIfNeeded(markStack, d()->regExpMatchesArrayStructure);
    markIfNeeded(markStack, d()->regExpStructure);
    markIfNeeded(markStack, d()->stringObjectStructure);

    // No need to mark the other structures, because their prototypes are all
    // guaranteed to be referenced elsewhere.

    if (d()->registerArray) {
        // Outside the execution of global code, when our variables are torn off,
        // we can mark the torn-off array.
        markStack.appendValues(d()->registerArray.get(), d()->registerArraySize);
    } else if (d()->registers) {
        // During execution of global code, when our variables are in the register file,
        // the symbol table tells us how many variables there are, and registers
        // points to where they end, and the registers used for execution begin.
        markStack.appendValues(d()->registers - symbolTable().size(), symbolTable().size());
    }
}

ExecState* JSGlobalObject::globalExec()
{
    return CallFrame::create(d()->globalCallFrame + RegisterFile::CallFrameHeaderSize);
}

bool JSGlobalObject::isDynamicScope(bool&) const
{
    return true;
}

void JSGlobalObject::copyGlobalsFrom(RegisterFile& registerFile)
{
    ASSERT(!d()->registerArray);
    ASSERT(!d()->registerArraySize);

    int numGlobals = registerFile.numGlobals();
    if (!numGlobals) {
        d()->registers = 0;
        return;
    }

    OwnArrayPtr<WriteBarrier<Unknown> > registerArray = copyRegisterArray(globalData(), reinterpret_cast<WriteBarrier<Unknown>*>(registerFile.lastGlobal()), numGlobals);
    WriteBarrier<Unknown>* registers = registerArray.get() + numGlobals;
    setRegisters(registers, registerArray.release(), numGlobals);
}

void JSGlobalObject::copyGlobalsTo(RegisterFile& registerFile)
{
    JSGlobalObject* lastGlobalObject = registerFile.globalObject();
    if (lastGlobalObject && lastGlobalObject != this)
        lastGlobalObject->copyGlobalsFrom(registerFile);

    registerFile.setGlobalObject(this);
    registerFile.setNumGlobals(symbolTable().size());

    if (d()->registerArray) {
        // The register file is always a gc root so no barrier is needed here
        memcpy(registerFile.start() - d()->registerArraySize, d()->registerArray.get(), d()->registerArraySize * sizeof(WriteBarrier<Unknown>));
        setRegisters(reinterpret_cast<WriteBarrier<Unknown>*>(registerFile.start()), 0, 0);
    }
}

void JSGlobalObject::resizeRegisters(int oldSize, int newSize)
{
    ASSERT(symbolTable().size() == newSize);
    if (newSize == oldSize)
        return;
    ASSERT(newSize && newSize > oldSize);
    if (d()->registerArray || !d()->registers) {
        ASSERT(static_cast<size_t>(oldSize) == d()->registerArraySize);
        WriteBarrier<Unknown>* registerArray = new WriteBarrier<Unknown>[newSize];
        for (int i = 0; i < oldSize; i++)
            registerArray[newSize - oldSize + i].set(globalData(), this, d()->registerArray[i].get());
        setRegisters(registerArray + newSize, registerArray, newSize);
    } else {
        ASSERT(static_cast<size_t>(newSize) < globalData().interpreter->registerFile().maxGlobals());
        globalData().interpreter->registerFile().setNumGlobals(newSize);
    }

    for (int i = -newSize; i < -oldSize; ++i)
        d()->registers[i].setUndefined();
}

void* JSGlobalObject::operator new(size_t size, JSGlobalData* globalData)
{
    return globalData->heap.allocate(size);
}

void JSGlobalObject::destroyJSGlobalObjectData(void* jsGlobalObjectData)
{
    delete static_cast<JSGlobalObjectData*>(jsGlobalObjectData);
}

DynamicGlobalObjectScope::DynamicGlobalObjectScope(CallFrame* callFrame, JSGlobalObject* dynamicGlobalObject)
    : m_dynamicGlobalObjectSlot(callFrame->globalData().dynamicGlobalObject)
    , m_savedDynamicGlobalObject(m_dynamicGlobalObjectSlot)
{
    if (!m_dynamicGlobalObjectSlot) {
#if ENABLE(ASSEMBLER)
        if (ExecutableAllocator::underMemoryPressure())
            callFrame->globalData().recompileAllJSFunctions();
#endif

        m_dynamicGlobalObjectSlot = dynamicGlobalObject;

        // Reset the date cache between JS invocations to force the VM
        // to observe time zone changes.
        callFrame->globalData().resetDateCache();
    }
}

} // namespace JSC
