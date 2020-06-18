/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#include "JITOperations.h"
#include "JSArrayBufferView.h"
#include "JSCJSValueInlines.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"

#if ENABLE(WEBASSEMBLY)
#include "WasmMemoryInformation.h"
#include "WasmContextInlines.h"
#endif

namespace JSC {

ExecutableBase* AssemblyHelpers::executableFor(const CodeOrigin& codeOrigin)
{
    auto* inlineCallFrame = codeOrigin.inlineCallFrame();
    if (!inlineCallFrame)
        return m_codeBlock->ownerExecutable();
    return inlineCallFrame->baselineCodeBlock->ownerExecutable();
}

AssemblyHelpers::Jump AssemblyHelpers::branchIfFastTypedArray(GPRReg baseGPR)
{
    return branch32(
        Equal,
        Address(baseGPR, JSArrayBufferView::offsetOfMode()),
        TrustedImm32(FastTypedArray));
}

AssemblyHelpers::Jump AssemblyHelpers::branchIfNotFastTypedArray(GPRReg baseGPR)
{
    return branch32(
        NotEqual,
        Address(baseGPR, JSArrayBufferView::offsetOfMode()),
        TrustedImm32(FastTypedArray));
}

void AssemblyHelpers::incrementSuperSamplerCount()
{
    add32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<const void*>(&g_superSamplerCount)));
}

void AssemblyHelpers::decrementSuperSamplerCount()
{
    sub32(TrustedImm32(1), AbsoluteAddress(bitwise_cast<const void*>(&g_superSamplerCount)));
}
    
