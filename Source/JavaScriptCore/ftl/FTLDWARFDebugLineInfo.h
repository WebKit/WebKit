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

#ifndef FTLDWARFDebugLineInfo_h
#define FTLDWARFDebugLineInfo_h

#if ENABLE(FTL_JIT)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace FTL {

class DebugLineInterpreter {
public:
    DebugLineInterpreter(const char* program);

    void run();

private:
    struct FileEntry {
        String name;
        uint32_t directoryIndex;
        uint32_t lastModified;
        uint32_t size;
    };

    void resetInterpreterState();
    void interpretStatementProgram();
    bool interpretOpcode(const char*&);
    void parsePrologue();
    void parseIncludeDirectories(const char*&);
    void parseFileEntries(const char*&);
    bool parseFileEntry(const char*&, DebugLineInterpreter::FileEntry&);
    uint32_t parseULEB128(const char*&);
    int32_t parseSLEB128(const char*&);

    const char* m_program;
    bool m_logResults;

    struct LineInfo {
        size_t address;
        uint32_t file;
        uint32_t line;
        uint32_t column;
        uint32_t isa;
        bool isStatement;
        bool isBasicBlock;
        bool endSequence;
        bool prologueEnd;
        bool epilogueBegin;
    };
    
    void printLineInfo(LineInfo&);
    void printLineInfo();

    LineInfo m_currentState;
    Vector<LineInfo> m_lineInfoMatrix;

    enum DwarfFormat {
        SixtyFourBit,
        ThirtyTwoBit
    };

    struct Prologue {
        Prologue()
            : totalLength(0)
            , format(ThirtyTwoBit)
            , version(0)
            , prologueLength(0)
            , minimumInstructionLength(0)
            , defaultIsStatement(0)
            , lineBase(0)
            , lineRange(0)
            , opcodeBase(0)
        {
        }

        uint64_t totalLength;
        DwarfFormat format;
        uint16_t version;
        uint64_t prologueLength;
        uint8_t minimumInstructionLength;
        uint8_t defaultIsStatement;
        int8_t lineBase;
        uint8_t lineRange;
        uint8_t opcodeBase;
        Vector<uint8_t> standardOpcodeLengths;
        Vector<String> includeDirectories;
        Vector<FileEntry> fileEntries;
    } m_prologue;

    enum ExtendedOpcode {
        DW_LNE_end_sequence = 0x1,
        DW_LNE_set_address  = 0x2,
        DW_LNE_define_file  = 0x3
    };

    enum StandardOpcode {
        ExtendedOpcodes             = 0x0,
        DW_LNS_copy                 = 0x1,
        DW_LNS_advance_pc           = 0x2,
        DW_LNS_advance_line         = 0x3,
        DW_LNS_set_file             = 0x4,
        DW_LNS_set_column           = 0x5,
        DW_LNS_negate_stmt          = 0x6,
        DW_LNS_set_basic_block      = 0x7,
        DW_LNS_const_add_pc         = 0x8,
        DW_LNS_fixed_advance_pc     = 0x9,
        DW_LNS_set_prologue_end     = 0xa,
        DW_LNS_set_epilogue_begin   = 0xb,
        DW_LNS_set_isa              = 0xc
    };
};

} } // namespace JSC::FTL
    
#endif // ENABLE(FTL_JIT)

#endif // FTLDWARFDebugLineInfo_h
