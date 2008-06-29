/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "Machine.h"
#include "MathObject.h"
#include "NativeErrorConstructor.h"
#include "NativeErrorPrototype.h"
#include "NumberConstructor.h"
#include "NumberPrototype.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "RegExpConstructor.h"
#include "RegExpPrototype.h"
#include "ScopeChainMark.h"
#include "StringConstructor.h"
#include "StringPrototype.h"
#include "debugger.h"

namespace KJS {

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

static inline void markIfNeeded(JSValue* v)
{
    if (v && !v->marked())
        v->mark();
}

JSGlobalObject::~JSGlobalObject()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (d()->debugger)
        d()->debugger->detach(this);

    d()->next->d()->prev = d()->prev;
    d()->prev->d()->next = d()->next;
    JSGlobalObject*& headObject = head();
    if (headObject == this)
        headObject = d()->next;
    if (headObject == this)
        headObject = 0;

    HashSet<ProgramCodeBlock*>::const_iterator end = codeBlocks().end();
    for (HashSet<ProgramCodeBlock*>::const_iterator it = codeBlocks().begin(); it != end; ++it)
        (*it)->globalObject = 0;
        
    RegisterFile& registerFile = globalData()->machine->registerFile();
    if (registerFile.globalObject() == this) {
        registerFile.setGlobalObject(0);
        registerFile.setNumGlobals(0);
    }
    delete d();
}

void JSGlobalObject::init(JSObject* thisValue)
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    d()->globalData = (Heap::heap(this) == JSGlobalData::sharedInstance().heap) ? &JSGlobalData::sharedInstance() : &JSGlobalData::threadInstance();

    if (JSGlobalObject*& headObject = head()) {
        d()->prev = headObject;
        d()->next = headObject->d()->next;
        headObject->d()->next->d()->prev = this;
        headObject->d()->next = this;
    } else
        headObject = d()->next = d()->prev = this;

    d()->recursion = 0;
    d()->debugger = 0;
    globalData()->machine->initTimeout();

    d()->globalExec.set(new ExecState(this, thisValue, d()->globalScopeChain.node()));

    d()->pageGroupIdentifier = 0;

    reset(prototype());
}

void JSGlobalObject::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

    if (symbolTablePut(propertyName, value))
        return;
    return JSVariableObject::put(exec, propertyName, value);
}

