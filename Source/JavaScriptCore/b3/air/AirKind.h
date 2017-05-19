/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef AirKind_h
#define AirKind_h

#if ENABLE(B3_JIT)

#include "AirOpcode.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace B3 { namespace Air {

// Air opcodes are always carried around with some flags. These flags are understood as having no
// meaning if they are set for an opcode to which they do not apply. This makes sense, since Air
// is a complex instruction set and most of these flags can apply to basically any opcode. In
// fact, it's recommended to only represent something as a flag if you believe that it is largely
// opcode-agnostic.

struct Kind {
    Kind(Opcode opcode)
        : opcode(opcode)
        , effects(false)
    {
    }
    
    Kind()
        : Kind(Nop)
    {
    }
    
    bool operator==(const Kind& other) const
    {
        return opcode == other.opcode
            && effects == other.effects;
    }
    
    bool operator!=(const Kind& other) const
    {
        return !(*this == other);
    }
    
    unsigned hash() const
    {
        return static_cast<unsigned>(opcode) + (static_cast<unsigned>(effects) << 16);
    }
    
    explicit operator bool() const
    {
        return *this != Kind();
    }
    
    void dump(PrintStream&) const;
    
    Opcode opcode;
    
    // This is an opcode-agnostic flag that indicates that we expect that this instruction will do
    // any of the following:
    // - Trap.
    // - Perform some non-arg non-control effect.
    bool effects : 1;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

#endif // AirKind_h

