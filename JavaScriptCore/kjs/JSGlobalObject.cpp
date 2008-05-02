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

#include "Activation.h"
#include "array_object.h"
#include "bool_object.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "math_object.h"
#include "number_object.h"
#include "object_object.h"
#include "regexp_object.h"
#include "scope_chain_mark.h"
#include "string_object.h"

#if USE(MULTIPLE_THREADS)
#include <wtf/ThreadSpecific.h>
using namespace WTF;
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

namespace KJS {

extern const HashTable arrayTable;
extern const HashTable dateTable;
extern const HashTable mathTable;
extern const HashTable numberTable;
extern const HashTable RegExpImpTable;
extern const HashTable RegExpObjectImpTable;
extern const HashTable stringTable;

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

static inline void markIfNeeded(JSValue* v)
{
    if (v && !v->marked())
        v->mark();
}
    
// Returns the current time in milliseconds
// It doesn't matter what "current time" is here, just as long as
// it's possible to measure the time difference correctly.
static inline unsigned getCurrentTime()
{
#if HAVE(SYS_TIME_H)
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    return t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    return timeGetTime();
#else
#error Platform does not have getCurrentTime function
#endif
}

void JSGlobalObject::deleteActivationStack()
{
    ActivationStackNode* prevNode = 0;
    for (ActivationStackNode* currentNode = d()->activations; currentNode; currentNode = prevNode) {
        prevNode = currentNode->prev;
        delete currentNode;
    }
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
    
    deleteActivationStack();
    
    delete d();
}

struct ThreadClassInfoHashTables {
    ThreadClassInfoHashTables()
        : arrayTable(KJS::arrayTable)
        , dateTable(KJS::dateTable)
        , mathTable(KJS::mathTable)
        , numberTable(KJS::numberTable)
        , RegExpImpTable(KJS::RegExpImpTable)
        , RegExpObjectImpTable(KJS::RegExpObjectImpTable)
        , stringTable(KJS::stringTable)
    {
    }

    ~ThreadClassInfoHashTables()
    {
#if USE(MULTIPLE_THREADS)
        delete[] arrayTable.table;
        delete[] dateTable.table;
        delete[] mathTable.table;
        delete[] numberTable.table;
        delete[] RegExpImpTable.table;
        delete[] RegExpObjectImpTable.table;
        delete[] stringTable.table;
#endif
    }

#if USE(MULTIPLE_THREADS)
    HashTable arrayTable;
    HashTable dateTable;
    HashTable mathTable;
    HashTable numberTable;
    HashTable RegExpImpTable;
    HashTable RegExpObjectImpTable;
    HashTable stringTable;
#else
    const HashTable& arrayTable;
    const HashTable& dateTable;
    const HashTable& mathTable;
    const HashTable& numberTable;
    const HashTable& RegExpImpTable;
    const HashTable& RegExpObjectImpTable;
    const HashTable& stringTable;
#endif
};

ThreadClassInfoHashTables* JSGlobalObject::threadClassInfoHashTables()
{
#if USE(MULTIPLE_THREADS)
    static ThreadSpecific<ThreadClassInfoHashTables> sharedInstance;
    return sharedInstance;
#else
    static ThreadClassInfoHashTables sharedInstance;
    return &sharedInstance;
#endif
}

JSGlobalObject*& JSGlobalObject::head()
{
#if USE(MULTIPLE_THREADS)
    static ThreadSpecific<JSGlobalObject*> sharedInstance;
    return *sharedInstance;
#else
    static JSGlobalObject* sharedInstance;
    return sharedInstance;
#endif
}

void JSGlobalObject::init(JSObject* thisValue)
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (JSGlobalObject*& headObject = head()) {
        d()->prev = headObject;
        d()->next = headObject->d()->next;
        headObject->d()->next->d()->prev = this;
        headObject->d()->next = this;
    } else
        headObject = d()->next = d()->prev = this;

    resetTimeoutCheck();
    d()->timeoutTime = 0;
    d()->timeoutCheckCount = 0;

    d()->recursion = 0;
    d()->debugger = 0;
    
    ActivationStackNode* newStackNode = new ActivationStackNode;
    newStackNode->prev = 0;    
    d()->activations = newStackNode;
    d()->activationCount = 0;

    d()->perThreadData.arrayTable = &threadClassInfoHashTables()->arrayTable;
    d()->perThreadData.dateTable = &threadClassInfoHashTables()->dateTable;
    d()->perThreadData.mathTable = &threadClassInfoHashTables()->mathTable;
    d()->perThreadData.numberTable = &threadClassInfoHashTables()->numberTable;
    d()->perThreadData.RegExpImpTable = &threadClassInfoHashTables()->RegExpImpTable;
    d()->perThreadData.RegExpObjectImpTable = &threadClassInfoHashTables()->RegExpObjectImpTable;
    d()->perThreadData.stringTable = &threadClassInfoHashTables()->stringTable;
    d()->perThreadData.propertyNames = CommonIdentifiers::shared();
    d()->perThreadData.heap = Heap::threadHeap();
    d()->perThreadData.functionCallDepth = 0;

