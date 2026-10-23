/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/AllowMacroScratchRegisterUsage.h>
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/LinkBuffer.h>
#include <JavaScriptCore/RegisterAtOffsetList.h>
#include <JavaScriptCore/RegisterSet.h>
#include <JavaScriptCore/StackAlignment.h>
#include <JavaScriptCore/WasmFormat.h>
#include <JavaScriptCore/WasmTypeDefinition.h>
#include <JavaScriptCore/WasmTypeDefinitionInlines.h>
#include <JavaScriptCore/WasmValueLocation.h>

namespace JSC { namespace Wasm {

constexpr unsigned numberOfIPIntCalleeSaveRegisters = 2;
constexpr unsigned numberOfIPIntInternalRegisters = 1; // UnboxedWasmCalleeStackSlot
constexpr ptrdiff_t WasmToJSScratchSpaceSize = 0x8 * 2; // Needs to be aligned to 0x10. 2 slots: callable function + IPInt return PC.
constexpr ptrdiff_t WasmToJSCallableFunctionSlot = -0x8;
constexpr ptrdiff_t WasmToJSIPIntReturnPCSlot = -0x10; // IPInt PC saved here by both the JIT and no-JIT WasmToJS stubs for collectCallStack.

struct ArgumentLocation {
#if USE(JSVALUE32_64)
    ArgumentLocation(ValueLocation loc, Width width, Width usedWidth)
        : location(loc)
        , width(width)
        , usedWidth(usedWidth)
    {
    }
    ArgumentLocation(ValueLocation loc, Width width)
        : location(loc)
        , width(width)
        , usedWidth(width)
    {
    }
#else
    ArgumentLocation(ValueLocation loc, Width width)
        : location(loc)
        , width(width)
    {
    }
#endif // USE(JSVALUE32_64)

    ArgumentLocation() {}

    ValueLocation location;
    Width width;
#if USE(JSVALUE32_64)
    Width usedWidth;
#endif
};

enum class CallRole : uint8_t {
    Caller,
    Callee,
};

struct CallInformation {
    CallInformation() = default;
    CallInformation(ArgumentLocation passedThisArgument, Vector<ArgumentLocation, 8>&& parameters, Vector<ArgumentLocation, 1>&& returnValues, size_t totalSize, size_t headerSize)
        : thisArgument(passedThisArgument)
        , params(WTF::move(parameters))
        , results(WTF::move(returnValues))
        , headerAndArgumentStackSizeInBytes(totalSize)
        , headerIncludingThisSizeInBytes(headerSize)
    { }

    RegisterAtOffsetList computeResultsOffsetList()
    {
        RegisterSet usedResultRegisters;
        for (auto loc : results) {
            if (loc.location.isGPR()) {
                usedResultRegisters.add(loc.location.jsr().payloadGPR(), IgnoreVectors);
#if USE(JSVALUE32_64)
                usedResultRegisters.add(loc.location.jsr().tagGPR(), IgnoreVectors);
#endif
            } else if (loc.location.isFPR())
                usedResultRegisters.add(loc.location.fpr(), loc.width);
        }

        RegisterAtOffsetList savedRegs(usedResultRegisters, RegisterAtOffsetList::ZeroBased);
        return savedRegs;
    }

    ArgumentLocation thisArgument { };
    Vector<ArgumentLocation, 8> params { };
    Vector<ArgumentLocation, 1> results { };
    // As a callee these include CallerFrameAndPC; as a caller it does not.
    size_t headerAndArgumentStackSizeInBytes { 0 };
    size_t headerIncludingThisSizeInBytes { 0 };
};

class WasmCallingConvention {
public:
    static constexpr unsigned headerSizeInBytes = CallFrame::headerSizeInRegisters * sizeof(Register);

