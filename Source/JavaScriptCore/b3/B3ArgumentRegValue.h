/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3Value.h"
#include "Reg.h"

namespace JSC { namespace B3 {

class JS_EXPORT_PRIVATE ArgumentRegValue final : public Value {
public:
    enum VectorTag { UsesVectorArgs };

    static bool accepts(Kind kind) { return kind == ArgumentReg; }
    
    ~ArgumentRegValue() final;

    Reg argumentReg() const { return m_reg[0]; }

#if CPU(ARM_THUMB2)
    bool isGPPair() const { return m_reg[1].isSet(); }

    Reg hiReg() const { return m_reg[0]; }
    Reg loReg() const { return m_reg[1]; }
#else
    bool isGPPair() const { return false; }
#endif

    B3_SPECIALIZE_VALUE_FOR_NO_CHILDREN

private:
    void dumpMeta(CommaPrinter&, PrintStream&) const final;

    friend class Procedure;
    friend class Value;
    
    static Opcode opcodeFromConstructor(Origin, Reg) { return ArgumentReg; }
    static Opcode opcodeFromConstructor(Origin, Reg, VectorTag) { return ArgumentReg; }
#if CPU(ARM_THUMB2)
    static Opcode opcodeFromConstructor(Origin, Reg, Reg) { return ArgumentReg; }
#endif

    ArgumentRegValue(Origin origin, Reg reg)
        : Value(CheckedOpcode, ArgumentReg, reg.isGPR() ? pointerType() : Double, Zero, origin)
        , m_reg { reg }
    {
        ASSERT(reg.isSet());
    }

    ArgumentRegValue(Origin origin, Reg reg, VectorTag)
        : Value(CheckedOpcode, ArgumentReg, V128, Zero, origin)
        , m_reg { reg }
    {
        ASSERT(reg.isSet());
        ASSERT(reg.isFPR());
    }

#if CPU(ARM_THUMB2)
    ArgumentRegValue(Origin origin, Reg hi, Reg lo)
        : Value(CheckedOpcode, ArgumentReg, Int64, Zero, origin)
        , m_reg { hi, lo }
    {
        ASSERT(m_reg[0].isSet());
        ASSERT(m_reg[0].isGPR());
        ASSERT(m_reg[1].isSet());
        ASSERT(m_reg[1].isGPR());
    }
#endif

    static constexpr size_t regCount = isARM_THUMB2() ? 2 : 1;
    Reg m_reg[regCount];
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
