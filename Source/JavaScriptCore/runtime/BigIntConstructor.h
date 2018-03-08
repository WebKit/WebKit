/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "InternalFunction.h"

namespace JSC {

class BigIntPrototype;
class GetterSetter;

class BigIntConstructor final : public InternalFunction {
public:
    using Base = InternalFunction;
    static const unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    static BigIntConstructor* create(VM& vm, Structure* structure, BigIntPrototype* bigIntPrototype, GetterSetter*)
    {
        BigIntConstructor* constructor = new (NotNull, allocateCell<BigIntConstructor>(vm.heap)) BigIntConstructor(vm, structure);
        constructor->finishCreation(vm, bigIntPrototype);
        return constructor;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

protected:
    void finishCreation(VM&, BigIntPrototype*);

private:
    BigIntConstructor(VM&, Structure*);
};

} // namespace JSC
