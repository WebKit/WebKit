/*
* Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)
#if USE(JSVALUE32_64)
#include "JIT.h"

#include "CodeBlock.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "ResultType.h"
#include "SlowPathCall.h"


namespace JSC {

void JIT::emit_op_unsigned(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpUnsigned>();
    VirtualRegister result = bytecode.m_dst;
    VirtualRegister op1 = bytecode.m_operand;
    
    emitLoad(op1, regT1, regT0);
    
    addSlowCase(branchIfNotInt32(regT1));
    addSlowCase(branch32(LessThan, regT0, TrustedImm32(0)));
    emitStoreInt32(result, regT0, result == op1);
}

void JIT::emit_op_inc(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInc>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitLoad(srcDst, regT1, regT0);

    addSlowCase(branchIfNotInt32(regT1));
    addSlowCase(branchAdd32(Overflow, TrustedImm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

void JIT::emit_op_dec(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDec>();
    VirtualRegister srcDst = bytecode.m_srcDst;

    emitLoad(srcDst, regT1, regT0);

    addSlowCase(branchIfNotInt32(regT1));
    addSlowCase(branchSub32(Overflow, TrustedImm32(1), regT0));
    emitStoreInt32(srcDst, regT0, true);
}

// Mod (%)

/* ------------------------------ BEGIN: OP_MOD ------------------------------ */

void JIT::emit_op_mod(const Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_mod);
    slowPathCall.call();
}

void JIT::emitSlow_op_mod(const Instruction*, Vector<SlowCaseEntry>::iterator&)
{
    // We would have really useful assertions here if it wasn't for the compiler's
    // insistence on attribute noreturn.
    // RELEASE_ASSERT_NOT_REACHED();
}

/* ------------------------------ END: OP_MOD ------------------------------ */

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