void AssemblyHelpers::purifyNaN(FPRReg fpr)
{
    MacroAssembler::Jump notNaN = branchIfNotNaN(fpr);
    static const double NaN = PNaN;
    loadDouble(TrustedImmPtr(&NaN), fpr);
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

#if ASSERT_ENABLED
#if USE(JSVALUE64)
void AssemblyHelpers::jitAssertIsInt32(GPRReg gpr)
{
#if CPU(X86_64) || CPU(ARM64)
    Jump checkInt32 = branch64(BelowOrEqual, gpr, TrustedImm64(static_cast<uintptr_t>(0xFFFFFFFFu)));
    abortWithReason(AHIsNotInt32);
    checkInt32.link(this);
#else
    UNUSED_PARAM(gpr);
#endif
}

void AssemblyHelpers::jitAssertIsJSInt32(GPRReg gpr)
{
    Jump checkJSInt32 = branch64(AboveOrEqual, gpr, GPRInfo::numberTagRegister);
    abortWithReason(AHIsNotJSInt32);
    checkJSInt32.link(this);
}

void AssemblyHelpers::jitAssertIsJSNumber(GPRReg gpr)
{
    Jump checkJSNumber = branchTest64(MacroAssembler::NonZero, gpr, GPRInfo::numberTagRegister);
    abortWithReason(AHIsNotJSNumber);
    checkJSNumber.link(this);
}

void AssemblyHelpers::jitAssertIsJSDouble(GPRReg gpr)
{
    Jump checkJSInt32 = branch64(AboveOrEqual, gpr, GPRInfo::numberTagRegister);
    Jump checkJSNumber = branchTest64(MacroAssembler::NonZero, gpr, GPRInfo::numberTagRegister);
    checkJSInt32.link(this);
    abortWithReason(AHIsNotJSDouble);
    checkJSNumber.link(this);
}

void AssemblyHelpers::jitAssertIsCell(GPRReg gpr)
{
    Jump checkCell = branchTest64(MacroAssembler::Zero, gpr, GPRInfo::notCellMaskRegister);
    abortWithReason(AHIsNotCell);
    checkCell.link(this);
}

void AssemblyHelpers::jitAssertTagsInPlace()
{
    Jump ok = branch64(Equal, GPRInfo::numberTagRegister, TrustedImm64(JSValue::NumberTag));
    abortWithReason(AHNumberTagNotInPlace);
    breakpoint();
    ok.link(this);
    
    ok = branch64(Equal, GPRInfo::notCellMaskRegister, TrustedImm64(JSValue::NotCellMask));
    abortWithReason(AHNotCellMaskNotInPlace);
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
    abortWithReason(AHIsNotJSInt32);
    checkJSInt32.link(this);
}

void AssemblyHelpers::jitAssertIsJSNumber(GPRReg gpr)
{
    Jump checkJSInt32 = branch32(Equal, gpr, TrustedImm32(JSValue::Int32Tag));
    Jump checkJSDouble = branch32(Below, gpr, TrustedImm32(JSValue::LowestTag));
    abortWithReason(AHIsNotJSNumber);
    checkJSInt32.link(this);
    checkJSDouble.link(this);
}

void AssemblyHelpers::jitAssertIsJSDouble(GPRReg gpr)
{
    Jump checkJSDouble = branch32(Below, gpr, TrustedImm32(JSValue::LowestTag));
    abortWithReason(AHIsNotJSDouble);
    checkJSDouble.link(this);
}

void AssemblyHelpers::jitAssertIsCell(GPRReg gpr)
{
    Jump checkCell = branch32(Equal, gpr, TrustedImm32(JSValue::CellTag));
    abortWithReason(AHIsNotCell);
    checkCell.link(this);
}

void AssemblyHelpers::jitAssertTagsInPlace()
{
}
#endif // USE(JSVALUE32_64)

void AssemblyHelpers::jitAssertHasValidCallFrame()
{
    Jump checkCFR = branchTestPtr(Zero, GPRInfo::callFrameRegister, TrustedImm32(7));
    abortWithReason(AHCallFrameMisaligned);
    checkCFR.link(this);
}

void AssemblyHelpers::jitAssertIsNull(GPRReg gpr)
{
    Jump checkNull = branchTestPtr(Zero, gpr);
    abortWithReason(AHIsNotNull);
    checkNull.link(this);
}

void AssemblyHelpers::jitAssertArgumentCountSane()
{
    Jump ok = branch32(Below, payloadFor(CallFrameSlot::argumentCountIncludingThis), TrustedImm32(10000000));
    abortWithReason(AHInsaneArgumentCount);
    ok.link(this);
}

#endif // ASSERT_ENABLED

void AssemblyHelpers::jitReleaseAssertNoException(VM& vm)
{
    Jump noException;
#if USE(JSVALUE64)
    noException = branchTest64(Zero, AbsoluteAddress(vm.addressOfException()));
#elif USE(JSVALUE32_64)
    noException = branch32(Equal, AbsoluteAddress(vm.addressOfException()), TrustedImm32(0));
#endif
    abortWithReason(JITUncoughtExceptionAfterCall);
    noException.link(this);
}

void AssemblyHelpers::callExceptionFuzz(VM& vm)
{
    if (!Options::useExceptionFuzz())
        return;

    EncodedJSValue* buffer = vm.exceptionFuzzingBuffer(sizeof(EncodedJSValue) * (GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters));

    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
        store64(GPRInfo::toRegister(i), buffer + i);
#else
        store32(GPRInfo::toRegister(i), buffer + i);
#endif
    }
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        move(TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        storeDouble(FPRInfo::toRegister(i), Address(GPRInfo::regT0));
    }

    // Set up one argument.
    move(TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
    move(TrustedImmPtr(tagCFunction<OperationPtrTag>(operationExceptionFuzzWithCallFrame)), GPRInfo::nonPreservedNonReturnGPR);
    prepareCallOperation(vm);
    call(GPRInfo::nonPreservedNonReturnGPR, OperationPtrTag);

    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        move(TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        loadDouble(Address(GPRInfo::regT0), FPRInfo::toRegister(i));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
        load64(buffer + i, GPRInfo::toRegister(i));
#else
        load32(buffer + i, GPRInfo::toRegister(i));
#endif
    }
}

AssemblyHelpers::Jump AssemblyHelpers::emitJumpIfException(VM& vm)
{
    return emitExceptionCheck(vm, NormalExceptionCheck);
}

AssemblyHelpers::Jump AssemblyHelpers::emitExceptionCheck(VM& vm, ExceptionCheckKind kind, ExceptionJumpWidth width)
{
    callExceptionFuzz(vm);

    if (width == FarJumpWidth)
        kind = (kind == NormalExceptionCheck ? InvertedExceptionCheck : NormalExceptionCheck);
    
    Jump result;
#if USE(JSVALUE64)
    result = branchTest64(kind == NormalExceptionCheck ? NonZero : Zero, AbsoluteAddress(vm.addressOfException()));
#elif USE(JSVALUE32_64)
    result = branch32(kind == NormalExceptionCheck ? NotEqual : Equal, AbsoluteAddress(vm.addressOfException()), TrustedImm32(0));
#endif
    
    if (width == NormalJumpWidth)
        return result;

    PatchableJump realJump = patchableJump();
    result.link(this);
    
    return realJump.m_jump;
}

AssemblyHelpers::Jump AssemblyHelpers::emitNonPatchableExceptionCheck(VM& vm)
{
    callExceptionFuzz(vm);

    Jump result;
#if USE(JSVALUE64)
    result = branchTest64(NonZero, AbsoluteAddress(vm.addressOfException()));
#elif USE(JSVALUE32_64)
    result = branch32(NotEqual, AbsoluteAddress(vm.addressOfException()), TrustedImm32(0));
#endif
    
    return result;
}

void AssemblyHelpers::emitStoreStructureWithTypeInfo(AssemblyHelpers& jit, TrustedImmPtr structure, RegisterID dest)
{
    const Structure* structurePtr = reinterpret_cast<const Structure*>(structure.m_value);
#if USE(JSVALUE64)
    jit.store64(TrustedImm64(structurePtr->idBlob()), MacroAssembler::Address(dest, JSCell::structureIDOffset()));
    if (ASSERT_ENABLED) {
        Jump correctStructure = jit.branch32(Equal, MacroAssembler::Address(dest, JSCell::structureIDOffset()), TrustedImm32(structurePtr->id()));
        jit.abortWithReason(AHStructureIDIsValid);
        correctStructure.link(&jit);

        Jump correctIndexingType = jit.branch8(Equal, MacroAssembler::Address(dest, JSCell::indexingTypeAndMiscOffset()), TrustedImm32(structurePtr->indexingModeIncludingHistory()));
        jit.abortWithReason(AHIndexingTypeIsValid);
        correctIndexingType.link(&jit);

        Jump correctType = jit.branch8(Equal, MacroAssembler::Address(dest, JSCell::typeInfoTypeOffset()), TrustedImm32(structurePtr->typeInfo().type()));
        jit.abortWithReason(AHTypeInfoIsValid);
        correctType.link(&jit);

        Jump correctFlags = jit.branch8(Equal, MacroAssembler::Address(dest, JSCell::typeInfoFlagsOffset()), TrustedImm32(structurePtr->typeInfo().inlineTypeFlags()));
        jit.abortWithReason(AHTypeInfoInlineTypeFlagsAreValid);
        correctFlags.link(&jit);
    }
#else
    // Do a 32-bit wide store to initialize the cell's fields.
    jit.store32(TrustedImm32(structurePtr->objectInitializationBlob()), MacroAssembler::Address(dest, JSCell::indexingTypeAndMiscOffset()));
    jit.storePtr(structure, MacroAssembler::Address(dest, JSCell::structureIDOffset()));
#endif
}

void AssemblyHelpers::loadProperty(GPRReg object, GPRReg offset, JSValueRegs result)
{
    Jump isInline = branch32(LessThan, offset, TrustedImm32(firstOutOfLineOffset));
    
    loadPtr(Address(object, JSObject::butterflyOffset()), result.payloadGPR());
    neg32(offset);
    signExtend32ToPtr(offset, offset);
    Jump ready = jump();
    
    isInline.link(this);
    addPtr(
        TrustedImm32(
            static_cast<int32_t>(sizeof(JSObject)) -
            (static_cast<int32_t>(firstOutOfLineOffset) - 2) * static_cast<int32_t>(sizeof(EncodedJSValue))),
        object, result.payloadGPR());
    
    ready.link(this);
    
    loadValue(
        BaseIndex(
            result.payloadGPR(), offset, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)),
        result);
}

