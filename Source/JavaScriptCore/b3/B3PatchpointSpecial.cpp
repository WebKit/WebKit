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
#include "B3PatchpointSpecial.h"

#if ENABLE(B3_JIT)

#include "AirGenerationContext.h"
#include "B3StackmapGenerationParams.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

using namespace Air;

PatchpointSpecial::PatchpointSpecial()
{
}

PatchpointSpecial::~PatchpointSpecial()
{
}

void PatchpointSpecial::forEachArg(Inst& inst, const ScopedLambda<Inst::EachArgCallback>& callback)
{
    // FIXME: Allow B3 Patchpoints to specify LateUse.
    // https://bugs.webkit.org/show_bug.cgi?id=151335
    
    if (inst.origin->type() == Void) {
        forEachArgImpl(0, 1, inst, SameAsRep, callback);
        return;
    }

    callback(inst.args[1], Arg::Def, inst.origin->airType());
    forEachArgImpl(0, 2, inst, SameAsRep, callback);
}

bool PatchpointSpecial::isValid(Inst& inst)
{
    if (inst.origin->type() == Void)
        return isValidImpl(0, 1, inst);

    if (inst.args.size() < 2)
        return false;
    PatchpointValue* patchpoint = inst.origin->as<PatchpointValue>();
    if (!isArgValidForValue(inst.args[1], patchpoint))
        return false;
    if (!isArgValidForRep(code(), inst.args[1], patchpoint->resultConstraint))
        return false;

    return isValidImpl(0, 2, inst);
}

bool PatchpointSpecial::admitsStack(Inst& inst, unsigned argIndex)
{
    if (inst.origin->type() == Void)
        return admitsStackImpl(0, 1, inst, argIndex);

    if (argIndex == 1) {
        switch (inst.origin->as<PatchpointValue>()->resultConstraint.kind()) {
        case ValueRep::WarmAny:
        case ValueRep::StackArgument:
            return true;
        case ValueRep::SomeRegister:
        case ValueRep::Register:
            return false;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }

    return admitsStackImpl(0, 2, inst, argIndex);
}

CCallHelpers::Jump PatchpointSpecial::generate(
    Inst& inst, CCallHelpers& jit, GenerationContext& context)
{
    StackmapValue* value = inst.origin->as<StackmapValue>();
    ASSERT(value);

    Vector<ValueRep> reps;
    unsigned offset = 1;
    if (inst.origin->type() != Void)
        reps.append(repForArg(*context.code, inst.args[offset++]));
    appendRepsImpl(context, offset, inst, reps);
    
    value->m_generator->run(jit, StackmapGenerationParams(value, reps, context));

    return CCallHelpers::Jump();
}

void PatchpointSpecial::dumpImpl(PrintStream& out) const
{
    out.print("Patchpoint");
}

void PatchpointSpecial::deepDumpImpl(PrintStream& out) const
{
    out.print("Lowered B3::PatchpointValue.");
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
