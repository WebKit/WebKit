/*
 * Copyright (C) 2011, 2013, 2014 Apple Inc. All rights reserved.
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
#include "AssemblyHelpers.h"

#if ENABLE(JIT)

#include "JSCInlines.h"

namespace JSC {

ExecutableBase* AssemblyHelpers::executableFor(const CodeOrigin& codeOrigin)
{
    if (!codeOrigin.inlineCallFrame)
        return m_codeBlock->ownerExecutable();
    
    return codeOrigin.inlineCallFrame->executable.get();
}

Vector<BytecodeAndMachineOffset>& AssemblyHelpers::decodedCodeMapFor(CodeBlock* codeBlock)
{
    ASSERT(codeBlock == codeBlock->baselineVersion());
    ASSERT(codeBlock->jitType() == JITCode::BaselineJIT);
    ASSERT(codeBlock->jitCodeMap());
    
    HashMap<CodeBlock*, Vector<BytecodeAndMachineOffset>>::AddResult result = m_decodedCodeMaps.add(codeBlock, Vector<BytecodeAndMachineOffset>());
    
    if (result.isNewEntry)
        codeBlock->jitCodeMap()->decode(result.iterator->value);
    
    return result.iterator->value;
}

void AssemblyHelpers::sanitizeDouble(FPRReg fpr)
{
    MacroAssembler::Jump notNaN = branchDouble(DoubleEqual, fpr, fpr);
    static const double NaN = QNaN;
    loadDouble(&NaN, fpr);
    notNaN.link(this);
}

#if ENABLE(SAMPLING_FLAGS)
void AssemblyHelpers::setSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    or32(TrustedImm32(1u << (flag - 1)), AbsoluteAddress(SamplingFlags::addressOfFlags()));
}

void AssemblyHelpers::clearSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    and32(TrustedImm32(~(1u << (flag - 1))), AbsoluteAddress(SamplingFlags::addressOfFlags()));
}
#endif

#if !ASSERT_DISABLED
#if USE(JSVALUE64)
void AssemblyHelpers::jitAssertIsInt32(GPRReg gpr)
{
#if CPU(X86_64)
    Jump checkInt32 = branch64(BelowOrEqual, gpr, TrustedImm64(static_cast<uintptr_t>(0xFFFFFFFFu)));
    breakpoint();
    checkInt32.link(this);
#else
    UNUSED_PARAM(gpr);
#endif
}

void AssemblyHelpers::jitAssertIsJSInt32(GPRReg gpr)
{
    Jump checkJSInt32 = branch64(AboveOrEqual, gpr, GPRInfo::tagTypeNumberRegister);
    breakpoint();
    checkJSInt32.link(this);
}

void AssemblyHelpers::jitAssertIsJSNumber(GPRReg gpr)
{
    Jump checkJSNumber = branchTest64(MacroAssembler::NonZero, gpr, GPRInfo::tagTypeNumberRegister);
    breakpoint();
    checkJSNumber.link(this);
}

void AssemblyHelpers::jitAssertIsJSDouble(GPRReg gpr)
{
    Jump checkJSInt32 = branch64(AboveOrEqual, gpr, GPRInfo::tagTypeNumberRegister);
    Jump checkJSNumber = branchTest64(MacroAssembler::NonZero, gpr, GPRInfo::tagTypeNumberRegister);
    checkJSInt32.link(this);
    breakpoint();
    checkJSNumber.link(this);
}

void AssemblyHelpers::jitAssertIsCell(GPRReg gpr)
{
    Jump checkCell = branchTest64(MacroAssembler::Zero, gpr, GPRInfo::tagMaskRegister);
    breakpoint();
    checkCell.link(this);
}

void AssemblyHelpers::jitAssertTagsInPlace()
{
    Jump ok = branch64(Equal, GPRInfo::tagTypeNumberRegister, TrustedImm64(TagTypeNumber));
    breakpoint();
    ok.link(this);
    
    ok = branch64(Equal, GPRInfo::tagMaskRegister, TrustedImm64(TagMask));
    breakpoint();
    ok.link(this);
}
#elif USE(JSVALUE32_64)
void AssemblyHelpers::jitAssertIsInt32(GPRReg gpr)
{
    UNUSED_PARAM(gpr);
}

void AssemblyHelpers::jitAssertIsJSInt32(GPRReg gpr)
{
    Jump checkJSInt32 = branch32(Equal, gpr, TrustedImm32(JSValue::Int32Tag));
    breakpoint();
    checkJSInt32.link(this);
}

void AssemblyHelpers::jitAssertIsJSNumber(GPRReg gpr)
{
    Jump checkJSInt32 = branch32(Equal, gpr, TrustedImm32(JSValue::Int32Tag));
    Jump checkJSDouble = branch32(Below, gpr, TrustedImm32(JSValue::LowestTag));
    breakpoint();
    checkJSInt32.link(this);
    checkJSDouble.link(this);
}

void AssemblyHelpers::jitAssertIsJSDouble(GPRReg gpr)
{
    Jump checkJSDouble = branch32(Below, gpr, TrustedImm32(JSValue::LowestTag));
    breakpoint();
    checkJSDouble.link(this);
}

void AssemblyHelpers::jitAssertIsCell(GPRReg gpr)
{
    Jump checkCell = branch32(Equal, gpr, TrustedImm32(JSValue::CellTag));
    breakpoint();
    checkCell.link(this);
}

void AssemblyHelpers::jitAssertTagsInPlace()
{
}
#endif // USE(JSVALUE32_64)

void AssemblyHelpers::jitAssertHasValidCallFrame()
{
    Jump checkCFR = branchTestPtr(Zero, GPRInfo::callFrameRegister, TrustedImm32(7));
    breakpoint();
    checkCFR.link(this);
}

void AssemblyHelpers::jitAssertIsNull(GPRReg gpr)
{
    Jump checkNull = branchTestPtr(Zero, gpr);
    breakpoint();
    checkNull.link(this);
}

void AssemblyHelpers::jitAssertArgumentCountSane()
{
    Jump ok = branch32(Below, payloadFor(JSStack::ArgumentCount), TrustedImm32(10000000));
    breakpoint();
    ok.link(this);
}
#endif // !ASSERT_DISABLED

} // namespace JSC

#endif // ENABLE(JIT)