void AssemblyHelpers::emitLoadStructure(VM& vm, RegisterID source, RegisterID dest, RegisterID scratch)
{
#if USE(JSVALUE64)
#if CPU(ARM64)
    RegisterID scratch2 = dataTempRegister;
#elif CPU(X86_64)
    RegisterID scratch2 = scratchRegister();
#else
#error "Unsupported cpu"
#endif

    ASSERT(dest != scratch);
    ASSERT(dest != scratch2);
    ASSERT(scratch != scratch2);

    load32(MacroAssembler::Address(source, JSCell::structureIDOffset()), scratch2);
    loadPtr(vm.heap.structureIDTable().base(), scratch);
    rshift32(scratch2, TrustedImm32(StructureIDTable::s_numberOfEntropyBits), dest);
    loadPtr(MacroAssembler::BaseIndex(scratch, dest, MacroAssembler::ScalePtr), dest);
    lshiftPtr(TrustedImm32(StructureIDTable::s_entropyBitsShiftForStructurePointer), scratch2);
    xorPtr(scratch2, dest);
#else // not USE(JSVALUE64)
    UNUSED_PARAM(scratch);
    UNUSED_PARAM(vm);
    loadPtr(MacroAssembler::Address(source, JSCell::structureIDOffset()), dest);
#endif // not USE(JSVALUE64)
}

void AssemblyHelpers::makeSpaceOnStackForCCall()
{
    unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), maxFrameExtentForSlowPathCall);
    if (stackOffset)
        subPtr(TrustedImm32(stackOffset), stackPointerRegister);
}

void AssemblyHelpers::reclaimSpaceOnStackForCCall()
{
    unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), maxFrameExtentForSlowPathCall);
    if (stackOffset)
        addPtr(TrustedImm32(stackOffset), stackPointerRegister);
}

#if USE(JSVALUE64)
template<typename LoadFromHigh, typename StoreToHigh, typename LoadFromLow, typename StoreToLow>
void emitRandomThunkImpl(AssemblyHelpers& jit, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, FPRReg result, const LoadFromHigh& loadFromHigh, const StoreToHigh& storeToHigh, const LoadFromLow& loadFromLow, const StoreToLow& storeToLow)
{
    // Inlined WeakRandom::advance().
    // uint64_t x = m_low;
    loadFromLow(scratch0);
    // uint64_t y = m_high;
    loadFromHigh(scratch1);
    // m_low = y;
    storeToLow(scratch1);

    // x ^= x << 23;
    jit.move(scratch0, scratch2);
    jit.lshift64(AssemblyHelpers::TrustedImm32(23), scratch2);
    jit.xor64(scratch2, scratch0);

    // x ^= x >> 17;
    jit.move(scratch0, scratch2);
    jit.rshift64(AssemblyHelpers::TrustedImm32(17), scratch2);
    jit.xor64(scratch2, scratch0);

    // x ^= y ^ (y >> 26);
    jit.move(scratch1, scratch2);
    jit.rshift64(AssemblyHelpers::TrustedImm32(26), scratch2);
    jit.xor64(scratch1, scratch2);
    jit.xor64(scratch2, scratch0);

    // m_high = x;
    storeToHigh(scratch0);

    // return x + y;
    jit.add64(scratch1, scratch0);

    // Extract random 53bit. [0, 53] bit is safe integer number ranges in double representation.
    jit.move(AssemblyHelpers::TrustedImm64((1ULL << 53) - 1), scratch1);
    jit.and64(scratch1, scratch0);
    // Now, scratch0 is always in range of int64_t. Safe to convert it to double with cvtsi2sdq.
    jit.convertInt64ToDouble(scratch0, result);

    // Convert `(53bit double integer value) / (1 << 53)` to `(53bit double integer value) * (1.0 / (1 << 53))`.
    // In latter case, `1.0 / (1 << 53)` will become a double value represented as (mantissa = 0 & exp = 970, it means 1e-(2**54)).
    static constexpr double scale = 1.0 / (1ULL << 53);

    // Multiplying 1e-(2**54) with the double integer does not change anything of the mantissa part of the double integer.
    // It just reduces the exp part of the given 53bit double integer.
    // (Except for 0.0. This is specially handled and in this case, exp just becomes 0.)
    // Now we get 53bit precision random double value in [0, 1).
    jit.move(AssemblyHelpers::TrustedImmPtr(&scale), scratch1);
    jit.mulDouble(AssemblyHelpers::Address(scratch1), result);
}

