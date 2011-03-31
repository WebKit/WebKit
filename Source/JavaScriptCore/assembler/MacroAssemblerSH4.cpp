/*
 * Copyright (C) 2011 STMicroelectronics. All rights reserved.
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(SH4)

#include "MacroAssemblerSH4.h"

namespace JSC {
const Condition MacroAssemblerSH4::Equal = SH4Assembler::EQ;
const Condition MacroAssemblerSH4::NotEqual = SH4Assembler::NE;
const Condition MacroAssemblerSH4::GreaterThan = SH4Assembler::GT;
const Condition MacroAssemblerSH4::GreaterThanOrEqual = SH4Assembler::GE;
const Condition MacroAssemblerSH4::LessThan = SH4Assembler::LT;
const Condition MacroAssemblerSH4::LessThanOrEqual = SH4Assembler::LE;
const Condition MacroAssemblerSH4::UGreaterThan = SH4Assembler::HI;
const Condition MacroAssemblerSH4::UGreaterThanOrEqual = SH4Assembler::HS;
const Condition MacroAssemblerSH4::ULessThan = SH4Assembler::LI;
const Condition MacroAssemblerSH4::ULessThanOrEqual = SH4Assembler::LS;
const Condition MacroAssemblerSH4::Zero = SH4Assembler::EQ;
const Condition MacroAssemblerSH4::NonZero = SH4Assembler::NE;
const Condition MacroAssemblerSH4::Overflow = SH4Assembler::OF;
const Condition MacroAssemblerSH4::Above = SH4Assembler::HI;
const Condition MacroAssemblerSH4::AboveOrEqual = SH4Assembler::HS;
const Condition MacroAssemblerSH4::Below = SH4Assembler::LI;
const Condition MacroAssemblerSH4::BelowOrEqual = SH4Assembler::LS;
const Condition MacroAssemblerSH4::DoubleEqual = SH4Assembler::EQ;
const Condition MacroAssemblerSH4::DoubleNotEqual = SH4Assembler::NE;
const Condition MacroAssemblerSH4::DoubleGreaterThan = SH4Assembler::GT;
const Condition MacroAssemblerSH4::DoubleGreaterThanOrEqual = SH4Assembler::GE;
const Condition MacroAssemblerSH4::DoubleLessThan = SH4Assembler::LT;
const Condition MacroAssemblerSH4::DoubleLessThanOrEqual = SH4Assembler::LE;
const Condition MacroAssemblerSH4::DoubleEqualOrUnordered = SH4Assembler::EQU;
const Condition MacroAssemblerSH4::DoubleNotEqualOrUnordered = SH4Assembler::NEU;
const Condition MacroAssemblerSH4::DoubleGreaterThanOrUnordered = SH4Assembler::GTU;
const Condition MacroAssemblerSH4::DoubleGreaterThanOrEqualOrUnordered = SH4Assembler::GEU;
const Condition MacroAssemblerSH4::DoubleLessThanOrUnordered = SH4Assembler::LTU;
const Condition MacroAssemblerSH4::DoubleLessThanOrEqualOrUnordered = SH4Assembler::LEU;
const Condition MacroAssemblerSH4::Signed = SH4Assembler::SI;

void MacroAssemblerSH4::linkCall(void* code, Call call, FunctionPtr function)
{
    SH4Assembler::linkCall(code, call.m_jmp, function.value());
}

void MacroAssemblerSH4::repatchCall(CodeLocationCall call, CodeLocationLabel destination)
{
    SH4Assembler::relinkCall(call.dataLocation(), destination.executableAddress());
}

void MacroAssemblerSH4::repatchCall(CodeLocationCall call, FunctionPtr destination)
{
    SH4Assembler::relinkCall(call.dataLocation(), destination.executableAddress());
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
