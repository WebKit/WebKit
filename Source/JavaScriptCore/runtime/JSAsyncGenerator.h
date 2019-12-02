/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

namespace JSC {

class JSAsyncGenerator final : public JSInternalFieldObjectImpl<8> {
public:
    using Base = JSInternalFieldObjectImpl<8>;

    // JSAsyncGenerator has one inline storage slot, which is pointing internalField(0).
    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, inlineCapacity == 0U);
        return sizeof(JSAsyncGenerator);
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.asyncGeneratorSpace<mode>();
    }

    enum class AsyncGeneratorState : int32_t {
        Completed = -1,
        Executing = -2,
        SuspendedStart = -3,
        SuspendedYield = -4,
        AwaitingReturn = -5,
    };
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Completed) == static_cast<int32_t>(JSGenerator::GeneratorState::Completed));
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Executing) == static_cast<int32_t>(JSGenerator::GeneratorState::Executing));

    enum class AsyncGeneratorSuspendReason : int32_t {
        None = 0,
        Yield = -1,
        Await = -2
    };

    enum class Field : uint32_t {
        // FIXME: JSAsyncGenerator should support PolyProto, since generator tends to be created with poly proto mode.
        // We reserve the first internal field for PolyProto property. This offset is identical to JSFinalObject's first inline storage slot which will be used for PolyProto.
        PolyProto = 0,
        State,
        Next,
        This,
        Frame,
        SuspendReason,
        QueueFirst,
        QueueLast,
    };
    static_assert(numberOfInternalFields == 8);
    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsNumber(static_cast<int32_t>(AsyncGeneratorState::SuspendedStart)),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
            jsNumber(static_cast<int32_t>(AsyncGeneratorSuspendReason::None)),
            jsNull(),
            jsNull(),
        } };
    }

    static JSAsyncGenerator* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    static void visitChildren(JSCell*, SlotVisitor&);

protected:
    JSAsyncGenerator(VM&, Structure*);
    void finishCreation(VM&);
};

} // namespace JSC
