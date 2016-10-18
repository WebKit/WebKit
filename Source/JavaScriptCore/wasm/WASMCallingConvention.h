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

#if ENABLE(WEBASSEMBLY)

#include "AllowMacroScratchRegisterUsage.h"
#include "B3ArgumentRegValue.h"
#include "B3BasicBlock.h"
#include "B3Const64Value.h"
#include "B3ConstrainedValue.h"
#include "B3MemoryValue.h"
#include "B3PatchpointValue.h"
#include "B3Procedure.h"
#include "B3StackmapGenerationParams.h"
#include "CallFrame.h"
#include "LinkBuffer.h"
#include "RegisterSet.h"
#include "WASMFormat.h"

namespace JSC { namespace WASM {

typedef unsigned (*NextOffset)(unsigned currentOffset, B3::Type type);

template<unsigned headerSize, NextOffset updateOffset>
class CallingConvention {
public:
    CallingConvention(Vector<GPRReg>&& registerArguments, RegisterSet&& calleeSaveRegisters)
        : m_registerArguments(registerArguments)
        , m_calleeSaveRegisters(calleeSaveRegisters)
    {
    }

    template<typename Functor>
    void iterate(const Vector<Type>& argumentTypes, B3::Procedure& proc, B3::BasicBlock* block, B3::Origin origin, const Functor& functor) const
    {
        unsigned currentOffset = headerSize;
        B3::Value* framePointer = block->appendNew<B3::Value>(proc, B3::FramePointer, origin);

        for (unsigned i = 0; i < argumentTypes.size(); ++i) {
            B3::Value* argument;
            if (i < m_registerArguments.size())
                argument = block->appendNew<B3::ArgumentRegValue>(proc, origin, m_registerArguments[i]);
            else {
                B3::Value* address = block->appendNew<B3::Value>(proc, B3::Add, origin, framePointer,
                    block->appendNew<B3::Const64Value>(proc, origin, currentOffset));
                argument = block->appendNew<B3::MemoryValue>(proc, B3::Load, toB3Type(argumentTypes[i]), origin, address);
                currentOffset = updateOffset(currentOffset, toB3Type(argumentTypes[i]));
            }
            functor(argument, i);
        }
    }

    template<typename Functor>
    B3::Value* setupCall(B3::Procedure& proc, B3::BasicBlock* block, B3::Origin origin, MacroAssemblerCodePtr target, const Vector<B3::Value*>& arguments, B3::Type returnType, const Functor& patchpointFunctor) const
    {
        size_t stackArgumentCount = arguments.size() < m_registerArguments.size() ? 0 : arguments.size() - m_registerArguments.size();
        unsigned offset = headerSize - sizeof(CallerFrameAndPC);

        proc.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), headerSize + (stackArgumentCount * sizeof(Register))));
        Vector<B3::ConstrainedValue> constrainedArguments;
        for (unsigned i = 0; i < arguments.size(); ++i) {
            B3::ValueRep rep;
            if (i < m_registerArguments.size())
                rep = B3::ValueRep::reg(m_registerArguments[i]);
            else
                rep = B3::ValueRep::stackArgument(offset);
            constrainedArguments.append(B3::ConstrainedValue(arguments[i], rep));
            offset = updateOffset(offset, arguments[i]->type());
        }

        B3::PatchpointValue* patchpoint = block->appendNew<B3::PatchpointValue>(proc, returnType, origin);
        patchpoint->appendVector(constrainedArguments);
        patchpointFunctor(patchpoint);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);

            CCallHelpers::Call call = jit.call();
            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                linkBuffer.link(call, FunctionPtr(target.executableAddress()));
            });
        });

        if (returnType == B3::Void)
            return nullptr;

        patchpoint->resultConstraint = B3::ValueRep::reg(GPRInfo::returnValueGPR);
        return patchpoint;
    }

    const Vector<GPRReg> m_registerArguments;
    const RegisterSet m_calleeSaveRegisters;
    const RegisterSet m_callerSaveRegisters;
};

inline unsigned nextJSCOffset(unsigned currentOffset, B3::Type)
{
    return currentOffset + sizeof(Register);
}

constexpr unsigned jscHeaderSize = ExecState::headerSizeInRegisters * sizeof(Register);
typedef CallingConvention<jscHeaderSize, nextJSCOffset> JSCCallingConvention;

const JSCCallingConvention& jscCallingConvention();

} } // namespace JSC::WASM

#endif // ENABLE(WEBASSEMBLY)
