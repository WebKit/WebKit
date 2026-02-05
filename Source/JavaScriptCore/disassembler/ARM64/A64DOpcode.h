/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include <stdint.h>
#include <wtf/Assertions.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace ARM64Disassembler {

class A64DOpcode {
public:
    A64DOpcode(uint32_t* startPC = nullptr, uint32_t* endPC = nullptr)
        : m_startPC(startPC)
        , m_endPC(endPC)
        , m_currentPC(nullptr)
        , m_bufferOffset(0)
        , m_builtConstant(0)
        , m_moveWideDestReg(255)
    {
        m_formatBuffer[0] = '\0';
    }

    const char* disassemble(uint32_t* currentPC);

private:
    void bufferPrintf(const char* format, ...) WTF_ATTRIBUTE_PRINTF(2, 3);

    // Append PC-relative target address with JSC-specific metadata
    void appendPCRelativeOffset(uint32_t* pc, int32_t immediate);

    // MoveWide constant tracking across instruction sequences
    void trackMoveWideConstant(int category, int64_t immediate, uint8_t shiftAmount, uint8_t destRegister, uint8_t is64Bit);
    void maybeAnnotateBuiltConstant();

    // JSC pointer analysis
    bool handlePotentialDataPointer(void* ptr);
#if CPU(ARM64E)
    bool handlePotentialPtrTag(uintptr_t value);
#endif

    static constexpr int bufferSize = 512;

    char m_formatBuffer[bufferSize];
    uint32_t* m_startPC;
    uint32_t* m_endPC;
    uint32_t* m_currentPC;
    int m_bufferOffset;
    uintptr_t m_builtConstant;
    uint8_t m_moveWideDestReg;
};

} } // namespace JSC::ARM64Disassembler

using JSC::ARM64Disassembler::A64DOpcode;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
