/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "AirCode.h"
#include "AirGenerationContext.h"
#include "B3ProcedureInlines.h"
#include "B3StackmapGenerationParams.h"
#include "B3ValueInlines.h"

#include <wtf/ListDump.h>

namespace JSC { namespace B3 {

using Arg = Air::Arg;
using Inst = Air::Inst;

PatchpointSpecial::PatchpointSpecial()
{
}

PatchpointSpecial::~PatchpointSpecial()
{
}

void PatchpointSpecial::forEachArg(Inst& inst, const ScopedLambda<Inst::EachArgCallback>& callback)
{
    const Procedure& procedure = code().proc();
    PatchpointValue* patchpoint = inst.origin->as<PatchpointValue>();
    unsigned argIndex = 1;

    Type type = patchpoint->type();
    for (; argIndex <= procedure.resultCount(type); ++argIndex) {
        Arg::Role role;
        if (patchpoint->resultConstraints[argIndex - 1].kind() == ValueRep::SomeEarlyRegister)
            role = Arg::EarlyDef;
        else
            role = Arg::Def;

        Type argType = type.isTuple() ? procedure.extractFromTuple(type, argIndex - 1) : type;
        callback(inst.args[argIndex], role, bankForType(argType), widthForType(argType));
    }

    forEachArgImpl(0, argIndex, inst, SameAsRep, std::nullopt, callback, std::nullopt);
    argIndex += inst.origin->numChildren();

    for (unsigned i = patchpoint->numGPScratchRegisters; i--;)
        callback(inst.args[argIndex++], Arg::Scratch, GP, conservativeWidth(GP));
    for (unsigned i = patchpoint->numFPScratchRegisters; i--;)
        callback(inst.args[argIndex++], Arg::Scratch, FP, Options::useWebAssemblySIMD() ? conservativeWidth(FP) : conservativeWidthWithoutVectors(FP));
}

bool PatchpointSpecial::isValid(Inst& inst)
{
    const Procedure& procedure = code().proc();
    PatchpointValue* patchpoint = inst.origin->as<PatchpointValue>();
    unsigned argIndex = 1;

    Type type = patchpoint->type();
    for (; argIndex <= procedure.resultCount(type); ++argIndex) {
        if (argIndex >= inst.args.size())
            return false;
        
        if (!isArgValidForType(inst.args[argIndex], type.isTuple() ? procedure.extractFromTuple(type, argIndex - 1) : type))
            return false;
        if (!isArgValidForRep(code(), inst.args[argIndex], patchpoint->resultConstraints[argIndex - 1]))
            return false;
    }

    if (!isValidImpl(0, argIndex, inst))
        return false;
    argIndex += patchpoint->numChildren();

    if (argIndex + patchpoint->numGPScratchRegisters + patchpoint->numFPScratchRegisters
        != inst.args.size())
        return false;

    for (unsigned i = patchpoint->numGPScratchRegisters; i--;) {
        Arg arg = inst.args[argIndex++];
        if (!arg.isGPTmp())
            return false;
    }
    for (unsigned i = patchpoint->numFPScratchRegisters; i--;) {
        Arg arg = inst.args[argIndex++];
        if (!arg.isFPTmp())
            return false;
    }

    return true;
}

bool PatchpointSpecial::admitsStack(Inst& inst, unsigned argIndex)
{
    ASSERT(argIndex);

    Type type = inst.origin->type();
    unsigned returnCount = code().proc().resultCount(type);

    if (argIndex <= returnCount) {
        switch (inst.origin->as<PatchpointValue>()->resultConstraints[argIndex - 1].kind()) {
        case ValueRep::WarmAny:
        case ValueRep::StackArgument:
            return true;
        case ValueRep::SomeRegister:
        case ValueRep::SomeRegisterWithClobber:
        case ValueRep::SomeEarlyRegister:
        case ValueRep::SomeLateRegister:
        case ValueRep::Register:
        case ValueRep::LateRegister:
            return false;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return false;
        }
    }

    return admitsStackImpl(0, returnCount + 1, inst, argIndex);
}

bool PatchpointSpecial::admitsExtendedOffsetAddr(Inst& inst, unsigned argIndex)
{
    return admitsStack(inst, argIndex);
}

MacroAssembler::Jump PatchpointSpecial::generate(Inst& inst, CCallHelpers& jit, Air::GenerationContext& context)
{
    const Procedure& procedure = code().proc();
    PatchpointValue* value = inst.origin->as<PatchpointValue>();
    ASSERT(value);

    Vector<ValueRep> reps;
    unsigned offset = 1;

    Type type = value->type();
    while (offset <= procedure.resultCount(type))
        reps.append(repForArg(*context.code, inst.args[offset++]));
    reps.appendVector(repsImpl(context, 0, offset, inst));
    offset += value->numChildren();

    StackmapGenerationParams params(value, reps, context);
    JIT_COMMENT(jit, "Patchpoint body start, with arg value reps: ", listDump(reps));

    for (unsigned i = value->numGPScratchRegisters; i--;)
        params.m_gpScratch.append(inst.args[offset++].gpr());
    for (unsigned i = value->numFPScratchRegisters; i--;)
        params.m_fpScratch.append(inst.args[offset++].fpr());
    
    value->m_generator->run(jit, params);

    return MacroAssembler::Jump();
}

bool PatchpointSpecial::isTerminal(Inst& inst)
{
    return inst.origin->as<PatchpointValue>()->effects.terminal;
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
