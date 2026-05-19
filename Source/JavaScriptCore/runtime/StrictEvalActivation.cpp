/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StrictEvalActivation.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo StrictEvalActivation::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(StrictEvalActivation) };

StrictEvalActivation::StrictEvalActivation(VM& vm, Structure* structure, JSObject* currentScope)
    : Base(vm, structure)
    , m_next(currentScope, WriteBarrierEarlyInit)
{
}

template<typename Visitor>
void StrictEvalActivation::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<StrictEvalActivation>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_bindingStorage);
    visitor.append(thisObject->m_next);
}

DEFINE_VISIT_CHILDREN(StrictEvalActivation);

JSObject* StrictEvalActivation::ensureBindingStorage(JSGlobalObject* globalObject)
{
    if (auto* existing = m_bindingStorage.get())
        return existing;
    VM& vm = globalObject->vm();
    JSObject* storage = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    m_bindingStorage.set(vm, this, storage);
    return storage;
}

bool StrictEvalActivation::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    auto* thisObject = uncheckedDowncast<StrictEvalActivation>(object);
    if (JSObject* storage = thisObject->bindingStorage()) {
        if (storage->getOwnPropertySlot(storage, globalObject, propertyName, slot)) {
            slot.setThisValue(JSValue(thisObject));
            return true;
        }
    }
    return false;
}

bool StrictEvalActivation::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    auto* thisObject = uncheckedDowncast<StrictEvalActivation>(cell);
    JSObject* storage = thisObject->ensureBindingStorage(globalObject);
    storage->putDirect(globalObject->vm(), propertyName, value, slot);
    return true;
}

bool StrictEvalActivation::deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&)
{
    return false;
}

}
