/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "SavedBuiltins.h"
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
#include "string_object.h"

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
    
// Returns the current time in milliseconds
// It doesn't matter what "current time" is here, just as long as
// it's possible to measure the time difference correctly.
static inline unsigned getCurrentTime() {
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

JSGlobalObject* JSGlobalObject::s_head = 0;

JSGlobalObject::~JSGlobalObject()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    if (d->debugger)
        d->debugger->detach(this);

    d->next->d->prev = d->prev;
    d->prev->d->next = d->next;
    s_head = d->next;
    if (s_head == this)
        s_head = 0;
}

void JSGlobalObject::init()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    d.reset(new JSGlobalObjectData(this));

    if (s_head) {
        d->prev = s_head;
        d->next = s_head->d->next;
        s_head->d->next->d->prev = this;
        s_head->d->next = this;
    } else
        s_head = d->next = d->prev = this;

    d->compatMode = NativeMode;

    resetTimeoutCheck();
    d->timeoutTime = 0;
    d->timeoutCheckCount = 0;

    d->currentExec = 0;
    d->recursion = 0;
    d->debugger = 0;
    
    reset(prototype());
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
    // Clear before inititalizing, to avoid marking uninitialized (dangerous) or 
    // stale (wasteful) pointers during possible garbage collection while creating
    // new objects below.

    ExecState* exec = &d->globalExec;

    // Prototypes
    d->functionPrototype = 0;
    d->objectPrototype = 0;

    d->arrayPrototype = 0;
    d->stringPrototype = 0;
    d->booleanPrototype = 0;
    d->numberPrototype = 0;
    d->datePrototype = 0;
    d->regExpPrototype = 0;
    d->errorPrototype = 0;
    
    d->evalErrorPrototype = 0;
    d->rangeErrorPrototype = 0;
    d->referenceErrorPrototype = 0;
    d->syntaxErrorPrototype = 0;
    d->typeErrorPrototype = 0;
    d->URIErrorPrototype = 0;

    // Constructors
    d->objectConstructor = 0;
    d->functionConstructor = 0;
    d->arrayConstructor = 0;
    d->stringConstructor = 0;
    d->booleanConstructor = 0;
    d->numberConstructor = 0;
    d->dateConstructor = 0;
    d->regExpConstructor = 0;
    d->errorConstructor = 0;
    
    d->evalErrorConstructor = 0;
    d->rangeErrorConstructor = 0;
    d->referenceErrorConstructor = 0;
    d->syntaxErrorConstructor = 0;
    d->typeErrorConstructor = 0;
    d->URIErrorConstructor = 0;

    // Prototypes
    d->functionPrototype = new FunctionPrototype(exec);
    d->objectPrototype = new ObjectPrototype(exec, d->functionPrototype);
    d->functionPrototype->setPrototype(d->objectPrototype);

    d->arrayPrototype = new ArrayPrototype(exec, d->objectPrototype);
    d->stringPrototype = new StringPrototype(exec, d->objectPrototype);
    d->booleanPrototype = new BooleanPrototype(exec, d->objectPrototype, d->functionPrototype);
    d->numberPrototype = new NumberPrototype(exec, d->objectPrototype, d->functionPrototype);
    d->datePrototype = new DatePrototype(exec, d->objectPrototype);
    d->regExpPrototype = new RegExpPrototype(exec, d->objectPrototype, d->functionPrototype);;
    d->errorPrototype = new ErrorPrototype(exec, d->objectPrototype, d->functionPrototype);
    
    d->evalErrorPrototype = new NativeErrorPrototype(exec, d->errorPrototype, EvalError, "EvalError", "EvalError");
    d->rangeErrorPrototype = new NativeErrorPrototype(exec, d->errorPrototype, RangeError, "RangeError", "RangeError");
    d->referenceErrorPrototype = new NativeErrorPrototype(exec, d->errorPrototype, ReferenceError, "ReferenceError", "ReferenceError");
    d->syntaxErrorPrototype = new NativeErrorPrototype(exec, d->errorPrototype, SyntaxError, "SyntaxError", "SyntaxError");
    d->typeErrorPrototype = new NativeErrorPrototype(exec, d->errorPrototype, TypeError, "TypeError", "TypeError");
    d->URIErrorPrototype = new NativeErrorPrototype(exec, d->errorPrototype, URIError, "URIError", "URIError");

    // Constructors
    d->objectConstructor = new ObjectObjectImp(exec, d->objectPrototype, d->functionPrototype);
    d->functionConstructor = new FunctionObjectImp(exec, d->functionPrototype);
    d->arrayConstructor = new ArrayObjectImp(exec, d->functionPrototype, d->arrayPrototype);
    d->stringConstructor = new StringObjectImp(exec, d->functionPrototype, d->stringPrototype);
    d->booleanConstructor = new BooleanObjectImp(exec, d->functionPrototype, d->booleanPrototype);
    d->numberConstructor = new NumberObjectImp(exec, d->functionPrototype, d->numberPrototype);
    d->dateConstructor = new DateObjectImp(exec, d->functionPrototype, d->datePrototype);
    d->regExpConstructor = new RegExpObjectImp(exec, d->functionPrototype, d->regExpPrototype);
    d->errorConstructor = new ErrorObjectImp(exec, d->functionPrototype, d->errorPrototype);
    
    d->evalErrorConstructor = new NativeErrorImp(exec, d->functionPrototype, d->evalErrorPrototype);
    d->rangeErrorConstructor = new NativeErrorImp(exec, d->functionPrototype, d->rangeErrorPrototype);
    d->referenceErrorConstructor = new NativeErrorImp(exec, d->functionPrototype, d->referenceErrorPrototype);
    d->syntaxErrorConstructor = new NativeErrorImp(exec, d->functionPrototype, d->syntaxErrorPrototype);
    d->typeErrorConstructor = new NativeErrorImp(exec, d->functionPrototype, d->typeErrorPrototype);
    d->URIErrorConstructor = new NativeErrorImp(exec, d->functionPrototype, d->URIErrorPrototype);
    
    d->functionPrototype->put(exec, exec->propertyNames().constructor, d->functionConstructor, DontEnum);

    d->objectPrototype->put(exec, exec->propertyNames().constructor, d->objectConstructor, DontEnum | DontDelete | ReadOnly);
    d->functionPrototype->put(exec, exec->propertyNames().constructor, d->functionConstructor, DontEnum | DontDelete | ReadOnly);
    d->arrayPrototype->put(exec, exec->propertyNames().constructor, d->arrayConstructor, DontEnum | DontDelete | ReadOnly);
    d->booleanPrototype->put(exec, exec->propertyNames().constructor, d->booleanConstructor, DontEnum | DontDelete | ReadOnly);
    d->stringPrototype->put(exec, exec->propertyNames().constructor, d->stringConstructor, DontEnum | DontDelete | ReadOnly);
    d->numberPrototype->put(exec, exec->propertyNames().constructor, d->numberConstructor, DontEnum | DontDelete | ReadOnly);
    d->datePrototype->put(exec, exec->propertyNames().constructor, d->dateConstructor, DontEnum | DontDelete | ReadOnly);
    d->regExpPrototype->put(exec, exec->propertyNames().constructor, d->regExpConstructor, DontEnum | DontDelete | ReadOnly);
    d->errorPrototype->put(exec, exec->propertyNames().constructor, d->errorConstructor, DontEnum | DontDelete | ReadOnly);
    d->evalErrorPrototype->put(exec, exec->propertyNames().constructor, d->evalErrorConstructor, DontEnum | DontDelete | ReadOnly);
    d->rangeErrorPrototype->put(exec, exec->propertyNames().constructor, d->rangeErrorConstructor, DontEnum | DontDelete | ReadOnly);
    d->referenceErrorPrototype->put(exec, exec->propertyNames().constructor, d->referenceErrorConstructor, DontEnum | DontDelete | ReadOnly);
    d->syntaxErrorPrototype->put(exec, exec->propertyNames().constructor, d->syntaxErrorConstructor, DontEnum | DontDelete | ReadOnly);
    d->typeErrorPrototype->put(exec, exec->propertyNames().constructor, d->typeErrorConstructor, DontEnum | DontDelete | ReadOnly);
    d->URIErrorPrototype->put(exec, exec->propertyNames().constructor, d->URIErrorConstructor, DontEnum | DontDelete | ReadOnly);

    // Set global constructors

    // FIXME: kjs_window.cpp checks Internal/DontEnum as a performance hack, to
    // see that these values can be put directly without a check for override
    // properties.

    // FIXME: These properties should be handled by a static hash table.

    putDirect("Object", d->objectConstructor, DontEnum);
    putDirect("Function", d->functionConstructor, DontEnum);
    putDirect("Array", d->arrayConstructor, DontEnum);
    putDirect("Boolean", d->booleanConstructor, DontEnum);
    putDirect("String", d->stringConstructor, DontEnum);
    putDirect("Number", d->numberConstructor, DontEnum);
    putDirect("Date", d->dateConstructor, DontEnum);
    putDirect("RegExp", d->regExpConstructor, DontEnum);
    putDirect("Error", d->errorConstructor, DontEnum);
    putDirect("EvalError", d->evalErrorConstructor, Internal);
    putDirect("RangeError", d->rangeErrorConstructor, Internal);
    putDirect("ReferenceError", d->referenceErrorConstructor, Internal);
    putDirect("SyntaxError", d->syntaxErrorConstructor, Internal);
    putDirect("TypeError", d->typeErrorConstructor, Internal);
    putDirect("URIError", d->URIErrorConstructor, Internal);

    // Set global values.

    putDirect("Math", new MathObjectImp(exec, d->objectPrototype), DontEnum);

    putDirect("NaN", jsNaN(), DontEnum | DontDelete);
    putDirect("Infinity", jsNumber(Inf), DontEnum | DontDelete);
    putDirect("undefined", jsUndefined(), DontEnum | DontDelete);

    // Set global functions.

    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::Eval, 1, "eval"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::ParseInt, 2, "parseInt"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::ParseFloat, 1, "parseFloat"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::IsNaN, 1, "isNaN"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::IsFinite, 1, "isFinite"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::Escape, 1, "escape"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::UnEscape, 1, "unescape"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::DecodeURI, 1, "decodeURI"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::DecodeURIComponent, 1, "decodeURIComponent"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::EncodeURI, 1, "encodeURI"), DontEnum);
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::EncodeURIComponent, 1, "encodeURIComponent"), DontEnum);
#ifndef NDEBUG
    putDirectFunction(new GlobalFuncImp(exec, d->functionPrototype, GlobalFuncImp::KJSPrint, 1, "kjsprint"), DontEnum);