    d()->globalExec.set(new GlobalExecState(this, thisValue));

    d()->pageGroupIdentifier = 0;

    reset(prototype());
}

bool JSGlobalObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (symbolTableGet(propertyName, slot))
        return true;
    return JSVariableObject::getOwnPropertySlot(exec, propertyName, slot);
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
    localStorage().clear();
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
    d()->objectConstructor = 0;
    d()->functionConstructor = 0;
    d()->arrayConstructor = 0;
    d()->stringConstructor = 0;
    d()->booleanConstructor = 0;
    d()->numberConstructor = 0;
    d()->dateConstructor = 0;
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
    d()->objectConstructor = new (exec) ObjectObjectImp(exec, d()->objectPrototype, d()->functionPrototype);
    d()->functionConstructor = new (exec) FunctionObjectImp(exec, d()->functionPrototype);
    d()->arrayConstructor = new (exec) ArrayObjectImp(exec, d()->functionPrototype, d()->arrayPrototype);
    d()->stringConstructor = new (exec) StringObjectImp(exec, d()->functionPrototype, d()->stringPrototype);
    d()->booleanConstructor = new (exec) BooleanObjectImp(exec, d()->functionPrototype, d()->booleanPrototype);
    d()->numberConstructor = new (exec) NumberObjectImp(exec, d()->functionPrototype, d()->numberPrototype);
    d()->dateConstructor = new (exec) DateObjectImp(exec, d()->functionPrototype, d()->datePrototype);
    d()->regExpConstructor = new (exec) RegExpObjectImp(exec, d()->functionPrototype, d()->regExpPrototype);
    d()->errorConstructor = new (exec) ErrorObjectImp(exec, d()->functionPrototype, d()->errorPrototype);
    
    d()->evalErrorConstructor = new (exec) NativeErrorImp(exec, d()->functionPrototype, d()->evalErrorPrototype);
    d()->rangeErrorConstructor = new (exec) NativeErrorImp(exec, d()->functionPrototype, d()->rangeErrorPrototype);
    d()->referenceErrorConstructor = new (exec) NativeErrorImp(exec, d()->functionPrototype, d()->referenceErrorPrototype);
    d()->syntaxErrorConstructor = new (exec) NativeErrorImp(exec, d()->functionPrototype, d()->syntaxErrorPrototype);
    d()->typeErrorConstructor = new (exec) NativeErrorImp(exec, d()->functionPrototype, d()->typeErrorPrototype);
    d()->URIErrorConstructor = new (exec) NativeErrorImp(exec, d()->functionPrototype, d()->URIErrorPrototype);
    
    d()->functionPrototype->putDirect(exec->propertyNames().constructor, d()->functionConstructor, DontEnum);

    d()->objectPrototype->putDirect(exec->propertyNames().constructor, d()->objectConstructor, DontEnum);
    d()->functionPrototype->putDirect(exec->propertyNames().constructor, d()->functionConstructor, DontEnum);
    d()->arrayPrototype->putDirect(exec->propertyNames().constructor, d()->arrayConstructor, DontEnum);
    d()->booleanPrototype->putDirect(exec->propertyNames().constructor, d()->booleanConstructor, DontEnum);
    d()->stringPrototype->putDirect(exec->propertyNames().constructor, d()->stringConstructor, DontEnum);
    d()->numberPrototype->putDirect(exec->propertyNames().constructor, d()->numberConstructor, DontEnum);
    d()->datePrototype->putDirect(exec->propertyNames().constructor, d()->dateConstructor, DontEnum);
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

    putDirect("Object", d()->objectConstructor, DontEnum);
    putDirect("Function", d()->functionConstructor, DontEnum);
    putDirect("Array", d()->arrayConstructor, DontEnum);
    putDirect("Boolean", d()->booleanConstructor, DontEnum);
    putDirect("String", d()->stringConstructor, DontEnum);
    putDirect("Number", d()->numberConstructor, DontEnum);
    putDirect("Date", d()->dateConstructor, DontEnum);
    putDirect("RegExp", d()->regExpConstructor, DontEnum);
    putDirect("Error", d()->errorConstructor, DontEnum);
    putDirect("EvalError", d()->evalErrorConstructor);
    putDirect("RangeError", d()->rangeErrorConstructor);
    putDirect("ReferenceError", d()->referenceErrorConstructor);
    putDirect("SyntaxError", d()->syntaxErrorConstructor);
    putDirect("TypeError", d()->typeErrorConstructor);
    putDirect("URIError", d()->URIErrorConstructor);

    // Set global values.
    Identifier mathIdent = "Math";
    JSValue* mathObject = new (exec) MathObjectImp(exec, d()->objectPrototype);
    symbolTableInsert(mathIdent, mathObject, DontEnum | DontDelete);
    
    Identifier nanIdent = "NaN";
    JSValue* nanValue = jsNaN(exec);
    symbolTableInsert(nanIdent, nanValue, DontEnum | DontDelete);
    
    Identifier infinityIdent = "Infinity";
    JSValue* infinityValue = jsNumber(exec, Inf);
    symbolTableInsert(infinityIdent, infinityValue, DontEnum | DontDelete);
    
    Identifier undefinedIdent = "undefined";
    JSValue* undefinedValue = jsUndefined();
    symbolTableInsert(undefinedIdent, undefinedValue, DontEnum | DontDelete);

    // Set global functions.

    d()->evalFunction = new (exec) PrototypeReflexiveFunction(exec, d()->functionPrototype, 1, exec->propertyNames().eval, globalFuncEval, this);
    putDirectFunction(d()->evalFunction, DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 2, "parseInt", globalFuncParseInt), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "parseFloat", globalFuncParseFloat), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "isNaN", globalFuncIsNaN), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "isFinite", globalFuncIsFinite), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "escape", globalFuncEscape), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "unescape", globalFuncUnescape), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "decodeURI", globalFuncDecodeURI), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "decodeURIComponent", globalFuncDecodeURIComponent), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "encodeURI", globalFuncEncodeURI), DontEnum);
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "encodeURIComponent", globalFuncEncodeURIComponent), DontEnum);
#ifndef NDEBUG
    putDirectFunction(new (exec) PrototypeFunction(exec, d()->functionPrototype, 1, "kjsprint", globalFuncKJSPrint), DontEnum);
