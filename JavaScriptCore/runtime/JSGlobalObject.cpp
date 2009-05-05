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
#include "GlobalEvalFunction.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLock.h"
#include "Interpreter.h"
#include "MathObject.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Profiler.h"
#include "PrototypeFunction.h"
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

static inline void markIfNeeded(JSValue v)
{
    if (v && !v.marked())
        v.mark();
}

static inline void markIfNeeded(const RefPtr<Structure>& s)
{
    if (s)
        s->mark();
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

    d()->next->d()->prev = d()->prev;
    d()->prev->d()->next = d()->next;
    JSGlobalObject*& headObject = head();
    if (headObject == this)
        headObject = d()->next;
    if (headObject == this)
        headObject = 0;

    HashSet<ProgramCodeBlock*>::const_iterator end = codeBlocks().end();
    for (HashSet<ProgramCodeBlock*>::const_iterator it = codeBlocks().begin(); it != end; ++it)
        (*it)->clearGlobalObject();
        
    RegisterFile& registerFile = globalData()->interpreter->registerFile();
    if (registerFile.globalObject() == this) {
        registerFile.setGlobalObject(0);
        registerFile.setNumGlobals(0);
    }
    delete d();
}

void JSGlobalObject::init(JSObject* thisValue)
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    d()->globalData = Heap::heap(this)->globalData();
    d()->globalScopeChain = ScopeChain(this, d()->globalData.get(), thisValue);

    JSGlobalObject::globalExec()->init(0, 0, d()->globalScopeChain.node(), CallFrame::noCaller(), 0, 0, 0);

    if (JSGlobalObject*& headObject = head()) {
        d()->prev = headObject;
        d()->next = headObject->d()->next;
        headObject->d()->next->d()->prev = this;
        headObject->d()->next = this;
    } else
        headObject = d()->next = d()->prev = this;

    d()->recursion = 0;
    d()->debugger = 0;

    d()->profileGroup = 0;

    reset(prototype());
}

void JSGlobalObject::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (symbolTablePut(propertyName, value))
        return;
    JSVariableObject::put(exec, propertyName, value, slot);
}

void JSGlobalObject::putWithAttributes(ExecState* exec, const Identifier& propertyName, JSValue value, unsigned attributes)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (symbolTablePutWithAttributes(propertyName, value, attributes))
        return;

    JSValue valueBefore = getDirect(propertyName);
    PutPropertySlot slot;
    JSVariableObject::put(exec, propertyName, value, slot);
    if (!valueBefore) {
        JSValue valueAfter = getDirect(propertyName);
        if (valueAfter)
            putDirect(propertyName, valueAfter, attributes);
    }
}

void JSGlobalObject::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunc)
{
    PropertySlot slot;
    if (!symbolTableGet(propertyName, slot))
        JSVariableObject::defineGetter(exec, propertyName, getterFunc);
}

