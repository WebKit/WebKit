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
#include "B3Procedure.h"

namespace JSC { namespace B3 { namespace Air {

namespace CCallDetail {

void RegisterArgument::recordArgs(Vector<Arg>& result) const
{
    result.append(Tmp(reg));
}

#if CPU(ARM_THUMB2)
void RegisterPairArgument::recordArgs(Vector<Arg>& result) const
{
    result.append(Tmp(lo));
    result.append(Tmp(hi));
}
#endif

void StackArgument::recordArgs(Vector<Arg>& result) const
{
    auto const width = cCallArgumentRegisterWidth(type);
    auto const count = cCallArgumentRegisterCount(type);
    ASSERT(count == 1 || isARM_THUMB2());

    unsigned stackOffset = offset;
    for (unsigned i = 0; i < count; i++) {
        result.append(Arg::callArg(stackOffset));
        stackOffset += bytesForWidth(width);
    }
}

void recordAirArgs(Vector<Arg>& result, const Argument& arg)
{
    std::visit(
        [&](auto&& cArg) -> void { cArg.recordArgs(result); },
        arg
    );
}

} // namespace CCallDetail

namespace {

template<typename BankInfo>
CCallDetail::Argument marshallCCallArgumentImpl(unsigned& argumentCount, unsigned& stackOffset, Type type)
{
    const auto registerCount = cCallArgumentRegisterCount(type);
    if (isARM_THUMB2() && type == Int64)
        argumentCount = WTF::roundUpToMultipleOf<2>(argumentCount);

    if (argumentCount < BankInfo::numberOfArgumentRegisters) {
        switch (registerCount) {
        case 1:
            return CCallDetail::RegisterArgument {
                BankInfo::toArgumentRegister(argumentCount++),
                type
            };
#if CPU(ARM_THUMB2)
        case 2: {
            auto const lo = BankInfo::toArgumentRegister(argumentCount++);
            auto const hi = BankInfo::toArgumentRegister(argumentCount++);
            return CCallDetail::RegisterPairArgument(hi, lo);
        }
#endif
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    unsigned slotSize, slotAlignment;
    if ((isARM64() && isDarwin()) || isARM_THUMB2()) {
        // Arguments are packed to their natural alignment.
        slotSize = sizeofType(type);
        slotAlignment = sizeofType(type);
    } else {
        // On other platforms, arguments are always aligned to machine word
        // boundaries.
        slotSize = 8;
        slotAlignment = slotSize;
    }

    stackOffset = WTF::roundUpToMultipleOf(slotAlignment, stackOffset);
    unsigned offset = stackOffset;
    stackOffset += slotSize;
    return CCallDetail::StackArgument { offset, type };
}

CCallDetail::Argument marshallCCallArgument(unsigned& gpArgumentCount, unsigned& fpArgumentCount, unsigned& stackOffset, Type type)
{
    switch (bankForType(type)) {
    case GP:
        return marshallCCallArgumentImpl<GPRInfo>(gpArgumentCount, stackOffset, type);
    case FP:
        return marshallCCallArgumentImpl<FPRInfo>(fpArgumentCount, stackOffset, type);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // anonymous namespace

Vector<CCallDetail::Argument> computeCCallArguments(const Vector<Type>& types)
{
    Vector<CCallDetail::Argument> result;
    unsigned gpArgumentCount = 0;
    unsigned fpArgumentCount = 0;
    unsigned stackOffset = 0;

    for (auto type : types)
        result.append(marshallCCallArgument(gpArgumentCount, fpArgumentCount, stackOffset, type));

    return result;
}


Vector<Arg> computeCCallingConvention(Code& code, CCallValue* value)
{
    Vector<Arg> result;
    result.append(Tmp(CCallSpecial::scratchRegister)); // For callee
    unsigned gpArgumentCount = 0;
    unsigned fpArgumentCount = 0;
    unsigned stackOffset = 0;
    for (unsigned i = 1; i < value->numChildren(); ++i) {
        CCallDetail::recordAirArgs(
            result,
            marshallCCallArgument(gpArgumentCount, fpArgumentCount, stackOffset, value->child(i)->type())
        );
    }
    code.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf(stackAlignmentBytes(), stackOffset));
    return result;
}

size_t cCallResultCount(Code& code, Type type)
{
    switch (type.kind()) {
    case Void:
        return 0;
    case Int64:
        if constexpr (is32Bit())
            return 2;
        return 1;
    case Tuple:
        // We only support tuples that return exactly two register sized ints.
        UNUSED_PARAM(code);
        ASSERT(code.proc().resultCount(type) == 2);
        ASSERT(code.proc().typeAtOffset(type, 0) == pointerType());
        ASSERT(code.proc().typeAtOffset(type, 1) == pointerType());
        return 2;
    default:
        return 1;

    }
}

size_t cCallResultCount(Code& code, const CCallValue* value)
{
    return cCallResultCount(code, value->type());
}

size_t cCallArgumentRegisterCount(Type type)
{
    switch (type.kind()) {
    case Void:
        return 0;
    case Int64:
        if constexpr (is32Bit())
            return 2;
        return 1;
    case Tuple:
        RELEASE_ASSERT_NOT_REACHED();
    default:
        return 1;
    }
}

Width cCallArgumentRegisterWidth(Type type)
{
    if constexpr (is32Bit()) {
        if (type == Int64)
            return Width32;
    }

    return widthForType(type);
}

Tmp cCallResult(Code& code, CCallValue* value, unsigned index)
{
    ASSERT(index < 2);
    switch (value->type().kind()) {
    case Void:
        return Tmp();
    case Int32:
        return Tmp(GPRInfo::returnValueGPR);
    case Int64:
        if (is32Bit() && index == 1)
            return Tmp(GPRInfo::returnValueGPR2);
        return Tmp(GPRInfo::returnValueGPR);
    case Float:
    case Double:
        ASSERT(!index);
        return Tmp(FPRInfo::returnValueFPR);
    case Tuple:
        ASSERT_UNUSED(code, code.proc().resultCount(value->type()) == 2);
        // We only support functions that return each parameter in its own register for now.
        ASSERT(code.proc().typeAtOffset(value->type(), 0) == registerType());
        ASSERT(code.proc().typeAtOffset(value->type(), 1) == registerType());
        return index ? Tmp(GPRInfo::returnValueGPR2) : Tmp(GPRInfo::returnValueGPR);
    case V128:
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

