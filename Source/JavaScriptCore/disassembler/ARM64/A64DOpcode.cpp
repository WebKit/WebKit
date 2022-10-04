/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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

#if ENABLE(ARM64_DISASSEMBLER)

#include "A64DOpcode.h"

#include "Disassembler.h"
#include "ExecutableAllocator.h"
#include "GPRInfo.h"
#include "Integrity.h"
#include "JSCJSValue.h"
#include "LLIntPCRanges.h"
#include "PureNaN.h"
#include "VMInspector.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <wtf/PtrTag.h>
#include <wtf/Range.h>

namespace JSC { namespace ARM64Disassembler {

A64DOpcode::OpcodeGroup* A64DOpcode::opcodeTable[32];

const char* const A64DOpcode::s_conditionNames[16] = {
    "eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "ne"
};

const char* const A64DOpcode::s_optionName[8] = {
    "uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"
};

const char* const A64DOpcode::s_shiftNames[4] = {
    "lsl", "lsr", "asr", "ror"
};

const char A64DOpcode::s_FPRegisterPrefix[5] = {
    'b', 'h', 's', 'd', 'q'
};

struct OpcodeGroupInitializer {
    unsigned m_opcodeGroupNumber;
    uint32_t m_mask;
    uint32_t m_pattern;
    const char* (*m_format)(A64DOpcode*);
};

#define OPCODE_GROUP_ENTRY(groupIndex, groupClass) \
{ groupIndex, groupClass::mask, groupClass::pattern, groupClass::format }

static const OpcodeGroupInitializer opcodeGroupList[] = {
    OPCODE_GROUP_ENTRY(0x08, A64DOpcodeCAS),
    OPCODE_GROUP_ENTRY(0x08, A64DOpcodeLoadStoreRegisterPair),
    OPCODE_GROUP_ENTRY(0x08, A64DOpcodeLoadStoreExclusive),
    OPCODE_GROUP_ENTRY(0x09, A64DOpcodeLoadStoreRegisterPair),
    OPCODE_GROUP_ENTRY(0x0a, A64DOpcodeLogicalShiftedRegister),
    OPCODE_GROUP_ENTRY(0x0b, A64DOpcodeAddSubtractExtendedRegister),
    OPCODE_GROUP_ENTRY(0x0b, A64DOpcodeAddSubtractShiftedRegister),
    OPCODE_GROUP_ENTRY(0x0c, A64DOpcodeLoadStoreRegisterPair),
    OPCODE_GROUP_ENTRY(0x0d, A64DOpcodeLoadStoreRegisterPair),
    OPCODE_GROUP_ENTRY(0x11, A64DOpcodeAddSubtractImmediate),
    OPCODE_GROUP_ENTRY(0x12, A64DOpcodeMoveWide),
    OPCODE_GROUP_ENTRY(0x12, A64DOpcodeLogicalImmediate),
    OPCODE_GROUP_ENTRY(0x13, A64DOpcodeBitfield),
    OPCODE_GROUP_ENTRY(0x13, A64DOpcodeExtract),
    OPCODE_GROUP_ENTRY(0x14, A64DOpcodeUnconditionalBranchImmediate),
    OPCODE_GROUP_ENTRY(0x14, A64DOpcodeConditionalBranchImmediate),
    OPCODE_GROUP_ENTRY(0x14, A64DOpcodeCompareAndBranchImmediate),
    OPCODE_GROUP_ENTRY(0x14, A64OpcodeExceptionGeneration),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeUnconditionalBranchImmediate),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeConditionalBranchImmediate),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeCompareAndBranchImmediate),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeHint),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeSystemSync),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeMSRImmediate),
    OPCODE_GROUP_ENTRY(0x15, A64DOpcodeMSROrMRSRegister),
    OPCODE_GROUP_ENTRY(0x16, A64DOpcodeUnconditionalBranchImmediate),
    OPCODE_GROUP_ENTRY(0x16, A64DOpcodeUnconditionalBranchRegister),
    OPCODE_GROUP_ENTRY(0x16, A64DOpcodeTestAndBranchImmediate),
    OPCODE_GROUP_ENTRY(0x17, A64DOpcodeUnconditionalBranchImmediate),
    OPCODE_GROUP_ENTRY(0x17, A64DOpcodeUnconditionalBranchRegister),
    OPCODE_GROUP_ENTRY(0x17, A64DOpcodeTestAndBranchImmediate),
    OPCODE_GROUP_ENTRY(0x18, A64DOpcodeLoadStoreImmediate),
    OPCODE_GROUP_ENTRY(0x18, A64DOpcodeLoadStoreRegisterOffset),
    OPCODE_GROUP_ENTRY(0x18, A64DOpcodeLoadStoreAuthenticated),
    OPCODE_GROUP_ENTRY(0x18, A64DOpcodeLoadAtomic),
    OPCODE_GROUP_ENTRY(0x18, A64DOpcodeSwapAtomic),
    OPCODE_GROUP_ENTRY(0x19, A64DOpcodeLoadStoreUnsignedImmediate),
    OPCODE_GROUP_ENTRY(0x1a, A64DOpcodeConditionalSelect),
    OPCODE_GROUP_ENTRY(0x1a, A64DOpcodeDataProcessing1Source),
    OPCODE_GROUP_ENTRY(0x1a, A64DOpcodeDataProcessing2Source),
    OPCODE_GROUP_ENTRY(0x1b, A64DOpcodeDataProcessing3Source),
    OPCODE_GROUP_ENTRY(0x1c, A64DOpcodeLoadStoreImmediate),
    OPCODE_GROUP_ENTRY(0x1c, A64DOpcodeLoadStoreRegisterOffset),
    OPCODE_GROUP_ENTRY(0x1d, A64DOpcodeLoadStoreUnsignedImmediate),
    OPCODE_GROUP_ENTRY(0x1e, A64DOpcodeFloatingPointCompare),
    OPCODE_GROUP_ENTRY(0x1e, A64DOpcodeFloatingPointConditionalSelect),
    OPCODE_GROUP_ENTRY(0x1e, A64DOpcodeFloatingPointDataProcessing2Source),
    OPCODE_GROUP_ENTRY(0x1e, A64DOpcodeFloatingPointDataProcessing1Source),
    OPCODE_GROUP_ENTRY(0x1e, A64DOpcodeFloatingFixedPointConversions),
    OPCODE_GROUP_ENTRY(0x1e, A64DOpcodeFloatingPointIntegerConversions),
};

bool A64DOpcode::s_initialized = false;

void A64DOpcode::init()
{
    if (s_initialized)
        return;

    OpcodeGroup* lastGroups[32];

    for (unsigned i = 0; i < 32; i++) {
        opcodeTable[i] = 0;
        lastGroups[i] = 0;
    }

    for (unsigned i = 0; i < sizeof(opcodeGroupList) / sizeof(struct OpcodeGroupInitializer); i++) {
        OpcodeGroup* newOpcodeGroup = new OpcodeGroup(opcodeGroupList[i].m_mask, opcodeGroupList[i].m_pattern, opcodeGroupList[i].m_format);
        uint32_t opcodeGroupNumber = opcodeGroupList[i].m_opcodeGroupNumber;

        if (!opcodeTable[opcodeGroupNumber])
            opcodeTable[opcodeGroupNumber] = newOpcodeGroup;
        else
            lastGroups[opcodeGroupNumber]->setNext(newOpcodeGroup);
        lastGroups[opcodeGroupNumber] = newOpcodeGroup;
    }

    s_initialized = true;
}

