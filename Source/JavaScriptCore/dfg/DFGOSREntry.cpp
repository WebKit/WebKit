/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include "DFGOSREntry.h"

#if ENABLE(DFG_JIT)

#include "BytecodeStructs.h"
#include "CallFrame.h"
#include "CodeBlock.h"
#include "DFGJITCode.h"
#include "DFGNode.h"
#include "JSCJSValueInlines.h"
#include "RegisterAtOffsetList.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC { namespace DFG {

void OSREntryData::dumpInContext(PrintStream& out, DumpContext* context) const
{
    out.print(m_bytecodeIndex, ", machine code = ", RawPointer(m_machineCode.taggedPtr()));
    out.print(", stack rules = [");
    
    auto printOperand = [&] (VirtualRegister reg) {
        out.print(inContext(m_expectedValues.operand(reg), context), " (");
        VirtualRegister toReg;
        bool overwritten = false;
        for (OSREntryReshuffling reshuffling : m_reshufflings) {
            if (reg == VirtualRegister(reshuffling.fromOffset)) {
                toReg = VirtualRegister(reshuffling.toOffset);
                break;
            }
            if (reg == VirtualRegister(reshuffling.toOffset))
                overwritten = true;
        }
        if (!overwritten && !toReg.isValid())
            toReg = reg;
        if (toReg.isValid()) {
            if (toReg.isLocal() && !m_machineStackUsed.get(toReg.toLocal()))
                out.print("ignored");
            else
                out.print("maps to ", toReg);
        } else
            out.print("overwritten");
        if (reg.isLocal() && m_localsForcedDouble.get(reg.toLocal()))
            out.print(", forced double");
        if (reg.isLocal() && m_localsForcedAnyInt.get(reg.toLocal()))
            out.print(", forced machine int");
        out.print(")");
    };
    
    CommaPrinter comma;
    for (size_t argumentIndex = m_expectedValues.numberOfArguments(); argumentIndex--;) {
        out.print(comma, "arg", argumentIndex, ":");
        printOperand(virtualRegisterForArgumentIncludingThis(argumentIndex));
    }
    for (size_t localIndex = 0; localIndex < m_expectedValues.numberOfLocals(); ++localIndex) {
        out.print(comma, "loc", localIndex, ":");
        printOperand(virtualRegisterForLocal(localIndex));
    }
    
    out.print("], machine stack used = ", m_machineStackUsed);
}

void OSREntryData::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

