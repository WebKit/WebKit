
/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "WasmFunctionIPIntMetadataGenerator.h"

#include <numeric>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FunctionIPIntMetadataGenerator);

const RTT* FunctionIPIntMetadataGenerator::addSignature(const TypeDefinition& signature)
{
    // This is held by wasm module.
    return TypeInformation::getCanonicalRTT(signature.index()).unsafePtr();
}

void FunctionIPIntMetadataGenerator::setTailCall(uint32_t functionIndex, bool isImportedFunctionFromFunctionIndexSpace)
{
    m_hasTailCallSuccessors = true;
    m_tailCallSuccessors.set(functionIndex);
    if (isImportedFunctionFromFunctionIndexSpace)
        setTailCallClobbersInstance();
}

void FunctionIPIntMetadataGenerator::addLength(size_t length)
{
    IPInt::InstructionLengthMetadata instructionLength {
        .length = safeCast<uint8_t>(length)
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(instructionLength));
    WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, instructionLength, IPInt::InstructionLengthMetadata);
}

void FunctionIPIntMetadataGenerator::addLEB128ConstantInt32AndLength(uint32_t value, size_t length)
{
    IPInt::Const32Metadata mdConst {
        .instructionLength = { .length = safeCast<uint8_t>(length) },
        .value = value
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(mdConst));
    WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::Const32Metadata);
}

void FunctionIPIntMetadataGenerator::addLEB128ConstantInt64AndLength(uint64_t value, size_t length)
{
    IPInt::Const64Metadata mdConst {
        .value = value,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(mdConst));
    WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::Const64Metadata);
}

void FunctionIPIntMetadataGenerator::addLEB128ConstantAndLengthForType(Type type, uint64_t value, size_t length)
{
    if (type.isI32()) {
        size_t size = m_metadata.size();
        if (length == 2) {
            IPInt::InstructionLengthMetadata mdConst {
                .length = safeCast<uint8_t>((value >> 7) & 1)
            };
            m_metadata.grow(size + sizeof(mdConst));
            WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::InstructionLengthMetadata);
        } else {
            IPInt::Const32Metadata mdConst {
                .instructionLength = { .length = safeCast<uint8_t>(length) },
                .value = static_cast<uint32_t>(value)
            };
            m_metadata.grow(size + sizeof(mdConst));
            WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::Const32Metadata);
        }
    } else if (type.isI64()) {
        size_t size = m_metadata.size();
        IPInt::Const64Metadata mdConst {
            .value = static_cast<uint64_t>(value),
            .instructionLength = { .length = safeCast<uint8_t>(length) }
        };
        m_metadata.grow(size + sizeof(mdConst));
        WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::Const64Metadata);
    } else if (type.isRef() || type.isRefNull() || type.isFuncref()) {
        size_t size = m_metadata.size();
        IPInt::Const32Metadata mdConst {
            .instructionLength = { .length = safeCast<uint8_t>(length) },
            .value = static_cast<uint32_t>(value)
        };
        m_metadata.grow(size + sizeof(mdConst));
        WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::Const32Metadata);
    } else if (!type.isF32() && !type.isF64())
        ASSERT_NOT_IMPLEMENTED_YET();
}

void FunctionIPIntMetadataGenerator::addLEB128V128Constant(v128_t value, size_t length)
{
    IPInt::Const128Metadata mdConst {
        .value = value,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(mdConst));
    WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, mdConst, IPInt::Const128Metadata);
}

void FunctionIPIntMetadataGenerator::addReturnData(const FunctionSignature& sig, const CallInformation& returnCC)
{
    m_uINTBytecode.reserveInitialCapacity(sig.returnCount() + 1);
    // uINT: the interpreter smaller than mINT
    constexpr static int NUM_UINT_GPRS = 8;
    constexpr static int NUM_UINT_FPRS = 8;
    ASSERT_UNUSED(NUM_UINT_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_UINT_GPRS);
    ASSERT_UNUSED(NUM_UINT_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_UINT_FPRS);

    m_uINTBytecode.appendUsingFunctor(returnCC.results.size(),
        [&](unsigned index) -> uint8_t {
            const ArgumentLocation& argLoc = returnCC.results[index];
            const ValueLocation& loc = argLoc.location;

            if (loc.isGPR()) {
#if USE(JSVALUE64)
                ASSERT_UNUSED(NUM_UINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().gpr()) < NUM_UINT_GPRS);
                return static_cast<uint8_t>(IPInt::UIntBytecode::RetGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                ASSERT_UNUSED(NUM_UINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_UINT_GPRS);
                ASSERT_UNUSED(NUM_UINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().tagGPR()) < NUM_UINT_GPRS);
                return static_cast<uint8_t>(IPInt::UIntBytecode::RetGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord));
#endif
            }

            if (loc.isFPR()) {
                ASSERT_UNUSED(NUM_UINT_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_UINT_FPRS);
                return static_cast<uint8_t>(IPInt::UIntBytecode::RetFPR) + FPRInfo::toArgumentIndex(loc.fpr());
            }

            RELEASE_ASSERT(loc.isStack());
            m_topOfReturnStackFPOffset = loc.offsetFromFP() + bytesForWidth(argLoc.width);
            switch (argLoc.width) {
            case Width::Width64:
                return static_cast<uint8_t>(IPInt::UIntBytecode::Stack);
            case Width::Width128:
                return static_cast<uint8_t>(IPInt::UIntBytecode::StackVector);
            default:
                RELEASE_ASSERT_NOT_REACHED("No uINT bytecode for result width");
            }
        });

    m_uINTBytecode.reverse();
    m_uINTBytecode.append(static_cast<uint8_t>(IPInt::UIntBytecode::End));
}


} }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