void A64DOpcode::setPCAndOpcode(uint32_t* newPC, uint32_t newOpcode)
{
    m_currentPC = newPC;
    m_opcode = newOpcode;
    m_bufferOffset = 0;
    m_formatBuffer[0] = '\0';
}

const char* A64DOpcode::disassemble(uint32_t* currentPC)
{
    setPCAndOpcode(currentPC, *currentPC);

    OpcodeGroup* opGroup = opcodeTable[opcodeGroupNumber(m_opcode)];

    while (opGroup) {
        if (opGroup->matches(m_opcode))
            return opGroup->format(this);
        opGroup = opGroup->next();
    }

    return A64DOpcode::format();
}

void A64DOpcode::bufferPrintf(const char* format, ...)
{
    if (m_bufferOffset >= bufferSize)
        return;

    va_list argList;
    va_start(argList, format);

    m_bufferOffset += vsnprintf(m_formatBuffer + m_bufferOffset, bufferSize - m_bufferOffset, format, argList);

    va_end(argList);
}

const char* A64DOpcode::format()
{
    bufferPrintf("   .long  %08x", m_opcode);
    return m_formatBuffer;
}

void A64DOpcode::appendPCRelativeOffset(uint32_t* pc, int32_t immediate)
{
    uint32_t* targetPC = pc + immediate;
    constexpr size_t bufferSize = 101;
    char buffer[bufferSize];
    const char* targetInfo = buffer;
    if (!m_startPC)
        targetInfo = "";
    else if (targetPC >= m_startPC && targetPC < m_endPC)
        snprintf(buffer, bufferSize - 1, " -> <%u>", static_cast<unsigned>((targetPC - m_startPC) * sizeof(uint32_t)));
    else if (const char* label = labelFor(targetPC))
        snprintf(buffer, bufferSize - 1, " -> %s", label);
    else if (isJITPC(targetPC))
        targetInfo = " -> JIT PC";
    else if (LLInt::isLLIntPC(targetPC))
        targetInfo = " -> LLInt PC";
    else
        targetInfo = " -> <unknown>";

    bufferPrintf("0x%" PRIxPTR "%s", bitwise_cast<uintptr_t>(targetPC),  targetInfo);
}

void A64DOpcode::appendRegisterName(unsigned registerNumber, bool is64Bit)
{
    if (registerNumber == 29) {
        bufferPrintf(is64Bit ? "fp" : "wfp");
        return;
    }

    if (registerNumber == 30) {
        bufferPrintf(is64Bit ? "lr" : "wlr");
        return;
    }

    bufferPrintf("%c%u", is64Bit ? 'x' : 'w', registerNumber);
}

void A64DOpcode::appendFPRegisterName(unsigned registerNumber, unsigned registerSize)
{
    bufferPrintf("%c%u", FPRegisterPrefix(registerSize), registerNumber);
}

const char* const A64DOpcodeAddSubtract::s_opNames[4] = { "add", "adds", "sub", "subs" };

const char* A64DOpcodeAddSubtractImmediate::format()
{
    if (isCMP())
        appendInstructionName(cmpName());
    else {
        if (isMovSP())
            appendInstructionName("mov");
        else
            appendInstructionName(opName());
        appendSPOrRegisterName(rd(), is64Bit());
        appendSeparator();
    }
    appendSPOrRegisterName(rn(), is64Bit());

    if (!isMovSP()) {
        appendSeparator();
        appendUnsignedImmediate(immed12());
        if (shift()) {
            appendSeparator();
            appendString(shift() == 1 ? "lsl" : "reserved");
        }
    }
    return m_formatBuffer;
}