#endif

    // Set prototype, and also insert the object prototype at the end of the chain.

    setPrototype(prototype);
    lastInPrototypeChain(this)->setPrototype(d()->objectPrototype);
}

void JSGlobalObject::startTimeoutCheck()
{
    if (!d()->timeoutCheckCount)
        resetTimeoutCheck();
    
    ++d()->timeoutCheckCount;
}

void JSGlobalObject::stopTimeoutCheck()
{
    --d()->timeoutCheckCount;
}

void JSGlobalObject::resetTimeoutCheck()
{
    d()->tickCount = 0;
    d()->ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    d()->timeAtLastCheckTimeout = 0;
    d()->timeExecuting = 0;
}

bool JSGlobalObject::checkTimeout()
{    
    d()->tickCount = 0;
    
    unsigned currentTime = getCurrentTime();

    if (!d()->timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        d()->timeAtLastCheckTimeout = currentTime;
        return false;
    }

    unsigned timeDiff = currentTime - d()->timeAtLastCheckTimeout;

    if (timeDiff == 0)
        timeDiff = 1;
    
    d()->timeExecuting += timeDiff;
    d()->timeAtLastCheckTimeout = currentTime;
    
    // Adjust the tick threshold so we get the next checkTimeout call in the interval specified in 
    // preferredScriptCheckTimeInterval
    d()->ticksUntilNextTimeoutCheck = (unsigned)((float)preferredScriptCheckTimeInterval / timeDiff) * d()->ticksUntilNextTimeoutCheck;

    // If the new threshold is 0 reset it to the default threshold. This can happen if the timeDiff is higher than the
    // preferred script check time interval.
    if (d()->ticksUntilNextTimeoutCheck == 0)
        d()->ticksUntilNextTimeoutCheck = initialTickCountThreshold;

    if (d()->timeoutTime && d()->timeExecuting > d()->timeoutTime) {
        if (shouldInterruptScript())
            return true;
        
        resetTimeoutCheck();
    }
    
    return false;
}

void JSGlobalObject::mark()
{
    JSVariableObject::mark();

    ExecStateStack::const_iterator end = d()->activeExecStates.end();
    for (ExecStateStack::const_iterator it = d()->activeExecStates.begin(); it != end; ++it)
        (*it)->m_scopeChain.mark();

    markIfNeeded(d()->globalExec->exception());

    markIfNeeded(d()->objectConstructor);
    markIfNeeded(d()->functionConstructor);
    markIfNeeded(d()->arrayConstructor);
    markIfNeeded(d()->booleanConstructor);
    markIfNeeded(d()->stringConstructor);
    markIfNeeded(d()->numberConstructor);
    markIfNeeded(d()->dateConstructor);
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

void JSGlobalObject::tearOffActivation(ExecState* exec, bool leaveRelic)
{
    ActivationImp* oldActivation = exec->activationObject();
    if (!oldActivation || !oldActivation->isOnStack())
        return;

    ASSERT(exec->codeType() == FunctionCode);
    ActivationImp* newActivation = new (exec) ActivationImp(*oldActivation->d(), leaveRelic);
    
    if (!leaveRelic) {
        checkActivationCount();
        d()->activationCount--;
    }
    
    oldActivation->d()->localStorage.shrink(0);
    
    exec->setActivationObject(newActivation);
    exec->setVariableObject(newActivation);
    exec->setLocalStorage(&newActivation->localStorage());
    exec->replaceScopeChainTop(newActivation);
}

bool JSGlobalObject::isDynamicScope() const
{
    return true;
}

void* JSGlobalObject::operator new(size_t size)
{
    return Heap::threadHeap()->allocate(size);
}

} // namespace KJS