void AssemblyHelpers::emitRandomThunk(JSGlobalObject* globalObject, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, FPRReg result)
{
    void* lowAddress = reinterpret_cast<uint8_t*>(globalObject) + JSGlobalObject::weakRandomOffset() + WeakRandom::lowOffset();
    void* highAddress = reinterpret_cast<uint8_t*>(globalObject) + JSGlobalObject::weakRandomOffset() + WeakRandom::highOffset();

    auto loadFromHigh = [&](GPRReg high) {
        load64(highAddress, high);
    };
    auto storeToHigh = [&](GPRReg high) {
        store64(high, highAddress);
    };
    auto loadFromLow = [&](GPRReg low) {
        load64(lowAddress, low);
    };
    auto storeToLow = [&](GPRReg low) {
        store64(low, lowAddress);
    };

    emitRandomThunkImpl(*this, scratch0, scratch1, scratch2, result, loadFromHigh, storeToHigh, loadFromLow, storeToLow);
}

void AssemblyHelpers::emitRandomThunk(VM& vm, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, FPRReg result)
{
    emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, scratch3);
    emitLoadStructure(vm, scratch3, scratch3, scratch0);
    loadPtr(Address(scratch3, Structure::globalObjectOffset()), scratch3);
    // Now, scratch3 holds JSGlobalObject*.

    auto loadFromHigh = [&](GPRReg high) {
        load64(Address(scratch3, JSGlobalObject::weakRandomOffset() + WeakRandom::highOffset()), high);
    };
    auto storeToHigh = [&](GPRReg high) {
        store64(high, Address(scratch3, JSGlobalObject::weakRandomOffset() + WeakRandom::highOffset()));
    };
    auto loadFromLow = [&](GPRReg low) {
        load64(Address(scratch3, JSGlobalObject::weakRandomOffset() + WeakRandom::lowOffset()), low);
    };
    auto storeToLow = [&](GPRReg low) {
        store64(low, Address(scratch3, JSGlobalObject::weakRandomOffset() + WeakRandom::lowOffset()));
    };

    emitRandomThunkImpl(*this, scratch0, scratch1, scratch2, result, loadFromHigh, storeToHigh, loadFromLow, storeToLow);
}
#endif

void AssemblyHelpers::emitAllocateWithNonNullAllocator(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, GPRReg scratchGPR, JumpList& slowPath, Optional<GPRReg> optionalScratchGPR2, Optional<GPRReg> optionalScratchGPR3)
{
    if (Options::forceGCSlowPaths()) {
        slowPath.append(jump());
        return;
    }

    // NOTE, some invariants of this function:
    // - When going to the slow path, we must leave resultGPR with zero in it.
    // - We *can not* use RegisterSet::macroScratchRegisters on x86.
    // - We *can* use RegisterSet::macroScratchRegisters on ARM.

    Jump popPath;
    JumpList done;
    
    if (allocator.isConstant())
        move(TrustedImmPtr(allocator.allocator().localAllocator()), allocatorGPR);

    load32(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfRemaining()), resultGPR);
    popPath = branchTest32(Zero, resultGPR);
    if (allocator.isConstant())
        add32(TrustedImm32(-allocator.allocator().cellSize()), resultGPR, scratchGPR);
    else {
        move(resultGPR, scratchGPR);
        sub32(Address(allocatorGPR, LocalAllocator::offsetOfCellSize()), scratchGPR);
    }
    negPtr(resultGPR);
    store32(scratchGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfRemaining()));
    Address payloadEndAddr = Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfPayloadEnd());
    addPtr(payloadEndAddr, resultGPR);

    done.append(jump());
        
