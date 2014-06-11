/*
 * Copyright (C) 2013 Apple, Inc. All rights reserved.
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
#include "JSArrayIterator.h"

#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSArrayIterator::s_info = { "ArrayIterator", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSArrayIterator) };

static EncodedJSValue JSC_HOST_CALL arrayIteratorNextKey(ExecState*);
static EncodedJSValue JSC_HOST_CALL arrayIteratorNextValue(ExecState*);
static EncodedJSValue JSC_HOST_CALL arrayIteratorNextGeneric(ExecState*);

void JSArrayIterator::finishCreation(VM& vm, JSGlobalObject* globalObject, ArrayIterationKind kind, JSObject* iteratedObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_iterationKind = kind;
    m_iteratedObject.set(vm, this, iteratedObject);
    switch (kind) {
    case ArrayIterateKey:
        JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->iteratorNextPrivateName, arrayIteratorNextKey, DontEnum, 0, ArrayIteratorNextKeyIntrinsic);
        break;
    case ArrayIterateValue:
        JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->iteratorNextPrivateName, arrayIteratorNextValue, DontEnum, 0, ArrayIteratorNextValueIntrinsic);
        break;
    default:
        JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->iteratorNextPrivateName, arrayIteratorNextGeneric, DontEnum, 0, ArrayIteratorNextGenericIntrinsic);
        break;
    }

}
    
    
void JSArrayIterator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSArrayIterator* thisObject = jsCast<JSArrayIterator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
        
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_iteratedObject);

}

static EncodedJSValue createIteratorResult(CallFrame* callFrame, ArrayIterationKind kind, size_t index, JSValue result, bool done)
{
    if (done)
        return JSValue::encode(callFrame->vm().iterationTerminator.get());
    
    switch (kind & ~ArrayIterateSparseTag) {
    case ArrayIterateKey:
        return JSValue::encode(jsNumber(index));
        
    case ArrayIterateValue:
        return JSValue::encode(result);
        
    case ArrayIterateKeyValue: {
        MarkedArgumentBuffer args;
        args.append(jsNumber(index));
        args.append(result);
        JSGlobalObject* globalObject = callFrame->callee()->globalObject();
        return JSValue::encode(constructArray(callFrame, 0, globalObject, args));
        
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return JSValue::encode(JSValue());
}

static inline EncodedJSValue JSC_HOST_CALL arrayIteratorNext(CallFrame* callFrame)
{
    JSArrayIterator* iterator = jsDynamicCast<JSArrayIterator*>(callFrame->thisValue());
    if (!iterator) {
        ASSERT_NOT_REACHED();
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Cannot call ArrayIterator.next() on a non-ArrayIterator object")));
    }
    JSObject* iteratedObject = iterator->iteratedObject();
    size_t index = iterator->nextIndex();
    ArrayIterationKind kind = iterator->iterationKind();
    JSValue jsLength = JSValue(iteratedObject).get(callFrame, callFrame->propertyNames().length);
    if (callFrame->hadException())
        return JSValue::encode(jsNull());
    
    size_t length = jsLength.toUInt32(callFrame);
    if (callFrame->hadException())
        return JSValue::encode(jsNull());
    
    if (index >= length) {
        iterator->finish();
        return createIteratorResult(callFrame, kind, index, jsUndefined(), true);
    }
    if (JSValue result = iteratedObject->tryGetIndexQuickly(index)) {
        iterator->setNextIndex(index + 1);
        return createIteratorResult(callFrame, kind, index, result, false);
    }
    
    JSValue result = jsUndefined();
    PropertySlot slot(iteratedObject);
    if (kind > ArrayIterateSparseTag) {
        // We assume that the indexed property will be an own property so cache the getOwnProperty
        // method locally
        auto getOwnPropertySlotByIndex = iteratedObject->methodTable()->getOwnPropertySlotByIndex;
        while (index < length) {
            if (getOwnPropertySlotByIndex(iteratedObject, callFrame, index, slot)) {
                result = slot.getValue(callFrame, index);
                break;
            }
            if (iteratedObject->getPropertySlot(callFrame, index, slot)) {
                result = slot.getValue(callFrame, index);
                break;
            }
            index++;
        }
    } else if (iteratedObject->getPropertySlot(callFrame, index, slot))
        result = slot.getValue(callFrame, index);
    
    if (index == length)
        iterator->finish();
    else
        iterator->setNextIndex(index + 1);
    return createIteratorResult(callFrame, kind, index, jsUndefined(), index == length);
}
    
EncodedJSValue JSC_HOST_CALL arrayIteratorNextKey(CallFrame* callFrame)
{
    return arrayIteratorNext(callFrame);
}
    
EncodedJSValue JSC_HOST_CALL arrayIteratorNextValue(CallFrame* callFrame)
{
    return arrayIteratorNext(callFrame);
}
    
EncodedJSValue JSC_HOST_CALL arrayIteratorNextGeneric(CallFrame* callFrame)
{
    return arrayIteratorNext(callFrame);
}

}
