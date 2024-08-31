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
#include "B3BasicBlockInlines.h"
#include "B3BreakCriticalEdges.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 { namespace Air {

namespace {

template<typename BankInfo>
void marshallCCallArgumentImpl(Vector<Arg>& result, unsigned& argumentCount, unsigned& stackOffset, Type childType)
{
    const auto registerCount = cCallArgumentRegisterCount(childType);
    if (is32Bit() && childType == Int64)
        argumentCount = WTF::roundUpToMultipleOf<2>(argumentCount);

    if (argumentCount < BankInfo::numberOfArgumentRegisters) {
        for (unsigned i = 0; i < registerCount; i++)
            result.append(Tmp(BankInfo::toArgumentRegister(argumentCount++)));
        return;
    }

    unsigned slotSize, slotAlignment;
    if ((isARM64() && isDarwin()) || isARM_THUMB2()) {
        // Arguments are packed to their natural alignment.
        //
        // In the rare case when the Arg width does not match the argument width
        // (32-bit arm passing a 64-bit argument), we respect the width needed
        // for each stack access:
        slotSize = bytesForWidth(cCallArgumentRegisterWidth(childType));

        // but the logical stack slot uses the natural alignment of the argument
        slotAlignment = sizeofType(childType);

    } else {
        // On other platforms, arguments are always aligned to machine word
        // boundaries.
        slotSize = 8;
        slotAlignment = slotSize;
    }

    stackOffset = WTF::roundUpToMultipleOf(slotAlignment, stackOffset);
    for (unsigned i = 0; i < registerCount; i++) {
        result.append(Arg::callArg(stackOffset));
        stackOffset += slotSize;
    }
}


void marshallCCallArgument(Vector<Arg> &result, unsigned& gpArgumentCount, unsigned& fpArgumentCount, unsigned& stackOffset, Type childType)
{
    switch (bankForType(childType)) {
    case GP:
        marshallCCallArgumentImpl<GPRInfo>(result, gpArgumentCount, stackOffset, childType);
        return;
    case FP:
        marshallCCallArgumentImpl<FPRInfo>(result, fpArgumentCount, stackOffset, childType);
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
        marshallCCallArgument(result, gpArgumentCount, fpArgumentCount, stackOffset, value->child(i)->type());
    code.requestCallArgAreaSizeInBytes(WTF::roundUpToMultipleOf<stackAlignmentBytes()>(stackOffset));
    return result;
}

size_t cCallResultCount(Code& code, CCallValue* value)
{
    switch (value->type().kind()) {
    case Void:
        return 0;
    case Int64:
        if constexpr (is32Bit())
            return 2;
        return 1;
    case Tuple:
        // We only support functions that return each parameter in its own register for now.
        UNUSED_PARAM(code);
        ASSERT(code.proc().resultCount(value->type()) == 2);
        if (is32Bit())
            ASSERT(code.proc().typeAtOffset(value->type(), 0) == pointerType());
        else
            ASSERT(code.proc().typeAtOffset(value->type(), 0).isInt());
        ASSERT(code.proc().typeAtOffset(value->type(), 1) == pointerType());
        return 2;
    default:
        return 1;

    }
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
        if (is32Bit())
            ASSERT(code.proc().typeAtOffset(value->type(), 0) == registerType());
        else
            ASSERT(code.proc().typeAtOffset(value->type(), 0).isInt());
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

#if CPU(ARM_THUMB2)
Value* ArgumentValueList::makeStitch(B3::BasicBlock*, Value* hi, Value* low) const
{
    return block->appendNew<Value>(procedure, Stitch, Origin(), hi, low);
}
#endif

Value* ArgumentValueList::makeCCallValue(B3::BasicBlock* block, Type type, Air::Arg arg) const
{
    if (arg.isTmp() && arg.tmp().isReg()) {
        Value* val = block->appendNew<ArgumentRegValue>(procedure, Origin(), arg.reg());
        if constexpr (!is32Bit()) {
            if (type == Int32)
                val = block->appendNew<Value>(procedure, Trunc, Origin(), val);
        }

        if (type == Float)
            val = block->appendNew<Value>(procedure, Trunc, Origin(), val);

        return val;
    }

    // we really shouldn't be using this except on arm32, so, assert just in
    // case, since the details are likely wrong for other architectures, and
    // it's not expected to end up here on 64-bit
    RELEASE_ASSERT(isARM_THUMB2() && arg.isCallArg());

    return block->appendNew<MemoryValue>(procedure,
        Load,
        type,
        Origin(),
        block->appendNew<Value>(procedure, FramePointer, Origin()),
        arg.offset() + 2 * sizeof(void*)
    );
}

Value* ArgumentValueList::makeCCallValue(B3::BasicBlock* block, size_t idx) const
{
    unsigned firstUnderlyingArg = argUnderlyingCounts[idx];
    unsigned argCount = 0;
    if (idx + 1 < types.size())
        argCount = argUnderlyingCounts[idx + 1] - firstUnderlyingArg;
    else
        argCount = underlyingArgs.size() - firstUnderlyingArg;
    RELEASE_ASSERT(firstUnderlyingArg < underlyingArgs.size());
    RELEASE_ASSERT(firstUnderlyingArg + argCount <= underlyingArgs.size());

    switch (types[idx].kind()) {
    case Int32:
    case Float:
    case Double:
        RELEASE_ASSERT(argCount == 1);
        return makeCCallValue(block, types[idx], underlyingArgs[firstUnderlyingArg]);

    case Int64:
        RELEASE_ASSERT(argCount == sizeof(uint64_t) / sizeof(uintptr_t));
#if CPU(ARM_THUMB2)
            return makeStitch(block, makeCCallValue(block, types[idx], underlyingArgs[firstUnderlyingArg + 1]),
                makeCCallValue(block, types[idx], underlyingArgs[firstUnderlyingArg]));
#else
        return makeCCallValue(block, types[idx], underlyingArgs[firstUnderlyingArg]);
#endif

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

ArgumentValueList computeCCallArguments(Procedure& procedure, B3::BasicBlock* block, const Vector<Type>& types)
{
    Vector<Air::Arg> underlyingArgs;
    Vector<unsigned> argUnderlyingCounts;
    unsigned gpArgumentCount = 0;
    unsigned fpArgumentCount = 0;
    unsigned stackOffset = 0;

    for (auto type : types) {
        argUnderlyingCounts.append(underlyingArgs.size());
        marshallCCallArgument(underlyingArgs, gpArgumentCount, fpArgumentCount, stackOffset, type);
    }

    return ArgumentValueList { procedure, block, types, WTFMove(underlyingArgs), WTFMove(argUnderlyingCounts) };
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

