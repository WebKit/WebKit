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

#pragma once

#include "JSInternalFieldObjectImpl.h"

namespace JSC {

const static uint8_t JSRegExpStringIteratorNumberOfInternalFields = 5;

class JSRegExpStringIterator final : public JSInternalFieldObjectImpl<JSRegExpStringIteratorNumberOfInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSRegExpStringIteratorNumberOfInternalFields>;

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

    enum class Field : uint8_t {
        RegExp = 0,
        String,
        Global,
        FullUnicode,
        Done,
    };
    static_assert(numberOfInternalFields == JSRegExpStringIteratorNumberOfInternalFields);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsNull(),
            jsBoolean(false),
            jsBoolean(false),
            jsBoolean(false),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.regExpStringIteratorSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSRegExpStringIterator* createWithInitialValues(VM&, Structure*);

    void setRegExp(VM& vm, JSObject* regExp) { internalField(Field::RegExp).set(vm, this, regExp); }
    void setString(VM& vm, JSValue string) { internalField(Field::String).set(vm, this, string); }
    void setGlobal(VM& vm, JSValue global) { internalField(Field::Global).set(vm, this, global); }
    void setFullUnicode(VM& vm, JSValue fullUnicode) { internalField(Field::FullUnicode).set(vm, this, fullUnicode); }

private:
    JSRegExpStringIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(VM&);
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSRegExpStringIterator);

JSC_DECLARE_HOST_FUNCTION(regExpStringIteratorPrivateFuncCreate);

} // namespace JSC