#endif

    // Set prototype, and also insert the object prototype at the end of the chain.

    setPrototype(prototype);
    lastInPrototypeChain(this)->setPrototype(d->objectPrototype);
}

void JSGlobalObject::startTimeoutCheck()
{
    if (!d->timeoutCheckCount)
        resetTimeoutCheck();
    
    ++d->timeoutCheckCount;
}

void JSGlobalObject::stopTimeoutCheck()
{
    --d->timeoutCheckCount;
}

void JSGlobalObject::resetTimeoutCheck()
{
    d->tickCount = 0;
    d->ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    d->timeAtLastCheckTimeout = 0;
    d->timeExecuting = 0;
}

bool JSGlobalObject::checkTimeout()
{    
    d->tickCount = 0;
    
    unsigned currentTime = getCurrentTime();

    if (!d->timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        d->timeAtLastCheckTimeout = currentTime;
        return false;
    }

    unsigned timeDiff = currentTime - d->timeAtLastCheckTimeout;

    if (timeDiff == 0)
        timeDiff = 1;
    
    d->timeExecuting += timeDiff;
    d->timeAtLastCheckTimeout = currentTime;
    
    // Adjust the tick threshold so we get the next checkTimeout call in the interval specified in 
    // preferredScriptCheckTimeInterval
    d->ticksUntilNextTimeoutCheck = (unsigned)((float)preferredScriptCheckTimeInterval / timeDiff) * d->ticksUntilNextTimeoutCheck;

    // If the new threshold is 0 reset it to the default threshold. This can happen if the timeDiff is higher than the
    // preferred script check time interval.
    if (d->ticksUntilNextTimeoutCheck == 0)
        d->ticksUntilNextTimeoutCheck = initialTickCountThreshold;

    if (d->timeoutTime && d->timeExecuting > d->timeoutTime) {
        if (shouldInterruptScript())
            return true;
        
        resetTimeoutCheck();
    }
    
    return false;
}