const char* A64DOpcodeAddSubtractExtendedRegister::format()
{
    if (immediate3() > 4)
        return A64DOpcode::format();

    if (isCMP())
        appendInstructionName(cmpName());
    else {
        appendInstructionName(opName());
        appendSPOrRegisterName(rd(), is64Bit());
        appendSeparator();
    }
    appendSPOrRegisterName(rn(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rm(), is64Bit() && ((option() & 0x3) == 0x3));
    appendSeparator();
    if (option() == 0x2 && ((rd() == 31) || (rn() == 31)))
        appendString("lsl");
    else
        appendString(optionName());
    if (immediate3()) {
        appendCharacter(' ');
        appendUnsignedImmediate(immediate3());
    }

    return m_formatBuffer;
}

const char* A64DOpcodeAddSubtractShiftedRegister::format()
{
    if (!is64Bit() && immediate6() & 0x20)
        return A64DOpcode::format();

    if (shift() == 0x3)
        return A64DOpcode::format();

    if (isCMP())
        appendInstructionName(cmpName());
    else {
        if (isNeg())
            appendInstructionName(cmpName());
        else
            appendInstructionName(opName());
        appendSPOrRegisterName(rd(), is64Bit());
        appendSeparator();
    }
    if (!isNeg()) {
        appendRegisterName(rn(), is64Bit());
        appendSeparator();
    }
    appendZROrRegisterName(rm(), is64Bit());
    if (immediate6()) {
        appendSeparator();
        appendShiftType(shift());
        appendUnsignedImmediate(immediate6());
    }

    return m_formatBuffer;
}

const char* const A64DOpcodeBitfield::s_opNames[3] = { "sbfm", "bfm", "ubfm" };
const char* const A64DOpcodeBitfield::s_extendPseudoOpNames[3][3] = {
    { "sxtb", "sxth", "sxtw" }, { 0, 0, 0} , { "uxtb", "uxth", "uxtw" } };
const char* const A64DOpcodeBitfield::s_insertOpNames[3] = { "sbfiz", "bfi", "ubfiz" };
const char* const A64DOpcodeBitfield::s_extractOpNames[3] = { "sbfx", "bfxil", "ubfx" };

const char* A64DOpcodeBitfield::format()
{
    if (opc() == 0x3)
        return A64DOpcode::format();

    if (is64Bit() != nBit())
        return A64DOpcode::format();

    if (!is64Bit() && ((immediateR() & 0x20) || (immediateS() & 0x20)))
        return A64DOpcode::format();

    if (!(opc() & 0x1) && !immediateR()) {
        // [un]signed {btye,half-word,word} extend
        bool isSTXType = false;
        if (immediateS() == 7) {
            appendInstructionName(extendPseudoOpNames(0));
            isSTXType = true;
        } else if (immediateS() == 15) {
            appendInstructionName(extendPseudoOpNames(1));
            isSTXType = true;
        } else if (immediateS() == 31 && is64Bit() && !opc()) {
            appendInstructionName(extendPseudoOpNames(2));
            isSTXType = true;
        }

        if (isSTXType) {
            appendRegisterName(rd(), is64Bit());
            appendSeparator();
            appendRegisterName(rn(), false);

            return m_formatBuffer;
        }
    }

    if (!(opc() & 0x1) && ((immediateS() & 0x1f) == 0x1f) && (is64Bit() == (immediateS() >> 5))) {
        // asr/lsr
        appendInstructionName(!opc() ? "asr" : "lsr");

        appendRegisterName(rd(), is64Bit());
        appendSeparator();
        appendRegisterName(rn(), is64Bit());
        appendSeparator();
        appendUnsignedImmediate(immediateR());

        return m_formatBuffer;
    }

    if (opc() == 0x2 && (immediateS() + 1) == immediateR()) {
        // lsl
        appendInstructionName("lsl");
        appendRegisterName(rd(), is64Bit());
        appendSeparator();
        appendRegisterName(rn(), is64Bit());
        appendSeparator();
        appendUnsignedImmediate((is64Bit() ? 64u : 32u) - immediateR());
        
        return m_formatBuffer;
    }
    
    if (immediateS() < immediateR()) {
        if (opc() != 1 || rn() != 0x1f) {
            // bit field insert
            appendInstructionName(insertOpNames());

            appendRegisterName(rd(), is64Bit());
            appendSeparator();
            appendRegisterName(rn(), is64Bit());
            appendSeparator();
            appendUnsignedImmediate((is64Bit() ? 64u : 32u) - immediateR());
            appendSeparator();
            appendUnsignedImmediate(immediateS() + 1);

            return m_formatBuffer;
        }
        
        appendInstructionName(opName());
        appendRegisterName(rd(), is64Bit());
        appendSeparator();
        appendRegisterName(rn(), is64Bit());
        appendSeparator();
        appendUnsignedImmediate(immediateR());
        appendSeparator();
        appendUnsignedImmediate(immediateS());
        
        return m_formatBuffer;
    }
    
    // bit field extract
    appendInstructionName(extractOpNames());

    appendRegisterName(rd(), is64Bit());
    appendSeparator();
    appendRegisterName(rn(), is64Bit());
    appendSeparator();
    appendUnsignedImmediate(immediateR());
    appendSeparator();
    appendUnsignedImmediate(immediateS() - immediateR() + 1);

    return m_formatBuffer;
}

const char* A64DOpcodeCompareAndBranchImmediate::format()
{
    appendInstructionName(opBit() ? "cbnz" : "cbz");
    appendRegisterName(rt(), is64Bit());
    appendSeparator();    
    appendPCRelativeOffset(m_currentPC, static_cast<int32_t>(immediate19()));
    return m_formatBuffer;
}

const char* A64DOpcodeConditionalBranchImmediate::format()
{
    bufferPrintf("   b.%-7.7s", conditionName(condition()));
    appendPCRelativeOffset(m_currentPC, static_cast<int32_t>(immediate19()));
    return m_formatBuffer;
}

const char* const A64DOpcodeConditionalSelect::s_opNames[4] = {
    "csel", "csinc", "csinv", "csneg"
};

const char* A64DOpcodeConditionalSelect::format()
{
    if (sBit())
        return A64DOpcode::format();

    if (op2() & 0x2)
        return A64DOpcode::format();

    if (rn() == rm() && (opNum() == 1 || opNum() == 2)) {
        if (rn() == 31) {
            appendInstructionName((opNum() == 1) ? "cset" : "csetm");
            appendRegisterName(rd(), is64Bit());
        } else {
            appendInstructionName((opNum() == 1) ? "cinc" : "cinv");
            appendRegisterName(rd(), is64Bit());
            appendSeparator();
            appendZROrRegisterName(rn(), is64Bit());
        }
        appendSeparator();
        appendString(conditionName(condition() ^ 0x1));

        return m_formatBuffer;
    }

    appendInstructionName(opName());
    appendRegisterName(rd(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rn(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rm(), is64Bit());
    appendSeparator();
    appendString(conditionName(condition()));

    return m_formatBuffer;

}

const char* const A64DOpcodeDataProcessing1Source::s_opNames[8] = {
    "rbit", "rev16", "rev32", "rev", "clz", "cls", 0, 0
};
    
const char* const A64DOpcodeDataProcessing1Source::s_pacAutOpNames[18] = {
    "pacia", "pacib", "pacda", "pacdb", "autia", "autib", "autda", "autdb",
    "paciza", "pacizb", "pacdza", "pacdzb", "autiza", "autizb", "autdza", "autdzb",
    "xpaci", "xpacd"
};

const char* A64DOpcodeDataProcessing1Source::format()
{
    if (sBit())
        return A64DOpcode::format();

    if (opCode2() == 1 && is64Bit() && opCode() <= 0x1001) {
        if (opCode() <= 0x00111 || rt() == 0x11111) {
            appendInstructionName(s_pacAutOpNames[opCode()]);
            appendZROrRegisterName(rd(), is64Bit());
            if (opCode() <= 0x00111) {
                appendSeparator();
                appendZROrRegisterName(rn(), is64Bit());
            }
            return m_formatBuffer;
        }
        return A64DOpcode::format();
    }

    if (opCode2())
        return A64DOpcode::format();

    if (opCode() & 0x38)
        return A64DOpcode::format();

    if ((opCode() & 0x3e) == 0x6)
        return A64DOpcode::format();

    if (is64Bit() && opCode() == 0x3)
        return A64DOpcode::format();

    if (!is64Bit() && opCode() == 0x2)
        appendInstructionName("rev");
    else
        appendInstructionName(opName());
    appendZROrRegisterName(rd(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rn(), is64Bit());
    
    return m_formatBuffer;
}

const char* const A64DOpcodeDataProcessing2Source::s_opNames[16] = {
    // We use the pseudo-op names for the shift/rotate instructions
    0, 0, "udiv", "sdiv", 0, 0, 0, 0,
    "lsl", "lsr", "asr", "ror", 0, "pacga", 0, 0
};

const char* A64DOpcodeDataProcessing2Source::format()
{
    if (sBit())
        return A64DOpcode::format();

    if (!(opCode() & 0x3e))
        return A64DOpcode::format();

    if (opCode() & 0x30)
        return A64DOpcode::format();

    if ((opCode() & 0x3c) == 0x4)
        return A64DOpcode::format();

    const char* opcodeName = opName();
    if (!opcodeName)
        return A64DOpcode::format();

    appendInstructionName(opcodeName);
    appendZROrRegisterName(rd(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rn(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rm(), is64Bit());

    return m_formatBuffer;
}

const char* const A64DOpcodeDataProcessing3Source::s_opNames[16] = {
    "madd", "msub", "smaddl", "smsubl", "smulh", 0, 0, 0,
    0, 0, "umaddl", "umsubl", "umulh", 0, 0, 0
};

const char* const A64DOpcodeDataProcessing3Source::s_pseudoOpNames[16] = {
    "mul", "mneg", "smull", "smnegl", "smulh", 0, 0, 0,
    0, 0, "umull", "umnegl", "umulh", 0, 0, 0
};

const char* A64DOpcodeDataProcessing3Source::format()
{
    if (op54())
        return A64DOpcode::format();

    if (opNum() > 12)
        return A64DOpcode::format();

    if (!is64Bit() && opNum() > 1)
        return A64DOpcode::format();

    if (!opName())
        return A64DOpcode::format();

    if ((opNum() & 0x4) && (ra() != 31))
        return A64DOpcode::format();

    appendInstructionName(opName());
    appendZROrRegisterName(rd(), is64Bit());
    appendSeparator();
    bool srcOneAndTwoAre64Bit = is64Bit() && !(opNum() & 0x2);
    appendZROrRegisterName(rn(), srcOneAndTwoAre64Bit);
    appendSeparator();
    appendZROrRegisterName(rm(), srcOneAndTwoAre64Bit);

    if (ra() != 31) {
        appendSeparator();
        appendRegisterName(ra(), is64Bit());
    }

    return m_formatBuffer;
}

const char* A64OpcodeExceptionGeneration::format()
{
    const char* opname = 0;
    if (!op2()) {
        switch (opc()) {
        case 0x0: // SVC, HVC & SMC
            switch (ll()) {
            case 0x1:
                opname = "svc";
                break;
            case 0x2:
                opname = "hvc";
                break;
            case 0x3:
                opname = "smc";
                break;
            }
            break;
        case 0x1: // BRK
            if (!ll())
                opname = "brk";
            break;
        case 0x2: // HLT
            if (!ll())
                opname = "hlt";
            break;
        case 0x5: // DPCS1-3
            switch (ll()) {
            case 0x1:
                opname = "dpcs1";
                break;
            case 0x2:
                opname = "dpcs2";
                break;
            case 0x3:
                opname = "dpcs3";
                break;
            }
            break;
        }
    }

    if (!opname)
        return A64DOpcode::format();

    appendInstructionName(opname);
    appendUnsignedHexImmediate(immediate16());
    return m_formatBuffer;
}

const char* A64DOpcodeExtract::format()
{
    if (op21() || o0Bit())
        return A64DOpcode::format();

    if (is64Bit() != nBit())
        return A64DOpcode::format();

    if (!is64Bit() && (immediateS() & 0x20))
        return A64DOpcode::format();

    bool isROR = rn() == rm();
    const char* opName = (isROR) ? "ror" : "extr";

    appendInstructionName(opName);
    appendZROrRegisterName(rd(), is64Bit());
    appendSeparator();
    appendZROrRegisterName(rn(), is64Bit());
    if (!isROR) {
        appendSeparator();
        appendZROrRegisterName(rm(), is64Bit());
    }
    appendSeparator();
    appendUnsignedImmediate(immediateS());

    return m_formatBuffer;
}

const char* A64DOpcodeFloatingPointCompare::format()
{
    if (mBit())
        return A64DOpcode::format();

    if (sBit())
        return A64DOpcode::format();

    if (type() & 0x2)
        return A64DOpcode::format();

    if (op())
        return A64DOpcode::format();

    if (opCode2() & 0x7)
        return A64DOpcode::format();

    appendInstructionName(opName());
    unsigned registerSize = type() + 2;
    appendFPRegisterName(rn(), registerSize);
    appendSeparator();
    if (opCode2() & 0x8)
        bufferPrintf("#0.0");
    else
        appendFPRegisterName(rm(), registerSize);
    
    return m_formatBuffer;
}

const char* A64DOpcodeFloatingPointConditionalSelect::format()
{
    if (mBit())
        return A64DOpcode::format();
    
    if (sBit())
        return A64DOpcode::format();
    
    if (type() & 0x2)
        return A64DOpcode::format();

    appendInstructionName(opName());
    unsigned registerSize = type() + 2;
    appendFPRegisterName(rd(), registerSize);
    appendSeparator();
    appendFPRegisterName(rn(), registerSize);
    appendSeparator();
    appendFPRegisterName(rm(), registerSize);
    appendSeparator();
    appendString(conditionName(condition()));
    
    return m_formatBuffer;
}

const char* const A64DOpcodeFloatingPointDataProcessing1Source::s_opNames[16] = {
    "fmov", "fabs", "fneg", "fsqrt", "fcvt", "fcvt", 0, "fcvt",
    "frintn", "frintp", "frintm", "frintz", "frinta", 0, "frintx", "frinti"
};

const char* A64DOpcodeFloatingPointDataProcessing1Source::format()
{
    if (mBit())
        return A64DOpcode::format();

    if (sBit())
        return A64DOpcode::format();

    if (opNum() > 16)
        return A64DOpcode::format();

    switch (type()) {
    case 0:
        if ((opNum() == 0x4) || (opNum() == 0x6) || (opNum() == 0xd))
            return A64DOpcode::format();
        break;
    case 1:
        if ((opNum() == 0x5) || (opNum() == 0x6) || (opNum() == 0xd))
            return A64DOpcode::format();
        break;
    case 2:
        return A64DOpcode::format();
    case 3:
        if ((opNum() < 0x4) || (opNum() > 0x5))
            return A64DOpcode::format();
        break;
    }

    appendInstructionName(opName());
    if ((opNum() >= 0x4) && (opNum() <= 0x7)) {
        unsigned srcRegisterSize = type() ^ 0x2; // 0:s, 1:d & 3:h
        unsigned destRegisterSize = (opNum() & 0x3) ^ 0x2;
        appendFPRegisterName(rd(), destRegisterSize);
        appendSeparator();
        appendFPRegisterName(rn(), srcRegisterSize);
    } else {
        unsigned registerSize = type() + 2;
        appendFPRegisterName(rd(), registerSize);
        appendSeparator();
        appendFPRegisterName(rn(), registerSize);
    }

    return m_formatBuffer;
}

const char* const A64DOpcodeFloatingPointDataProcessing2Source::s_opNames[16] = {
    "fmul", "fdiv", "fadd", "fsub", "fmax", "fmin", "fmaxnm", "fminnm", "fnmul"
};

const char* A64DOpcodeFloatingPointDataProcessing2Source::format()
{
    if (mBit())
        return A64DOpcode::format();

    if (sBit())
        return A64DOpcode::format();

    if (type() & 0x2)
        return A64DOpcode::format();

    if (opNum() > 8)
        return A64DOpcode::format();

    appendInstructionName(opName());
    unsigned registerSize = type() + 2;
    appendFPRegisterName(rd(), registerSize);
    appendSeparator();
    appendFPRegisterName(rn(), registerSize);
    appendSeparator();
    appendFPRegisterName(rm(), registerSize);

    return m_formatBuffer;
}

const char* const A64DOpcodeFloatingFixedPointConversions::s_opNames[4] = {
    "fcvtzs", "fcvtzu", "scvtf", "ucvtf"
};

const char* A64DOpcodeFloatingFixedPointConversions::format()
{
    if (sBit())
        return A64DOpcode::format();

    if (type() & 0x2)
        return A64DOpcode::format();

    if (opcode() & 0x4)
        return A64DOpcode::format();

    if (!(rmode() & 0x1) && !(opcode() & 0x6))
        return A64DOpcode::format();

    if ((rmode() & 0x1) && (opcode() & 0x6) == 0x2)
        return A64DOpcode::format();

    if (!(rmode() & 0x2) && !(opcode() & 0x6))
        return A64DOpcode::format();

    if ((rmode() & 0x2) && (opcode() & 0x6) == 0x2)
        return A64DOpcode::format();

    if (!is64Bit() && scale() >= 32)
        return A64DOpcode::format();

    appendInstructionName(opName());
    unsigned FPRegisterSize = type() + 2;
    bool destIsFP = !rmode();
    
    if (destIsFP) {
        appendFPRegisterName(rd(), FPRegisterSize);
        appendSeparator();
        appendRegisterName(rn(), is64Bit());
    } else {
        appendRegisterName(rd(), is64Bit());
        appendSeparator();
        appendFPRegisterName(rn(), FPRegisterSize);
    }
    appendSeparator();
    appendUnsignedImmediate(64 - scale());
    
    return m_formatBuffer;
}

const char* const A64DOpcodeFloatingPointIntegerConversions::s_opNames[32] = {
    "fcvtns", "fcvtnu", "scvtf", "ucvtf", "fcvtas", "fcvtau", "fmov", "fmov",
    "fcvtps", "fcvtpu", 0, 0, 0, 0, "fmov", "fmov",
    "fcvtms", "fcvtmu", 0, 0, 0, 0, 0, 0,
    "fcvtzs", "fcvtzu", 0, 0, 0, 0, "fjcvtzs", 0
};

const char* A64DOpcodeFloatingPointIntegerConversions::format()
{
    if (sBit())
        return A64DOpcode::format();

    if (type() == 0x3)
        return A64DOpcode::format();

    if (((rmode() & 0x1) || (rmode() & 0x2)) && (((opcode() & 0x6) == 0x2) || ((opcode() & 0x6) == 0x4)))
        return A64DOpcode::format();

    if ((type() == 0x2) && (!(opcode() & 0x4) || ((opcode() & 0x6) == 0x4)))
        return A64DOpcode::format();

    if (!type() && (rmode() & 0x1) && ((opcode() & 0x6) == 0x6))
        return A64DOpcode::format();

    if (is64Bit() && type() == 0x2 && ((opNum() & 0xe) == 0x6))
        return A64DOpcode::format();

    if (!opName())
        return A64DOpcode::format();

    if ((opNum() & 0x1e) == 0xe) {
        // Handle fmov to/from upper half of quad separately
        if (!is64Bit() || (type() != 0x2))
            return A64DOpcode::format();

        appendInstructionName(opName());
        if (opcode() & 0x1) {
            // fmov Vd.D[1], Xn
            bufferPrintf("V%u.D[1]", rd());
            appendSeparator();
            appendZROrRegisterName(rn());
        } else {
            // fmov Xd, Vn.D[1]
            appendZROrRegisterName(rd());
            appendSeparator();
            bufferPrintf("V%u.D[1]", rn());
        }

        return m_formatBuffer;
    }

    appendInstructionName(opName());
    unsigned FPRegisterSize = type() + 2;
    bool destIsFP = ((opNum() == 2) || (opNum() == 3) || (opNum() == 7));

    if (destIsFP) {
        appendFPRegisterName(rd(), FPRegisterSize);
        appendSeparator();
        appendZROrRegisterName(rn(), is64Bit());
    } else {
        appendZROrRegisterName(rd(), is64Bit());
        appendSeparator();
        appendFPRegisterName(rn(), FPRegisterSize);
    }

    return m_formatBuffer;
}

const char* A64DOpcodeMSRImmediate::format()
{
    const char* pstateField = nullptr;

    if (!op1() && (op2() == 0x5))
        pstateField = "spsel";

    if ((op1() == 0x3) && (op2() == 0x6))
        pstateField = "daifset";

    if ((op1() == 0x3) && (op2() == 0x7))
        pstateField = "daifclr";

    if (!!op1() && !(op2() & 0x4))
        return A64DOpcode::format();

    if (!pstateField)
        return A64DOpcode::format();

    appendInstructionName("msr");
    appendString(pstateField);
    appendSeparator();
    appendUnsignedImmediate(crM());

    return m_formatBuffer;
}

const char* A64DOpcodeMSROrMRSRegister::format()
{
    appendInstructionName(opName());

    if (lBit()) {
        appendZROrRegisterName(rt());
        appendSeparator();
    }

    bufferPrintf("S%u_%u_C%u_C%u_%u", op0(), op1(), crN(), crM(), op2());

    if (!lBit()) {
        appendSeparator();
        appendZROrRegisterName(rt());
    }

    const char* systemRegisterName = nullptr;

    switch (systemRegister()) {
    case 0b1101100000000001:
        systemRegisterName = "ctr_el0";
        break;
    case 0b1101101000010000:
        systemRegisterName = "nzcv";
        break;
    case 0b1101101000010001:
        systemRegisterName = "daif";
        break;
    case 0b1101101000100000:
        systemRegisterName = "fpcr";
        break;
    case 0b1101101000100001:
        systemRegisterName = "fpsr";
        break;
    case 0b1101111010000010:
        systemRegisterName = "tpidr_el0";
        break;
    case 0b1101111010000011:
        systemRegisterName = "tpidrr0_el0";
        break;
    }

    if (systemRegisterName) {
        appendString("  ; ");
        appendString(systemRegisterName);
    }
    return m_formatBuffer;
}

const char* const A64DOpcodeHint::s_opNames[32] = {
    "nop", "yield", "wfe", "wfi", "sev", "sevl", 0, "xpaclri",
    "pacia1716", 0, "pacib1716", 0, "autia1716", 0, "autib1716", 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    "paciaz", "paciasp", "pacibz", "pacibsp", "autiaz", "autiasp", "autibz", "autibsp"
};

const char* A64DOpcodeHint::format()
{
    appendInstructionName(opName());

    if (immediate7() >= 32 || !s_opNames[immediate7()])
        appendUnsignedImmediate(immediate7());

    return m_formatBuffer;
}

const char* A64DOpcodeHint::opName()
{
    const char* opName = (immediate7() < 32 ? s_opNames[immediate7()] : 0);
    if (!opName)
        return "hint";

    return opName;
}

const char* const A64DOpcodeSystemSync::s_opNames[8] = {
    0, 0, "clrex", 0, "dsb", "dmb", "isb", 0
};

const char* const A64DOpcodeSystemSync::s_optionNames[16] = {
    0, "oshld", "oshst", "osh", 0, "nshld", "nshst", "nsh",
    0, "ishld", "ishst", "ish", 0, "ld", "st", "sy"
};

const char* A64DOpcodeSystemSync::format()
{
    const char* thisOpName = opName();

    if (!thisOpName)
        return A64DOpcode::format();

    appendInstructionName(thisOpName);

    if (op2() & 0x2) {
        if (crM() != 0xf) {
            appendCharacter('#');
            appendUnsignedImmediate(crM());
        }
    } else {
        const char* thisOption = option();
        if (thisOption)
            appendString(thisOption);
        else
            appendUnsignedImmediate(crM());
    }

    return m_formatBuffer;
}

const char* const A64DOpcodeLoadStoreExclusive::s_opNames[64] = {
    "stxrb", "stlxrb", 0, 0, "ldxrb", "ldaxrb", 0, 0,
    0, "stlrb", 0, 0, 0, "ldarb", 0, 0,
    "stxrh", "stlxrh", 0, 0, "ldxrh", "ldaxrh", 0, 0,
    0, "stlrh", 0, 0, 0, "ldarh", 0, 0,
    "stxr", "stlxr", "stxp", "stlxp", "ldxr", "ldaxr", "ldxp", "ldaxp",
    0, "stlr", 0, 0, 0, "ldar", 0, 0,
    "stxr", "stlxr", "stxp", "stlxp", "ldxr", "ldaxr", "ldxp", "ldaxp",
    0, "stlr", 0, 0, 0, "ldar", 0, 0
};

const char* A64DOpcodeLoadStoreExclusive::format()
{
    if (o2() && !o1() && !o0())
        return A64DOpcode::format();

    if (o2() && o1())
        return A64DOpcode::format();

    if ((size() < 2) && o1())
        return A64DOpcode::format();

    if (loadBit() && (rs() != 0x1f))
        return A64DOpcode::format();

    if (!isPairOp() && (rt2() != 0x1f))
        return A64DOpcode::format();

    const char* thisOpName = opName();

    if (!thisOpName)
        return A64DOpcode::format();

    appendInstructionName(thisOpName);

    if (!loadBit()) {
        appendZROrRegisterName(rs(), size() == 0x3);
        appendSeparator();
    }

    appendZROrRegisterName(rt(), size() == 0x3);
    appendSeparator();
    if (isPairOp()) {
        appendZROrRegisterName(rt2(), size() == 0x3);
        appendSeparator();
    }
    appendCharacter('[');
    appendSPOrRegisterName(rn());
    appendCharacter(']');

    return m_formatBuffer;
}

// A zero in an entry of the table means the instruction is Unallocated
const char* const A64DOpcodeLoadStore::s_opNames[32] = {
    "strb", "ldrb", "ldrsb", "ldrsb", "str", "ldr", "str", "ldr",
    "strh", "ldrh", "ldrsh", "ldrsh", "str", "ldr", 0, 0,
    "str", "ldr", "ldrsw", 0, "str", "ldr", 0, 0,
    "str", "ldr", 0, 0, "str", "ldr", 0, 0
};

// A zero in an entry of the table means the instruction is Unallocated
const char* const A64DOpcodeLoadStoreImmediate::s_unprivilegedOpNames[32] = {
    "sttrb", "ldtrb", "ldtrsb", "ldtrsb", 0, 0, 0, 0,
    "sttrh", "ldtrh", "ldtrsh", "ldtrsh", 0, 0, 0, 0,
    "sttr", "ldtr", "ldtrsw", 0, 0, 0, 0, 0,
    "sttr", "ldtr", 0, 0, 0, 0, 0, 0
};

// A zero in an entry of the table means the instruction is Unallocated
const char* const A64DOpcodeLoadStoreImmediate::s_unscaledOpNames[32] = {
    "sturb", "ldurb", "ldursb", "ldursb", "stur", "ldur", "stur", "ldur",
    "sturh", "ldurh", "ldursh", "ldursh", "stur", "ldur", 0, 0,
    "stur", "ldur", "ldursw", 0, "stur", "ldur", 0, 0,
    "stur", "ldur", "prfum", 0, "stur", "ldur", 0, 0
};

const char* A64DOpcodeLoadStoreImmediate::format()
{
    const char* thisOpName;

    if (type() & 0x1)
        thisOpName = opName();
    else if (!type())
        thisOpName = unscaledOpName();
    else
        thisOpName = unprivilegedOpName();

    if (!thisOpName)
        return A64DOpcode::format();

    appendInstructionName(thisOpName);
    if (vBit())
        appendFPRegisterName(rt(), size());
    else if (!opc())
        appendZROrRegisterName(rt(), is64BitRT());
    else
        appendRegisterName(rt(), is64BitRT());
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn());

    switch (type()) {
    case 0: // Unscaled Immediate
        if (immediate9()) {
            appendSeparator();
            appendSignedImmediate(immediate9());
        }
        appendCharacter(']');
        break;
    case 1: // Immediate Post-Indexed
        appendCharacter(']');
        if (immediate9()) {
            appendSeparator();
            appendSignedImmediate(immediate9());
        }
        break;
    case 2: // Unprivileged
        if (immediate9()) {
            appendSeparator();
            appendSignedImmediate(immediate9());
        }
        appendCharacter(']');
        break;
    case 3: // Immediate Pre-Indexed
        if (immediate9()) {
            appendSeparator();
            appendSignedImmediate(immediate9());
        }
        appendCharacter(']');
        appendCharacter('!');
        break;
    }

    return m_formatBuffer;
}

const char* A64DOpcodeLoadStoreRegisterOffset::format()
{
    const char* thisOpName = opName();

    if (!thisOpName)
        return A64DOpcode::format();

    if (!(option() & 0x2))
        return A64DOpcode::format();

    appendInstructionName(thisOpName);
    unsigned scale;
    if (vBit()) {
        appendFPRegisterName(rt(), size());
        scale = ((opc() & 2)<<1) | size();
    } else {
        if (!opc())
            appendZROrRegisterName(rt(), is64BitRT());
        else
            appendRegisterName(rt(), is64BitRT());
        scale = size();
    }
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn());
    if (rm() != 31) {
        appendSeparator();
        appendRegisterName(rm(), (option() & 0x3) == 0x3);

        unsigned shift = sBit() ? scale : 0;

        if (option() == 0x3) {
            if (shift) {
                appendSeparator();
                appendString("lsl ");
                appendUnsignedImmediate(shift);
            }
        } else {
            appendSeparator();
            appendString(optionName());
            if (shift)
                appendUnsignedImmediate(shift);
        }
    }

    appendCharacter(']');

    return m_formatBuffer;
}

const char* const A64DOpcodeLoadStoreAuthenticated::s_opNames[2] = {
    "ldraa", "ldrab"
};

const char* A64DOpcodeLoadStoreAuthenticated::format()
{
    appendInstructionName(opName());
    appendRegisterName(rt());
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn());

    if (wBit() || immediate10()) {
        appendSeparator();
        appendSignedImmediate(immediate10() << size());
    }
    appendCharacter(']');

    if (wBit())
        appendCharacter('!');
    
    return m_formatBuffer;
}

const char* const A64DOpcodeLoadAtomic::s_opNames[64] = {
    "ldaddb", "ldaddlb", "ldaddab", "ldaddalb",
    "ldaddh", "ldaddlh", "ldaddah", "ldaddalh",
    "ldadd", "ldaddl", "ldadda", "ldaddal",
    "ldadd", "ldaddl", "ldadda", "ldaddal",

    "ldclrb", "ldclrlb", "ldclrab", "ldclralb",
    "ldclrh", "ldclrlh", "ldclrah", "ldclralh",
    "ldclr", "ldclrl", "ldclra", "ldclral",
    "ldclr", "ldclrl", "ldclra", "ldclral",

    "ldeorb", "ldeorlb", "ldeorab", "ldeoralb",
    "ldeorh", "ldeorlh", "ldeorah", "ldeoralh",
    "ldeor", "ldeorl", "ldeora", "ldeoral",
    "ldeor", "ldeorl", "ldeora", "ldeoral",

    "ldsetb", "ldsetlb", "ldsetab", "ldsetalb",
    "ldseth", "ldsetlh", "ldsetah", "ldsetalh",
    "ldset", "ldsetl", "ldseta", "ldsetal",
    "ldset", "ldsetl", "ldseta", "ldsetal",
};

const char* A64DOpcodeLoadAtomic::format()
{
    const auto* name = opName();
    if (!name)
        return A64DOpcode::format();
    appendInstructionName(name);
    appendSPOrRegisterName(rs(), is64Bit());
    appendSeparator();
    appendSPOrRegisterName(rt(), is64Bit());
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn(), is64Bit());
    appendCharacter(']');
    return m_formatBuffer;
}

const char* const A64DOpcodeSwapAtomic::s_opNames[16] = {
    "swpb", "swplb", "swpab", "swpalb",
    "swph", "swplh", "swpah", "swpalh",
    "swp", "swpl", "swpa", "swpal",
    "swp", "swpl", "swpa", "swpal",
};

const char* A64DOpcodeSwapAtomic::format()
{
    const auto* name = opName();
    appendInstructionName(name);
    appendSPOrRegisterName(rs(), is64Bit());
    appendSeparator();
    appendSPOrRegisterName(rt(), is64Bit());
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn(), is64Bit());
    appendCharacter(']');
    return m_formatBuffer;
}

const char* const A64DOpcodeCAS::s_opNames[16] = {
    "casb", "caslb", "casab", "casalb",
    "cash", "caslh", "casah", "casalh",
    "cas", "casl", "casa", "casal",
    "cas", "casl", "casa", "casal",
};

const char* A64DOpcodeCAS::format()
{
    const auto* name = opName();
    appendInstructionName(name);
    appendSPOrRegisterName(rs(), is64Bit());
    appendSeparator();
    appendSPOrRegisterName(rt(), is64Bit());
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn(), is64Bit());
    appendCharacter(']');
    return m_formatBuffer;
}