#if ENABLE(BITMAP_FREELIST)
    ASSERT(resultGPR != scratchGPR);

    auto rowIndexGPR = resultGPR;
    auto rowBitmapGPR = scratchGPR;

    GPRReg scratchGPR2 = optionalScratchGPR2 ? optionalScratchGPR2.value() : scratchRegister();
    ASSERT(scratchGPR2 != resultGPR);
    ASSERT(scratchGPR2 != scratchGPR);

    auto rowAddressGPR = scratchGPR2;
    auto clearBit64ScratchGPR = scratchGPR2;

    bool canPreloadRowAddressGPR = false;
    if (optionalScratchGPR3) {
        clearBit64ScratchGPR = optionalScratchGPR3.value();
        canPreloadRowAddressGPR = true;
    } else if (isX86_64()) {
        // x86_64's clearBit64() does actually need to use clearBit64ScratchGPR.
        // So, we can preload the row address into it.
        clearBit64ScratchGPR = InvalidGPRReg;
        canPreloadRowAddressGPR = true;
#if CPU(ARM64)
    } else if (isARM64()) {
        // ARM64's fast path does actually need to use the memoryTempRegister.
        // So, we can use that for the clearBit64ScratchGPR and allow the
        // row address to be preloaded in scratchGPR2.
        clearBit64ScratchGPR = getCachedMemoryTempRegisterIDAndInvalidate();
        canPreloadRowAddressGPR = true;
#endif
    }
    ASSERT(clearBit64ScratchGPR != resultGPR);
    ASSERT(clearBit64ScratchGPR != scratchGPR);
    if (canPreloadRowAddressGPR)
        ASSERT(clearBit64ScratchGPR != scratchGPR2);

    // The code below for rowBitmapGPR relies on this.
    static_assert(sizeof(FreeList::AtomsBitmap::Word) == sizeof(uint64_t));

    // Check for middle path: have another row to visit?
    Label checkForMoreRows = label();

    load32(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentRowIndex()), rowIndexGPR);

    if (!canPreloadRowAddressGPR)
        loadPtr(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentMarkedBlockRowAddress()), rowAddressGPR);

    slowPath.append(branchTestPtr(Zero, rowIndexGPR));

    // Middle path: there is another row left to visit.
    Jump foundNonEmptyRow;
    Label checkNextRow = label();
    {
        // Load the next row bitmap and point m_currentMarkedBlockRowAddress to the next row.

        // Note: offsetOfBitmapRows() points to 1 word before m_bitmap. We do this
        // deliberately because it allows us to schedule instructions better and
        // do this load before the decrement below.
        load64(BaseIndex(allocatorGPR, rowIndexGPR, TimesEight, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfBitmapRows()), rowBitmapGPR);

        sub64(TrustedImm32(1), rowIndexGPR);
        subPtr(TrustedImm32(FreeList::atomsRowBytes), rowAddressGPR);

        foundNonEmptyRow = branchTest64(NonZero, rowBitmapGPR);
        branchTestPtr(NonZero, rowIndexGPR).linkTo(checkNextRow, this);
    }

    // Slow path: no more rows.
    // Both rowIndexGPR and rowBitmapGPR should be null here.
    store32(rowIndexGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentRowIndex()));
    store64(rowBitmapGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentRowBitmap()));
    slowPath.append(jump());

    // Transition from middle path back to fast path to allocate.
    foundNonEmptyRow.link(this);
    storePtr(rowAddressGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentMarkedBlockRowAddress()));
    store32(rowIndexGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentRowIndex()));

    Jump allocateFromCurrentRow = jump();

    popPath.link(this);

    // Check for fast path: have available bit in m_currentRowBitmap?
    load64(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentRowBitmap()), rowBitmapGPR);

    if (canPreloadRowAddressGPR) {
        // Preload the row address needed on the fast and middle path.
        loadPtr(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentMarkedBlockRowAddress()), rowAddressGPR);
    }

    branchTest64(Zero, rowBitmapGPR).linkTo(checkForMoreRows, this);

    // Fast path: we have a bit to use.
    allocateFromCurrentRow.link(this);
    {
        // Remove this bit from m_currentRowBitmap.
        auto atomIndexInRowGPR = resultGPR;
        countTrailingZeros64WithoutNullCheck(rowBitmapGPR, atomIndexInRowGPR);
        clearBit64(atomIndexInRowGPR, rowBitmapGPR, clearBit64ScratchGPR);

        if (!canPreloadRowAddressGPR)
            loadPtr(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentMarkedBlockRowAddress()), rowAddressGPR);

        store64(rowBitmapGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfCurrentRowBitmap()));

        // Compute atom address of this bit.
        ASSERT(resultGPR == atomIndexInRowGPR);
        shiftAndAdd(rowAddressGPR, resultGPR, FreeList::atomSizeShift, resultGPR);
    }

#else
    UNUSED_PARAM(optionalScratchGPR2);
    UNUSED_PARAM(optionalScratchGPR3);

    popPath.link(this);

    loadPtr(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfScrambledHead()), resultGPR);
    xorPtr(Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfSecret()), resultGPR);
    slowPath.append(branchTestPtr(Zero, resultGPR));
        
    // The object is half-allocated: we have what we know is a fresh object, but
    // it's still on the GC's free list.
    loadPtr(Address(resultGPR, FreeCell::offsetOfScrambledNext()), scratchGPR);
    storePtr(scratchGPR, Address(allocatorGPR, LocalAllocator::offsetOfFreeList() + FreeList::offsetOfScrambledHead()));
#endif

    done.link(this);
}

void AssemblyHelpers::emitAllocate(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, GPRReg scratchGPR, JumpList& slowPath)
{
    if (allocator.isConstant()) {
        if (!allocator.allocator()) {
            slowPath.append(jump());
            return;
        }
    } else
        slowPath.append(branchTestPtr(Zero, allocatorGPR));
    emitAllocateWithNonNullAllocator(resultGPR, allocator, allocatorGPR, scratchGPR, slowPath);
}

void AssemblyHelpers::emitAllocateVariableSized(GPRReg resultGPR, CompleteSubspace& subspace, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath)
{
    static_assert(!(MarkedSpace::sizeStep & (MarkedSpace::sizeStep - 1)), "MarkedSpace::sizeStep must be a power of two.");
    
    unsigned stepShift = getLSBSet(MarkedSpace::sizeStep);
    
    add32(TrustedImm32(MarkedSpace::sizeStep - 1), allocationSize, scratchGPR1);
    urshift32(TrustedImm32(stepShift), scratchGPR1);
    slowPath.append(branch32(Above, scratchGPR1, TrustedImm32(MarkedSpace::largeCutoff >> stepShift)));
    move(TrustedImmPtr(subspace.allocatorForSizeStep()), scratchGPR2);
    loadPtr(BaseIndex(scratchGPR2, scratchGPR1, ScalePtr), scratchGPR1);
    
    emitAllocate(resultGPR, JITAllocator::variable(), scratchGPR1, scratchGPR2, slowPath);
}