void JSGlobalObject::putWithAttributes(ExecState* exec, const Identifier& propertyName, JSValue* value, unsigned attributes)
{
    if (symbolTablePutWithAttributes(propertyName, value, attributes))
        return;

    JSValue* valueBefore = getDirect(propertyName);
    JSVariableObject::put(exec, propertyName, value);
    if (!valueBefore) {
        if (JSValue* valueAfter = getDirect(propertyName))
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
    while (o->prototype()->isObject())
        o = static_cast<JSObject*>(o->prototype());
    return o;
}

void JSGlobalObject::reset(JSValue* prototype)
{
    // Clear before inititalizing, to avoid calling mark() on stale pointers --
    // which would be wasteful -- or uninitialized pointers -- which would be
    // dangerous. (The allocations below may cause a GC.)

    _prop.clear();
    symbolTable().clear();

    // Prototypes
    d()->functionPrototype = 0;
    d()->objectPrototype = 0;

    d()->arrayPrototype = 0;
    d()->stringPrototype = 0;
    d()->booleanPrototype = 0;
    d()->numberPrototype = 0;
    d()->datePrototype = 0;
    d()->regExpPrototype = 0;
    d()->errorPrototype = 0;
    
    d()->evalErrorPrototype = 0;
    d()->rangeErrorPrototype = 0;
    d()->referenceErrorPrototype = 0;
    d()->syntaxErrorPrototype = 0;
    d()->typeErrorPrototype = 0;
    d()->URIErrorPrototype = 0;

    // Constructors
    d()->regExpConstructor = 0;
    d()->errorConstructor = 0;
    
    d()->evalErrorConstructor = 0;
    d()->rangeErrorConstructor = 0;
    d()->referenceErrorConstructor = 0;
    d()->syntaxErrorConstructor = 0;
    d()->typeErrorConstructor = 0;
    d()->URIErrorConstructor = 0;

    d()->evalFunction = 0;

    ExecState* exec = d()->globalExec.get();

    // Prototypes

    d()->functionPrototype = new (exec) FunctionPrototype(exec);
    d()->objectPrototype = new (exec) ObjectPrototype(exec, d()->functionPrototype);
    d()->functionPrototype->setPrototype(d()->objectPrototype);

    d()->arrayPrototype = new (exec) ArrayPrototype(exec, d()->objectPrototype);
    d()->stringPrototype = new (exec) StringPrototype(exec, d()->objectPrototype);
    d()->booleanPrototype = new (exec) BooleanPrototype(exec, d()->objectPrototype, d()->functionPrototype);
    d()->numberPrototype = new (exec) NumberPrototype(exec, d()->objectPrototype, d()->functionPrototype);
    d()->datePrototype = new (exec) DatePrototype(exec, d()->objectPrototype);
    d()->regExpPrototype = new (exec) RegExpPrototype(exec, d()->objectPrototype, d()->functionPrototype);
    d()->errorPrototype = new (exec) ErrorPrototype(exec, d()->objectPrototype, d()->functionPrototype);
    
    d()->evalErrorPrototype = new (exec) NativeErrorPrototype(exec, d()->errorPrototype, "EvalError", "EvalError");
    d()->rangeErrorPrototype = new (exec) NativeErrorPrototype(exec, d()->errorPrototype, "RangeError", "RangeError");
    d()->referenceErrorPrototype = new (exec) NativeErrorPrototype(exec, d()->errorPrototype, "ReferenceError", "ReferenceError");
    d()->syntaxErrorPrototype = new (exec) NativeErrorPrototype(exec, d()->errorPrototype, "SyntaxError", "SyntaxError");
    d()->typeErrorPrototype = new (exec) NativeErrorPrototype(exec, d()->errorPrototype, "TypeError", "TypeError");
    d()->URIErrorPrototype = new (exec) NativeErrorPrototype(exec, d()->errorPrototype, "URIError", "URIError");

    // Constructors

    JSValue* objectConstructor = new (exec) ObjectConstructor(exec, d()->objectPrototype, d()->functionPrototype);
    JSValue* functionConstructor = new (exec) FunctionConstructor(exec, d()->functionPrototype);
    JSValue* arrayConstructor = new (exec) ArrayConstructor(exec, d()->functionPrototype, d()->arrayPrototype);
    JSValue* stringConstructor = new (exec) StringConstructor(exec, d()->functionPrototype, d()->stringPrototype);
    JSValue* booleanConstructor = new (exec) BooleanConstructor(exec, d()->functionPrototype, d()->booleanPrototype);
    JSValue* numberConstructor = new (exec) NumberConstructor(exec, d()->functionPrototype, d()->numberPrototype);
    JSValue* dateConstructor = new (exec) DateConstructor(exec, d()->functionPrototype, d()->datePrototype);

    d()->regExpConstructor = new (exec) RegExpConstructor(exec, d()->functionPrototype, d()->regExpPrototype);

    d()->errorConstructor = new (exec) ErrorConstructor(exec, d()->functionPrototype, d()->errorPrototype);
    
    d()->evalErrorConstructor = new (exec) NativeErrorConstructor(exec, d()->functionPrototype, d()->evalErrorPrototype);
    d()->rangeErrorConstructor = new (exec) NativeErrorConstructor(exec, d()->functionPrototype, d()->rangeErrorPrototype);
    d()->referenceErrorConstructor = new (exec) NativeErrorConstructor(exec, d()->functionPrototype, d()->referenceErrorPrototype);
    d()->syntaxErrorConstructor = new (exec) NativeErrorConstructor(exec, d()->functionPrototype, d()->syntaxErrorPrototype);
    d()->typeErrorConstructor = new (exec) NativeErrorConstructor(exec, d()->functionPrototype, d()->typeErrorPrototype);
    d()->URIErrorConstructor = new (exec) NativeErrorConstructor(exec, d()->functionPrototype, d()->URIErrorPrototype);
    
    d()->functionPrototype->putDirect(exec->propertyNames().constructor, functionConstructor, DontEnum);

    d()->objectPrototype->putDirect(exec->propertyNames().constructor, objectConstructor, DontEnum);
    d()->functionPrototype->putDirect(exec->propertyNames().constructor, functionConstructor, DontEnum);
    d()->arrayPrototype->putDirect(exec->propertyNames().constructor, arrayConstructor, DontEnum);
    d()->booleanPrototype->putDirect(exec->propertyNames().constructor, booleanConstructor, DontEnum);
    d()->stringPrototype->putDirect(exec->propertyNames().constructor, stringConstructor, DontEnum);
    d()->numberPrototype->putDirect(exec->propertyNames().constructor, numberConstructor, DontEnum);
    d()->datePrototype->putDirect(exec->propertyNames().constructor, dateConstructor, DontEnum);
    d()->regExpPrototype->putDirect(exec->propertyNames().constructor, d()->regExpConstructor, DontEnum);
    d()->errorPrototype->putDirect(exec->propertyNames().constructor, d()->errorConstructor, DontEnum);
    d()->evalErrorPrototype->putDirect(exec->propertyNames().constructor, d()->evalErrorConstructor, DontEnum);
    d()->rangeErrorPrototype->putDirect(exec->propertyNames().constructor, d()->rangeErrorConstructor, DontEnum);
    d()->referenceErrorPrototype->putDirect(exec->propertyNames().constructor, d()->referenceErrorConstructor, DontEnum);
    d()->syntaxErrorPrototype->putDirect(exec->propertyNames().constructor, d()->syntaxErrorConstructor, DontEnum);
    d()->typeErrorPrototype->putDirect(exec->propertyNames().constructor, d()->typeErrorConstructor, DontEnum);
    d()->URIErrorPrototype->putDirect(exec->propertyNames().constructor, d()->URIErrorConstructor, DontEnum);

    // Set global constructors

    // FIXME: These properties could be handled by a static hash table.

    putDirect(Identifier(exec, "Object"), objectConstructor, DontEnum);
    putDirect(Identifier(exec, "Function"), functionConstructor, DontEnum);
    putDirect(Identifier(exec, "Array"), arrayConstructor, DontEnum);
    putDirect(Identifier(exec, "Boolean"), booleanConstructor, DontEnum);
    putDirect(Identifier(exec, "String"), stringConstructor, DontEnum);
    putDirect(Identifier(exec, "Number"), numberConstructor, DontEnum);
    putDirect(Identifier(exec, "Date"), dateConstructor, DontEnum);
    putDirect(Identifier(exec, "RegExp"), d()->regExpConstructor, DontEnum);
    putDirect(Identifier(exec, "Error"), d()->errorConstructor, DontEnum);
    putDirect(Identifier(exec, "EvalError"), d()->evalErrorConstructor);
    putDirect(Identifier(exec, "RangeError"), d()->rangeErrorConstructor);
    putDirect(Identifier(exec, "ReferenceError"), d()->referenceErrorConstructor);
    putDirect(Identifier(exec, "SyntaxError"), d()->syntaxErrorConstructor);
    putDirect(Identifier(exec, "TypeError"), d()->typeErrorConstructor);
    putDirect(Identifier(exec, "URIError"), d()->URIErrorConstructor);

    // Set global values.
    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(exec, "Math"), new (exec) MathObject(exec, d()->objectPrototype), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "NaN"), jsNaN(exec), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "Infinity"), jsNumber(exec, Inf), DontEnum | DontDelete),
        GlobalPropertyInfo(Identifier(exec, "undefined"), jsUndefined(), DontEnum | DontDelete)
    };

    addStaticGlobals(staticGlobals, sizeof(staticGlobals) / sizeof(GlobalPropertyInfo));

    // Set global functions.

    d()->evalFunction = new (exec) GlobalEvalFunction(exec, d()->functionPrototype, 1, exec->propertyNames().eval, globalFuncEval, this);
    putDirectFunction(d()->evalFunction, DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 2, Identifier(exec, "parseInt"), globalFuncParseInt), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "parseFloat"), globalFuncParseFloat), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "isNaN"), globalFuncIsNaN), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "isFinite"), globalFuncIsFinite), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "escape"), globalFuncEscape), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "unescape"), globalFuncUnescape), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "decodeURI"), globalFuncDecodeURI), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "decodeURIComponent"), globalFuncDecodeURIComponent), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "encodeURI"), globalFuncEncodeURI), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "encodeURIComponent"), globalFuncEncodeURIComponent), DontEnum);
