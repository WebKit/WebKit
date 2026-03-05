/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/JSInternalFieldObjectImpl.h>

namespace JSC {

class JSGenerator final : public JSInternalFieldObjectImpl<5> {
public:
    using Base = JSInternalFieldObjectImpl<5>;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.generatorSpace<mode>();
    }

    enum class ResumeMode : int32_t {
        NormalMode = 0,
        ReturnMode = 1,
        ThrowMode = 2
    };

    enum class State : int32_t {
        Completed = -1,
        Executing = -2,
        Init = 0,
    };

    // [this], @generator, @generatorState, @generatorValue, @generatorResumeMode, @generatorFrame.
    enum class Argument : int32_t {
        ThisValue = 0,
        Generator = 1,
        State = 2,
        Value = 3,
        ResumeMode = 4,
        Frame = 5,
        NumberOfArguments = Frame,
    };

    enum class Field : uint32_t {
        State = 0,
        Next,
        This,
        Frame,
        Context,
    };
    static_assert(numberOfInternalFields == 5);
    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNumber(static_cast<int32_t>(State::Init)),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
        } };
    }

    using Base::internalField;
    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    static JSGenerator* create(VM&, Structure*);
    static JSGenerator* createWithInitialValues(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    int32_t state() const
    {
        return internalField(Field::State).get().asInt32AsAnyInt();
    }

    void setState(int32_t state)
    {
        Base::internalField(static_cast<unsigned>(Field::State)).setWithoutWriteBarrier(jsNumber(state));
    }

    JSValue next() const
    {
        return Base::internalField(static_cast<unsigned>(Field::Next)).get();
    }

    JSValue thisValue() const
    {
        return Base::internalField(static_cast<unsigned>(Field::This)).get();
    }

    JSValue frame() const
    {
        return Base::internalField(static_cast<unsigned>(Field::Frame)).get();
    }

    JSValue context() const
    {
        return Base::internalField(static_cast<unsigned>(Field::Context)).get();
    }

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

private:
    JSGenerator(VM&, Structure*);
    void finishCreation(VM&);
};

OVERLOAD_RELATIONAL_OPERATORS_FOR_ENUM_CLASS_WITH_INTEGRALS(JSGenerator::Argument);

} // namespace JSC