void AssemblyHelpers::restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(EntryFrame*& topEntryFrame)
{
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
    RegisterAtOffsetList* allCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
    RegisterSet dontRestoreRegisters = RegisterSet::stackRegisters();
    unsigned registerCount = allCalleeSaves->size();

    GPRReg scratch = InvalidGPRReg;
    unsigned scratchGPREntryIndex = 0;

    // Use the first GPR entry's register as our scratch.
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        if (entry.reg().isGPR()) {
            scratchGPREntryIndex = i;
            scratch = entry.reg().gpr();
            break;
        }
    }
    ASSERT(scratch != InvalidGPRReg);

    loadPtr(&topEntryFrame, scratch);
    addPtr(TrustedImm32(EntryFrame::calleeSaveRegistersBufferOffset()), scratch);

    // Restore all callee saves except for the scratch.
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontRestoreRegisters.get(entry.reg()))
            continue;
        if (i == scratchGPREntryIndex)
            continue;
        loadReg(Address(scratch, entry.offset()), entry.reg());
    }

    // Restore the callee save value of the scratch.
    RegisterAtOffset entry = allCalleeSaves->at(scratchGPREntryIndex);
    ASSERT(!dontRestoreRegisters.get(entry.reg()));
    ASSERT(entry.reg().isGPR());
    ASSERT(scratch == entry.reg().gpr());
    loadReg(Address(scratch, entry.offset()), scratch);
#else
    UNUSED_PARAM(topEntryFrame);
#endif
}

void AssemblyHelpers::emitDumbVirtualCall(VM& vm, JSGlobalObject* globalObject, CallLinkInfo* info)
{
    move(TrustedImmPtr(info), GPRInfo::regT2);
    move(TrustedImmPtr(globalObject), GPRInfo::regT3);
    Call call = nearCall();
    addLinkTask(
        [=, &vm] (LinkBuffer& linkBuffer) {
            MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunk = virtualThunkFor(vm, *info);
            info->setSlowStub(GCAwareJITStubRoutine::create(virtualThunk, vm));
            linkBuffer.link(call, CodeLocationLabel<JITStubRoutinePtrTag>(virtualThunk.code()));
        });
}

#if USE(JSVALUE64)
void AssemblyHelpers::wangsInt64Hash(GPRReg inputAndResult, GPRReg scratch)
{
    GPRReg input = inputAndResult;
    // key += ~(key << 32);
    move(input, scratch);
    lshift64(TrustedImm32(32), scratch);
    not64(scratch);
    add64(scratch, input);
    // key ^= (key >> 22);
    move(input, scratch);
    urshift64(TrustedImm32(22), scratch);
    xor64(scratch, input);
    // key += ~(key << 13);
    move(input, scratch);
    lshift64(TrustedImm32(13), scratch);
    not64(scratch);
    add64(scratch, input);
    // key ^= (key >> 8);
    move(input, scratch);
    urshift64(TrustedImm32(8), scratch);
    xor64(scratch, input);
    // key += (key << 3);
    move(input, scratch);
    lshift64(TrustedImm32(3), scratch);
    add64(scratch, input);
    // key ^= (key >> 15);
    move(input, scratch);
    urshift64(TrustedImm32(15), scratch);
    xor64(scratch, input);
    // key += ~(key << 27);
    move(input, scratch);
    lshift64(TrustedImm32(27), scratch);
    not64(scratch);
    add64(scratch, input);
    // key ^= (key >> 31);
    move(input, scratch);
    urshift64(TrustedImm32(31), scratch);
    xor64(scratch, input);

    // return static_cast<unsigned>(result)
    void* mask = bitwise_cast<void*>(static_cast<uintptr_t>(UINT_MAX));
    and64(TrustedImmPtr(mask), inputAndResult);
}
#endif // USE(JSVALUE64)