void JSGlobalObject::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunc)
{
    PropertySlot slot;
    if (!symbolTableGet(propertyName, slot))
        JSVariableObject::defineSetter(exec, propertyName, setterFunc);
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

    d()->functionPrototype = new (exec) FunctionPrototype(exec, FunctionPrototype::createStructure(jsNull())); // The real prototype will be set once ObjectPrototype is created.
    d()->prototypeFunctionStructure = PrototypeFunction::createStructure(d()->functionPrototype);
    NativeFunctionWrapper* callFunction = 0;
    NativeFunctionWrapper* applyFunction = 0;
    d()->functionPrototype->addFunctionProperties(exec, d()->prototypeFunctionStructure.get(), &callFunction, &applyFunction);
    d()->callFunction = callFunction;
    d()->applyFunction = applyFunction;
    d()->objectPrototype = new (exec) ObjectPrototype(exec, ObjectPrototype::createStructure(jsNull()), d()->prototypeFunctionStructure.get());
    d()->functionPrototype->structure()->setPrototypeWithoutTransition(d()->objectPrototype);

    d()->emptyObjectStructure = d()->objectPrototype->inheritorID();

    d()->functionStructure = JSFunction::createStructure(d()->functionPrototype);
    d()->callbackFunctionStructure = JSCallbackFunction::createStructure(d()->functionPrototype);
    d()->argumentsStructure = Arguments::createStructure(d()->objectPrototype);
    d()->callbackConstructorStructure = JSCallbackConstructor::createStructure(d()->objectPrototype);
    d()->callbackObjectStructure = JSCallbackObject<JSObject>::createStructure(d()->objectPrototype);

    d()->arrayPrototype = new (exec) ArrayPrototype(ArrayPrototype::createStructure(d()->objectPrototype));
    d()->arrayStructure = JSArray::createStructure(d()->arrayPrototype);
    d()->regExpMatchesArrayStructure = RegExpMatchesArray::createStructure(d()->arrayPrototype);

    d()->stringPrototype = new (exec) StringPrototype(exec, StringPrototype::createStructure(d()->objectPrototype));
    d()->stringObjectStructure = StringObject::createStructure(d()->stringPrototype);

    d()->booleanPrototype = new (exec) BooleanPrototype(exec, BooleanPrototype::createStructure(d()->objectPrototype), d()->prototypeFunctionStructure.get());
    d()->booleanObjectStructure = BooleanObject::createStructure(d()->booleanPrototype);

    d()->numberPrototype = new (exec) NumberPrototype(exec, NumberPrototype::createStructure(d()->objectPrototype), d()->prototypeFunctionStructure.get());
    d()->numberObjectStructure = NumberObject::createStructure(d()->numberPrototype);

    d()->datePrototype = new (exec) DatePrototype(exec, DatePrototype::createStructure(d()->objectPrototype));
    d()->dateStructure = DateInstance::createStructure(d()->datePrototype);

    d()->regExpPrototype = new (exec) RegExpPrototype(exec, RegExpPrototype::createStructure(d()->objectPrototype), d()->prototypeFunctionStructure.get());
    d()->regExpStructure = RegExpObject::createStructure(d()->regExpPrototype);

    ErrorPrototype* errorPrototype = new (exec) ErrorPrototype(exec, ErrorPrototype::createStructure(d()->objectPrototype), d()->prototypeFunctionStructure.get());
    d()->errorStructure = ErrorInstance::createStructure(errorPrototype);

    RefPtr<Structure> nativeErrorPrototypeStructure = NativeErrorPrototype::createStructure(errorPrototype);

    NativeErrorPrototype* evalErrorPrototype = new (exec) NativeErrorPrototype(exec, nativeErrorPrototypeStructure, "EvalError", "EvalError");
    NativeErrorPrototype* rangeErrorPrototype = new (exec) NativeErrorPrototype(exec, nativeErrorPrototypeStructure, "RangeError", "RangeError");
    NativeErrorPrototype* referenceErrorPrototype = new (exec) NativeErrorPrototype(exec, nativeErrorPrototypeStructure, "ReferenceError", "ReferenceError");
    NativeErrorPrototype* syntaxErrorPrototype = new (exec) NativeErrorPrototype(exec, nativeErrorPrototypeStructure, "SyntaxError", "SyntaxError");
    NativeErrorPrototype* typeErrorPrototype = new (exec) NativeErrorPrototype(exec, nativeErrorPrototypeStructure, "TypeError", "TypeError");
    NativeErrorPrototype* URIErrorPrototype = new (exec) NativeErrorPrototype(exec, nativeErrorPrototypeStructure, "URIError", "URIError");

    // Constructors

    JSValue objectConstructor = new (exec) ObjectConstructor(exec, ObjectConstructor::createStructure(d()->functionPrototype), d()->objectPrototype);
    JSValue functionConstructor = new (exec) FunctionConstructor(exec, FunctionConstructor::createStructure(d()->functionPrototype), d()->functionPrototype);
    JSValue arrayConstructor = new (exec) ArrayConstructor(exec, ArrayConstructor::createStructure(d()->functionPrototype), d()->arrayPrototype);
    JSValue stringConstructor = new (exec) StringConstructor(exec, StringConstructor::createStructure(d()->functionPrototype), d()->prototypeFunctionStructure.get(), d()->stringPrototype);
    JSValue booleanConstructor = new (exec) BooleanConstructor(exec, BooleanConstructor::createStructure(d()->functionPrototype), d()->booleanPrototype);
    JSValue numberConstructor = new (exec) NumberConstructor(exec, NumberConstructor::createStructure(d()->functionPrototype), d()->numberPrototype);
    JSValue dateConstructor = new (exec) DateConstructor(exec, DateConstructor::createStructure(d()->functionPrototype), d()->prototypeFunctionStructure.get(), d()->datePrototype);

    d()->regExpConstructor = new (exec) RegExpConstructor(exec, RegExpConstructor::createStructure(d()->functionPrototype), d()->regExpPrototype);

    d()->errorConstructor = new (exec) ErrorConstructor(exec, ErrorConstructor::createStructure(d()->functionPrototype), errorPrototype);

    RefPtr<Structure> nativeErrorStructure = NativeErrorConstructor::createStructure(d()->functionPrototype);

    d()->evalErrorConstructor = new (exec) NativeErrorConstructor(exec, nativeErrorStructure, evalErrorPrototype);
    d()->rangeErrorConstructor = new (exec) NativeErrorConstructor(exec, nativeErrorStructure, rangeErrorPrototype);
    d()->referenceErrorConstructor = new (exec) NativeErrorConstructor(exec, nativeErrorStructure, referenceErrorPrototype);
    d()->syntaxErrorConstructor = new (exec) NativeErrorConstructor(exec, nativeErrorStructure, syntaxErrorPrototype);
    d()->typeErrorConstructor = new (exec) NativeErrorConstructor(exec, nativeErrorStructure, typeErrorPrototype);
    d()->URIErrorConstructor = new (exec) NativeErrorConstructor(exec, nativeErrorStructure, URIErrorPrototype);

    d()->objectPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, objectConstructor, DontEnum);
    d()->functionPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, functionConstructor, DontEnum);
    d()->arrayPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, arrayConstructor, DontEnum);
    d()->booleanPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, booleanConstructor, DontEnum);
    d()->stringPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, stringConstructor, DontEnum);
    d()->numberPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, numberConstructor, DontEnum);
    d()->datePrototype->putDirectWithoutTransition(exec->propertyNames().constructor, dateConstructor, DontEnum);
    d()->regExpPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, d()->regExpConstructor, DontEnum);
    errorPrototype->putDirectWithoutTransition(exec->propertyNames().constructor, d()->errorConstructor, DontEnum);

    evalErrorPrototype->putDirect(exec->propertyNames().constructor, d()->evalErrorConstructor, DontEnum);
    rangeErrorPrototype->putDirect(exec->propertyNames().constructor, d()->rangeErrorConstructor, DontEnum);
    referenceErrorPrototype->putDirect(exec->propertyNames().constructor, d()->referenceErrorConstructor, DontEnum);
    syntaxErrorPrototype->putDirect(exec->propertyNames().constructor, d()->syntaxErrorConstructor, DontEnum);
    typeErrorPrototype->putDirect(exec->propertyNames().constructor, d()->typeErrorConstructor, DontEnum);
    URIErrorPrototype->putDirect(exec->propertyNames().constructor, d()->URIErrorConstructor, DontEnum);

    // Set global constructors

    // FIXME: These properties could be handled by a static hash table.

    putDirectWithoutTransition(Identifier(exec, "Object"), objectConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "Function"), functionConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "Array"), arrayConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "Boolean"), booleanConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "String"), stringConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "Number"), numberConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "Date"), dateConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "RegExp"), d()->regExpConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "Error"), d()->errorConstructor, DontEnum);
    putDirectWithoutTransition(Identifier(exec, "EvalError"), d()->evalErrorConstructor);
    putDirectWithoutTransition(Identifier(exec, "RangeError"), d()->rangeErrorConstructor);
    putDirectWithoutTransition(Identifier(exec, "ReferenceError"), d()->referenceErrorConstructor);
    putDirectWithoutTransition(Identifier(exec, "SyntaxError"), d()->syntaxErrorConstructor);
    putDirectWithoutTransition(Identifier(exec, "TypeError"), d()->typeErrorConstructor);
    putDirectWithoutTransition(Identifier(exec, "URIError"), d()->URIErrorConstructor);

    // Set global values.
    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(exec, "Math"), new (exec) MathObject(exec, MathObject::createStructure(d()->objectPrototype)), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "NaN"), jsNaN(exec), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "Infinity"), jsNumber(exec, Inf), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "undefined"), jsUndefined(), DontEnum | DontDelete)
    };

    addStaticGlobals(staticGlobals, sizeof(staticGlobals) / sizeof(GlobalPropertyInfo));

    // Set global functions.

    d()->evalFunction = new (exec) GlobalEvalFunction(exec, GlobalEvalFunction::createStructure(d()->functionPrototype), 1, exec->propertyNames().eval, globalFuncEval, this);
    putDirectFunctionWithoutTransition(exec, d()->evalFunction, DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 2, Identifier(exec, "parseInt"), globalFuncParseInt), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "parseFloat"), globalFuncParseFloat), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "isNaN"), globalFuncIsNaN), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "isFinite"), globalFuncIsFinite), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "escape"), globalFuncEscape), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "unescape"), globalFuncUnescape), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "decodeURI"), globalFuncDecodeURI), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "decodeURIComponent"), globalFuncDecodeURIComponent), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "encodeURI"), globalFuncEncodeURI), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "encodeURIComponent"), globalFuncEncodeURIComponent), DontEnum);
