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

#include "config.h"
#include "AirCCallSpecial.h"

#if ENABLE(B3_JIT)

namespace JSC { namespace B3 { namespace Air {

CCallSpecial::CCallSpecial()
{
    m_clobberedRegs = RegisterSet::allRegisters();
    m_clobberedRegs.exclude(RegisterSet::stackRegisters());
    m_clobberedRegs.exclude(RegisterSet::reservedHardwareRegisters());
    m_clobberedRegs.exclude(RegisterSet::calleeSaveRegisters());
}

CCallSpecial::~CCallSpecial()
{
}

void CCallSpecial::forEachArg(Inst& inst, const ScopedLambda<Inst::EachArgCallback>& callback)
{
    callback(inst.args[1], Arg::Use, Arg::GP);
    
    for (unsigned i = 2; i < inst.args.size(); ++i) {
        // For the type, we can just query the arg's type. The arg will have a type, because we
        // require these args to be argument registers.
        callback(inst.args[i], Arg::Use, inst.args[i].type());
    }
}

bool CCallSpecial::isValid(Inst& inst)
{
    if (inst.args.size() < 2)
        return false;

    switch (inst.args[1].kind()) {
    case Arg::Imm:
        if (is32Bit())
            break;
        return false;
    case Arg::Imm64:
        if (is64Bit())
            break;
        return false;
    case Arg::Tmp:
    case Arg::Addr:
    case Arg::Stack:
    case Arg::CallArg:
        break;
    default:
        return false;
    }

    if (!inst.args[1].isGP())
        return false;

    for (unsigned i = 2; i < inst.args.size(); ++i) {
        if (!inst.args[i].isReg())
            return false;

        if (inst.args[i] == Tmp(scratchRegister))
            return false;
    }
    return true;
}

bool CCallSpecial::admitsStack(Inst&, unsigned argIndex)
{
    // Arg 1 can be a stack address for sure.
    if (argIndex == 1)
        return true;
    
    return false;
}

void CCallSpecial::reportUsedRegisters(Inst&, const RegisterSet&)
{
}

CCallHelpers::Jump CCallSpecial::generate(Inst& inst, CCallHelpers& jit, GenerationContext&)
{
    switch (inst.args[1].kind()) {
    case Arg::Imm:
    case Arg::Imm64:
        jit.move(inst.args[1].asTrustedImmPtr(), scratchRegister);
        jit.call(scratchRegister);
        break;
    case Arg::Tmp:
        jit.call(inst.args[1].gpr());
        break;
    case Arg::Addr:
        jit.call(inst.args[1].asAddress());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    return CCallHelpers::Jump();
}

const RegisterSet& CCallSpecial::extraClobberedRegs(Inst&)
{
    return m_clobberedRegs;
}

void CCallSpecial::dumpImpl(PrintStream& out) const
{
    out.print("CCall");
}

void CCallSpecial::deepDumpImpl(PrintStream& out) const
{
    out.print("function call that uses the C calling convention.");
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