void AssemblyHelpers::emitConvertValueToBoolean(VM& vm, JSValueRegs value, GPRReg result, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg valueAsFPR, FPRReg tempFPR, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject* globalObject, bool invert)
{
    // Implements the following control flow structure:
    // if (value is cell) {
    //     if (value is string or value is HeapBigInt)
    //         result = !!value->length
    //     else {
    //         do evil things for masquerades-as-undefined
    //         result = true
    //     }
    // } else if (value is int32) {
    //     result = !!unboxInt32(value)
    // } else if (value is number) {
    //     result = !!unboxDouble(value)
    // } else if (value is BigInt32) {
    //     result = !!unboxBigInt32(value)
    // } else {
    //     result = value == jsTrue
    // }

    JumpList done;

    auto notCell = branchIfNotCell(value);
    auto isString = branchIfString(value.payloadGPR());
    auto isHeapBigInt = branchIfHeapBigInt(value.payloadGPR());

    if (shouldCheckMasqueradesAsUndefined) {
        ASSERT(scratchIfShouldCheckMasqueradesAsUndefined != InvalidGPRReg);
        JumpList isNotMasqueradesAsUndefined;
        isNotMasqueradesAsUndefined.append(branchTest8(Zero, Address(value.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)));
        emitLoadStructure(vm, value.payloadGPR(), result, scratchIfShouldCheckMasqueradesAsUndefined);
        move(TrustedImmPtr(globalObject), scratchIfShouldCheckMasqueradesAsUndefined);
        isNotMasqueradesAsUndefined.append(branchPtr(NotEqual, Address(result, Structure::globalObjectOffset()), scratchIfShouldCheckMasqueradesAsUndefined));

        // We act like we are "undefined" here.
        move(invert ? TrustedImm32(1) : TrustedImm32(0), result);
        done.append(jump());
        isNotMasqueradesAsUndefined.link(this);
    }
    move(invert ? TrustedImm32(0) : TrustedImm32(1), result);
    done.append(jump());

    isString.link(this);
    move(TrustedImmPtr(jsEmptyString(vm)), result);
    comparePtr(invert ? Equal : NotEqual, value.payloadGPR(), result, result);
    done.append(jump());

    isHeapBigInt.link(this);
    load32(Address(value.payloadGPR(), JSBigInt::offsetOfLength()), result);
    compare32(invert ? Equal : NotEqual, result, TrustedImm32(0), result);
    done.append(jump());

    notCell.link(this);
    auto notInt32 = branchIfNotInt32(value);
    compare32(invert ? Equal : NotEqual, value.payloadGPR(), TrustedImm32(0), result);
    done.append(jump());

    notInt32.link(this);
    auto notDouble = branchIfNotDoubleKnownNotInt32(value);
#if USE(JSVALUE64)
    unboxDouble(value.gpr(), result, valueAsFPR);
#else
    unboxDouble(value, valueAsFPR, tempFPR);
#endif
    move(invert ? TrustedImm32(1) : TrustedImm32(0), result);
    done.append(branchDoubleZeroOrNaN(valueAsFPR, tempFPR));
    move(invert ? TrustedImm32(0) : TrustedImm32(1), result);
    done.append(jump());

    notDouble.link(this);
#if USE(BIGINT32)
    auto isNotBigInt32 = branchIfNotBigInt32(value.gpr(), result);
    move(value.gpr(), result);
    urshift64(TrustedImm32(16), result);
    compare32(invert ? Equal : NotEqual, result, TrustedImm32(0), result);
    done.append(jump());

    isNotBigInt32.link(this);
#endif // USE(BIGINT32)
#if USE(JSVALUE64)
    compare64(invert ? NotEqual : Equal, value.gpr(), TrustedImm32(JSValue::ValueTrue), result);
#else
    move(invert ? TrustedImm32(1) : TrustedImm32(0), result);
    done.append(branchIfNotBoolean(value, InvalidGPRReg));
    compare32(invert ? Equal : NotEqual, value.payloadGPR(), TrustedImm32(0), result);
#endif

    done.link(this);
}

AssemblyHelpers::JumpList AssemblyHelpers::branchIfValue(VM& vm, JSValueRegs value, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg valueAsFPR, FPRReg tempFPR, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject* globalObject, bool invert)
{
    // Implements the following control flow structure:
    // if (value is cell) {
    //     if (value is string or value is HeapBigInt)
    //         result = !!value->length
    //     else {
    //         do evil things for masquerades-as-undefined
    //         result = true
    //     }
    // } else if (value is int32) {
    //     result = !!unboxInt32(value)
    // } else if (value is number) {
    //     result = !!unboxDouble(value)
    // } else if (value is BigInt32) {
    //     result = !!unboxBigInt32(value)
    // } else {
    //     result = value == jsTrue
    // }

    JumpList done;
    JumpList truthy;

    auto notCell = branchIfNotCell(value);
    auto isString = branchIfString(value.payloadGPR());
    auto isHeapBigInt = branchIfHeapBigInt(value.payloadGPR());

    if (shouldCheckMasqueradesAsUndefined) {
        ASSERT(scratchIfShouldCheckMasqueradesAsUndefined != InvalidGPRReg);
        JumpList isNotMasqueradesAsUndefined;
        isNotMasqueradesAsUndefined.append(branchTest8(Zero, Address(value.payloadGPR(), JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)));
        emitLoadStructure(vm, value.payloadGPR(), scratch, scratchIfShouldCheckMasqueradesAsUndefined);
        move(TrustedImmPtr(globalObject), scratchIfShouldCheckMasqueradesAsUndefined);
        isNotMasqueradesAsUndefined.append(branchPtr(NotEqual, Address(scratch, Structure::globalObjectOffset()), scratchIfShouldCheckMasqueradesAsUndefined));

        // We act like we are "undefined" here.
        if (invert)
            truthy.append(jump());
        else
            done.append(jump());

        if (invert)
            done.append(isNotMasqueradesAsUndefined);
        else
            truthy.append(isNotMasqueradesAsUndefined);
    } else {
        if (invert)
            done.append(jump());
        else
            truthy.append(jump());
    }

    isString.link(this);
    truthy.append(branchPtr(invert ? Equal : NotEqual, value.payloadGPR(), TrustedImmPtr(jsEmptyString(vm))));
    done.append(jump());

    isHeapBigInt.link(this);
    truthy.append(branchTest32(invert ? Zero : NonZero, Address(value.payloadGPR(), JSBigInt::offsetOfLength())));
    done.append(jump());

    notCell.link(this);
    auto notInt32 = branchIfNotInt32(value);
    truthy.append(branchTest32(invert ? Zero : NonZero, value.payloadGPR()));
    done.append(jump());

    notInt32.link(this);
    auto notDouble = branchIfNotDoubleKnownNotInt32(value);
#if USE(JSVALUE64)
    unboxDouble(value.gpr(), scratch, valueAsFPR);
#else
    unboxDouble(value, valueAsFPR, tempFPR);
#endif
    if (invert) {
        truthy.append(branchDoubleZeroOrNaN(valueAsFPR, tempFPR));
        done.append(jump());
    } else {
        done.append(branchDoubleZeroOrNaN(valueAsFPR, tempFPR));
        truthy.append(jump());
    }

    notDouble.link(this);
#if USE(BIGINT32)
    auto isNotBigInt32 = branchIfNotBigInt32(value.gpr(), scratch);
    move(value.gpr(), scratch);
    urshift64(TrustedImm32(16), scratch);
    truthy.append(branchTest32(invert ? Zero : NonZero, scratch));
    done.append(jump());

    isNotBigInt32.link(this);
#endif // USE(BIGINT32)
#if USE(JSVALUE64)
    truthy.append(branch64(invert ? NotEqual : Equal, value.gpr(), TrustedImm64(JSValue::encode(jsBoolean(true)))));
#else
    auto notBoolean = branchIfNotBoolean(value, InvalidGPRReg);
    if (invert)
        truthy.append(notBoolean);
    else
        done.append(notBoolean);
    truthy.append(branch32(invert ? Equal : NotEqual, value.payloadGPR(), TrustedImm32(0)));
#endif

    done.link(this);

    return truthy;
}

