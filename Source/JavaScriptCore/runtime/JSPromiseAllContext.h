/*
 * Copyright (C) 2025 Codeblog CORP.
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

#include "JSInternalFieldObjectImpl.h"

namespace JSC {

const static uint8_t JSPromiseAllContextNumberOfInternalFields = 2;

class JSPromiseAllContext final : public JSInternalFieldObjectImpl<JSPromiseAllContextNumberOfInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSPromiseAllContextNumberOfInternalFields>;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    enum class Field : uint8_t {
        GlobalContext = 0,
        Index,
    };
    static_assert(numberOfInternalFields == JSPromiseAllContextNumberOfInternalFields);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsNumber(0),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.promiseAllContextSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSPromiseAllContext* createWithInitialValues(VM&, Structure*);
    static JSPromiseAllContext* create(VM&, Structure*, JSValue globalContext, JSValue index);

    JSValue globalContext() const { return internalField(Field::GlobalContext).get(); }
    JSValue index() const { return internalField(Field::Index).get(); }

    void setGlobalContext(VM& vm, JSValue globalContext) { internalField(Field::GlobalContext).set(vm, this, globalContext); }
    void setIndex(VM& vm, JSValue index) { internalField(Field::Index).set(vm, this, index); }

private:
    JSPromiseAllContext(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(VM&, JSValue globalContext, JSValue index);
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseAllContext);

JSC_DECLARE_HOST_FUNCTION(promiseAllContextPrivateFuncCreate);

} // namespace JSC
