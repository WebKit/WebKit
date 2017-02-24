/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "AirDumpAsJS.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirInstInlines.h"

namespace JSC { namespace B3 { namespace Air {

namespace {

CString varNameForBlockAtIndex(unsigned index)
{
    return toCString("bb", index);
}

CString varName(BasicBlock* block)
{
    return varNameForBlockAtIndex(block->index());
}

CString varNameForStackSlotAtIndex(unsigned index)
{
    return toCString("slot", index);
}

CString varName(StackSlot* slot)
{
    return varNameForStackSlotAtIndex(slot->index());
}

CString varName(Reg reg)
{
    return toCString("Reg.", reg.debugName());
}

CString varNameForTmpWithBankAndIndex(Bank bank, unsigned index)
{
    return toCString(bank == FP ? "f" : "", "tmp", index);
}

CString varName(Tmp tmp)
{
    if (tmp.isReg())
        return varName(tmp.reg());
    return varNameForTmpWithBankAndIndex(Arg(tmp).bank(), tmp.tmpIndex());
}

} // anonymous namespace

void dumpAsJS(Code& code, PrintStream& out)
{
    out.println("let code = new Code();");
    
    for (unsigned i = 0; i < code.size(); ++i)
        out.println("let ", varNameForBlockAtIndex(i), " = code.addBlock();");
    
    out.println("let hash;");

    for (unsigned i = 0; i < code.stackSlots().size(); ++i) {
        StackSlot* slot = code.stackSlots()[i];
        if (slot) {
            out.println("let ", varName(slot), " = code.addStackSlot(", slot->byteSize(), ", ", slot->kind(), ");");
            if (slot->offsetFromFP())
                out.println(varName(slot), ".setOffsetFromFP(", slot->offsetFromFP(), ");");
            out.println("hash = ", varName(slot), ".hash();");
            out.println("if (hash != ", slot->jsHash(), ")");
            out.println("    throw new Error(\"Bad hash: \" + hash);");
        } else
            out.println("code.addStackSlot(1, Spill);");
    }
    
    forEachBank(
        [&] (Bank bank) {
            for (unsigned i = code.numTmps(bank); i--;) {
                out.println(
                    "let ", varNameForTmpWithBankAndIndex(bank, i), " = code.newTmp(", bank, ");");
            }
        });
    
    out.println("let inst;");
    out.println("let arg;");
    
    for (BasicBlock* block : code) {
        for (FrequentedBlock successor : block->successors()) {
            out.println(
                varName(block), ".successors.push(new FrequentedBlock(",
                varName(successor.block()), ", ", successor.frequency(), "));");
        }
        
        for (BasicBlock* predecessor : block->predecessors())
            out.println(varName(block), ".predecessors.push(", varName(predecessor), ");");
        
        for (Inst& inst : *block) {
            // FIXME: This should do something for flags.
            // https://bugs.webkit.org/show_bug.cgi?id=162751
            out.println("inst = new Inst(", inst.kind.opcode, ");");
            
            inst.forEachArg(
                [&] (Arg& arg, Arg::Role, Bank, Width) {
                    switch (arg.kind()) {
                    case Arg::Invalid:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                        
                    case Arg::Tmp:
                        out.println("arg = Arg.createTmp(", varName(arg.tmp()), ");");
                        break;
                        
                    case Arg::Imm:
                        out.println("arg = Arg.createImm(", arg.value(), ");");
                        break;
                        
                    case Arg::BigImm:
                        out.println(
                            "arg = Arg.createBigImm(",
                            static_cast<int32_t>(arg.value()), ", ",
                            static_cast<int32_t>(arg.value() >> 32), ");");
                        break;
                        
                    case Arg::BitImm:
                        out.println("arg = Arg.createBitImm(", arg.value(), ");");
                        break;
                        
                    case Arg::BitImm64:
                        out.println(
                            "arg = Arg.createBitImm64(",
                            static_cast<int32_t>(arg.value()), ", ",
                            static_cast<int32_t>(arg.value() >> 32), ");");
                        break;
                        
                    case Arg::Addr:
                        out.println(
                            "arg = Arg.createAddr(", varName(arg.base()), ", ", arg.offset(), ");");
                        break;
                        
                    case Arg::Stack:
                        out.println(
                            "arg = Arg.createStack(", varName(arg.stackSlot()), ", ", arg.offset(), ");");
                        break;
                        
                    case Arg::CallArg:
                        out.println("arg = Arg.createCallArg(", arg.offset(), ");");
                        break;
                        
                    case Arg::Index:
                        out.println(
                            "arg = Arg.createIndex(", varName(arg.base()), ", ",
                            varName(arg.index()), ", ", arg.scale(), ", ", arg.offset(), ");");
                        break;
                        
                    case Arg::RelCond:
                        out.println("arg = Arg.createRelCond(", arg.asRelationalCondition(), ");");
                        break;
                        
                    case Arg::ResCond:
                        out.println("arg = Arg.createResCond(", arg.asResultCondition(), ");");
                        break;
                        
                    case Arg::DoubleCond:
                        out.println("arg = Arg.createDoubleCond(", arg.asDoubleCondition(), ");");
                        break;
                        
                    case Arg::Special:
                        out.println("arg = Arg.createSpecial();");
                        break;
                        
                    case Arg::WidthArg:
                        out.println("arg = Arg.createWidthArg(", arg.width(), ");");
                        break;
                    }
                    
                    out.println("inst.args.push(arg);");
                });
            
            if (inst.kind.opcode == Patch) {
                if (inst.hasNonArgEffects())
                    out.println("inst.patchHasNonArgEffects = true;");
                
                out.println("inst.extraEarlyClobberedRegs = new Set();");
                out.println("inst.extraClobberedRegs = new Set();");
                inst.extraEarlyClobberedRegs().forEach(
                    [&] (Reg reg) {
                        out.println("inst.extraEarlyClobberedRegs.add(", varName(reg), ");");
                    });
                inst.extraClobberedRegs().forEach(
                    [&] (Reg reg) {
                        out.println("inst.extraClobberedRegs.add(", varName(reg), ");");
                    });
                
                out.println("inst.patchArgData = [];");
                inst.forEachArg(
                    [&] (Arg&, Arg::Role role, Bank bank, Width width) {
                        out.println(
                            "inst.patchArgData.push({role: Arg.", role, ", type: ", bank,
                            ", width: ", width, "});");
                    });
            }
            
            if (inst.kind.opcode == CCall || inst.kind.opcode == ColdCCall) {
                out.println("inst.cCallType = ", inst.origin->type());
                out.println("inst.cCallArgTypes = [];");
                for (unsigned i = 1; i < inst.origin->numChildren(); ++i)
                    out.println("inst.cCallArgTypes.push(", inst.origin->child(i)->type(), ");");
            }
            
            out.println("hash = inst.hash();");
            out.println("if (hash != ", inst.jsHash(), ")");
            out.println("    throw new Error(\"Bad hash: \" + hash);");
            
            out.println(varName(block), ".append(inst);");
        }
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