const char* A64DOpcodeLoadStoreRegisterPair::opName()
{
    if (!vBit() && lBit() && size() == 0x1)
        return "ldpsw";
    if (lBit())
        return "ldp";
    return "stp";
}

const char* A64DOpcodeLoadStoreRegisterPair::format()
{
    const char* thisOpName = opName();
    
    if (size() == 0x3)
        return A64DOpcode::format();

    if ((offsetMode() < 0x1) || (offsetMode() > 0x3))
        return A64DOpcode::format();

    if ((offsetMode() == 0x1) && !vBit() && !lBit())
        return A64DOpcode::format();

    appendInstructionName(thisOpName);
    unsigned offsetShift;
    if (vBit()) {
        appendFPRegisterName(rt(), size() + 2);
        appendSeparator();
        appendFPRegisterName(rt2(), size() + 2);
        offsetShift = size() + 2;
    } else {
        if (!lBit())
            appendZROrRegisterName(rt(), is64Bit());
        else
            appendRegisterName(rt(), is64Bit());
        appendSeparator();
        if (!lBit())
            appendZROrRegisterName(rt2(), is64Bit());
        else
            appendRegisterName(rt2(), is64Bit());
        offsetShift = (size() >> 1) + 2;
    }

    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn());

    int offset = immediate7() << offsetShift;

    if (offsetMode() == 1) {
        appendCharacter(']');
        appendSeparator();
        appendSignedImmediate(offset);
    } else {
        appendSeparator();
        appendSignedImmediate(offset);
        appendCharacter(']');
        if (offsetMode() == 0x3)
            appendCharacter('!');
    }

    return m_formatBuffer;
}