#ifndef NDEBUG
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, Identifier(exec, "kjsprint"), globalFuncKJSPrint), DontEnum);
#endif

    // Set prototype, and also insert the object prototype at the end of the chain.

    setPrototype(prototype);
    lastInPrototypeChain(this)->setPrototype(d()->objectPrototype);
}

void JSGlobalObject::setTimeoutTime(unsigned timeoutTime)
{
    globalData()->machine->setTimeoutTime(timeoutTime);
}

void JSGlobalObject::startTimeoutCheck()
{
    globalData()->machine->startTimeoutCheck();
}

void JSGlobalObject::stopTimeoutCheck()
{
    globalData()->machine->stopTimeoutCheck();
}

void JSGlobalObject::mark()
{
    JSVariableObject::mark();
    
    HashSet<ProgramCodeBlock*>::const_iterator end = codeBlocks().end();
    for (HashSet<ProgramCodeBlock*>::const_iterator it = codeBlocks().begin(); it != end; ++it)
        (*it)->mark();

    markIfNeeded(d()->globalExec->exception());

    markIfNeeded(d()->regExpConstructor);
    markIfNeeded(d()->errorConstructor);
    markIfNeeded(d()->evalErrorConstructor);
    markIfNeeded(d()->rangeErrorConstructor);
    markIfNeeded(d()->referenceErrorConstructor);
    markIfNeeded(d()->syntaxErrorConstructor);
    markIfNeeded(d()->typeErrorConstructor);
    markIfNeeded(d()->URIErrorConstructor);
    
    markIfNeeded(d()->evalFunction);
    
    markIfNeeded(d()->objectPrototype);
    markIfNeeded(d()->functionPrototype);
    markIfNeeded(d()->arrayPrototype);
    markIfNeeded(d()->booleanPrototype);
    markIfNeeded(d()->stringPrototype);
    markIfNeeded(d()->numberPrototype);
    markIfNeeded(d()->datePrototype);
    markIfNeeded(d()->regExpPrototype);
    markIfNeeded(d()->errorPrototype);
    markIfNeeded(d()->evalErrorPrototype);
    markIfNeeded(d()->rangeErrorPrototype);
    markIfNeeded(d()->referenceErrorPrototype);
    markIfNeeded(d()->syntaxErrorPrototype);
    markIfNeeded(d()->typeErrorPrototype);
    markIfNeeded(d()->URIErrorPrototype);
}

