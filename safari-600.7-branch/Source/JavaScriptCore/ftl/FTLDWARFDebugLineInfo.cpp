/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FTLDWARFDebugLineInfo.h"

#if ENABLE(FTL_JIT)

#include <wtf/DataLog.h>

namespace JSC { namespace FTL {

DebugLineInterpreter::DebugLineInterpreter(const char* program)
    : m_program(program)
    , m_logResults(false)
{
    resetInterpreterState();
}

template <typename T> inline T read(const char*& program)
{
    T result = *reinterpret_cast<const T*>(program);
    program += sizeof(T);
    return result;
}

uint32_t DebugLineInterpreter::parseULEB128(const char*& offset)
{
    uint32_t result = 0;
    uint8_t byte;
    unsigned shiftAmount = 0;
    do {
        byte = read<uint8_t>(offset);
        result |= (byte & ~0x80) << shiftAmount;
        shiftAmount += 7;
    } while (byte & 0x80);
    return result;
}

int32_t DebugLineInterpreter::parseSLEB128(const char*& offset)
{
    int32_t result = 0;
    uint8_t byte;
    unsigned shiftAmount = 0;
    do {
        byte = read<uint8_t>(offset);
        result |= (byte & ~0x80) << shiftAmount;
        shiftAmount += 7;
    } while (byte & 0x80);
    
    // If the sign bit (in this case, the second MSB) on the last byte is set we need to zero extend.
    if (byte & 0x40)
        result |= -(1 << shiftAmount);
    return result;
}

void DebugLineInterpreter::run()
{
    parsePrologue();
    interpretStatementProgram();
    if (m_logResults)
        printLineInfo();
}

void DebugLineInterpreter::parsePrologue()
{
    const char* currentProgramOffset = m_program;
    m_prologue.totalLength = read<uint32_t>(currentProgramOffset);
    if (m_prologue.totalLength == 0xffffffff) {
        // This is 64-bit DWARF format.
        m_prologue.format = SixtyFourBit;
        m_prologue.totalLength = read<uint64_t>(currentProgramOffset);
    } else
        m_prologue.format = ThirtyTwoBit;
    m_prologue.version = read<uint16_t>(currentProgramOffset);
    
    if (m_prologue.format == ThirtyTwoBit)
        m_prologue.prologueLength = read<uint32_t>(currentProgramOffset);
    else
        m_prologue.prologueLength = read<uint64_t>(currentProgramOffset);
    const char* afterLengthOffset = currentProgramOffset;
    
    m_prologue.minimumInstructionLength = read<uint8_t>(currentProgramOffset);
    m_prologue.defaultIsStatement = read<uint8_t>(currentProgramOffset);
    m_prologue.lineBase = read<int8_t>(currentProgramOffset);
    m_prologue.lineRange = read<uint8_t>(currentProgramOffset);
    m_prologue.opcodeBase = read<uint8_t>(currentProgramOffset);
    for (unsigned i = 1; i < m_prologue.opcodeBase; ++i)
        m_prologue.standardOpcodeLengths.append(read<uint8_t>(currentProgramOffset));
    parseIncludeDirectories(currentProgramOffset);
    parseFileEntries(currentProgramOffset);
    
    m_program = afterLengthOffset + m_prologue.prologueLength;

    if (!m_logResults)
        return;

    dataLog("\nPrologue:\n");
    dataLog("totalLength = ", m_prologue.totalLength, "\n");
    dataLog("version = ", m_prologue.version, "\n");
    dataLog("prologueLength = ", m_prologue.prologueLength, "\n");
    dataLog("minimumInstructionLength = ", m_prologue.minimumInstructionLength, "\n");
    dataLog("defaultIsStatement = ", m_prologue.defaultIsStatement, "\n");
    dataLog("lineBase = ", m_prologue.lineBase, "\n");
    dataLog("lineRange = ", m_prologue.lineRange, "\n");
    dataLog("opcodeBase = ", m_prologue.opcodeBase, "\n");
    
    dataLog("\nStandard Opcode Lengths:\n");
    for (unsigned i = 1; i < m_prologue.opcodeBase; ++i)
        dataLog("standardOpcodeLengths[", i - 1, "] = ", m_prologue.standardOpcodeLengths[i - 1], "\n");
    
    dataLog("\nInclude Directories:\n");
    for (unsigned i = 0; i < m_prologue.includeDirectories.size(); ++i)
        dataLog("includeDirectories[", i, "] = ", m_prologue.includeDirectories[i], "\n");
    
    dataLog("\nFiles:\n");
    for (unsigned i = 0; i < m_prologue.fileEntries.size(); ++i) {
        FileEntry& entry = m_prologue.fileEntries[i];
        dataLog("fileEntries[", i, "] = {name: \"", entry.name, "\", dir_index: ", entry.directoryIndex, ", last_modified: ", entry.lastModified, ", size: ", entry.size, "}\n");
    }
}

void DebugLineInterpreter::parseIncludeDirectories(const char*& offset)
{
    size_t length = 0;
    while ((length = strlen(offset))) {
        m_prologue.includeDirectories.append(offset);
        offset += length + 1;
    }
    
    // Extra increment to get past the last null byte.
    offset += 1;
}

void DebugLineInterpreter::parseFileEntries(const char*& offset)
{
    while (true) {
        DebugLineInterpreter::FileEntry nextEntry;
        if (!parseFileEntry(offset, nextEntry))
            break;
        m_prologue.fileEntries.append(nextEntry);
    }
}

bool DebugLineInterpreter::parseFileEntry(const char*& offset, FileEntry& entry)
{
    size_t length = strlen(offset);
    if (!length) {
        offset += 1;
        return false;
    }
    entry.name = offset;
    offset += length + 1;
    entry.directoryIndex = parseULEB128(offset);
    entry.lastModified = parseULEB128(offset);
    entry.size = parseULEB128(offset);
    
    return true;
}

void DebugLineInterpreter::interpretStatementProgram()
{
    const char* currentProgramOffset = m_program;
    bool keepGoing = true;
    do {
        keepGoing = interpretOpcode(currentProgramOffset);
    } while (keepGoing);
}

bool DebugLineInterpreter::interpretOpcode(const char*& offset)
{
    uint8_t nextOpcode = read<uint8_t>(offset);
    switch (nextOpcode) {
    case ExtendedOpcodes: {
        uint32_t length = parseULEB128(offset);
        if (!length)
            return false;
        uint8_t extendedOpcode = read<uint8_t>(offset);
        switch (extendedOpcode) {
        case DW_LNE_end_sequence: {
            m_currentState.endSequence = true;
            m_lineInfoMatrix.append(m_currentState);
            resetInterpreterState();
            break;
        }
        case DW_LNE_set_address: {
            m_currentState.address = read<size_t>(offset);
            break;
        }
        case DW_LNE_define_file: {
            fprintf(stderr, "Unimplemented extended opcode DW_LNE_define_file.\n");
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        default: {
            fprintf(stderr, "Unknown extended opcode.\n");
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        }
        break;
    }
        /* Standard opcodes */
    case DW_LNS_copy: {
        m_lineInfoMatrix.append(m_currentState);
        m_currentState.isBasicBlock = false;
        m_currentState.prologueEnd = false;
        m_currentState.epilogueBegin = false;
        break;
    }
    case DW_LNS_advance_pc: {
        uint32_t advance = parseULEB128(offset);
        m_currentState.address += advance * m_prologue.minimumInstructionLength;
        break;
    }
    case DW_LNS_advance_line: {
        int32_t advance = parseSLEB128(offset);
        m_currentState.line += advance;
        break;
    }
    case DW_LNS_set_file: {
        uint32_t fileIndex = parseULEB128(offset);
        m_currentState.file = fileIndex;
        break;
    }
    case DW_LNS_set_column: {
        m_currentState.column = parseULEB128(offset);
        break;
    }
    case DW_LNS_negate_stmt: {
        m_currentState.isStatement = !m_currentState.isStatement;
        break;
    }
    case DW_LNS_set_basic_block: {
        m_currentState.isBasicBlock = true;
        break;
    }
    case DW_LNS_const_add_pc: {
        uint8_t adjustedOpcode = nextOpcode - m_prologue.opcodeBase;
        uint32_t addressIncrement = (adjustedOpcode / m_prologue.lineRange) * m_prologue.minimumInstructionLength;
        m_currentState.address += addressIncrement;
        break;
    }
    case DW_LNS_fixed_advance_pc: {
        uint16_t advance = read<uint16_t>(offset);
        m_currentState.address += advance;
        break;
    }
    case DW_LNS_set_prologue_end: {
        m_currentState.prologueEnd = true;
        break;
    }
    case DW_LNS_set_epilogue_begin: {
        m_currentState.epilogueBegin = true;
        break;
    }
    case DW_LNS_set_isa: {
        m_currentState.isa = parseULEB128(offset);
        break;
    }
        /* Special opcodes */
    default: {
        uint8_t adjustedOpcode = nextOpcode - m_prologue.opcodeBase;
        uint32_t addressIncrement = (adjustedOpcode / m_prologue.lineRange) * m_prologue.minimumInstructionLength;
        int32_t lineIncrement = m_prologue.lineBase + (adjustedOpcode % m_prologue.lineRange);
        m_currentState.address += addressIncrement;
        m_currentState.line += lineIncrement;
        m_lineInfoMatrix.append(m_currentState);
        m_currentState.isBasicBlock = false;
        m_currentState.prologueEnd = false;
        m_currentState.epilogueBegin = false;
        break;
    }
    }
    return true;
}

void DebugLineInterpreter::printLineInfo()
{
    dataLog("\nLine Info Matrix:\n");
    for (unsigned i = 0; i < m_lineInfoMatrix.size(); ++i)
        printLineInfo(m_lineInfoMatrix[i]);
    dataLog("\n");
}

void DebugLineInterpreter::printLineInfo(LineInfo& info)
{
    dataLogF("address: %p", reinterpret_cast<void*>(info.address));
    dataLog("  file: ", info.file, "  line: ", info.line, "  column: ", info.column, "  isa: ", info.isa, "  ");
    dataLog("  statement?: ", info.isStatement);
    dataLog("  basic block?: ", info.isBasicBlock);
    dataLog("  end sequence?: ", info.endSequence);
    dataLog("  prologue end?: ", info.prologueEnd);
    dataLog("  epilogue begin?: ", info.epilogueBegin);
    dataLog("\n");
}

void DebugLineInterpreter::resetInterpreterState()
{
    m_currentState.address = 0;
    m_currentState.file = 1;
    m_currentState.line = 1;
    m_currentState.column = 0;
    m_currentState.isa = 0;
    m_currentState.isStatement = false;
    m_currentState.isBasicBlock = false;
    m_currentState.endSequence = false;
    m_currentState.prologueEnd = false;
    m_currentState.epilogueBegin = false;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