void JSGlobalObject::saveBuiltins(SavedBuiltins& builtins) const
{
    if (!builtins._internal)
        builtins._internal = new SavedBuiltinsInternal;

    builtins._internal->objectConstructor = d->objectConstructor;
    builtins._internal->functionConstructor = d->functionConstructor;
    builtins._internal->arrayConstructor = d->arrayConstructor;
    builtins._internal->booleanConstructor = d->booleanConstructor;
    builtins._internal->stringConstructor = d->stringConstructor;
    builtins._internal->numberConstructor = d->numberConstructor;
    builtins._internal->dateConstructor = d->dateConstructor;
    builtins._internal->regExpConstructor = d->regExpConstructor;
    builtins._internal->errorConstructor = d->errorConstructor;
    builtins._internal->evalErrorConstructor = d->evalErrorConstructor;
    builtins._internal->rangeErrorConstructor = d->rangeErrorConstructor;
    builtins._internal->referenceErrorConstructor = d->referenceErrorConstructor;
    builtins._internal->syntaxErrorConstructor = d->syntaxErrorConstructor;
    builtins._internal->typeErrorConstructor = d->typeErrorConstructor;
    builtins._internal->URIErrorConstructor = d->URIErrorConstructor;
    
    builtins._internal->objectPrototype = d->objectPrototype;
    builtins._internal->functionPrototype = d->functionPrototype;
    builtins._internal->arrayPrototype = d->arrayPrototype;
    builtins._internal->booleanPrototype = d->booleanPrototype;
    builtins._internal->stringPrototype = d->stringPrototype;
    builtins._internal->numberPrototype = d->numberPrototype;
    builtins._internal->datePrototype = d->datePrototype;
    builtins._internal->regExpPrototype = d->regExpPrototype;
    builtins._internal->errorPrototype = d->errorPrototype;
    builtins._internal->evalErrorPrototype = d->evalErrorPrototype;
    builtins._internal->rangeErrorPrototype = d->rangeErrorPrototype;
    builtins._internal->referenceErrorPrototype = d->referenceErrorPrototype;
    builtins._internal->syntaxErrorPrototype = d->syntaxErrorPrototype;
    builtins._internal->typeErrorPrototype = d->typeErrorPrototype;
    builtins._internal->URIErrorPrototype = d->URIErrorPrototype;
}