const char* A64DOpcodeLoadStoreUnsignedImmediate::format()
{
    const char* thisOpName = opName();

    if (!thisOpName)
        return A64DOpcode::format();

    appendInstructionName(thisOpName);
    unsigned scale;
    if (vBit()) {
        appendFPRegisterName(rt(), size());
        scale = ((opc() & 2)<<1) | size();
    } else {
        if (!opc())
            appendZROrRegisterName(rt(), is64BitRT());
        else
            appendRegisterName(rt(), is64BitRT());
        scale = size();
    }
    appendSeparator();
    appendCharacter('[');
    appendSPOrRegisterName(rn());

    if (immediate12()) {
        appendSeparator();
        appendUnsignedImmediate(immediate12() << scale);
    }

    appendCharacter(']');

    return m_formatBuffer;
}

// A zero in an entry of the table means the instruction is Unallocated
const char* const A64DOpcodeLogical::s_opNames[8] = {
    "and", "bic", "orr", "orn", "eor", "eon", "ands", "bics"
};

const char* A64DOpcodeLogicalShiftedRegister::format()
{
    if (!is64Bit() && immediate6() & 0x20)
        return A64DOpcode::format();

    if (isTst())
        appendInstructionName("tst");
    else {
        if (isMov())
            appendInstructionName(nBit() ? "mvn" : "mov");
        else
            appendInstructionName(opName(opNumber()));
        appendZROrRegisterName(rd(), is64Bit());
        appendSeparator();
    }

    if (!isMov()) {
        appendZROrRegisterName(rn(), is64Bit());
        appendSeparator();
    }

    appendZROrRegisterName(rm(), is64Bit());
    if (immediate6()) {
        appendSeparator();
        appendShiftType(shift());
        appendUnsignedImmediate(immediate6());
    }

    return m_formatBuffer;
}

