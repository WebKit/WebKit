/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef B3Const64Value_h
#define B3Const64Value_h

#if ENABLE(B3_JIT)

#include "B3Value.h"

namespace JSC { namespace B3 {

class JS_EXPORT_PRIVATE Const64Value : public Value {
public:
    static bool accepts(Opcode opcode) { return opcode == Const64; }
    
    ~Const64Value();
    
    int64_t value() const { return m_value; }

    Value* negConstant(Procedure&) const override;
    Value* addConstant(Procedure&, int32_t other) const override;
    Value* addConstant(Procedure&, Value* other) const override;
    Value* subConstant(Procedure&, Value* other) const override;
    Value* bitAndConstant(Procedure&, Value* other) const override;
    Value* bitOrConstant(Procedure&, Value* other) const override;
    Value* bitXorConstant(Procedure&, Value* other) const override;
    Value* shlConstant(Procedure&, Value* other) const override;
    Value* sShrConstant(Procedure&, Value* other) const override;
    Value* zShrConstant(Procedure&, Value* other) const override;

    TriState equalConstant(Value* other) const override;
    TriState notEqualConstant(Value* other) const override;
    TriState lessThanConstant(Value* other) const override;
    TriState greaterThanConstant(Value* other) const override;
    TriState lessEqualConstant(Value* other) const override;
    TriState greaterEqualConstant(Value* other) const override;
    TriState aboveConstant(Value* other) const override;
    TriState belowConstant(Value* other) const override;
    TriState aboveEqualConstant(Value* other) const override;
    TriState belowEqualConstant(Value* other) const override;

protected:
    void dumpMeta(CommaPrinter&, PrintStream&) const override;

    friend class Procedure;

    Const64Value(unsigned index, Origin origin, int64_t value)
        : Value(index, CheckedOpcode, Const64, Int64, origin)
        , m_value(value)
    {
    }
    
private:
    int64_t m_value;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Const64Value_h