SUPPRESS_ASAN
void* prepareOSREntry(VM& vm, CallFrame* callFrame, CodeBlock* codeBlock, BytecodeIndex bytecodeIndex)
{
    ASSERT(JITCode::isOptimizingJIT(codeBlock->jitType()));
    ASSERT(codeBlock->alternative());
    ASSERT(codeBlock->alternative()->jitType() == JITType::BaselineJIT);
    ASSERT(codeBlock->jitCode()->dfgCommon()->isStillValid());
    ASSERT(!codeBlock->isJettisoned());

    if (!Options::useOSREntryToDFG())
        return nullptr;

    dataLogLnIf(Options::verboseOSR(),
        "DFG OSR in ", *codeBlock->alternative(), " -> ", *codeBlock,
        " from ", bytecodeIndex);
    
    sanitizeStackForVM(vm);
    
    if (bytecodeIndex)
        codeBlock->ownerExecutable()->setDidTryToEnterInLoop(true);
    
    if (codeBlock->jitType() != JITType::DFGJIT) {
        RELEASE_ASSERT(codeBlock->jitType() == JITType::FTLJIT);
        
        // When will this happen? We could have:
        //
        // - An exit from the FTL JIT into the baseline JIT followed by an attempt
        //   to reenter. We're fine with allowing this to fail. If it happens
        //   enough we'll just reoptimize. It basically means that the OSR exit cost
        //   us dearly and so reoptimizing is the right thing to do.
        //
        // - We have recursive code with hot loops. Consider that foo has a hot loop
        //   that calls itself. We have two foo's on the stack, lets call them foo1
        //   and foo2, with foo1 having called foo2 from foo's hot loop. foo2 gets
        //   optimized all the way into the FTL. Then it returns into foo1, and then
        //   foo1 wants to get optimized. It might reach this conclusion from its
        //   hot loop and attempt to OSR enter. And we'll tell it that it can't. It
        //   might be worth addressing this case, but I just think this case will
        //   be super rare. For now, if it does happen, it'll cause some compilation
        //   thrashing.
        
        dataLogLnIf(Options::verboseOSR(), "    OSR failed because the target code block is not DFG.");
        return nullptr;
    }
    
    JITCode* jitCode = codeBlock->jitCode()->dfg();
    OSREntryData* entry = jitCode->osrEntryDataForBytecodeIndex(bytecodeIndex);
    
    if (!entry) {
        dataLogLnIf(Options::verboseOSR(), "    OSR failed because the entrypoint was optimized out.");
        return nullptr;
    }
    
    ASSERT(entry->m_bytecodeIndex == bytecodeIndex);
    
    // The code below checks if it is safe to perform OSR entry. It may find
    // that it is unsafe to do so, for any number of reasons, which are documented
    // below. If the code decides not to OSR then it returns 0, and it's the caller's
    // responsibility to patch up the state in such a way as to ensure that it's
    // both safe and efficient to continue executing baseline code for now. This
    // should almost certainly include calling either codeBlock->optimizeAfterWarmUp()
    // or codeBlock->dontOptimizeAnytimeSoon().
    
    // 1) Verify predictions. If the predictions are inconsistent with the actual
    //    values, then OSR entry is not possible at this time. It's tempting to
    //    assume that we could somehow avoid this case. We can certainly avoid it
    //    for first-time loop OSR - that is, OSR into a CodeBlock that we have just
    //    compiled. Then we are almost guaranteed that all of the predictions will
    //    check out. It would be pretty easy to make that a hard guarantee. But
    //    then there would still be the case where two call frames with the same
    //    baseline CodeBlock are on the stack at the same time. The top one
    //    triggers compilation and OSR. In that case, we may no longer have
    //    accurate value profiles for the one deeper in the stack. Hence, when we
    //    pop into the CodeBlock that is deeper on the stack, we might OSR and
    //    realize that the predictions are wrong. Probably, in most cases, this is
    //    just an anomaly in the sense that the older CodeBlock simply went off
    //    into a less-likely path. So, the wisest course of action is to simply not
    //    OSR at this time.
    
    for (size_t argument = 0; argument < entry->m_expectedValues.numberOfArguments(); ++argument) {
        JSValue value;
        if (!argument)
            value = callFrame->thisValue();
        else
            value = callFrame->argument(argument - 1);
        
        if (!entry->m_expectedValues.argument(argument).validateOSREntryValue(value, FlushedJSValue)) {
            dataLogLnIf(Options::verboseOSR(),
                "    OSR failed because argument ", argument, " is ", value,
                ", expected ", entry->m_expectedValues.argument(argument));
            return nullptr;
        }
    }
    
    for (size_t local = 0; local < entry->m_expectedValues.numberOfLocals(); ++local) {
        int localOffset = virtualRegisterForLocal(local).offset();
        JSValue value = callFrame->registers()[localOffset].asanUnsafeJSValue();
        FlushFormat format = FlushedJSValue;

        if (entry->m_localsForcedAnyInt.get(local)) {
            if (!value.isAnyInt()) {
                dataLogLnIf(Options::verboseOSR(),
                    "    OSR failed because variable ", localOffset, " is ",
                    value, ", expected ",
                    "machine int.");
                return nullptr;
            }
            value = jsDoubleNumber(value.asAnyInt());
            format = FlushedInt52;
        }

        if (entry->m_localsForcedDouble.get(local)) {
            if (!value.isNumber()) {
                dataLogLnIf(Options::verboseOSR(),
                    "    OSR failed because variable ", localOffset, " is ",
                    value, ", expected number.");
                return nullptr;
            }
            value = jsDoubleNumber(value.asNumber());
            format = FlushedDouble;
        }

        if (!entry->m_expectedValues.local(local).validateOSREntryValue(value, format)) {
            dataLogLnIf(Options::verboseOSR(),
                "    OSR failed because variable ", VirtualRegister(localOffset), " is ",
                value, ", expected ",
                entry->m_expectedValues.local(local), ".");
            return nullptr;
        }
    }

    // 2) Check the stack height. The DFG JIT may require a taller stack than the
    //    baseline JIT, in some cases. If we can't grow the stack, then don't do
    //    OSR right now. That's the only option we have unless we want basic block
    //    boundaries to start throwing RangeErrors. Although that would be possible,
    //    it seems silly: you'd be diverting the program to error handling when it
    //    would have otherwise just kept running albeit less quickly.
    
    unsigned frameSizeForCheck = jitCode->common.requiredRegisterCountForExecutionAndExit();
    if (UNLIKELY(!vm.ensureStackCapacityFor(&callFrame->registers()[virtualRegisterForLocal(frameSizeForCheck - 1).offset()]))) {
        dataLogLnIf(Options::verboseOSR(), "    OSR failed because stack growth failed.");
        return nullptr;
    }
    
    dataLogLnIf(Options::verboseOSR(), "    OSR should succeed.");

    // At this point we're committed to entering. We will do some work to set things up,
    // but we also rely on our caller recognizing that when we return a non-null pointer,
    // that means that we're already past the point of no return and we must succeed at
    // entering.
    
    // 3) Set up the data in the scratch buffer and perform data format conversions.

    unsigned frameSize = jitCode->common.frameRegisterCount;
    unsigned baselineFrameSize = entry->m_expectedValues.numberOfLocals();
    unsigned maxFrameSize = std::max(frameSize, baselineFrameSize);

    Register* scratch = bitwise_cast<Register*>(vm.scratchBufferForSize(sizeof(Register) * (2 + CallFrame::headerSizeInRegisters + maxFrameSize))->dataBuffer());
    
    *bitwise_cast<size_t*>(scratch + 0) = frameSize;
    
    void* targetPC = entry->m_machineCode.taggedPtr();
    RELEASE_ASSERT(codeBlock->jitCode()->contains(entry->m_machineCode.untaggedPtr()));
    dataLogLnIf(Options::verboseOSR(), "    OSR using target PC ", RawPointer(targetPC));
    RELEASE_ASSERT(targetPC);
    *bitwise_cast<void**>(scratch + 1) = tagCodePtrWithStackPointerForJITCall(untagCodePtr<OSREntryPtrTag>(targetPC), callFrame);

    Register* pivot = scratch + 2 + CallFrame::headerSizeInRegisters;
    
    for (int index = -CallFrame::headerSizeInRegisters; index < static_cast<int>(baselineFrameSize); ++index) {
        VirtualRegister reg(-1 - index);
        
        if (reg.isLocal()) {
            if (entry->m_localsForcedDouble.get(reg.toLocal())) {
                *bitwise_cast<double*>(pivot + index) = callFrame->registers()[reg.offset()].asanUnsafeJSValue().asNumber();
                continue;
            }
            
            if (entry->m_localsForcedAnyInt.get(reg.toLocal())) {
                *bitwise_cast<int64_t*>(pivot + index) = callFrame->registers()[reg.offset()].asanUnsafeJSValue().asAnyInt() << JSValue::int52ShiftAmount;
                continue;
            }
        }
        
        pivot[index] = callFrame->registers()[reg.offset()].asanUnsafeJSValue();
    }
    
    // 4) Reshuffle those registers that need reshuffling.
    Vector<JSValue> temporaryLocals(entry->m_reshufflings.size());
    for (unsigned i = entry->m_reshufflings.size(); i--;)
        temporaryLocals[i] = pivot[VirtualRegister(entry->m_reshufflings[i].fromOffset).toLocal()].asanUnsafeJSValue();
    for (unsigned i = entry->m_reshufflings.size(); i--;)
        pivot[VirtualRegister(entry->m_reshufflings[i].toOffset).toLocal()] = temporaryLocals[i];
    
    // 5) Clear those parts of the call frame that the DFG ain't using. This helps GC on
    //    some programs by eliminating some stale pointer pathologies.
    for (unsigned i = frameSize; i--;) {
        if (entry->m_machineStackUsed.get(i))
            continue;
        pivot[i] = JSValue();
    }

    // 6) Copy our callee saves to buffer.
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
    const RegisterAtOffsetList* registerSaveLocations = codeBlock->jitCode()->calleeSaveRegisters();
    RegisterAtOffsetList* allCalleeSaves = RegisterSetBuilder::vmCalleeSaveRegisterOffsets();
    auto dontSaveRegisters = RegisterSetBuilder::stackRegisters();

    unsigned registerCount = registerSaveLocations->registerCount();
    VMEntryRecord* record = vmEntryRecord(vm.topEntryFrame);
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset currentEntry = registerSaveLocations->at(i);
        ASSERT(dontSaveRegisters.contains(currentEntry.reg(), IgnoreVectors) == dontSaveRegisters.contains(currentEntry.reg(), Width128));
        if (dontSaveRegisters.contains(currentEntry.reg(), IgnoreVectors))
            continue;
        RELEASE_ASSERT(currentEntry.reg().isGPR());
        RegisterAtOffset* calleeSavesEntry = allCalleeSaves->find(currentEntry.reg());
        
        if constexpr (CallerFrameAndPC::sizeInRegisters == 2)
            *(bitwise_cast<intptr_t*>(pivot - 1) - currentEntry.offsetAsIndex()) = record->calleeSaveRegistersBuffer[calleeSavesEntry->offsetAsIndex()];
        else {
            // We need to adjust 4-bytes on 32-bits, otherwise we will clobber some parts of
            // pivot[-1] when currentEntry.offsetAsIndex() returns -1. This region contains
            // CallerFrameAndPC and if it is cloberred, we will have a corrupted stack.
            // Also, we need to store callee-save registers swapped in pairs on scratch buffer,
            // otherwise they will be swapped when copied to call frame during OSR Entry code.
            // Here is how we would like to have the buffer configured:
            //
            // pivot[-4] = ArgumentCountIncludingThis
            // pivot[-3] = Callee
            // pivot[-2] = CodeBlock
            // pivot[-1] = CallerFrameAndReturnPC
            // pivot[0]  = csr1/csr0
            // pivot[1]  = csr3/csr2
            // ...
            ASSERT(sizeof(intptr_t) == 4);
            ASSERT(CallerFrameAndPC::sizeInRegisters == 1);
            ASSERT(currentEntry.offsetAsIndex() < 0);

            int offsetAsIndex = currentEntry.offsetAsIndex();
            int properIndex = offsetAsIndex % 2 ? offsetAsIndex - 1 : offsetAsIndex + 1;
            *(bitwise_cast<intptr_t*>(pivot - 1) + 1 - properIndex) = record->calleeSaveRegistersBuffer[calleeSavesEntry->offsetAsIndex()];
        }
    }
