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

const static uint8_t JSAsyncFromSyncIteratorNumberOfInternalFields = 2;

class JSAsyncFromSyncIterator final : public JSInternalFieldObjectImpl<JSAsyncFromSyncIteratorNumberOfInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSAsyncFromSyncIteratorNumberOfInternalFields>;

    DECLARE_EXPORT_INFO;

    enum class Field : uint8_t {
        SyncIterator = 0,
        NextMethod,
    };
    static_assert(numberOfInternalFields == JSAsyncFromSyncIteratorNumberOfInternalFields);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsNull(),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.asyncFromSyncIteratorSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSAsyncFromSyncIterator* createWithInitialValues(VM&, Structure*);

    void setSyncIterator(VM& vm, JSObject* syncIterator) { internalField(Field::SyncIterator).set(vm, this, syncIterator); }
    void setNextMethod(VM& vm, JSObject* nextMethod) { internalField(Field::NextMethod).set(vm, this, nextMethod); }

private:
    JSAsyncFromSyncIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSAsyncFromSyncIterator);

JSC_DECLARE_HOST_FUNCTION(asyncFromSyncIteratorPrivateFuncCreate);

} // namespace JSC