void JSGlobalObject::restoreBuiltins(const SavedBuiltins& builtins)
{
    if (!builtins._internal)
        return;

    d->objectConstructor = builtins._internal->objectConstructor;
    d->functionConstructor = builtins._internal->functionConstructor;
    d->arrayConstructor = builtins._internal->arrayConstructor;
    d->booleanConstructor = builtins._internal->booleanConstructor;
    d->stringConstructor = builtins._internal->stringConstructor;
    d->numberConstructor = builtins._internal->numberConstructor;
    d->dateConstructor = builtins._internal->dateConstructor;
    d->regExpConstructor = builtins._internal->regExpConstructor;
    d->errorConstructor = builtins._internal->errorConstructor;
    d->evalErrorConstructor = builtins._internal->evalErrorConstructor;
    d->rangeErrorConstructor = builtins._internal->rangeErrorConstructor;
    d->referenceErrorConstructor = builtins._internal->referenceErrorConstructor;
    d->syntaxErrorConstructor = builtins._internal->syntaxErrorConstructor;
    d->typeErrorConstructor = builtins._internal->typeErrorConstructor;
    d->URIErrorConstructor = builtins._internal->URIErrorConstructor;

    d->objectPrototype = builtins._internal->objectPrototype;
    d->functionPrototype = builtins._internal->functionPrototype;
    d->arrayPrototype = builtins._internal->arrayPrototype;
    d->booleanPrototype = builtins._internal->booleanPrototype;
    d->stringPrototype = builtins._internal->stringPrototype;
    d->numberPrototype = builtins._internal->numberPrototype;
    d->datePrototype = builtins._internal->datePrototype;
    d->regExpPrototype = builtins._internal->regExpPrototype;
    d->errorPrototype = builtins._internal->errorPrototype;
    d->evalErrorPrototype = builtins._internal->evalErrorPrototype;
    d->rangeErrorPrototype = builtins._internal->rangeErrorPrototype;
    d->referenceErrorPrototype = builtins._internal->referenceErrorPrototype;
    d->syntaxErrorPrototype = builtins._internal->syntaxErrorPrototype;
    d->typeErrorPrototype = builtins._internal->typeErrorPrototype;
    d->URIErrorPrototype = builtins._internal->URIErrorPrototype;
}

void JSGlobalObject::mark()
{
    JSObject::mark();

    if (d->currentExec)
        d->currentExec->mark();

    markIfNeeded(d->globalExec.exception());

    markIfNeeded(d->objectConstructor);
    markIfNeeded(d->functionConstructor);
    markIfNeeded(d->arrayConstructor);
    markIfNeeded(d->booleanConstructor);
    markIfNeeded(d->stringConstructor);
    markIfNeeded(d->numberConstructor);
    markIfNeeded(d->dateConstructor);
    markIfNeeded(d->regExpConstructor);
    markIfNeeded(d->errorConstructor);
    markIfNeeded(d->evalErrorConstructor);
    markIfNeeded(d->rangeErrorConstructor);
    markIfNeeded(d->referenceErrorConstructor);
    markIfNeeded(d->syntaxErrorConstructor);
    markIfNeeded(d->typeErrorConstructor);
    markIfNeeded(d->URIErrorConstructor);
    
    markIfNeeded(d->objectPrototype);
    markIfNeeded(d->functionPrototype);
    markIfNeeded(d->arrayPrototype);
    markIfNeeded(d->booleanPrototype);
    markIfNeeded(d->stringPrototype);
    markIfNeeded(d->numberPrototype);
    markIfNeeded(d->datePrototype);
    markIfNeeded(d->regExpPrototype);
    markIfNeeded(d->errorPrototype);
    markIfNeeded(d->evalErrorPrototype);
    markIfNeeded(d->rangeErrorPrototype);
    markIfNeeded(d->referenceErrorPrototype);
    markIfNeeded(d->syntaxErrorPrototype);
    markIfNeeded(d->typeErrorPrototype);
    markIfNeeded(d->URIErrorPrototype);
}

ExecState* JSGlobalObject::globalExec()
{
    return &d->globalExec;
}

} // namespace KJS