#ifndef NDEBUG
    putDirectFunctionWithoutTransition(exec, new (exec) NativeFunctionWrapper(exec, d()->prototypeFunctionStructure.get(), 1, Identifier(exec, "jscprint"), globalFuncJSCPrint), DontEnum);
#endif

    resetPrototype(prototype);
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(JSValue prototype)
{
    setPrototype(prototype);

    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(this);
    JSObject* objectPrototype = d()->objectPrototype;
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototype(objectPrototype);
}

void JSGlobalObject::mark()
{
    JSVariableObject::mark();
    
    HashSet<ProgramCodeBlock*>::const_iterator end = codeBlocks().end();
    for (HashSet<ProgramCodeBlock*>::const_iterator it = codeBlocks().begin(); it != end; ++it)
        (*it)->mark();

    RegisterFile& registerFile = globalData()->interpreter->registerFile();
    if (registerFile.globalObject() == this)
        registerFile.markGlobals(&globalData()->heap);

    markIfNeeded(d()->regExpConstructor);
    markIfNeeded(d()->errorConstructor);
    markIfNeeded(d()->evalErrorConstructor);
    markIfNeeded(d()->rangeErrorConstructor);
    markIfNeeded(d()->referenceErrorConstructor);
    markIfNeeded(d()->syntaxErrorConstructor);
    markIfNeeded(d()->typeErrorConstructor);
    markIfNeeded(d()->URIErrorConstructor);

    markIfNeeded(d()->evalFunction);
    markIfNeeded(d()->callFunction);
    markIfNeeded(d()->applyFunction);

    markIfNeeded(d()->objectPrototype);
    markIfNeeded(d()->functionPrototype);
    markIfNeeded(d()->arrayPrototype);
    markIfNeeded(d()->booleanPrototype);
    markIfNeeded(d()->stringPrototype);
    markIfNeeded(d()->numberPrototype);
    markIfNeeded(d()->datePrototype);
    markIfNeeded(d()->regExpPrototype);

    markIfNeeded(d()->errorStructure);

    // No need to mark the other structures, because their prototypes are all
    // guaranteed to be referenced elsewhere.

    Register* registerArray = d()->registerArray.get();
    if (!registerArray)
        return;

    size_t size = d()->registerArraySize;
    for (size_t i = 0; i < size; ++i) {
        Register& r = registerArray[i];
        if (!r.marked())
            r.mark();
    }
}

ExecState* JSGlobalObject::globalExec()
{
    return CallFrame::create(d()->globalCallFrame + RegisterFile::CallFrameHeaderSize);
}

bool JSGlobalObject::isDynamicScope() const
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
    
    Register* registerArray = copyRegisterArray(registerFile.lastGlobal(), numGlobals);
    setRegisters(registerArray + numGlobals, registerArray, numGlobals);
}

void JSGlobalObject::copyGlobalsTo(RegisterFile& registerFile)
{
    JSGlobalObject* lastGlobalObject = registerFile.globalObject();
    if (lastGlobalObject && lastGlobalObject != this)
        lastGlobalObject->copyGlobalsFrom(registerFile);

    registerFile.setGlobalObject(this);
    registerFile.setNumGlobals(symbolTable().size());

    if (d()->registerArray) {
        memcpy(registerFile.start() - d()->registerArraySize, d()->registerArray.get(), d()->registerArraySize * sizeof(Register));
        setRegisters(registerFile.start(), 0, 0);
    }
}

void* JSGlobalObject::operator new(size_t size, JSGlobalData* globalData)
{
#ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
    return globalData->heap.inlineAllocate(size);
#else
    return globalData->heap.allocate(size);
#endif
}

} // namespace JSC
