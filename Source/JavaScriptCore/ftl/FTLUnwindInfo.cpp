/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics
 * Copyright (C) 2014 University of Szeged
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
 *
 * ==============================================================================
 *
 * University of Illinois/NCSA
 * Open Source License
 *
 * Copyright (c) 2009-2014 by the contributors of LLVM/libc++abi project.
 *
 * All rights reserved.
 *
 * Developed by:
 *
 *    LLVM Team
 *
 *    University of Illinois at Urbana-Champaign
 *
 *    http://llvm.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal with
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimers.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimers in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the names of the LLVM Team, University of Illinois at
 *      Urbana-Champaign, nor the names of its contributors may be used to
 *      endorse or promote products derived from this Software without specific
 *      prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
 * SOFTWARE.
 *
 * ==============================================================================
 *
 * Copyright (c) 2009-2014 by the contributors of LLVM/libc++abi project.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include "FTLUnwindInfo.h"

#if ENABLE(FTL_JIT)

#if OS(DARWIN)
#include <mach-o/compact_unwind_encoding.h>
#endif
#include <wtf/ListDump.h>

namespace JSC { namespace FTL {

UnwindInfo::UnwindInfo() { }
UnwindInfo::~UnwindInfo() { }


namespace {
#if OS(DARWIN)
struct CompactUnwind {
    void* function;
    uint32_t size;
    compact_unwind_encoding_t encoding;
    void* personality;
    void* lsda;
};
#elif OS(LINUX)
// DWARF unwind instructions
enum {
    DW_CFA_nop                 = 0x0,
    DW_CFA_set_loc             = 0x1,
    DW_CFA_advance_loc1        = 0x2,
    DW_CFA_advance_loc2        = 0x3,
    DW_CFA_advance_loc4        = 0x4,
    DW_CFA_offset_extended     = 0x5,
    DW_CFA_def_cfa             = 0xC,
    DW_CFA_def_cfa_register    = 0xD,
    DW_CFA_def_cfa_offset      = 0xE,
    DW_CFA_offset_extended_sf = 0x11,
    DW_CFA_def_cfa_sf         = 0x12,
    DW_CFA_def_cfa_offset_sf  = 0x13,
    // high 2 bits are 0x1, lower 6 bits are delta
    DW_CFA_advance_loc        = 0x40,
    // high 2 bits are 0x2, lower 6 bits are register
    DW_CFA_offset             = 0x80
};

enum {
    DW_CFA_operand_mask = 0x3F // low 6 bits mask for opcode-encoded operands in DW_CFA_advance_loc and DW_CFA_offset
};

// FSF exception handling Pointer-Encoding constants
enum {
    DW_EH_PE_ptr       = 0x00,
    DW_EH_PE_uleb128   = 0x01,
    DW_EH_PE_udata2    = 0x02,
    DW_EH_PE_udata4    = 0x03,
    DW_EH_PE_udata8    = 0x04,
    DW_EH_PE_sleb128   = 0x09,
    DW_EH_PE_sdata2    = 0x0A,
    DW_EH_PE_sdata4    = 0x0B,
    DW_EH_PE_sdata8    = 0x0C,
    DW_EH_PE_absptr    = 0x00,
    DW_EH_PE_pcrel     = 0x10,
    DW_EH_PE_indirect  = 0x80
};

enum {
    DW_EH_PE_relative_mask = 0x70
};

// 64-bit x86_64 registers
enum {
    UNW_X86_64_rbx =  3,
    UNW_X86_64_rbp =  6,
    UNW_X86_64_r12 = 12,
    UNW_X86_64_r13 = 13,
    UNW_X86_64_r14 = 14,
    UNW_X86_64_r15 = 15
};

enum {
    DW_X86_64_RET_addr = 16
};

static uint8_t get8(uintptr_t addr)    { return *((uint8_t*)addr); }
static uint16_t get16(uintptr_t addr)  { return *((uint16_t*)addr); }
static uint32_t get32(uintptr_t addr)  { return *((uint32_t*)addr); }
static uint64_t get64(uintptr_t addr)  { return *((uint64_t*)addr); }

static uintptr_t getP(uintptr_t addr)
{
    // FIXME: add support for 32 bit pointers on 32 bit architectures
    return get64(addr);
}

static uint64_t getULEB128(uintptr_t& addr, uintptr_t end)
{
    const uint8_t* p = (uint8_t*)addr;
    const uint8_t* pend = (uint8_t*)end;
    uint64_t result = 0;
    int bit = 0;
    do  {
        uint64_t b;

        RELEASE_ASSERT(p != pend); // truncated uleb128 expression

        b = *p & 0x7f;

        RELEASE_ASSERT(!(bit >= 64 || b << bit >> bit != b)); // malformed uleb128 expression

        result |= b << bit;
        bit += 7;
    } while (*p++ >= 0x80);
    addr = (uintptr_t)p;
    return result;
}

static int64_t getSLEB128(uintptr_t& addr, uintptr_t end)
{
    const uint8_t* p = (uint8_t*)addr;
    const uint8_t* pend = (uint8_t*)end;

    int64_t result = 0;
    int bit = 0;
    uint8_t byte;
    do {
        RELEASE_ASSERT(p != pend); // truncated sleb128 expression

        byte = *p++;
        result |= ((byte & 0x7f) << bit);
        bit += 7;
    } while (byte & 0x80);
    // sign extend negative numbers
    if ((byte & 0x40))
        result |= (-1LL) << bit;
    addr = (uintptr_t)p;
    return result;
}

static uintptr_t getEncodedP(uintptr_t& addr, uintptr_t end, uint8_t encoding)
{
    uintptr_t startAddr = addr;
    const uint8_t* p = (uint8_t*)addr;
    uintptr_t result;

    // first get value
    switch (encoding & 0x0F) {
    case DW_EH_PE_ptr:
        result = getP(addr);
        p += sizeof(uintptr_t);
        addr = (uintptr_t)p;
        break;
    case DW_EH_PE_uleb128:
        result = getULEB128(addr, end);
        break;
    case DW_EH_PE_udata2:
        result = get16(addr);
        p += 2;
        addr = (uintptr_t)p;
        break;
    case DW_EH_PE_udata4:
        result = get32(addr);
        p += 4;
        addr = (uintptr_t)p;
        break;
    case DW_EH_PE_udata8:
        result = get64(addr);
        p += 8;
        addr = (uintptr_t)p;
        break;
    case DW_EH_PE_sleb128:
        result = getSLEB128(addr, end);
        break;
    case DW_EH_PE_sdata2:
        result = (int16_t)get16(addr);
        p += 2;
        addr = (uintptr_t)p;
        break;
    case DW_EH_PE_sdata4:
        result = (int32_t)get32(addr);
        p += 4;
        addr = (uintptr_t)p;
        break;
    case DW_EH_PE_sdata8:
        result = get64(addr);
        p += 8;
        addr = (uintptr_t)p;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED(); // unknown pointer encoding
    }

    // then add relative offset
    switch (encoding & DW_EH_PE_relative_mask) {
    case DW_EH_PE_absptr:
        // do nothing
        break;
    case DW_EH_PE_pcrel:
        result += startAddr;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED(); // unsupported or unknown pointer encoding
    }

    if (encoding & DW_EH_PE_indirect)
        result = getP(result);

    return result;
}

// Information encoded in a CIE (Common Information Entry)
struct CIE_Info {
    uintptr_t cieStart;
    uintptr_t cieLength;
    uintptr_t cieInstructions;
    uint8_t pointerEncoding;
    uint8_t lsdaEncoding;
    uint8_t personalityEncoding;
    uint8_t personalityOffsetInCIE;
    uintptr_t personality;
    int dataAlignFactor;
    bool fdesHaveAugmentationData;
};

// Information about an FDE (Frame Description Entry)
struct FDE_Info {
    uintptr_t fdeStart;
    uintptr_t fdeLength;
    uintptr_t fdeInstructions;
    uintptr_t lsda;
};

// Information about a frame layout and registers saved determined
// by "running" the dwarf FDE "instructions"
enum { MaxRegisterNumber = 17 };

struct RegisterLocation {
    bool saved;
    int64_t offset;
};

struct PrologInfo {
    uint32_t cfaRegister;
    int32_t cfaRegisterOffset; // CFA = (cfaRegister)+cfaRegisterOffset
    RegisterLocation savedRegisters[MaxRegisterNumber]; // from where to restore registers
};

static void parseCIE(uintptr_t cie, CIE_Info* cieInfo)
{
    cieInfo->pointerEncoding = 0;
    cieInfo->lsdaEncoding = 0;
    cieInfo->personalityEncoding = 0;
    cieInfo->personalityOffsetInCIE = 0;
    cieInfo->personality = 0;
    cieInfo->dataAlignFactor = 0;
    cieInfo->fdesHaveAugmentationData = false;
    cieInfo->cieStart = cie;

    uintptr_t p = cie;
    uint64_t cieLength = get32(p);
    p += 4;
    uintptr_t cieContentEnd = p + cieLength;
    if (cieLength == 0xffffffff) {
        // 0xffffffff means length is really next 8 bytes
        cieLength = get64(p);
        p += 8;
        cieContentEnd = p + cieLength;
    }

    RELEASE_ASSERT(cieLength);

    // CIE ID is always 0
    RELEASE_ASSERT(!get32(p)); // CIE ID is not zero
    p += 4;
    // Version is always 1 or 3
    uint8_t version = get8(p);
    RELEASE_ASSERT((version == 1) || (version == 3)); // CIE version is not 1 or 3

    ++p;
    // save start of augmentation string and find end
    uintptr_t strStart = p;
    while (get8(p))
        ++p;
    ++p;
    // parse code aligment factor
    getULEB128(p, cieContentEnd);
    // parse data alignment factor
    cieInfo->dataAlignFactor = getSLEB128(p, cieContentEnd);
    // parse return address register
    getULEB128(p, cieContentEnd);
    // parse augmentation data based on augmentation string
    if (get8(strStart) == 'z') {
        // parse augmentation data length
        getULEB128(p, cieContentEnd);
        for (uintptr_t s = strStart; get8(s) != '\0'; ++s) {
            switch (get8(s)) {
            case 'z':
                cieInfo->fdesHaveAugmentationData = true;
                break;
            case 'P': // FIXME: should assert on personality (just to keep in sync with the CU behaviour)
                cieInfo->personalityEncoding = get8(p);
                ++p;
                cieInfo->personalityOffsetInCIE = p - cie;
                cieInfo->personality = getEncodedP(p, cieContentEnd, cieInfo->personalityEncoding);
                break;
            case 'L': // FIXME: should assert on LSDA (just to keep in sync with the CU behaviour)
                cieInfo->lsdaEncoding = get8(p);
                ++p;
                break;
            case 'R':
                cieInfo->pointerEncoding = get8(p);
                ++p;
                break;
            default:
                // ignore unknown letters
                break;
            }
        }
    }
    cieInfo->cieLength = cieContentEnd - cieInfo->cieStart;
    cieInfo->cieInstructions = p;
}

static void findFDE(uintptr_t pc, uintptr_t ehSectionStart, uint32_t sectionLength, FDE_Info* fdeInfo, CIE_Info* cieInfo)
{
    uintptr_t p = ehSectionStart;
    const uintptr_t ehSectionEnd = p + sectionLength;
    while (p < ehSectionEnd) {
        uintptr_t currentCFI = p;
        uint64_t cfiLength = get32(p);
        p += 4;
        if (cfiLength == 0xffffffff) {
            // 0xffffffff means length is really next 8 bytes
            cfiLength = get64(p);
            p += 8;
        }
        RELEASE_ASSERT(cfiLength); // end marker reached before finding FDE for pc

        uint32_t id = get32(p);
        if (!id) {
            // skip over CIEs
            p += cfiLength;
        } else {
            // process FDE to see if it covers pc
            uintptr_t nextCFI = p + cfiLength;
            uint32_t ciePointer = get32(p);
            uintptr_t cieStart = p - ciePointer;
            // validate pointer to CIE is within section
            RELEASE_ASSERT((ehSectionStart <= cieStart) && (cieStart < ehSectionEnd)); // malformed FDE. CIE is bad

            parseCIE(cieStart, cieInfo);

            p += 4;
            // parse pc begin and range
            uintptr_t pcStart = getEncodedP(p, nextCFI, cieInfo->pointerEncoding);
            uintptr_t pcRange = getEncodedP(p, nextCFI, cieInfo->pointerEncoding & 0x0F);

            // test if pc is within the function this FDE covers
            // if pc is not in begin/range, skip this FDE
            if ((pcStart <= pc) && (pc < pcStart+pcRange)) {
                // parse rest of info
                fdeInfo->lsda = 0;
                // check for augmentation length
                if (cieInfo->fdesHaveAugmentationData) {
                    uintptr_t augLen = getULEB128(p, nextCFI);
                    uintptr_t endOfAug = p + augLen;
                    if (cieInfo->lsdaEncoding) {
                        // peek at value (without indirection). Zero means no lsda
                        uintptr_t lsdaStart = p;
                        if (getEncodedP(p, nextCFI, cieInfo->lsdaEncoding & 0x0F)) {
                            // reset pointer and re-parse lsda address
                            p = lsdaStart;
                            fdeInfo->lsda = getEncodedP(p, nextCFI, cieInfo->lsdaEncoding);
                        }
                    }
                    p = endOfAug;
                }
                fdeInfo->fdeStart = currentCFI;
                fdeInfo->fdeLength = nextCFI - currentCFI;
                fdeInfo->fdeInstructions = p;

                return; // FDE found
            }

            p = nextCFI;
        }
    }

    RELEASE_ASSERT_NOT_REACHED(); // no FDE found for pc
}

static void executeDefCFARegister(uint64_t reg, PrologInfo* results)
{
    RELEASE_ASSERT(reg <= MaxRegisterNumber); // reg too big
    results->cfaRegister = reg;
}

static void executeDefCFAOffset(int64_t offset, PrologInfo* results)
{
    RELEASE_ASSERT(offset <= 0x80000000); // cfa has negative offset (dwarf might contain epilog)
    results->cfaRegisterOffset = offset;
}

static void executeOffset(uint64_t reg, int64_t offset, PrologInfo *results)
{
    if (reg > MaxRegisterNumber)
        return;

    RELEASE_ASSERT(!results->savedRegisters[reg].saved);
    results->savedRegisters[reg].saved = true;
    results->savedRegisters[reg].offset = offset;
}

static void parseInstructions(uintptr_t instructions, uintptr_t instructionsEnd, const CIE_Info& cieInfo, PrologInfo* results)
{
    uintptr_t p = instructions;

    // see Dwarf Spec, section 6.4.2 for details on unwind opcodes
    while ((p < instructionsEnd)) {
        uint64_t reg;
        uint8_t opcode = get8(p);
        uint8_t operand;
        ++p;
        switch (opcode) {
        case DW_CFA_nop:
            break;
        case DW_CFA_set_loc:
            getEncodedP(p, instructionsEnd, cieInfo.pointerEncoding);
            break;
        case DW_CFA_advance_loc1:
            p += 1;
            break;
        case DW_CFA_advance_loc2:
            p += 2;
            break;
        case DW_CFA_advance_loc4:
            p += 4;
            break;
        case DW_CFA_def_cfa:
            executeDefCFARegister(getULEB128(p, instructionsEnd), results);
            executeDefCFAOffset(getULEB128(p, instructionsEnd), results);
            break;
        case DW_CFA_def_cfa_sf:
            executeDefCFARegister(getULEB128(p, instructionsEnd), results);
            executeDefCFAOffset(getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor, results);
            break;
        case DW_CFA_def_cfa_register:
            executeDefCFARegister(getULEB128(p, instructionsEnd), results);
            break;
        case DW_CFA_def_cfa_offset:
            executeDefCFAOffset(getULEB128(p, instructionsEnd), results);
            break;
        case DW_CFA_def_cfa_offset_sf:
            executeDefCFAOffset(getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor, results);
            break;
        case DW_CFA_offset_extended:
            reg = getULEB128(p, instructionsEnd);
            executeOffset(reg, getULEB128(p, instructionsEnd) * cieInfo.dataAlignFactor, results);
            break;
        case DW_CFA_offset_extended_sf:
            reg = getULEB128(p, instructionsEnd);
            executeOffset(reg, getSLEB128(p, instructionsEnd) * cieInfo.dataAlignFactor, results);
            break;
        default:
            operand = opcode & DW_CFA_operand_mask;
            switch (opcode & ~DW_CFA_operand_mask) {
            case DW_CFA_offset:
                executeOffset(operand, getULEB128(p, instructionsEnd) * cieInfo.dataAlignFactor, results);
                break;
            case DW_CFA_advance_loc:
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED(); // unknown or unsupported CFA opcode
            }
        }
    }
}

static void parseFDEInstructions(const FDE_Info& fdeInfo, const CIE_Info& cieInfo, PrologInfo* results)
{
    // clear results
    bzero(results, sizeof(PrologInfo));

    // parse CIE then FDE instructions
    parseInstructions(cieInfo.cieInstructions, cieInfo.cieStart + cieInfo.cieLength, cieInfo, results);
    parseInstructions(fdeInfo.fdeInstructions, fdeInfo.fdeStart + fdeInfo.fdeLength, cieInfo, results);
}
#endif
} // anonymous namespace

bool UnwindInfo::parse(void* section, size_t size, GeneratedFunction generatedFunction)
{
    m_registers.clear();
    RELEASE_ASSERT(!!section);
    if (!section)
        return false;

#if OS(DARWIN)
    RELEASE_ASSERT(size >= sizeof(CompactUnwind));
    
    CompactUnwind* data = bitwise_cast<CompactUnwind*>(section);
    
    RELEASE_ASSERT(!data->personality); // We don't know how to handle this.
    RELEASE_ASSERT(!data->lsda); // We don't know how to handle this.
    RELEASE_ASSERT(data->function == generatedFunction); // The unwind data better be for our function.
    
    compact_unwind_encoding_t encoding = data->encoding;
    RELEASE_ASSERT(!(encoding & UNWIND_IS_NOT_FUNCTION_START));
    RELEASE_ASSERT(!(encoding & UNWIND_HAS_LSDA));
    RELEASE_ASSERT(!(encoding & UNWIND_PERSONALITY_MASK));

#if CPU(X86_64)
    RELEASE_ASSERT((encoding & UNWIND_X86_64_MODE_MASK) == UNWIND_X86_64_MODE_RBP_FRAME);
    
    int32_t offset = -((encoding & UNWIND_X86_64_RBP_FRAME_OFFSET) >> 16) * 8;
    uint32_t nextRegisters = encoding;
    for (unsigned i = 5; i--;) {
        uint32_t currentRegister = nextRegisters & 7;
        nextRegisters >>= 3;
        
        switch (currentRegister) {
        case UNWIND_X86_64_REG_NONE:
            break;
            
        case UNWIND_X86_64_REG_RBX:
            m_registers.append(RegisterAtOffset(X86Registers::ebx, offset));
            break;
            
        case UNWIND_X86_64_REG_R12:
            m_registers.append(RegisterAtOffset(X86Registers::r12, offset));
            break;
            
        case UNWIND_X86_64_REG_R13:
            m_registers.append(RegisterAtOffset(X86Registers::r13, offset));
            break;
            
        case UNWIND_X86_64_REG_R14:
            m_registers.append(RegisterAtOffset(X86Registers::r14, offset));
            break;
            
        case UNWIND_X86_64_REG_R15:
            m_registers.append(RegisterAtOffset(X86Registers::r15, offset));
            break;
            
        case UNWIND_X86_64_REG_RBP:
            m_registers.append(RegisterAtOffset(X86Registers::ebp, offset));
            break;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        offset += 8;
    }
#elif CPU(ARM64)
    RELEASE_ASSERT((encoding & UNWIND_ARM64_MODE_MASK) == UNWIND_ARM64_MODE_FRAME);
    
    m_registers.append(RegisterAtOffset(ARM64Registers::fp, 0));
    
    int32_t offset = 0;
    if (encoding & UNWIND_ARM64_FRAME_X19_X20_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x19, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x20, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X21_X22_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x21, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x22, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X23_X24_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x23, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x24, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X25_X26_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x25, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x26, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_X27_X28_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::x27, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::x28, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D8_D9_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q8, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q9, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D10_D11_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q10, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q11, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D12_D13_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q12, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q13, offset -= 8));
    }
    if (encoding & UNWIND_ARM64_FRAME_D14_D15_PAIR) {
        m_registers.append(RegisterAtOffset(ARM64Registers::q14, offset -= 8));
        m_registers.append(RegisterAtOffset(ARM64Registers::q15, offset -= 8));
    }
#else
#error "Unrecognized architecture"
#endif

#elif OS(LINUX)
    FDE_Info fdeInfo;
    CIE_Info cieInfo;
    PrologInfo prolog;