    WasmCallingConvention(Vector<JSValueRegs>&& jsrs, Vector<FPRReg>&& fprs, Vector<GPRReg>&& scratches, RegisterSet&& calleeSaves)
        : jsrArgs(WTF::move(jsrs))
        , fprArgs(WTF::move(fprs))
        , prologueScratchGPRs(WTF::move(scratches))
        , calleeSaveRegisters(calleeSaves)
    { }

    WTF_MAKE_NONCOPYABLE(WasmCallingConvention);

private:
    template <typename RegType>
    ArgumentLocation marshallRegs(const Vector<RegType>& regArgs, size_t& count, size_t valueSize, Width width) const
    {
        if constexpr (std::is_same<RegType, JSValueRegs>::value) {
            JSValueRegs jsr = regArgs[count++];
#if USE(JSVALUE32_64)
                if (valueSize == 4)
                    return ArgumentLocation { ValueLocation { jsr }, width , Width32 };
#else
                UNUSED_PARAM(valueSize);
#endif
            return ArgumentLocation { ValueLocation { jsr }, width };
        } else
            return ArgumentLocation { ValueLocation { regArgs[count++] }, width };
    }

    template<typename RegType>
    ArgumentLocation marshallLocationImpl(CallRole role, const Vector<RegType>& regArgs, size_t& count, size_t& stackOffset, size_t valueSize) const
    {
        size_t alignedSize = WTF::roundUpToMultipleOf(valueSize, sizeof(Register));
        Width width = widthForBytes(alignedSize);

        if (count < regArgs.size())
            return marshallRegs(regArgs, count, valueSize, width);

        count++;
        ArgumentLocation result = { role == CallRole::Caller ? ValueLocation::stackArgument(stackOffset) : ValueLocation::stack(stackOffset), width };
        stackOffset += alignedSize;
        return result;
    }

