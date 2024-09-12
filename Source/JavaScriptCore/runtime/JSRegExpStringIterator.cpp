/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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
#include "JSRegExpStringIterator.h"

#include "Error.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSRegExpStringIterator::s_info = { "RegExpStringIterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSRegExpStringIterator) };

JSRegExpStringIterator* JSRegExpStringIterator::createWithInitialValues(VM& vm, Structure* structure)
{
    JSRegExpStringIterator* iterator = new (NotNull, allocateCell<JSRegExpStringIterator>(vm)) JSRegExpStringIterator(vm, structure);
    iterator->finishCreation(vm);
    return iterator;
}

void JSRegExpStringIterator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSRegExpStringIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSRegExpStringIterator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSRegExpStringIterator);

JSC_DEFINE_HOST_FUNCTION(regExpStringIteratorPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());
    ASSERT(callFrame->argument(1).isString());
    ASSERT(callFrame->argument(2).isBoolean());
    ASSERT(callFrame->argument(3).isBoolean());

    VM& vm = globalObject->vm();

    auto* regExpStringIterator = JSRegExpStringIterator::createWithInitialValues(vm, globalObject->regExpStringIteratorStructure());

    regExpStringIterator->setRegExp(vm, asObject(callFrame->uncheckedArgument(0)));
    regExpStringIterator->setString(vm, callFrame->uncheckedArgument(1));
    regExpStringIterator->setGlobal(vm, callFrame->argument(2));
    regExpStringIterator->setFullUnicode(vm, callFrame->argument(3));

    return JSValue::encode(regExpStringIterator);
}

} // namespace JSC
