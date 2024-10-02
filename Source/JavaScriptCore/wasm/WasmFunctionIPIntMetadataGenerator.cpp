
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

namespace JSC {

namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FunctionIPIntMetadataGenerator);

unsigned FunctionIPIntMetadataGenerator::addSignature(const TypeDefinition& signature)
{
    unsigned index = m_signatures.size();
    m_signatures.append(&signature);
    return index;
}

void FunctionIPIntMetadataGenerator::addLength(size_t length)
{
    IPInt::InstructionLengthMetadata instructionLength {
        .length = safeCast<uint8_t>(length)
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(instructionLength));
    WRITE_TO_METADATA(m_metadata.data() + size, instructionLength, IPInt::InstructionLengthMetadata);
}

void FunctionIPIntMetadataGenerator::addLEB128ConstantInt32AndLength(uint32_t value, size_t length)
{
    IPInt::Const32Metadata mdConst {
        .instructionLength = { .length = safeCast<uint8_t>(length) },
        .value = value
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(mdConst));
    WRITE_TO_METADATA(m_metadata.data() + size, mdConst, IPInt::Const32Metadata);
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
            WRITE_TO_METADATA(m_metadata.data() + size, mdConst, IPInt::InstructionLengthMetadata);
        } else {
            IPInt::Const32Metadata mdConst {
                .instructionLength = { .length = safeCast<uint8_t>(length) },
                .value = static_cast<uint32_t>(value)
            };
            m_metadata.grow(size + sizeof(mdConst));
            WRITE_TO_METADATA(m_metadata.data() + size, mdConst, IPInt::Const32Metadata);
        }
    } else if (type.isI64()) {
        size_t size = m_metadata.size();
        IPInt::Const64Metadata mdConst {
            .instructionLength = { .length = safeCast<uint8_t>(length) },
            .value = static_cast<uint64_t>(value)
        };
        m_metadata.grow(size + sizeof(mdConst));
        WRITE_TO_METADATA(m_metadata.data() + size, mdConst, IPInt::Const64Metadata);
    } else if (type.isRef() || type.isRefNull() || type.isFuncref()) {
        size_t size = m_metadata.size();
        IPInt::Const32Metadata mdConst {
            .instructionLength = { .length = safeCast<uint8_t>(length) },
            .value = static_cast<uint32_t>(value)
        };
        m_metadata.grow(size + sizeof(mdConst));
        WRITE_TO_METADATA(m_metadata.data() + size, mdConst, IPInt::Const32Metadata);
    } else if (!type.isF32() && !type.isF64())
        ASSERT_NOT_IMPLEMENTED_YET();
}

void FunctionIPIntMetadataGenerator::addLEB128V128Constant(v128_t value, size_t length)
{
    IPInt::Const128Metadata mdConst {
        .instructionLength = { .length = safeCast<uint8_t>(length) },
        .value = value
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(mdConst));
    WRITE_TO_METADATA(m_metadata.data() + size, mdConst, IPInt::Const128Metadata);
}

void FunctionIPIntMetadataGenerator::addReturnData(const FunctionSignature& sig)
{
    auto fprToIndex = [&](FPRReg r) -> unsigned {
        for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i) {
            if (FPRInfo::toArgumentRegister(i) == r)
                return i;
        }
        RELEASE_ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
        return 0;
    };

    CallInformation returnCC = wasmCallingConvention().callInformationFor(sig, CallRole::Callee);
    m_uINTBytecode.reserveInitialCapacity(sig.returnCount() + 1);
    // uINT: the interpreter smaller than mINT
    // 0x00-0x07: r0 - r7
    // 0x08-0x0f: f0 - f7
    // 0x10: stack
    // 0x11: return

    constexpr static int NUM_UINT_GPRS = 8;
    constexpr static int NUM_UINT_FPRS = 8;
    ASSERT_UNUSED(NUM_UINT_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_UINT_GPRS);
    ASSERT_UNUSED(NUM_UINT_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_UINT_FPRS);

    for (size_t i = 0; i < sig.returnCount(); ++i) {
        auto loc = returnCC.results[i].location;

        if (loc.isGPR()) {
            ASSERT_UNUSED(NUM_UINT_GPRS, loc.jsr().payloadGPR() < NUM_UINT_GPRS);
            m_uINTBytecode.append(loc.jsr().payloadGPR());
        } else if (loc.isFPR()) {
            ASSERT_UNUSED(NUM_UINT_FPRS, fprToIndex(loc.fpr()) < NUM_UINT_FPRS);
            m_uINTBytecode.append(0x08 + fprToIndex(loc.fpr()));
        } else if (loc.isStack()) {
            m_highestReturnStackOffset = loc.offsetFromFP();
            m_uINTBytecode.append(0x10);
        }
    }
    m_uINTBytecode.reverse();
    m_uINTBytecode.append(0x11);
}

} }

#endif // ENABLE(WEBASSEMBLY)