    ArgumentLocation marshallLocation(CallRole role, Type valueType, size_t& gpArgumentCount, size_t& fpArgumentCount, size_t& stackOffset) const
    {
        ASSERT(isValueType(valueType));
        unsigned valueSize = bytesForWidth(valueType.width());
        switch (valueType.kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Funcref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
            return marshallLocationImpl(role, jsrArgs, gpArgumentCount, stackOffset, valueSize);
        case TypeKind::F32:
        case TypeKind::F64:
        case TypeKind::V128:
            return marshallLocationImpl(role, fprArgs, fpArgumentCount, stackOffset, valueSize);
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

public:
    CallInformation callInformationFor(const RTT& signature, CallRole role = CallRole::Caller) const
    {
        size_t gpArgumentCount = 0;
        size_t fpArgumentCount = 0;
        size_t headerSize = headerSizeInBytes;
        if (role == CallRole::Caller)
            headerSize -= sizeof(CallerFrameAndPC);

        ArgumentLocation thisArgument = { role == CallRole::Caller ? ValueLocation::stackArgument(headerSize) : ValueLocation::stack(headerSize), widthForBytes(sizeof(void*)) };
        headerSize += sizeof(Register); // thisArgument

        size_t argStackOffset = headerSize;
        Vector<ArgumentLocation, 8> params(signature.argumentCount(),
            [&](unsigned index) {
                return marshallLocation(role, signature.argumentType(index), gpArgumentCount, fpArgumentCount, argStackOffset);
            });

        gpArgumentCount = 0;
        fpArgumentCount = 0;
        size_t resultStackOffset = headerSize;
        Vector<ArgumentLocation, 1> results(signature.returnCount(),
            [&](unsigned index) {
                return marshallLocation(role, signature.returnType(index), gpArgumentCount, fpArgumentCount, resultStackOffset);
            });

        ASSERT(!(headerSize % stackAlignmentBytes()));
        size_t totalFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(std::max(argStackOffset, resultStackOffset));

        return { thisArgument, WTF::move(params), WTF::move(results), totalFrameSize, headerSize };
    }

    RegisterSet argumentGPRs() const { return RegisterSet::argumentGPRs(); }

    const Vector<JSValueRegs> jsrArgs;
    const Vector<FPRReg> fprArgs;
    const Vector<GPRReg> prologueScratchGPRs;
    const RegisterSet calleeSaveRegisters;
};

class JSCallingConvention {
public:
    static constexpr unsigned headerSizeInBytes = CallFrame::headerSizeInRegisters * sizeof(Register);

    JSCallingConvention(Vector<JSValueRegs>&& gprs, Vector<FPRReg>&& fprs, RegisterSet&& calleeSaves)
        : jsrArgs(WTF::move(gprs))
        , fprArgs(WTF::move(fprs))
        , calleeSaveRegisters(calleeSaves)
    { }

    WTF_MAKE_NONCOPYABLE(JSCallingConvention);
private:
    template <typename RegType>
    ArgumentLocation marshallLocationImpl(CallRole role, const Vector<RegType>& regArgs, size_t& count, size_t& stackOffset) const
    {
        if (count < regArgs.size())
            return ArgumentLocation { ValueLocation { regArgs[count++] }, Width64 };

        count++;
        ArgumentLocation result = { role == CallRole::Caller ? ValueLocation::stackArgument(stackOffset) : ValueLocation::stack(stackOffset), Width64 };
        stackOffset += sizeof(Register);
        return result;
    }

    ArgumentLocation marshallLocation(CallRole role, Type valueType, size_t& gpArgumentCount, size_t& fpArgumentCount, size_t& stackOffset) const
    {
        ASSERT(isValueType(valueType));
        switch (valueType.kind) {
        case TypeKind::I32:
        case TypeKind::I64:
        case TypeKind::Funcref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::Ref:
        case TypeKind::RefNull:
            return marshallLocationImpl(role, jsrArgs, gpArgumentCount, stackOffset);
        case TypeKind::F32:
        case TypeKind::F64:
            return marshallLocationImpl(role, fprArgs, fpArgumentCount, stackOffset);
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

public:
    CallInformation callInformationFor(const RTT& signature, CallRole role = CallRole::Callee) const
    {
        size_t gpArgumentCount = 0;
        size_t fpArgumentCount = 0;
        size_t stackOffset = headerSizeInBytes;
        if (role == CallRole::Caller)
            stackOffset -= sizeof(CallerFrameAndPC);

        ArgumentLocation thisArgument = { role == CallRole::Caller ? ValueLocation::stackArgument(stackOffset) : ValueLocation::stack(stackOffset), widthForBytes(sizeof(void*)) };
        stackOffset += sizeof(Register);
        size_t headerSize = stackOffset;

        Vector<ArgumentLocation, 8> params(signature.argumentCount(),
            [&](unsigned index) {
                return marshallLocation(role, signature.argumentType(index), gpArgumentCount, fpArgumentCount, stackOffset);
            });
        Vector<ArgumentLocation, 1> results { ArgumentLocation { ValueLocation { JSRInfo::returnValueJSR }, Width64 } };
        return { thisArgument, WTF::move(params), WTF::move(results), stackOffset, headerSize };
    }

    const Vector<JSValueRegs> jsrArgs;
    const Vector<FPRReg> fprArgs;
    const RegisterSet calleeSaveRegisters;
};

const JSCallingConvention& jsCallingConvention();
const WasmCallingConvention& wasmCallingConvention();

#if CPU(ARM_THUMB2)

class CCallingConventionArmThumb2 {
public:
    static constexpr unsigned headerSizeInBytes = 0;

    CCallingConventionArmThumb2(Vector<GPRReg>&& gprs, Vector<FPRReg>&& fprs, Vector<GPRReg>&& scratches, RegisterSet&& calleeSaves)
        : gprArgs(WTF::move(gprs))
        , fprArgs(WTF::move(fprs))
        , prologueScratchGPRs(WTF::move(scratches))
        , calleeSaveRegisters(calleeSaves)
    { }

    WTF_MAKE_NONCOPYABLE(CCallingConventionArmThumb2);

private:
    ArgumentLocation marshallLocationImplGPReg(CallRole role, const Vector<GPRReg>& regArgs, size_t& count, size_t& stackOffset, size_t valueSize) const
    {
        if (count < regArgs.size())
            return ArgumentLocation { ValueLocation { JSValueRegs::payloadOnly(regArgs[count++]) }, widthForBytes(valueSize) };

        count++;
        ArgumentLocation result = { role == CallRole::Caller ? ValueLocation::stackArgument(stackOffset) : ValueLocation::stack(stackOffset), widthForBytes(valueSize) };
        stackOffset += valueSize;
        return result;
    }
    ArgumentLocation marshallLocationImplGPRegPair(CallRole role, const Vector<GPRReg>& regArgs, size_t& count, size_t& stackOffset, size_t valueSize) const
    {
        count = WTF::roundUpToMultipleOf(2, count);
        if (count+1 < regArgs.size()) {
            auto payloadReg = regArgs[count];
            auto tagReg = regArgs[count+1];
            count += 2;
            return ArgumentLocation { ValueLocation { JSValueRegs::withTwoAvailableRegs(tagReg, payloadReg) }, widthForBytes(valueSize) };
        }

        count += 2;
        stackOffset = WTF::roundUpToMultipleOf(valueSize, stackOffset);
        ArgumentLocation result = { role == CallRole::Caller ? ValueLocation::stackArgument(stackOffset) : ValueLocation::stack(stackOffset), widthForBytes(valueSize) };
        stackOffset += valueSize;
        return result;
    }
    ArgumentLocation marshallLocationImplFPReg(CallRole role, const Vector<FPRReg>& regArgs, size_t& count, size_t& stackOffset, size_t valueSize) const
    {
        if (count < regArgs.size())
            return ArgumentLocation { ValueLocation { regArgs[count++] }, widthForBytes(valueSize) };

        count++;
        stackOffset = WTF::roundUpToMultipleOf(valueSize, stackOffset);
        ArgumentLocation result = { role == CallRole::Caller ? ValueLocation::stackArgument(stackOffset) : ValueLocation::stack(stackOffset), widthForBytes(valueSize) };
        stackOffset += valueSize;
        return result;
    }

    ArgumentLocation marshallLocation(CallRole role, Type valueType, size_t& gpArgumentCount, size_t& fpArgumentCount, size_t& stackOffset) const
    {
        ASSERT(isValueType(valueType));
        unsigned alignedWidth = bytesForWidth(valueType.width());
        switch (valueType.kind) {
        case TypeKind::I64:
        case TypeKind::Funcref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            return marshallLocationImplGPRegPair(role, gprArgs, gpArgumentCount, stackOffset, alignedWidth);
        case TypeKind::I32:
            return marshallLocationImplGPReg(role, gprArgs, gpArgumentCount, stackOffset, alignedWidth);
        case TypeKind::F32:
        case TypeKind::F64:
            return marshallLocationImplFPReg(role, fprArgs, fpArgumentCount, stackOffset, alignedWidth);
        default:
            break;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

public:
    uint32_t numberOfStackResults(const RTT& signature) const
    {
        const uint32_t gprCount = gprArgs.size();
        const uint32_t fprCount = fprArgs.size();
        uint32_t gprIndex = 0;
        uint32_t fprIndex = 0;
        uint32_t stackCount = 0;
        for (uint32_t i = 0; i < signature.returnCount(); i++) {
            switch (signature.returnType(i).kind) {
            case TypeKind::I64:
            case TypeKind::Funcref:
            case TypeKind::Exnref:
            case TypeKind::Externref:
            case TypeKind::RefNull:
            case TypeKind::Ref:
                if (gprIndex < gprCount)
                    gprIndex += 2;
                else
                    stackCount += 2;
                break;
            case TypeKind::I32:
                if (gprIndex < gprCount)
                    ++gprIndex;
                else
                    ++stackCount;
                break;
            case TypeKind::F32:
            case TypeKind::F64:
                if (fprIndex < fprCount)
                    ++fprIndex;
                else
                    ++stackCount;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        return stackCount;
    }

    uint32_t numberOfStackArguments(const RTT& signature) const
    {
        const uint32_t gprCount = gprArgs.size();
        const uint32_t fprCount = fprArgs.size();
        uint32_t gprIndex = 0;
        uint32_t fprIndex = 0;
        uint32_t stackCount = 0;
        for (uint32_t i = 0; i < signature.argumentCount(); i++) {
            switch (signature.argumentType(i).kind) {
            case TypeKind::I64:
            case TypeKind::Funcref:
            case TypeKind::Exnref:
            case TypeKind::Externref:
            case TypeKind::RefNull:
            case TypeKind::Ref:
                gprIndex = WTF::roundUpToMultipleOf(2, gprIndex);
                stackCount = WTF::roundUpToMultipleOf(2, stackCount);
                if (gprIndex < gprCount)
                    gprIndex += 2;
                else
                    stackCount += 2;
                break;
            case TypeKind::I32:
                if (gprIndex < gprCount)
                    ++gprIndex;
                else
                    ++stackCount;
                break;
            case TypeKind::F32:
            case TypeKind::F64:
                if (fprIndex < fprCount)
                    ++fprIndex;
                else
                    ++stackCount;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        return stackCount;
    }

    CallInformation callInformationFor(const RTT& signature, CallRole role = CallRole::Caller) const
    {
        size_t gpArgumentCount = 0;
        size_t fpArgumentCount = 0;
        size_t headerSize = headerSizeInBytes;
        if (role == CallRole::Caller)
            headerSize -= sizeof(CallerFrameAndPC);

        ArgumentLocation thisArgument = { role == CallRole::Caller ? ValueLocation::stackArgument(headerSize) : ValueLocation::stack(headerSize), widthForBytes(sizeof(void*)) };
        headerSize += sizeof(Register);

        size_t argStackOffset = headerSize;
        Vector<ArgumentLocation, 8> params(signature.argumentCount(),
            [&](unsigned index) {
                auto argumentType = signature.argumentType(index);
                ASSERT(!argumentType.isV128());
                return marshallLocation(role, argumentType, gpArgumentCount, fpArgumentCount, argStackOffset);
            });

        gpArgumentCount = 0;
        fpArgumentCount = 0;
        size_t resultStackOffset = headerSize;
        Vector<ArgumentLocation, 1> results(signature.returnCount(),
            [&](unsigned index) {
                ASSERT(!signature.returnType(index).isV128());
                return marshallLocation(role, signature.returnType(index), gpArgumentCount, fpArgumentCount, resultStackOffset);
            });
        size_t totalFrameSize = headerSize + WTF::roundUpToMultipleOf<stackAlignmentBytes()>(std::max(argStackOffset - headerSize, resultStackOffset - headerSize));
        return { thisArgument, WTF::move(params), WTF::move(results), totalFrameSize, headerSize };
    }

    const Vector<GPRReg> gprArgs;
    const Vector<FPRReg> fprArgs;
    const Vector<GPRReg> prologueScratchGPRs;
    const RegisterSet calleeSaveRegisters;
};

const CCallingConventionArmThumb2& cCallingConventionArmThumb2();
#endif

} } // namespace JSC::Wasm

namespace WTF {

template<>
struct VectorTraits<JSC::Wasm::ArgumentLocation> : VectorTraitsBase<false, JSC::Wasm::ValueLocation> {
    static constexpr bool canInitializeWithMemset = true;
    static constexpr bool canMoveWithMemcpy = true;
    static constexpr bool canCopyWithMemcpy = true;
};

} // namespace WTF

#endif // ENABLE(WEBASSEMBLY)
