/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#include "ValueRecovery.h"

#include "JSCJSValueInlines.h"

namespace JSC {

JSValue ValueRecovery::recover(CallFrame* callFrame) const
{
    switch (technique()) {
    case DisplacedInJSStack:
        return callFrame->r(virtualRegister()).jsValue();
    case Int32DisplacedInJSStack:
        return jsNumber(callFrame->r(virtualRegister()).unboxedInt32());
    case Int52DisplacedInJSStack:
        return jsNumber(callFrame->r(virtualRegister()).unboxedInt52());
    case StrictInt52DisplacedInJSStack:
        return jsNumber(callFrame->r(virtualRegister()).unboxedStrictInt52());
    case DoubleDisplacedInJSStack:
        return jsNumber(purifyNaN(callFrame->r(virtualRegister()).unboxedDouble()));
    case CellDisplacedInJSStack:
        return callFrame->r(virtualRegister()).unboxedCell();
    case BooleanDisplacedInJSStack:
#if USE(JSVALUE64)
        return callFrame->r(virtualRegister()).jsValue();
#else
        return jsBoolean(callFrame->r(virtualRegister()).unboxedBoolean());
#endif
    case Constant:
        return constant();
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return JSValue();
    }
}

#if ENABLE(JIT)

void ValueRecovery::dumpInContext(PrintStream& out, DumpContext* context) const
{
    switch (technique()) {
    case InGPR:
        out.print(gpr());
        return;
    case UnboxedInt32InGPR:
        out.print("int32(", gpr(), ")");
        return;
    case UnboxedInt52InGPR:
        out.print("int52(", gpr(), ")");
        return;
    case UnboxedStrictInt52InGPR:
        out.print("strictInt52(", gpr(), ")");
        return;
    case UnboxedBooleanInGPR:
        out.print("bool(", gpr(), ")");
        return;
    case UnboxedCellInGPR:
        out.print("cell(", gpr(), ")");
        return;
    case InFPR:
        out.print(fpr());
        return;
    case UnboxedDoubleInFPR:
        out.print("double(", fpr(), ")");
        return;
#if USE(JSVALUE32_64)
    case InPair:
        out.print("pair(", tagGPR(), ", ", payloadGPR(), ")");
        return;
#endif
    case DisplacedInJSStack:
        out.print("*", virtualRegister());
        return;
    case Int32DisplacedInJSStack:
        out.print("*int32(", virtualRegister(), ")");
        return;
#if USE(JSVALUE32_64)
    case Int32TagDisplacedInJSStack:
        out.print("*int32Tag(", virtualRegister(), ")");
        return;
#endif
    case Int52DisplacedInJSStack:
        out.print("*int52(", virtualRegister(), ")");
        return;
    case StrictInt52DisplacedInJSStack:
        out.print("*strictInt52(", virtualRegister(), ")");
        return;
    case DoubleDisplacedInJSStack:
        out.print("*double(", virtualRegister(), ")");
        return;
    case CellDisplacedInJSStack:
        out.print("*cell(", virtualRegister(), ")");
        return;
    case BooleanDisplacedInJSStack:
        out.print("*bool(", virtualRegister(), ")");
        return;
    case DirectArgumentsThatWereNotCreated:
        out.print("DirectArguments(", nodeID(), ")");
        return;
    case ClonedArgumentsThatWereNotCreated:
        out.print("ClonedArguments(", nodeID(), ")");
        return;
    case Constant:
        out.print("[", inContext(constant(), context), "]");
        return;
    case DontKnow:
        out.printf("!");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void ValueRecovery::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}
#endif // ENABLE(JIT)

} // namespace JSC