#if ENABLE(WEBASSEMBLY)
void AssemblyHelpers::loadWasmContextInstance(GPRReg dst)
{
#if ENABLE(FAST_TLS_JIT)
    if (Wasm::Context::useFastTLS()) {
        loadFromTLSPtr(fastTLSOffsetForKey(WTF_WASM_CONTEXT_KEY), dst);
        return;
    }
#endif
    move(Wasm::PinnedRegisterInfo::get().wasmContextInstancePointer, dst);
}

void AssemblyHelpers::storeWasmContextInstance(GPRReg src)
{
#if ENABLE(FAST_TLS_JIT)
    if (Wasm::Context::useFastTLS()) {
        storeToTLSPtr(src, fastTLSOffsetForKey(WTF_WASM_CONTEXT_KEY));
        return;
    }
#endif
    move(src, Wasm::PinnedRegisterInfo::get().wasmContextInstancePointer);
}

bool AssemblyHelpers::loadWasmContextInstanceNeedsMacroScratchRegister()
{
#if ENABLE(FAST_TLS_JIT)
    if (Wasm::Context::useFastTLS())
        return loadFromTLSPtrNeedsMacroScratchRegister();
#endif
    return false;
}

bool AssemblyHelpers::storeWasmContextInstanceNeedsMacroScratchRegister()
{
#if ENABLE(FAST_TLS_JIT)
    if (Wasm::Context::useFastTLS())
        return storeToTLSPtrNeedsMacroScratchRegister();
#endif
    return false;
}

#endif // ENABLE(WEBASSEMBLY)

void AssemblyHelpers::debugCall(VM& vm, V_DebugOperation_EPP function, void* argument)
{
    size_t scratchSize = sizeof(EncodedJSValue) * (GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);
    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());

    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
        store64(GPRInfo::toRegister(i), buffer + i);
#else
        store32(GPRInfo::toRegister(i), buffer + i);
#endif
    }

    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        move(TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        storeDouble(FPRInfo::toRegister(i), GPRInfo::regT0);
    }

    // Tell GC mark phase how much of the scratch buffer is active during call.
    move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), GPRInfo::regT0);
    storePtr(TrustedImmPtr(scratchSize), GPRInfo::regT0);

#if CPU(X86_64) || CPU(ARM_THUMB2) || CPU(ARM64) || CPU(MIPS)
    move(TrustedImmPtr(buffer), GPRInfo::argumentGPR2);
    move(TrustedImmPtr(argument), GPRInfo::argumentGPR1);
    move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    GPRReg scratch = selectScratchGPR(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2);
#else
#error "JIT not supported on this platform."
#endif
    prepareCallOperation(vm);
    move(TrustedImmPtr(tagCFunctionPtr<OperationPtrTag>(function)), scratch);
    call(scratch, OperationPtrTag);

    move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), GPRInfo::regT0);
    storePtr(TrustedImmPtr(nullptr), GPRInfo::regT0);

    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        move(TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        loadDouble(GPRInfo::regT0, FPRInfo::toRegister(i));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
        load64(buffer + i, GPRInfo::toRegister(i));
#else
        load32(buffer + i, GPRInfo::toRegister(i));
#endif
    }
}

void AssemblyHelpers::copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(GPRReg calleeSavesBuffer)
{
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
    addPtr(TrustedImm32(EntryFrame::calleeSaveRegistersBufferOffset()), calleeSavesBuffer);

    RegisterAtOffsetList* allCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
    RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
    unsigned registerCount = allCalleeSaves->size();
    
    for (unsigned i = 0; i < registerCount; i++) {
        RegisterAtOffset entry = allCalleeSaves->at(i);
        if (dontCopyRegisters.get(entry.reg()))
            continue;
        storeReg(entry.reg(), Address(calleeSavesBuffer, entry.offset()));
    }
#else
    UNUSED_PARAM(calleeSavesBuffer);
#endif
}

void AssemblyHelpers::sanitizeStackInline(VM& vm, GPRReg scratch)
{
    loadPtr(vm.addressOfLastStackTop(), scratch);
    Jump done = branchPtr(BelowOrEqual, stackPointerRegister, scratch);
    Label loop = label();
    storePtr(TrustedImmPtr(nullptr), scratch);
    addPtr(TrustedImmPtr(sizeof(void*)), scratch);
    branchPtr(Above, stackPointerRegister, scratch).linkTo(loop, this);
    done.link(this);
    move(stackPointerRegister, scratch);
    storePtr(scratch, vm.addressOfLastStackTop());
}

} // namespace JSC

#endif // ENABLE(JIT)

