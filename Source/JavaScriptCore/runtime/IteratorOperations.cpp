/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "config.h"
#include "IteratorOperations.h"

#include "Error.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"

using namespace WTF;

namespace JSC {

JSValue iteratorNext(ExecState* exec, JSValue iterator, JSValue value)
{
    JSValue nextFunction = iterator.get(exec, exec->vm().propertyNames->next);
    if (exec->hadException())
        return jsUndefined();

    CallData nextFunctionCallData;
    CallType nextFunctionCallType = getCallData(nextFunction, nextFunctionCallData);
    if (nextFunctionCallType == CallTypeNone)
        return throwTypeError(exec);

    MarkedArgumentBuffer nextFunctionArguments;
    if (!value.isEmpty())
        nextFunctionArguments.append(value);
    JSValue result = call(exec, nextFunction, nextFunctionCallType, nextFunctionCallData, iterator, nextFunctionArguments);
    if (exec->hadException())
        return jsUndefined();

    if (!result.isObject())
        return throwTypeError(exec, ASCIILiteral("Iterator result interface is not an object."));

    return result;
}

JSValue iteratorNext(ExecState* exec, JSValue iterator)
{
    return iteratorNext(exec, iterator, JSValue());
}

JSValue iteratorValue(ExecState* exec, JSValue iterResult)
{
    return iterResult.get(exec, exec->vm().propertyNames->value);
}

bool iteratorComplete(ExecState* exec, JSValue iterResult)
{
    JSValue done = iterResult.get(exec, exec->vm().propertyNames->done);
    return done.toBoolean(exec);
}

JSValue iteratorStep(ExecState* exec, JSValue iterator)
{
    JSValue result = iteratorNext(exec, iterator);
    if (exec->hadException())
        return jsUndefined();
    bool done = iteratorComplete(exec, result);
    if (exec->hadException())
        return jsUndefined();
    if (done)
        return jsBoolean(false);
    return result;
}

void iteratorClose(ExecState* exec, JSValue iterator)
{
    Exception* exception = nullptr;
    if (exec->hadException()) {
        exception = exec->exception();
        exec->clearException();
    }
    JSValue returnFunction = iterator.get(exec, exec->vm().propertyNames->returnKeyword);
    if (exec->hadException())
        return;

    if (returnFunction.isUndefined()) {
        if (exception)
            exec->vm().throwException(exec, exception);
        return;
    }

    CallData returnFunctionCallData;
    CallType returnFunctionCallType = getCallData(returnFunction, returnFunctionCallData);
    if (returnFunctionCallType == CallTypeNone) {
        if (exception)
            exec->vm().throwException(exec, exception);
        else
            throwTypeError(exec);
        return;
    }

    MarkedArgumentBuffer returnFunctionArguments;
    JSValue innerResult = call(exec, returnFunction, returnFunctionCallType, returnFunctionCallData, iterator, returnFunctionArguments);

    if (exception) {
        exec->vm().throwException(exec, exception);
        return;
    }

    if (exec->hadException())
        return;

    if (!innerResult.isObject()) {
        throwTypeError(exec, ASCIILiteral("Iterator result interface is not an object."));
        return;
    }
}

static const PropertyOffset donePropertyOffset = 0;
static const PropertyOffset valuePropertyOffset = 1;

Structure* createIteratorResultObjectStructure(VM& vm, JSGlobalObject& globalObject)
{
    Structure* iteratorResultStructure = vm.prototypeMap.emptyObjectStructureForPrototype(globalObject.objectPrototype(), JSFinalObject::defaultInlineCapacity());
    PropertyOffset offset;
    iteratorResultStructure = Structure::addPropertyTransition(vm, iteratorResultStructure, vm.propertyNames->done, 0, offset);
    RELEASE_ASSERT(offset == donePropertyOffset);
    iteratorResultStructure = Structure::addPropertyTransition(vm, iteratorResultStructure, vm.propertyNames->value, 0, offset);
    RELEASE_ASSERT(offset == valuePropertyOffset);
    return iteratorResultStructure;
}

JSObject* createIteratorResultObject(ExecState* exec, JSValue value, bool done)
{
    JSObject* resultObject = constructEmptyObject(exec, exec->lexicalGlobalObject()->iteratorResultObjectStructure());
    resultObject->putDirect(exec->vm(), donePropertyOffset, jsBoolean(done));
    resultObject->putDirect(exec->vm(), valuePropertyOffset, value);
    return resultObject;
}

} // namespace JSC