    findFDE((uintptr_t)generatedFunction, (uintptr_t)section, size, &fdeInfo, &cieInfo);
    parseFDEInstructions(fdeInfo, cieInfo, &prolog);

#if CPU(X86_64)
    RELEASE_ASSERT(prolog.cfaRegister == UNW_X86_64_rbp);
    RELEASE_ASSERT(prolog.cfaRegisterOffset == 16);
    RELEASE_ASSERT(prolog.savedRegisters[UNW_X86_64_rbp].saved);
    RELEASE_ASSERT(prolog.savedRegisters[UNW_X86_64_rbp].offset == -prolog.cfaRegisterOffset);

    for (int i = 0; i < MaxRegisterNumber; ++i) {
        if (prolog.savedRegisters[i].saved) {
            switch (i) {
            case UNW_X86_64_rbx:
                m_registers.append(RegisterAtOffset(X86Registers::ebx, prolog.savedRegisters[i].offset + prolog.cfaRegisterOffset));
                break;
            case UNW_X86_64_r12:
                m_registers.append(RegisterAtOffset(X86Registers::r12, prolog.savedRegisters[i].offset + prolog.cfaRegisterOffset));
                break;
            case UNW_X86_64_r13:
                m_registers.append(RegisterAtOffset(X86Registers::r13, prolog.savedRegisters[i].offset + prolog.cfaRegisterOffset));
                break;
            case UNW_X86_64_r14:
                m_registers.append(RegisterAtOffset(X86Registers::r14, prolog.savedRegisters[i].offset + prolog.cfaRegisterOffset));
                break;
            case UNW_X86_64_r15:
                m_registers.append(RegisterAtOffset(X86Registers::r15, prolog.savedRegisters[i].offset + prolog.cfaRegisterOffset));
                break;
            case UNW_X86_64_rbp:
                m_registers.append(RegisterAtOffset(X86Registers::ebp, prolog.savedRegisters[i].offset + prolog.cfaRegisterOffset));
                break;
            case DW_X86_64_RET_addr:
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED(); // non-standard register being saved in prolog
            }
        }
    }
#elif CPU(ARM64)
    // FIXME: Implement stackunwinding based on eh_frame on ARM64
#else
#error "Unrecognized architecture"
#endif

#endif
    std::sort(m_registers.begin(), m_registers.end());
    return true;
}

void UnwindInfo::dump(PrintStream& out) const
{
    out.print(listDump(m_registers));
}

RegisterAtOffset* UnwindInfo::find(Reg reg) const
{
    return tryBinarySearch<RegisterAtOffset, Reg>(m_registers, m_registers.size(), reg, RegisterAtOffset::getReg);
}

unsigned UnwindInfo::indexOf(Reg reg) const
{
    if (RegisterAtOffset* pointer = find(reg))
        return pointer - m_registers.begin();
    return UINT_MAX;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

