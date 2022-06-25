/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "MacroAssembler.h"

#if ENABLE(ASSEMBLER)

#include "JSCPtrTag.h"
#include "Options.h"
#include "ProbeContext.h"
#include <wtf/PrintStream.h>
#include <wtf/ScopedLambda.h>

namespace JSC {

const double MacroAssembler::twoToThe32 = (double)0x100000000ull;

void MacroAssembler::jitAssert(const ScopedLambda<Jump(void)>& functor)
{
    if (Options::enableJITDebugAssertions()) {
        Jump passed = functor();
        breakpoint();
        passed.link(this);
    }
}

static void stdFunctionCallback(Probe::Context& context)
{
    auto func = context.arg<const Function<void(Probe::Context&)>*>();
    (*func)(context);
}
    
void MacroAssembler::probeDebug(Function<void(Probe::Context&)> func)
{
    probe(tagCFunction<JITProbePtrTag>(stdFunctionCallback), new Function<void(Probe::Context&)>(WTFMove(func)));
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, MacroAssembler::RelationalCondition cond)
{
    switch (cond) {
    case MacroAssembler::Equal:
        out.print("Equal");
        return;
    case MacroAssembler::NotEqual:
        out.print("NotEqual");
        return;
    case MacroAssembler::Above:
        out.print("Above");
        return;
    case MacroAssembler::AboveOrEqual:
        out.print("AboveOrEqual");
        return;
    case MacroAssembler::Below:
        out.print("Below");
        return;
    case MacroAssembler::BelowOrEqual:
        out.print("BelowOrEqual");
        return;
    case MacroAssembler::GreaterThan:
        out.print("GreaterThan");
        return;
    case MacroAssembler::GreaterThanOrEqual:
        out.print("GreaterThanOrEqual");
        return;
    case MacroAssembler::LessThan:
        out.print("LessThan");
        return;
    case MacroAssembler::LessThanOrEqual:
        out.print("LessThanOrEqual");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, MacroAssembler::ResultCondition cond)
{
    switch (cond) {
    case MacroAssembler::Overflow:
        out.print("Overflow");
        return;
    case MacroAssembler::Signed:
        out.print("Signed");
        return;
    case MacroAssembler::PositiveOrZero:
        out.print("PositiveOrZero");
        return;
    case MacroAssembler::Zero:
        out.print("Zero");
        return;
    case MacroAssembler::NonZero:
        out.print("NonZero");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, MacroAssembler::DoubleCondition cond)
{
    switch (cond) {
    case MacroAssembler::DoubleEqualAndOrdered:
        out.print("DoubleEqualAndOrdered");
        return;
    case MacroAssembler::DoubleNotEqualAndOrdered:
        out.print("DoubleNotEqualAndOrdered");
        return;
    case MacroAssembler::DoubleGreaterThanAndOrdered:
        out.print("DoubleGreaterThanAndOrdered");
        return;
    case MacroAssembler::DoubleGreaterThanOrEqualAndOrdered:
        out.print("DoubleGreaterThanOrEqualAndOrdered");
        return;
    case MacroAssembler::DoubleLessThanAndOrdered:
        out.print("DoubleLessThanAndOrdered");
        return;
    case MacroAssembler::DoubleLessThanOrEqualAndOrdered:
        out.print("DoubleLessThanOrEqualAndOrdered");
        return;
    case MacroAssembler::DoubleEqualOrUnordered:
        out.print("DoubleEqualOrUnordered");
        return;
    case MacroAssembler::DoubleNotEqualOrUnordered:
        out.print("DoubleNotEqualOrUnordered");
        return;
    case MacroAssembler::DoubleGreaterThanOrUnordered:
        out.print("DoubleGreaterThanOrUnordered");
        return;
    case MacroAssembler::DoubleGreaterThanOrEqualOrUnordered:
        out.print("DoubleGreaterThanOrEqualOrUnordered");
        return;
    case MacroAssembler::DoubleLessThanOrUnordered:
        out.print("DoubleLessThanOrUnordered");
        return;
    case MacroAssembler::DoubleLessThanOrEqualOrUnordered:
        out.print("DoubleLessThanOrEqualOrUnordered");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(ASSEMBLER)

