/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>.
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "BigIntObject.h"

#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(BigIntObject);

const ClassInfo BigIntObject::s_info = { "BigInt", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(BigIntObject) };


BigIntObject* BigIntObject::create(VM& vm, JSGlobalObject* globalObject, JSValue bigInt)
{
    ASSERT(bigInt.isBigInt());
    BigIntObject* object = new (NotNull, allocateCell<BigIntObject>(vm.heap)) BigIntObject(vm, globalObject->bigIntObjectStructure());
    object->finishCreation(vm, bigInt);
    return object;
}

BigIntObject::BigIntObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void BigIntObject::finishCreation(VM& vm, JSValue bigInt)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    setInternalValue(vm, bigInt);
}

JSValue BigIntObject::defaultValue(const JSObject* object, JSGlobalObject*, PreferredPrimitiveType)
{
    const BigIntObject* bigIntObject = jsCast<const BigIntObject*>(object);
    return bigIntObject->internalValue();
}

} // namespace JSC
