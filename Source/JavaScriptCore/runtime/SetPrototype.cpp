/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SetPrototype.h"

#include "CachedCall.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "JSCJSValueInlines.h"
#include "JSFunctionInlines.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "MapData.h"

namespace JSC {

const ClassInfo SetPrototype::s_info = { "Set", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(SetPrototype) };

static EncodedJSValue JSC_HOST_CALL setProtoFuncAdd(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncClear(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncDelete(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncForEach(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncHas(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncKeys(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncValues(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncEntries(ExecState*);


static EncodedJSValue JSC_HOST_CALL setProtoFuncSize(ExecState*);

void SetPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.prototypeMap.addPrototype(this);

    JSC_NATIVE_FUNCTION(vm.propertyNames->add, setProtoFuncAdd, DontEnum, 1);
    JSC_NATIVE_FUNCTION(vm.propertyNames->clear, setProtoFuncClear, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->deleteKeyword, setProtoFuncDelete, DontEnum, 1);
    JSC_NATIVE_FUNCTION(vm.propertyNames->forEach, setProtoFuncForEach, DontEnum, 1);
    JSC_NATIVE_FUNCTION(vm.propertyNames->has, setProtoFuncHas, DontEnum, 1);
    JSC_NATIVE_FUNCTION(vm.propertyNames->keys, setProtoFuncKeys, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->values, setProtoFuncValues, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->entries, setProtoFuncEntries, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->iteratorPrivateName, setProtoFuncKeys, DontEnum, 0);

    GetterSetter* accessor = GetterSetter::create(vm);
    JSFunction* function = JSFunction::create(vm, globalObject, 0, vm.propertyNames->size.string(), setProtoFuncSize);
    accessor->setGetter(vm, function);
    putDirectNonIndexAccessor(vm, vm.propertyNames->size, accessor, DontEnum | Accessor);
}

ALWAYS_INLINE static MapData* getMapData(CallFrame* callFrame, JSValue thisValue)
{
    if (!thisValue.isObject()) {
        throwVMError(callFrame, createNotAnObjectError(callFrame, thisValue));
        return 0;
    }
    JSSet* set = jsDynamicCast<JSSet*>(thisValue);
    if (!set) {
        throwTypeError(callFrame, ASCIILiteral("Set operation called on non-Set object"));
        return 0;
    }
    return set->mapData();
}

EncodedJSValue JSC_HOST_CALL setProtoFuncAdd(CallFrame* callFrame)
{
    MapData* data = getMapData(callFrame, callFrame->thisValue());
    if (!data)
        return JSValue::encode(jsUndefined());
    data->set(callFrame, callFrame->argument(0), callFrame->argument(0));
    return JSValue::encode(callFrame->thisValue());
}

EncodedJSValue JSC_HOST_CALL setProtoFuncClear(CallFrame* callFrame)
{
    MapData* data = getMapData(callFrame, callFrame->thisValue());
    if (!data)
        return JSValue::encode(jsUndefined());
    data->clear();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL setProtoFuncDelete(CallFrame* callFrame)
{
    MapData* data = getMapData(callFrame, callFrame->thisValue());
    if (!data)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(data->remove(callFrame, callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncForEach(CallFrame* callFrame)
{
    MapData* data = getMapData(callFrame, callFrame->thisValue());
    if (!data)
        return JSValue::encode(jsUndefined());
    JSValue callBack = callFrame->argument(0);
    CallData callData;
    CallType callType = getCallData(callBack, callData);
    if (callType == CallTypeNone)
        return JSValue::encode(throwTypeError(callFrame, WTF::ASCIILiteral("Set.prototype.forEach called without callback")));
    JSValue thisValue = callFrame->argument(1);
    VM* vm = &callFrame->vm();
    if (callType == CallTypeJS) {
        JSFunction* function = jsCast<JSFunction*>(callBack);
        CachedCall cachedCall(callFrame, function, 1);
        for (auto ptr = data->begin(), end = data->end(); ptr != end && !vm->exception(); ++ptr) {
            cachedCall.setThis(thisValue);
            cachedCall.setArgument(0, ptr.key());
            cachedCall.call();
        }
    } else {
        for (auto ptr = data->begin(), end = data->end(); ptr != end && !vm->exception(); ++ptr) {
            MarkedArgumentBuffer args;
            args.append(ptr.key());
            JSC::call(callFrame, callBack, callType, callData, thisValue, args);
        }
    }
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL setProtoFuncHas(CallFrame* callFrame)
{
    MapData* data = getMapData(callFrame, callFrame->thisValue());
    if (!data)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(data->contains(callFrame, callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncSize(CallFrame* callFrame)
{
    MapData* data = getMapData(callFrame, callFrame->thisValue());
    if (!data)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(data->size(callFrame)));
}
    
EncodedJSValue JSC_HOST_CALL setProtoFuncValues(CallFrame* callFrame)
{
    JSSet* thisObj = jsDynamicCast<JSSet*>(callFrame->thisValue());
    if (!thisObj)
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Cannot create a Map value iterator for a non-Map object.")));
    return JSValue::encode(JSSetIterator::create(callFrame->vm(), callFrame->callee()->globalObject()->setIteratorStructure(), thisObj, SetIterateValue));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncEntries(CallFrame* callFrame)
{
    JSSet* thisObj = jsDynamicCast<JSSet*>(callFrame->thisValue());
    if (!thisObj)
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Cannot create a Map key iterator for a non-Map object.")));
    return JSValue::encode(JSSetIterator::create(callFrame->vm(), callFrame->callee()->globalObject()->setIteratorStructure(), thisObj, SetIterateKeyValue));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncKeys(CallFrame* callFrame)
{
    JSSet* thisObj = jsDynamicCast<JSSet*>(callFrame->thisValue());
    if (!thisObj)
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Cannot create a Map entry iterator for a non-Map object.")));
    return JSValue::encode(JSSetIterator::create(callFrame->vm(), callFrame->callee()->globalObject()->setIteratorStructure(), thisObj, SetIterateKey));
}

}
