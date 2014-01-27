/*
 * Copyright (C) 2014 Apple Inc. All Rights Reserved.
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
#include "UnlinkedInstructionStream.h"

namespace JSC {

// Unlinked instructions are packed in a simple stream format.
//
// The first byte is always the opcode.
// It's followed by an opcode-dependent number of argument values.
// The first 3 bits of each value determines the format:
//
//     5-bit positive integer (1 byte total)
//     5-bit negative integer (1 byte total)
//     13-bit positive integer (2 bytes total)
//     13-bit negative integer (2 bytes total)
//     5-bit constant register index, based at 0x40000000 (1 byte total)
//     13-bit constant register index, based at 0x40000000 (2 bytes total)
//     32-bit raw value (5 bytes total)

enum PackedValueType {
    Positive5Bit = 0,
    Negative5Bit,
    Positive13Bit,
    Negative13Bit,
    ConstantRegister5Bit,
    ConstantRegister13Bit,
    Full32Bit
};

UnlinkedInstructionStream::Reader::Reader(const UnlinkedInstructionStream& stream)
    : m_stream(stream)
    , m_index(0)
{
}

inline unsigned char UnlinkedInstructionStream::Reader::read8()
{
    return m_stream.m_data.data()[m_index++];
}

inline unsigned UnlinkedInstructionStream::Reader::read32()
{
    const unsigned char* data = &m_stream.m_data.data()[m_index];
    unsigned char type = data[0] >> 5;

    switch (type) {
    case Positive5Bit:
        m_index++;
        return data[0];
    case Negative5Bit:
        m_index++;
        return 0xffffffe0 | data[0];
    case Positive13Bit:
        m_index += 2;
        return ((data[0] & 0x1F) << 8) | data[1];
    case Negative13Bit:
        m_index += 2;
        return 0xffffe000 | ((data[0] & 0x1F) << 8) | data[1];
    case ConstantRegister5Bit:
        m_index++;
        return 0x40000000 | (data[0] & 0x1F);
    case ConstantRegister13Bit:
        m_index += 2;
        return 0x40000000 | ((data[0] & 0x1F) << 8) | data[1];
    default:
        ASSERT(type == Full32Bit);
        m_index += 5;
        return data[1] | data[2] << 8 | data[3] << 16 | data[4] << 24;
    }
}

const UnlinkedInstruction* UnlinkedInstructionStream::Reader::next()
{
    m_unpackedBuffer[0].u.opcode = static_cast<OpcodeID>(read8());
    unsigned opLength = opcodeLength(m_unpackedBuffer[0].u.opcode);
    for (unsigned i = 1; i < opLength; ++i)
        m_unpackedBuffer[i].u.index = read32();
    return m_unpackedBuffer;
}

static void append8(unsigned char*& ptr, unsigned char value)
{
    *(ptr++) = value;
}

static void append32(unsigned char*& ptr, unsigned value)
{
    if (!(value & 0xffffffe0)) {
        *(ptr++) = value;
        return;
    }

    if ((value & 0xffffffe0) == 0xffffffe0) {
        *(ptr++) = (Negative5Bit << 5) | (value & 0x1f);
        return;
    }

    if ((value & 0xffffffe0) == 0x40000000) {
        *(ptr++) = (ConstantRegister5Bit << 5) | (value & 0x1f);
        return;
    }

    if (!(value & 0xffffe000)) {
        *(ptr++) = (Positive13Bit << 5) | ((value >> 8) & 0x1f);
        *(ptr++) = value & 0xff;
        return;
    }

    if ((value & 0xffffe000) == 0xffffe000) {
        *(ptr++) = (Negative13Bit << 5) | ((value >> 8) & 0x1f);
        *(ptr++) = value & 0xff;
        return;
    }

    if ((value & 0xffffe000) == 0x40000000) {
        *(ptr++) = (ConstantRegister13Bit << 5) | ((value >> 8) & 0x1f);
        *(ptr++) = value & 0xff;
        return;
    }

    *(ptr++) = Full32Bit << 5;
    *(ptr++) = value & 0xff;
    *(ptr++) = (value >> 8) & 0xff;
    *(ptr++) = (value >> 16) & 0xff;
    *(ptr++) = (value >> 24) & 0xff;
}

UnlinkedInstructionStream::UnlinkedInstructionStream(const Vector<UnlinkedInstruction>& instructions)
    : m_instructionCount(instructions.size())
{
    Vector<unsigned char> buffer;

    // Reserve enough space up front so we never have to reallocate when appending.
    buffer.resizeToFit(m_instructionCount * 5);
    unsigned char* ptr = buffer.data();

    const UnlinkedInstruction* instructionsData = instructions.data();
    for (unsigned i = 0; i < m_instructionCount;) {
        const UnlinkedInstruction* pc = &instructionsData[i];
        OpcodeID opcode = pc[0].u.opcode;
        append8(ptr, opcode);

        unsigned opLength = opcodeLength(opcode);

        for (unsigned j = 1; j < opLength; ++j)
            append32(ptr, pc[j].u.index);

        i += opLength;
    }

    buffer.shrink(ptr - buffer.data());
    m_data = RefCountedArray<unsigned char>(buffer);
}

#ifndef NDEBUG
const RefCountedArray<UnlinkedInstruction>& UnlinkedInstructionStream::unpackForDebugging() const
{
    if (!m_unpackedInstructionsForDebugging.size()) {
        m_unpackedInstructionsForDebugging = RefCountedArray<UnlinkedInstruction>(m_instructionCount);

        Reader instructionReader(*this);
        for (unsigned i = 0; !instructionReader.atEnd(); ) {
            const UnlinkedInstruction* pc = instructionReader.next();
            unsigned opLength = opcodeLength(pc[0].u.opcode);
            for (unsigned j = 0; j < opLength; ++j)
                m_unpackedInstructionsForDebugging[i++] = pc[j];
        }
    }

    return m_unpackedInstructionsForDebugging;
}
#endif

}

