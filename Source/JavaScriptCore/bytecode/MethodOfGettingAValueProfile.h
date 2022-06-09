/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

// This is guarded by ENABLE_DFG_JIT only because it uses some value profiles
// that are currently only used if the DFG is enabled (i.e. they are not
// available in the profile-only configuration). Hopefully someday all of
// these #if's will disappear...
#if ENABLE(DFG_JIT)

#include "BytecodeIndex.h"
#include "GPRInfo.h"
#include "Operands.h"
#include "TagRegistersMode.h"

namespace JSC {

class UnaryArithProfile;
class BinaryArithProfile;
class CCallHelpers;
class CodeBlock;
class LazyOperandValueProfileKey;
struct ValueProfile;

class MethodOfGettingAValueProfile {
public:
    MethodOfGettingAValueProfile()
        : m_kind(None)
    {
    }
    
    MethodOfGettingAValueProfile(ValueProfile* profile)
    {
        if (profile) {
            m_kind = Ready;
            u.profile = profile;
        } else
            m_kind = None;
    }

    MethodOfGettingAValueProfile(UnaryArithProfile* profile)
    {
        if (profile) {
            m_kind = UnaryArithProfileReady;
            u.unaryArithProfile = profile;
        } else
            m_kind = None;
    }

    MethodOfGettingAValueProfile(BinaryArithProfile* profile)
    {
        if (profile) {
            m_kind = BinaryArithProfileReady;
            u.binaryArithProfile = profile;
        } else
            m_kind = None;
    }
    
    static MethodOfGettingAValueProfile fromLazyOperand(
        CodeBlock*, const LazyOperandValueProfileKey&);
    
    explicit operator bool() const { return m_kind != None; }

    // The temporary register is only needed on 64-bits builds (for testing BigInt32).
    void emitReportValue(CCallHelpers&, JSValueRegs, GPRReg tempGPR, TagRegistersMode = HaveTagRegisters) const;
    void reportValue(JSValue);

private:
    enum Kind {
        None,
        Ready,
        UnaryArithProfileReady,
        BinaryArithProfileReady,
        LazyOperand
    };
    
    Kind m_kind;
    union Data {
        Data()
            : profile(nullptr)
        { }

        ValueProfile* profile;
        UnaryArithProfile* unaryArithProfile;
        BinaryArithProfile* binaryArithProfile;
        struct {
            CodeBlock* codeBlock;
            BytecodeIndex bytecodeOffset;
            Operand operand;
        } lazyOperand;
    } u;
};

} // namespace JSC

#endif // ENABLE(DFG_JIT)
