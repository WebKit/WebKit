/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "HashMapImpl.h"
#include "JSObject.h"

namespace JSC {

class JSSet final : public HashMapImpl<HashMapBucket<HashMapBucketDataKey>> {
    using Base = HashMapImpl<HashMapBucket<HashMapBucketDataKey>>;
public:

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSSetType, StructureFlags), info());
    }

    static JSSet* create(ExecState* exec, VM& vm, Structure* structure)
    {
        return create(exec, vm, structure, 0);
    }

    static JSSet* create(ExecState* exec, VM& vm, Structure* structure, uint32_t size)
    {
        JSSet* instance = new (NotNull, allocateCell<JSSet>(vm.heap)) JSSet(vm, structure, size);
        instance->finishCreation(exec, vm);
        return instance;
    }

    bool isIteratorProtocolFastAndNonObservable();
    bool canCloneFastAndNonObservable(Structure*);
    JSSet* clone(ExecState*, VM&, Structure*);

private:
    JSSet(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    JSSet(VM& vm, Structure* structure, uint32_t sizeHint)
        : Base(vm, structure, sizeHint)
    {
    }

    static String toStringName(const JSObject*, ExecState*);
};

static_assert(std::is_final<JSSet>::value, "Required for JSType based casting");

} // namespace JSC