JSGlobalObject* JSGlobalObject::toGlobalObject(ExecState*) const
{
    return const_cast<JSGlobalObject*>(this);
}

ExecState* JSGlobalObject::globalExec()
{
    return d()->globalExec.get();
}

bool JSGlobalObject::isDynamicScope() const
{
    return true;
}

void JSGlobalObject::copyGlobalsFrom(RegisterFile& registerFile)
{
    ASSERT(!d()->registerArray);

    int numGlobals = registerFile.numGlobals();
    if (!numGlobals) {
        ASSERT(!d()->registerOffset);
        d()->registerBase = 0;
        return;
    }
    copyRegisterArray(registerFile.lastGlobal(), numGlobals);
}

void JSGlobalObject::copyGlobalsTo(RegisterFile& registerFile)
{
    JSGlobalObject* lastGlobalObject = registerFile.globalObject();
    if (lastGlobalObject && lastGlobalObject != this)
        lastGlobalObject->copyGlobalsFrom(registerFile);

    registerFile.setGlobalObject(this);
    registerFile.setNumGlobals(symbolTable().size());

    if (d()->registerArray) {
        memcpy(*registerFile.basePointer() - d()->registerOffset, d()->registerArray, d()->registerOffset * sizeof(Register));
        setRegisterArray(0, 0);
    }

    d()->registerBase = registerFile.basePointer();
    d()->registerOffset = 0;
}

void* JSGlobalObject::operator new(size_t size)
{
#ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
    return JSGlobalData::threadInstance().heap->inlineAllocate(size);
#else
    return JSGlobalData::threadInstance().heap->allocate(size);
#endif
}

void* JSGlobalObject::operator new(size_t size, SharedTag)
{
#ifdef JAVASCRIPTCORE_BUILDING_ALL_IN_ONE_FILE
    return JSGlobalData::sharedInstance().heap->inlineAllocate(size);
#else
    return JSGlobalData::sharedInstance().heap->allocate(size);
#endif
}

} // namespace KJS
