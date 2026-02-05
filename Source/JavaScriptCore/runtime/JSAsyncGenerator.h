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

#include "JSGenerator.h"
#include "JSInternalFieldObjectImpl.h"
#include <tuple>

namespace JSC {

class JSPromise;

class JSAsyncGenerator final : public JSInternalFieldObjectImpl<8> {
public:
    using Base = JSInternalFieldObjectImpl<8>;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.asyncGeneratorSpace<mode>();
    }

    enum class AsyncGeneratorState : int32_t {
        Completed = -1,
        Executing = -2,
        Init = 0,
        AwaitingReturn = -3,
    };
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Completed) == static_cast<int32_t>(JSGenerator::State::Completed));
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Executing) == static_cast<int32_t>(JSGenerator::State::Executing));
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Init) == static_cast<int32_t>(JSGenerator::State::Init));

    enum class AsyncGeneratorSuspendReason : int32_t {
        Await = 0,
        Yield = 1,
    };
    static constexpr int32_t reasonMask = 0x1;
    static constexpr int32_t reasonShift = 1;

    enum class AsyncGeneratorResumeMode : int32_t {
        Empty = -1,
        Normal = 0,
        Return = 1,
        Throw = 2
    };
    static_assert(static_cast<int32_t>(AsyncGeneratorResumeMode::Normal) == static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode));
    static_assert(static_cast<int32_t>(AsyncGeneratorResumeMode::Return) == static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode));
    static_assert(static_cast<int32_t>(AsyncGeneratorResumeMode::Throw) == static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));

    enum class Field : uint32_t {
        State = 0,
        Next,
        This,
        Frame,
        Queue,
        ResumeValue,
        ResumeMode,
        ResumePromise,
    };
    static_assert(numberOfInternalFields == 8);
    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNumber(static_cast<int32_t>(AsyncGeneratorState::Init)),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
            jsNull(),
            jsUndefined(),
            jsNumber(static_cast<int32_t>(AsyncGeneratorResumeMode::Empty)),
            jsUndefined(),
        } };
    }

    using Base::internalField;
    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    static JSAsyncGenerator* create(VM&, Structure*);
    static JSAsyncGenerator* createWithInitialValues(VM&, Structure*);
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

    JSValue queue() const
    {
        return Base::internalField(static_cast<unsigned>(Field::Queue)).get();
    }

    void setQueue(VM& vm, JSValue value)
    {
        Base::internalField(static_cast<unsigned>(Field::Queue)).set(vm, this, value);
    }

    JSValue resumeValue() const
    {
        return Base::internalField(static_cast<unsigned>(Field::ResumeValue)).get();
    }

    void setResumeValue(VM& vm, JSValue value)
    {
        Base::internalField(static_cast<unsigned>(Field::ResumeValue)).set(vm, this, value);
    }

    int32_t resumeMode() const
    {
        return Base::internalField(static_cast<unsigned>(Field::ResumeMode)).get().asInt32AsAnyInt();
    }

    void setResumeMode(int32_t mode)
    {
        Base::internalField(static_cast<unsigned>(Field::ResumeMode)).setWithoutWriteBarrier(jsNumber(mode));
    }

    JSValue resumePromise() const
    {
        return Base::internalField(static_cast<unsigned>(Field::ResumePromise)).get();
    }

    void setResumePromise(VM& vm, JSValue value)
    {
        Base::internalField(static_cast<unsigned>(Field::ResumePromise)).set(vm, this, value);
    }

    bool isQueueEmpty() const
    {
        return resumeMode() == static_cast<int32_t>(AsyncGeneratorResumeMode::Empty);
    }

    bool isExecutionState() const
    {
        int32_t state = this->state();
        if (state == static_cast<int32_t>(AsyncGeneratorState::Executing))
            return true;
        if (state > 0 && (state & reasonMask) == static_cast<int32_t>(AsyncGeneratorSuspendReason::Await))
            return true;
        return false;
    }

    void enqueue(VM&, JSValue value, int32_t resumeMode, JSPromise*);
    std::tuple<JSValue, int32_t, JSPromise*> dequeue(VM&);

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

private:
    JSAsyncGenerator(VM&, Structure*);
    void finishCreation(VM&);
};

} // namespace JSC
