/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLStackMaps_h
#define FTLStackMaps_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "DataView.h"
#include "GPRInfo.h"
#include <wtf/HashMap.h>

namespace JSC {

class MacroAssembler;

namespace FTL {

struct StackMaps {
    struct Constant {
        int64_t integer;
        
        void parse(DataView*, unsigned& offset);
        void dump(PrintStream& out) const;
    };

    struct StackSize {
        uint32_t functionOffset;
        uint32_t size;

        void parse(DataView*, unsigned& offset);
        void dump(PrintStream&) const;
    };

    struct Location {
        enum Kind : int8_t {
            Unprocessed,
            Register,
            Direct,
            Indirect,
            Constant,
            ConstantIndex
        };
        
        uint16_t dwarfRegNum; // Represented as a 12-bit int in the section.
        int8_t size;
        Kind kind;
        int32_t offset;
        
        void parse(DataView*, unsigned& offset);
        void dump(PrintStream& out) const;
        
        GPRReg directGPR() const;
        void restoreInto(MacroAssembler&, StackMaps&, char* savedRegisters, GPRReg result) const;
    };
    
    struct Record {
        uint32_t patchpointID;
        uint32_t instructionOffset;
        uint16_t flags;
        
        Vector<Location> locations;
        
        bool parse(DataView*, unsigned& offset);
        void dump(PrintStream&) const;
    };

    Vector<StackSize> stackSizes;
    Vector<Constant> constants;
    Vector<Record> records;
    
    bool parse(DataView*); // Returns true on parse success, false on failure. Failure means that LLVM is signaling compile failure to us.
    void dump(PrintStream&) const;
    void dumpMultiline(PrintStream&, const char* prefix) const;
    
    typedef HashMap<uint32_t, Vector<Record>, WTF::IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> RecordMap;
    
    RecordMap computeRecordMap() const;

    unsigned stackSize() const;
};

} } // namespace JSC::FTL

namespace WTF {

void printInternal(PrintStream&, JSC::FTL::StackMaps::Location::Kind);

} // namespace WTF

#endif // ENABLE(FTL_JIT)

#endif // FTLStackMaps_h