static unsigned highestBitSet(unsigned value)
{
    unsigned result = 0;

    while (value >>= 1)
        result++;

    return result;
}

static uint64_t rotateRight(uint64_t value, unsigned width, unsigned shift)
{
    uint64_t result = value;

    if (shift)
        result = (value >> (shift % width)) | (value << (width - shift));

    return result;
}

static uint64_t replicate(uint64_t value, unsigned width)
{
    uint64_t result = 0;

    for (unsigned totalBits = 0; totalBits < 64; totalBits += width)
        result = (result << width) | value;

    return result;
}

const char* A64DOpcodeLogicalImmediate::format()
{
    if (!is64Bit() && nBit())
        return A64DOpcode::format();

    unsigned len = highestBitSet(nBit() << 6 | (immediateS() ^ 0x3f));
    unsigned levels = (1 << len) - 1; // len number of 1 bits starting at LSB

    if ((immediateS() & levels) == levels)
        return A64DOpcode::format();

    unsigned r = immediateR() & levels;
    unsigned s = immediateS() & levels;
    unsigned eSize = 1 << len;
    uint64_t pattern = rotateRight((1ull << (s + 1)) - 1, eSize, r);

    uint64_t immediate = replicate(pattern, eSize);

    if (!is64Bit())
        immediate &= 0xffffffffull;

    if (isTst())
        appendInstructionName("tst");
    else {
        if (isMov())
            appendInstructionName("mov");
        else
            appendInstructionName(opName(opNumber()));
        appendRegisterName(rd(), is64Bit());
        appendSeparator();
    }
    if (!isMov()) {
        appendRegisterName(rn(), is64Bit());
        appendSeparator();
    }
    appendUnsignedImmediate64(immediate);

    return m_formatBuffer;
}

