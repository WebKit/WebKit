/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3ArgumentRegValue.h"
#include "B3BasicBlock.h"
#include "B3Const64Value.h"
#include "B3MemoryValue.h"
#include "B3Type.h"
#include "CallFrame.h"
#include "RegisterSet.h"

namespace JSC {

namespace B3 {

typedef unsigned (*NextOffset)(unsigned currentOffset, Type type);

template<unsigned offset, NextOffset updateOffset>
class CallingConvention {
public:
    static const unsigned headerSize = offset;

    CallingConvention(Vector<GPRReg>&& registerArguments, RegisterSet&& calleeSaveRegisters)
        : m_registerArguments(registerArguments)
        , m_calleeSaveRegisters(calleeSaveRegisters)
    {
    }

    template<typename Functor>
    void iterate(const Vector<Type>& argumentTypes, Procedure& proc, BasicBlock* block, Origin origin, const Functor& functor) const
    {
        unsigned currentOffset = headerSize;
        Value* framePointer = block->appendNew<Value>(proc, FramePointer, origin);

        for (unsigned i = 0; i < argumentTypes.size(); ++i) {
            Value* argument;
            if (i < m_registerArguments.size())
                argument = block->appendNew<ArgumentRegValue>(proc, origin, m_registerArguments[i]);
            else {
                Value* address = block->appendNew<Value>(proc, Add, origin, framePointer,
                    block->appendNew<Const64Value>(proc, origin, currentOffset));
                argument = block->appendNew<MemoryValue>(proc, Load, argumentTypes[i], origin, address);
                currentOffset = updateOffset(currentOffset, argumentTypes[i]);
            }
            functor(argument, i);
        }
    }

    const Vector<GPRReg> m_registerArguments;
    const RegisterSet m_calleeSaveRegisters;
    const RegisterSet m_callerSaveRegisters;
};

inline unsigned nextJSCOffset(unsigned currentOffset, Type)
{
    return currentOffset + sizeof(Register);
}

constexpr unsigned jscHeaderSize = ExecState::headerSizeInRegisters * sizeof(Register);
typedef CallingConvention<jscHeaderSize, nextJSCOffset> JSCCallingConvention;

JSCCallingConvention& jscCallingConvention();

} // namespace B3

} // namespace JSC

#endif // ENABLE(B3_JIT)