#endif

    // 7) Fix the call frame to have the right code block.

    *bitwise_cast<CodeBlock**>(pivot - (CallFrameSlot::codeBlock + 1)) = codeBlock;
    
    dataLogLnIf(Options::verboseOSR(), "    OSR returning data buffer ", RawPointer(scratch));
    return scratch;
}

CodePtr<ExceptionHandlerPtrTag> prepareCatchOSREntry(VM& vm, CallFrame* callFrame, CodeBlock* baselineCodeBlock, CodeBlock* optimizedCodeBlock, BytecodeIndex bytecodeIndex)
{
    ASSERT(optimizedCodeBlock->jitType() == JITType::DFGJIT || optimizedCodeBlock->jitType() == JITType::FTLJIT);
    ASSERT(optimizedCodeBlock->jitCode()->dfgCommon()->isStillValid());
    ASSERT(!optimizedCodeBlock->isJettisoned());

    if (!Options::useOSREntryToDFG() && optimizedCodeBlock->jitCode()->jitType() == JITType::DFGJIT)
        return nullptr;
    if (!Options::useOSREntryToFTL() && optimizedCodeBlock->jitCode()->jitType() == JITType::FTLJIT)
        return nullptr;

    CommonData* dfgCommon = optimizedCodeBlock->jitCode()->dfgCommon();
    RELEASE_ASSERT(dfgCommon);
    DFG::CatchEntrypointData* catchEntrypoint = dfgCommon->catchOSREntryDataForBytecodeIndex(bytecodeIndex);
    if (!catchEntrypoint) {
        // This can be null under some circumstances. The most common is that we didn't
        // compile this op_catch as an entrypoint since it had never executed when starting
        // the compilation.
        return nullptr;
    }

    // We're only allowed to OSR enter if we've proven we have compatible argument types.
    for (unsigned argument = 0; argument < catchEntrypoint->argumentFormats.size(); ++argument) {
        JSValue value = callFrame->uncheckedR(virtualRegisterForArgumentIncludingThis(argument)).jsValue();
        switch (catchEntrypoint->argumentFormats[argument]) {
        case DFG::FlushedInt32:
            if (!value.isInt32())
                return nullptr;
            break;
        case DFG::FlushedCell:
            if (!value.isCell())
                return nullptr;
            break;
        case DFG::FlushedBoolean:
            if (!value.isBoolean())
                return nullptr;
            break;
        case DFG::DeadFlush:
            // This means the argument is not alive. Therefore, it's allowed to be any type.
            break;
        case DFG::FlushedJSValue:
            // An argument is trivially a JSValue.
            break; 
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    unsigned frameSizeForCheck = dfgCommon->requiredRegisterCountForExecutionAndExit();
    if (UNLIKELY(!vm.ensureStackCapacityFor(&callFrame->registers()[virtualRegisterForLocal(frameSizeForCheck).offset()])))
        return nullptr;

    auto instruction = baselineCodeBlock->instructions().at(callFrame->bytecodeIndex());
    ASSERT(instruction->is<OpCatch>());
    ValueProfileAndVirtualRegisterBuffer* buffer = instruction->as<OpCatch>().metadata(baselineCodeBlock).m_buffer;
    JSValue* dataBuffer = reinterpret_cast<JSValue*>(dfgCommon->catchOSREntryBuffer->dataBuffer());
    unsigned index = 0;
    buffer->forEach([&] (ValueProfileAndVirtualRegister& profile) {
        if (!VirtualRegister(profile.m_operand).isLocal())
            return;
        dataBuffer[index] = callFrame->uncheckedR(profile.m_operand).jsValue();
        ++index;
    });

    // The active length of catchOSREntryBuffer will be zeroed by ClearCatchLocals node.
    dfgCommon->catchOSREntryBuffer->setActiveLength(sizeof(JSValue) * index);

    // At this point, we're committed to triggering an OSR entry immediately after we return. Hence, it is safe to modify stack here.
    callFrame->setCodeBlock(optimizedCodeBlock);

    return catchEntrypoint->machineCode;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