const char* const A64DOpcodeMoveWide::s_opNames[4] = { "movn", 0, "movz", "movk" };

class MoveWideFormatTrait {
public:
    using ResultType = const char*;
    static constexpr bool returnEarlyIfAccepted = false;

    ALWAYS_INLINE static const char* rejectedResult(A64DOpcodeMoveWide* opcode) { return opcode->baseFormat(); }
    ALWAYS_INLINE static const char* acceptedResult(A64DOpcodeMoveWide* opcode) { return opcode->formatBuffer(); }
};

class MoveWideIsValidTrait {
public:
    using ResultType = bool;
    static constexpr bool returnEarlyIfAccepted = true;

    static constexpr bool rejectedResult(A64DOpcodeMoveWide*) { return false; }
    static constexpr bool acceptedResult(A64DOpcodeMoveWide*) { return true; }
};

bool A64DOpcodeMoveWide::handlePotentialDataPointer(void* ptr)
{
    ASSERT(Integrity::isSanePointer(ptr));

    bool handled = false;
    VMInspector::forEachVM([&] (VM& vm) {
        if (ptr == &vm) {
            bufferPrintf(" vm");
            handled = true;
            return IterationStatus::Done;
        }

        if (!vm.isInService())
            return IterationStatus::Continue;

        auto* vmStart = reinterpret_cast<uint8_t*>(&vm);
        auto* vmEnd = vmStart + sizeof(VM);
        auto* u8Ptr = reinterpret_cast<uint8_t*>(ptr);
        Range vmRange(vmStart, vmEnd);
        if (vmRange.contains(u8Ptr)) {
            unsigned offset = u8Ptr - vmStart;
            bufferPrintf(" vm +%u", offset);

            const char* description = nullptr;
            if (ptr == &vm.topCallFrame)
                description = "vm.topCallFrame";
            else if (offset == VM::topEntryFrameOffset())
                description = "vm.topEntryFrame";
            else if (offset == VM::exceptionOffset())
                description = "vm.m_exception";
            else if (offset == VM::offsetOfHeapBarrierThreshold())
                description = "vm.heap.m_barrierThreshold";
            else if (offset == VM::callFrameForCatchOffset())
                description = "vm.callFrameForCatch";
            else if (ptr == vm.addressOfSoftStackLimit())
                description = "vm.m_softStackLimit";
            else if (ptr == &vm.osrExitIndex)
                description = "vm.osrExitIndex";
            else if (ptr == &vm.osrExitJumpDestination)
                description = "vm.osrExitJumpDestination";
            else if (ptr == vm.smallStrings.singleCharacterStrings())
                description = "vm.smallStrings.m_singleCharacterStrings";
            else if (ptr == &vm.targetMachinePCForThrow)
                description = "vm.targetMachinePCForThrow";
            else if (ptr == vm.traps().trapBitsAddress())
                description = "vm.m_traps.m_trapBits";
#if ENABLE(DFG_DOES_GC_VALIDATION)
            else if (ptr == vm.addressOfDoesGC())
                description = "vm.m_doesGC";
#endif
            if (description)
                bufferPrintf(": %s", description);

            handled = true;
            return IterationStatus::Done;
        }

        if (vm.isScratchBuffer(ptr)) {
            bufferPrintf(" vm scratchBuffer.m_buffer");
            handled = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return handled;
}

#if CPU(ARM64E)
bool A64DOpcodeMoveWide::handlePotentialPtrTag(uintptr_t value)
{
    if (!value || value > 0xffff)
        return false;

    PtrTag tag = static_cast<PtrTag>(value);
#if ENABLE(PTRTAG_DEBUGGING)
    const char* name = WTF::ptrTagName(tag);
    if (name[0] == '<')
        return false; // Only result that starts with '<' is "<unknown>".
#else
    // Without ENABLE(PTRTAG_DEBUGGING), not all PtrTags are registeredf for
    // printing. So, we'll just do the minimum with only the JSC specific tags.
    const char* name = ptrTagName(tag);
    if (!name)
        return false;
#endif

    // Also print '?' to indicate that this is a maybe. We do not know for certain
    // if the constant is meant to be used as a PtrTag.
    bufferPrintf(" -> %p %s ?", reinterpret_cast<void*>(value), name);
    return true;
}
#endif

template<typename Trait>
typename Trait::ResultType A64DOpcodeMoveWide::parse()
{
    if (opc() == 1)
        return Trait::rejectedResult(this);
    if (!is64Bit() && hw() >= 2)
        return Trait::rejectedResult(this);

    if constexpr (Trait::returnEarlyIfAccepted)
        return Trait::acceptedResult(this);

    if (!opc() && (!immediate16() || !hw()) && (is64Bit() || immediate16() != 0xffff)) {
        // MOV pseudo op for MOVN
        appendInstructionName("mov");
        appendRegisterName(rd(), is64Bit());
        appendSeparator();

        if (is64Bit()) {
            int64_t amount = immediate16() << (hw() * 16);
            amount = ~amount;
            appendSignedImmediate64(amount);
            m_builtConstant = static_cast<intptr_t>(amount);
        } else {
            int32_t amount = immediate16() << (hw() * 16);
            amount = ~amount;
            appendSignedImmediate(amount);
            m_builtConstant = static_cast<intptr_t>(amount);
        }
    } else {
        appendInstructionName(opName());
        appendRegisterName(rd(), is64Bit());
        appendSeparator();
        appendUnsignedHexImmediate(immediate16());
        if (hw()) {
            appendSeparator();
            appendShiftAmount(hw());
        }

        if (opc() == 2) // Encoding for movz
            m_builtConstant = 0;

        unsigned shift = hw() * 16;
        uintptr_t value = static_cast<uintptr_t>(immediate16()) << shift;
        uintptr_t mask = ~(static_cast<uintptr_t>(0xffff) << shift);
        m_builtConstant &= mask;
        m_builtConstant |= value;
    }

    auto dumpConstantData = [&] {
        uint32_t* nextPC = m_currentPC + 1;
        bool doneBuildingConstant = false;

        if (nextPC >= m_endPC)
            doneBuildingConstant = true;
        else {
            A64DOpcode nextOpcodeBase(m_startPC, m_endPC);
            A64DOpcodeMoveWide& nextOpcode = *reinterpret_cast<A64DOpcodeMoveWide*>(&nextOpcodeBase);
            nextOpcode.setPCAndOpcode(nextPC, *nextPC);

            bool nextIsMoveWideGroup = opcodeGroupNumber(m_opcode) == opcodeGroupNumber(*nextPC);

            if (!nextIsMoveWideGroup || !nextOpcode.isValid() || nextOpcode.rd() != rd())
                doneBuildingConstant = true;
        }
        if (!doneBuildingConstant)
            return;

        void* ptr = removeCodePtrTag(bitwise_cast<void*>(m_builtConstant));
        if (!ptr)
            return;

        if (Integrity::isSanePointer(ptr)) {
            bufferPrintf(" -> %p", ptr);
            if (const char* label = labelFor(ptr))
                return bufferPrintf(" %s", label);
            if (isJITPC(ptr))
                return bufferPrintf(" JIT PC");
            if (LLInt::isLLIntPC(ptr))
                return bufferPrintf(" LLInt PC");
            handlePotentialDataPointer(ptr);
            return;
        }
#if CPU(ARM64E)
        if (handlePotentialPtrTag(m_builtConstant))
            return;
#endif
        if (m_builtConstant < 0x10000)
            bufferPrintf(" -> %u", static_cast<unsigned>(m_builtConstant));
        else
            bufferPrintf(" -> %p", reinterpret_cast<void*>(m_builtConstant));
    };

    if (m_startPC)
        dumpConstantData();

    return Trait::acceptedResult(this);
}

const char* A64DOpcodeMoveWide::format() { return parse<MoveWideFormatTrait>(); }
bool A64DOpcodeMoveWide::isValid() { return parse<MoveWideIsValidTrait>(); }

const char* A64DOpcodeTestAndBranchImmediate::format()
{
    appendInstructionName(opBit() ? "tbnz" : "tbz");
    appendRegisterName(rt());
    appendSeparator();
    appendUnsignedImmediate(bitNumber());
    appendSeparator();
    appendPCRelativeOffset(m_currentPC, static_cast<int32_t>(immediate14()));
    return m_formatBuffer;
}

const char* A64DOpcodeUnconditionalBranchImmediate::format()
{
    appendInstructionName(op() ? "bl" : "b");
    appendPCRelativeOffset(m_currentPC, static_cast<int32_t>(immediate26()));
    return m_formatBuffer;
}

const char* const A64DOpcodeUnconditionalBranchRegister::s_opNames[8] = { "br", "blr", "ret", "", "eret", "drps", "", "" };
const char* const A64DOpcodeUnconditionalBranchRegister::s_AuthOpNames[20] = {
    "braaz", "brabz", "blraaz", "blrabz", "retaa", "retab", 0, 0,
    "eretaa", "eretab", 0, 0, 0, 0, 0, 0,
    "braa", "brab", "blraa", "blrab"
};

const char* A64DOpcodeUnconditionalBranchRegister::authOpName()
{
    unsigned opCode = authOpCode();
    if (opCode >= 20)
        return 0;
    return s_AuthOpNames[opCode];
}

const char* A64DOpcodeUnconditionalBranchRegister::format()
{
    unsigned opcValue = opc();
    if (op2() == 0x1f && (op3() & 0x3e) == 0x2) {
        const char* opName = authOpName();
        if (!opName)
            return A64DOpcode::format();
        if (rn() != 0x1f && (opcValue == 0x2 || opcValue == 0x4))
            return A64DOpcode::format();

        appendInstructionName(opName);
        if ((opcValue & 0x7) <= 0x1)
            appendRegisterName(rn());
        if (opcValue & 0x8) {
            appendSeparator();
            appendRegisterName(rm());
        }

        return m_formatBuffer;
    }
    if (opcValue == 3 || opcValue > 5)
        return A64DOpcode::format();
    if (((opcValue & 0xe) == 0x4) && rn() != 0x1f)
        return A64DOpcode::format();
    appendInstructionName(opName());
    if (opcValue <= 2)
        appendRegisterName(rn());
    return m_formatBuffer;
}

} } // namespace JSC::ARM64Disassembler

#endif // ENABLE(ARM64_DISASSEMBLER)
