/*
 * Copyright (C) 2013-2021 Apple, Inc. All rights reserved.
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
#include "JSSetIterator.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSSet.h"

namespace JSC {

const ClassInfo JSSetIterator::s_info = { "Set Iterator", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSSetIterator) };

JSSetIterator* JSSetIterator::createWithInitialValues(VM& vm, Structure* structure)
{
    JSSetIterator* iterator = new (NotNull, allocateCell<JSSetIterator>(vm)) JSSetIterator(vm, structure);
    iterator->finishCreation(vm);
    return iterator;
}

void JSSetIterator::finishCreation(VM& vm, JSSet* iteratedObject, IterationKind kind)
{
    Base::finishCreation(vm);
    internalField(Field::SetBucket).set(vm, this, iteratedObject->head());
    internalField(Field::IteratedObject).set(vm, this, iteratedObject);
    internalField(Field::Kind).set(vm, this, jsNumber(static_cast<int32_t>(kind)));
}

void JSSetIterator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSSetIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSSetIterator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSSetIterator);

JSValue JSSetIterator::createPair(JSGlobalObject* globalObject, JSValue key, JSValue value)
{
    MarkedArgumentBuffer args;
    args.append(key);
    args.append(value);
    ASSERT(!args.hasOverflowed());
    return constructArray(globalObject, static_cast<ArrayAllocationProfile*>(nullptr), args);
}

}
