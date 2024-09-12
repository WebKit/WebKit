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
#include "JSAsyncFromSyncIterator.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSAsyncFromSyncIterator::s_info = { "AsyncFromSyncIterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAsyncFromSyncIterator) };

JSAsyncFromSyncIterator* JSAsyncFromSyncIterator::createWithInitialValues(VM& vm, Structure* structure)
{
    JSAsyncFromSyncIterator* iterator = new (NotNull, allocateCell<JSAsyncFromSyncIterator>(vm)) JSAsyncFromSyncIterator(vm, structure);
    iterator->finishCreation(vm);
    return iterator;
}

void JSAsyncFromSyncIterator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSAsyncFromSyncIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSAsyncFromSyncIterator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSAsyncFromSyncIterator);

JSC_DEFINE_HOST_FUNCTION(asyncFromSyncIteratorPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* asyncFromSyncIterator = JSAsyncFromSyncIterator::createWithInitialValues(vm, globalObject->asyncFromSyncIteratorStructure());
    if (callFrame->argument(0).isCell())
        asyncFromSyncIterator->setSyncIterator(vm, asObject(callFrame->uncheckedArgument(0)));
    if (callFrame->argument(1).isCell())
        asyncFromSyncIterator->setNextMethod(vm, asObject(callFrame->uncheckedArgument(1)));

    return JSValue::encode(asyncFromSyncIterator);
}

} // namespace JSC
