/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "AirCCallingConvention.h"

#if ENABLE(B3_JIT)

#include "AirCCallSpecial.h"
#include "AirCode.h"
#include "B3CCallValue.h"

namespace JSC { namespace B3 { namespace Air {

namespace {

template<typename BankInfo>
void marshallCCallArgumentImpl(Vector<Arg> &result, unsigned& argumentCount, unsigned& stackOffset, Value* child)
{
    // FIXME: ARMv7 softfp support?

#if USE(JSVALUE32_64)
    if (child->type() == Int64)
        argumentCount = WTF::roundUpToMultipleOf<2>(argumentCount);
#endif

    if (argumentCount < BankInfo::numberOfArgumentRegisters) {
        result.append(Tmp(BankInfo::toArgumentRegister(argumentCount++)));
#if USE(JSVALUE32_64)
        if (child->type() == Int64) {
            ASSERT(argumentCount < BankInfo::numberOfArgumentRegisters);
            result.append(Tmp(BankInfo::toArgumentRegister(argumentCount++)));
        }
#endif
        return;
    }

    unsigned slotSize;
    if ((isARM64() && isDarwin()) || isARM()) {
        // Arguments are packed.
        slotSize = sizeofType(child->type());
    } else {
        // Arguments are aligned.
        slotSize = 8;
    }

    stackOffset = WTF::roundUpToMultipleOf(slotSize, stackOffset);
    result.append(Arg::callArg(stackOffset));
#if USE(JSVALUE32_64)
    if (child->type() == Int64)
        result.append(Arg::callArg(stackOffset + 4));
#endif
    stackOffset += slotSize;
}

void marshallCCallArgument(Vector<Arg> &result, unsigned& gpArgumentCount, unsigned& fpArgumentCount, unsigned& stackOffset, Value* child)
{
    switch (bankForType(child->type())) {
    case GP:
        marshallCCallArgumentImpl<GPRInfo>(result, gpArgumentCount, stackOffset, child);
        return;
    case FP:
        marshallCCallArgumentImpl<FPRInfo>(result, fpArgumentCount, stackOffset, child);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // anonymous namespace

Vector<Arg> computeCCallingConvention(Code& code, CCallValue* value)
{
    Vector<Arg> result;
    result.append(Tmp(CCallSpecial::scratchRegister)); // For callee
    unsigned gpArgumentCount = 0;
    unsigned fpArgumentCount = 0;
    unsigned stackOffset = 0;
    for (unsigned i = 1; i < value->numChildren(); ++i)
        marshallCCallArgument(result, gpArgumentCount, fpArgumentCount, stackOffset, value->child(i));
    code.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), stackOffset));
    return result;
}

size_t cCallResultCount(CCallValue* value)
{
    switch (value->type().kind()) {
    case Void:
        return 0;
#if USE(JSVALUE32_64)
    case Int64:
        return 2;
#endif
    case Tuple:
        RELEASE_ASSERT_NOT_REACHED();
    default:
        return 1;
    }
}

Tmp cCallResult(CCallValue* value, unsigned index)
{
    ASSERT_UNUSED(index, index <= (is64Bit() ? 1 : 2));
    switch (value->type().kind()) {
    case Void:
        return Tmp();
    case Int32:
        return Tmp(GPRInfo::returnValueGPR);
    case Int64:
#if USE(JSVALUE32_64)
        if (index == 1)
            return Tmp(GPRInfo::returnValueGPR2);
#endif
        return Tmp(GPRInfo::returnValueGPR);
    case Float:
    case Double:
        return Tmp(FPRInfo::returnValueFPR);
    case Tuple:
        break;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return Tmp();
}

Inst buildCCall(Code& code, Value* origin, const Vector<Arg>& arguments)
{
    Inst inst(Patch, origin, Arg::special(code.cCallSpecial()));
    inst.args.append(arguments[0]);
    inst.args.append(Tmp(GPRInfo::returnValueGPR));
    inst.args.append(Tmp(GPRInfo::returnValueGPR2));
    inst.args.append(Tmp(FPRInfo::returnValueFPR));
    for (unsigned i = 1; i < arguments.size(); ++i) {
        Arg arg = arguments[i];
        if (arg.isTmp())
            inst.args.append(arg);
    }
    return inst;
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

