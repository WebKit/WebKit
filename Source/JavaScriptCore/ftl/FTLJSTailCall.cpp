/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "FTLJSTailCall.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "CallFrameShuffler.h"
#include "DFGNode.h"
#include "FTLJITCode.h"
#include "FTLLocation.h"
#include "FTLStackMaps.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

namespace {

FTL::Location getRegisterWithAddend(const ExitValue& value, StackMaps::Record& record, StackMaps& stackmaps)
{
    if (value.kind() != ExitValueArgument)
        return { };

    auto location =
        FTL::Location::forStackmaps(&stackmaps, record.locations[value.exitArgument().argument()]);

    if (location.kind() != Location::Register || !location.addend())
        return { };

    RELEASE_ASSERT(location.isGPR());
    return location;
}

ValueRecovery recoveryFor(const ExitValue& value, StackMaps::Record& record, StackMaps& stackmaps)
{
    switch (value.kind()) {
    case ExitValueConstant:
        return ValueRecovery::constant(value.constant());

    case ExitValueArgument: {
        auto location =
            FTL::Location::forStackmaps(&stackmaps, record.locations[value.exitArgument().argument()]);
        auto format = value.exitArgument().format();

        switch (location.kind()) {
        case Location::Register:
            // We handle the addend outside
            return ValueRecovery::inRegister(location.reg(), format);

        case Location::Indirect:
            // Oh LLVM, you crazy...
            RELEASE_ASSERT(location.reg() == Reg(MacroAssembler::framePointerRegister));
            RELEASE_ASSERT(!(location.offset() % sizeof(void*)));
            // DataFormatInt32 and DataFormatBoolean should be already be boxed.
            RELEASE_ASSERT(format != DataFormatInt32 && format != DataFormatBoolean);
            return ValueRecovery::displacedInJSStack(VirtualRegister { static_cast<int>(location.offset() / sizeof(void*)) }, format);

        case Location::Constant:
            return ValueRecovery::constant(JSValue::decode(location.constant()));

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    case ExitValueInJSStack:
        return ValueRecovery::displacedInJSStack(value.virtualRegister(), DataFormatJS);

    case ExitValueInJSStackAsInt32:
        return ValueRecovery::displacedInJSStack(value.virtualRegister(), DataFormatInt32);

    case ExitValueInJSStackAsInt52:
        return ValueRecovery::displacedInJSStack(value.virtualRegister(), DataFormatInt52);

    case ExitValueInJSStackAsDouble:
        return ValueRecovery::displacedInJSStack(value.virtualRegister(), DataFormatDouble);

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

// This computes an estimated size (in bits) for the sequence of
// instructions required to load, box, and store a value of a given
// type, assuming no spilling is required.
uint32_t sizeFor(DataFormat format)
{
    switch (format) {
    case DataFormatInt32:
        // Boxing is zero-extending and tagging
#if CPU(X86_64)
        return 6 + sizeFor(DataFormatJS);
#elif CPU(ARM64)
        return 8 + sizeFor(DataFormatJS);
#else
        return sizeOfZeroExtend32 + sizeOfOrImm64 + sizeFor(DataFormatJS);
#endif

    case DataFormatInt52:
        // Boxing is first a conversion to StrictInt52, then
        // StrictInt52 boxing
#if CPU(X86_64)
        return 4 + sizeFor(DataFormatStrictInt52);
#elif CPU(ARM64)
        return 4 + sizeFor(DataFormatStrictInt52);
#else
        return sizeOfShiftImm32 + sizeFor(DataFormatStrictInt52);
#endif

    case DataFormatStrictInt52:
        // Boxing is first a conversion to double, then double boxing
#if CPU(X86_64)
        return 8 + sizeFor(DataFormatDouble);
#elif CPU(ARM64)
        return 4 + sizeFor(DataFormatDouble);
#else
        return sizeOfConvertInt64ToDouble + sizeFor(DataFormatDouble);
#endif

    case DataFormatDouble:
        // Boxing is purifying, moving to a GPR, and tagging
#if CPU(X86_64)
        return 38 + sizeFor(DataFormatJS);
#elif CPU(ARM64)
        return 28 + sizeFor(DataFormatJS);
#else
        return sizeOfPurifyNaN + sizeOfSubImm64 + sizeOfMoveDoubleTo64 + sizeFor(DataFormatJS);
#endif

    case DataFormatBoolean:
        // Boxing is adding ValueFalse
#if CPU(X86_64)
        return 4 + sizeFor(DataFormatJS);
#elif CPU(ARM64)
        return 4 + sizeFor(DataFormatJS);
#else
        return sizeOfAddImm32 + sizeFor(DataFormatJS);
#endif

    case DataFormatJS:
        // We will load (in a GPR or FPR) then store the value
#if CPU(X86_64)
        return 8;
#elif CPU(ARM64)
        return 8;
#else
        return sizeOfLoad + sizeOfStore;
#endif

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // anonymous namespace

JSTailCall::JSTailCall(unsigned stackmapID, Node* node, const Vector<ExitValue>& arguments)
    : JSCallBase(CallLinkInfo::TailCall, node->origin.semantic, node->origin.semantic)
    , m_stackmapID(stackmapID)
    , m_arguments(arguments)
    , m_instructionOffset(0)
{
    ASSERT(node->op() == TailCall);
    ASSERT(numArguments() == node->numChildren() - 1);

    // Estimate the size of the inline cache, assuming that every
    // value goes from the stack to the stack (in practice, this will
    // seldom be true, giving us some amount of leeway) and that no
    // spilling will occur (in practice, this will almost always be
    // true).

    // We first compute the new frame base and load the fp/lr
    // registers final values. On debug builds, we also need to
    // account for the fp-sp delta check (twice: fast and slow path).
#if CPU(X86_64)
    m_estimatedSize = 56;
#if !ASSERT_DISABLED
    m_estimatedSize += 26;
#  endif
#elif CPU(ARM64)
    m_estimatedSize = 44;
#if !ASSERT_DISABLED
    m_estimatedSize += 24;
#  endif
#else
    UNREACHABLE_FOR_PLATFORM();
#endif

    // Arguments will probably be loaded & stored twice (fast & slow)
    for (ExitValue& arg : m_arguments)
        m_estimatedSize += 2 * sizeFor(arg.dataFormat());

    // We also have the slow path check, the two calls, and the
    // CallLinkInfo load for the slow path
#if CPU(X86_64)
    m_estimatedSize += 55;
#elif CPU(ARM64)
    m_estimatedSize += 44;
#else
    m_estimatedSize += sizeOfCall + sizeOfJump + sizeOfLoad + sizeOfSlowPathCheck;
#endif
}

void JSTailCall::emit(JITCode& jitCode, CCallHelpers& jit)
{
    StackMaps::Record* record { nullptr };
    
    for (unsigned i = jitCode.stackmaps.records.size(); i--;) {
        record = &jitCode.stackmaps.records[i];
        if (record->patchpointID == m_stackmapID)
            break;
    }

    RELEASE_ASSERT(record->patchpointID == m_stackmapID);

    m_callLinkInfo = jit.codeBlock()->addCallLinkInfo();

    CallFrameShuffleData shuffleData;

    // The callee was the first passed argument, and must be in a GPR because
    // we used the "anyregcc" calling convention
    auto calleeLocation =
        FTL::Location::forStackmaps(nullptr, record->locations[0]);
    GPRReg calleeGPR = calleeLocation.directGPR();
    shuffleData.callee = ValueRecovery::inGPR(calleeGPR, DataFormatJS);

    // The tag type number was the second argument, if there was one
    auto tagTypeNumberLocation =
        FTL::Location::forStackmaps(&jitCode.stackmaps, record->locations[1]);
    if (tagTypeNumberLocation.isGPR() && !tagTypeNumberLocation.addend())
        shuffleData.tagTypeNumber = tagTypeNumberLocation.directGPR();

    shuffleData.args.grow(numArguments());
    HashMap<Reg, Vector<std::pair<ValueRecovery*, int32_t>>> withAddend;
    size_t numAddends { 0 };
    for (size_t i = 0; i < numArguments(); ++i) {
        shuffleData.args[i] = recoveryFor(m_arguments[i], *record, jitCode.stackmaps);
        if (FTL::Location addend = getRegisterWithAddend(m_arguments[i], *record, jitCode.stackmaps)) {
            withAddend.add(
                addend.reg(),
                Vector<std::pair<ValueRecovery*, int32_t>>()).iterator->value.append(
                    std::make_pair(&shuffleData.args[i], addend.addend()));
            numAddends++;
        }
    }

    numAddends = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), numAddends);

    shuffleData.numLocals = static_cast<int64_t>(jitCode.stackmaps.stackSizeForLocals()) / sizeof(void*) + numAddends;

    ASSERT(!numAddends == withAddend.isEmpty());

    if (!withAddend.isEmpty()) {
        jit.subPtr(MacroAssembler::TrustedImm32(numAddends * sizeof(void*)), MacroAssembler::stackPointerRegister);
        VirtualRegister spillBase { 1 - static_cast<int>(shuffleData.numLocals) };
        for (auto entry : withAddend) {
            for (auto pair : entry.value) {
                ASSERT(numAddends > 0);
                VirtualRegister spillSlot { spillBase + --numAddends };
                ASSERT(entry.key.isGPR());
                jit.addPtr(MacroAssembler::TrustedImm32(pair.second), entry.key.gpr());
                jit.storePtr(entry.key.gpr(), CCallHelpers::addressFor(spillSlot));
                jit.subPtr(MacroAssembler::TrustedImm32(pair.second), entry.key.gpr());
                *pair.first = ValueRecovery::displacedInJSStack(spillSlot, pair.first->dataFormat());
            }
        }
        ASSERT(numAddends < stackAlignmentRegisters());
    }

    shuffleData.args.resize(numArguments());
    for (size_t i = 0; i < numArguments(); ++i)
        shuffleData.args[i] = recoveryFor(m_arguments[i], *record, jitCode.stackmaps);

    shuffleData.setupCalleeSaveRegisters(jit.codeBlock());

    CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
        CCallHelpers::NotEqual, calleeGPR, m_targetToCheck,
        CCallHelpers::TrustedImmPtr(0));

    m_callLinkInfo->setFrameShuffleData(shuffleData);
    CallFrameShuffler(jit, shuffleData).prepareForTailCall();

    m_fastCall = jit.nearTailCall();

    slowPath.link(&jit);

    CallFrameShuffler slowPathShuffler(jit, shuffleData);
    slowPathShuffler.setCalleeJSValueRegs(JSValueRegs { GPRInfo::regT0 });
    slowPathShuffler.prepareForSlowPath();

    jit.move(CCallHelpers::TrustedImmPtr(m_callLinkInfo), GPRInfo::regT2);

    m_slowCall = jit.nearCall();

    jit.abortWithReason(JITDidReturnFromTailCall);

    m_callLinkInfo->setUpCall(m_type, m_semanticeOrigin, calleeGPR);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3
