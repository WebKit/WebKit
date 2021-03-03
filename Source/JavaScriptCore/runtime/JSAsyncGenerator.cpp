/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "JSAsyncGenerator.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSAsyncGenerator::s_info = { "AsyncGenerator", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAsyncGenerator) };

JSAsyncGenerator* JSAsyncGenerator::create(VM& vm, Structure* structure)
{
    JSAsyncGenerator* generator = new (NotNull, allocateCell<JSAsyncGenerator>(vm.heap)) JSAsyncGenerator(vm, structure);
    generator->finishCreation(vm);
    return generator;
}

Structure* JSAsyncGenerator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSAsyncGeneratorType, StructureFlags), info());
}

JSAsyncGenerator::JSAsyncGenerator(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSAsyncGenerator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    ASSERT(values.size() == numberOfInternalFields);
    for (unsigned index = 0; index < values.size(); ++index)
        internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSAsyncGenerator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSAsyncGenerator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSAsyncGenerator);

} // namespace JSC
