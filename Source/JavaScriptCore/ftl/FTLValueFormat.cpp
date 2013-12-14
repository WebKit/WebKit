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

#include "config.h"
#include "FTLValueFormat.h"

#if ENABLE(FTL_JIT)

#include "AssemblyHelpers.h"

namespace JSC { namespace FTL {

void reboxAccordingToFormat(
    ValueFormat format, AssemblyHelpers& jit, GPRReg value, GPRReg scratch1, GPRReg scratch2)
{
    switch (format) {
    case ValueFormatInt32: {
        jit.zeroExtend32ToPtr(value, value);
        jit.or64(GPRInfo::tagTypeNumberRegister, value);
        break;
    }
    
    case ValueFormatInt52: {
        jit.rshift64(AssemblyHelpers::TrustedImm32(JSValue::int52ShiftAmount), value);
        jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch2);
        jit.boxInt52(value, value, scratch1, FPRInfo::fpRegT0);
        jit.move64ToDouble(scratch2, FPRInfo::fpRegT0);
        break;
    }
    
    case ValueFormatStrictInt52: {
        jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch2);
        jit.boxInt52(value, value, scratch1, FPRInfo::fpRegT0);
        jit.move64ToDouble(scratch2, FPRInfo::fpRegT0);
        break;
    }
    
    case ValueFormatBoolean: {
        jit.zeroExtend32ToPtr(value, value);
        jit.or32(MacroAssembler::TrustedImm32(ValueFalse), value);
        break;
    }
    
    case ValueFormatJSValue: {
        // Done already!
        break;
    }
    
    case ValueFormatDouble: {
        jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch1);
        jit.move64ToDouble(value, FPRInfo::fpRegT0);
        jit.boxDouble(FPRInfo::fpRegT0, value);
        jit.move64ToDouble(scratch1, FPRInfo::fpRegT0);
        break;
    }
    
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

} } // namespace JSC::FTL

namespace WTF {

using namespace JSC::FTL;

void printInternal(PrintStream& out, ValueFormat format)
{
    switch (format) {
    case InvalidValueFormat:
        out.print("Invalid");
        return;
    case ValueFormatInt32:
        out.print("Int32");
        return;
    case ValueFormatInt52:
        out.print("Int52");
        return;
    case ValueFormatStrictInt52:
        out.print("StrictInt52");
        return;
    case ValueFormatBoolean:
        out.print("Boolean");
        return;
    case ValueFormatJSValue:
        out.print("JSValue");
        return;
    case ValueFormatDouble:
        out.print("Double");
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(FTL_JIT)

