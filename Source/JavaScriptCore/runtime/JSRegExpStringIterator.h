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

const static uint8_t JSRegExpStringIteratorNumberOfInternalFields = 3;

class JSRegExpStringIterator final : public JSInternalFieldObjectImpl<JSRegExpStringIteratorNumberOfInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSRegExpStringIteratorNumberOfInternalFields>;

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

    enum class Field : uint8_t {
        RegExp = 0,
        String,
        Flags, // Global, FullUnicode, Done as bit flags
    };
    static_assert(numberOfInternalFields == JSRegExpStringIteratorNumberOfInternalFields);

    enum class FlagBit : uint8_t {
        Global = 1 << 0,
        FullUnicode = 1 << 1,
        Done = 1 << 2,
    };

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsNull(),
            jsNumber(0),
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

    bool isGlobal() const { return flags() & static_cast<uint8_t>(FlagBit::Global); }
    bool isFullUnicode() const { return flags() & static_cast<uint8_t>(FlagBit::FullUnicode); }
    bool isDone() const { return flags() & static_cast<uint8_t>(FlagBit::Done); }

    void setRegExp(VM& vm, JSObject* regExp) { internalField(Field::RegExp).set(vm, this, regExp); }
    void setString(VM& vm, JSValue string) { internalField(Field::String).set(vm, this, string); }
    void setGlobal(bool global) { setFlag(FlagBit::Global, global); }
    void setFullUnicode(bool fullUnicode) { setFlag(FlagBit::FullUnicode, fullUnicode); }
    void setDone(bool done) { setFlag(FlagBit::Done, done); }
    void setFlags(bool global, bool fullUnicode, bool done = false)
    {
        uint8_t flagValue = 0;
        if (global)
            flagValue |= static_cast<uint8_t>(FlagBit::Global);
        if (fullUnicode)
            flagValue |= static_cast<uint8_t>(FlagBit::FullUnicode);
        if (done)
            flagValue |= static_cast<uint8_t>(FlagBit::Done);
        internalField(Field::Flags).setWithoutWriteBarrier(jsNumber(flagValue));
    }

private:
    uint8_t flags() const { return static_cast<uint8_t>(internalField(Field::Flags).get().asInt32()); }
    void setFlag(FlagBit bit, bool value)
    {
        uint8_t currentFlags = flags();
        if (value)
            currentFlags |= static_cast<uint8_t>(bit);
        else
            currentFlags &= ~static_cast<uint8_t>(bit);
        internalField(Field::Flags).setWithoutWriteBarrier(jsNumber(currentFlags));
    }

    JSRegExpStringIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(VM&);
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSRegExpStringIterator);

JSC_DECLARE_HOST_FUNCTION(regExpStringIteratorPrivateFuncCreate);

} // namespace JSC
