/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "InlineAccess.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "JSArray.h"
#include "JSCellInlines.h"
#include "LinkBuffer.h"
#include "ScratchRegisterAllocator.h"
#include "Structure.h"
#include "StructureStubInfo.h"

namespace JSC {

void InlineAccess::dumpCacheSizesAndCrash()
{
    GPRReg base = GPRInfo::regT0;
    GPRReg value = GPRInfo::regT1;
#if USE(JSVALUE32_64)
    JSValueRegs regs(base, value);
#else
    JSValueRegs regs(base);
#endif
    {
        CCallHelpers jit;

        GPRReg scratchGPR = value;
        jit.patchableBranch8(
            CCallHelpers::NotEqual,
            CCallHelpers::Address(base, JSCell::typeInfoTypeOffset()),
            CCallHelpers::TrustedImm32(StringType));

        jit.loadPtr(CCallHelpers::Address(base, JSString::offsetOfValue()), scratchGPR);
        auto isRope = jit.branchIfRopeStringImpl(scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, StringImpl::lengthMemoryOffset()), regs.payloadGPR());
        auto done = jit.jump();

        isRope.link(&jit);
        jit.load32(CCallHelpers::Address(base, JSRopeString::offsetOfLength()), regs.payloadGPR());

        done.link(&jit);
        jit.boxInt32(regs.payloadGPR(), regs);

        dataLog("string length size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        GPRReg scratchGPR = value;
        jit.load8(CCallHelpers::Address(base, JSCell::indexingTypeAndMiscOffset()), value);
        jit.and32(CCallHelpers::TrustedImm32(IsArray | IndexingShapeMask), value);
        jit.patchableBranch32(
            CCallHelpers::NotEqual, value, CCallHelpers::TrustedImm32(IsArray | ContiguousShape));
        jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), value);
        jit.load32(CCallHelpers::Address(value, ArrayStorage::lengthOffset()), value);
        jit.boxInt32(scratchGPR, regs);

        dataLog("array length size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(base, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));
        jit.loadPtr(
            CCallHelpers::Address(base, JSObject::butterflyOffset()),
            value);
        GPRReg storageGPR = value;
        jit.loadValue(
            CCallHelpers::Address(storageGPR, 0x000ab21ca), regs);

        dataLog("out of line offset cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(base, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));
        jit.loadValue(
            MacroAssembler::Address(base, 0x000ab21ca), regs);

        dataLog("inline offset cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(base, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));

        jit.storeValue(
            regs, MacroAssembler::Address(base, 0x000ab21ca));

        dataLog("replace cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(base, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));

        jit.loadPtr(MacroAssembler::Address(base, JSObject::butterflyOffset()), value);
        jit.storeValue(
            regs,
            MacroAssembler::Address(base, 120342));

        dataLog("replace out of line cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    CRASH();
}


template <typename Function>
ALWAYS_INLINE static bool linkCodeInline(const char* name, CCallHelpers& jit, StructureStubInfo& stubInfo, const Function& function)
{
    if (jit.m_assembler.buffer().codeSize() <= stubInfo.inlineCodeSize()) {
        bool needsBranchCompaction = true;
        LinkBuffer linkBuffer(jit, stubInfo.startLocation, stubInfo.inlineCodeSize(), LinkBuffer::Profile::InlineCache, JITCompilationMustSucceed, needsBranchCompaction);
        ASSERT(linkBuffer.isValid());
        function(linkBuffer);
        FINALIZE_CODE(linkBuffer, NoPtrTag, "InlineAccessType: '%s'", name);
        return true;
    }

    // This is helpful when determining the size for inline ICs on various
    // platforms. You want to choose a size that usually succeeds, but sometimes
    // there may be variability in the length of the code we generate just because
    // of randomness. It's helpful to flip this on when running tests or browsing
    // the web just to see how often it fails. You don't want an IC size that always fails.
    constexpr bool failIfCantInline = false;
    if (failIfCantInline) {
        dataLog("Failure for: ", name, "\n");
        dataLog("real size: ", jit.m_assembler.buffer().codeSize(), " inline size:", stubInfo.inlineCodeSize(), "\n");
        CRASH();
    }

    return false;
}

bool InlineAccess::generateSelfPropertyAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure, PropertyOffset offset)
{
    if (!stubInfo.hasConstantIdentifier)
        return false;

    if (codeBlock->useDataIC()) {
        // These dynamic slots get filled in by StructureStubInfo. Nothing else to do.
        return true;
    }

    CCallHelpers jit;
    
    GPRReg base = stubInfo.m_baseGPR;
    JSValueRegs value = stubInfo.valueRegs();

    auto branchToSlowPath = jit.patchableBranch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(base, JSCell::structureIDOffset()),
        MacroAssembler::TrustedImm32(bitwise_cast<uint32_t>(structure->id())));
    GPRReg storage;
    if (isInlineOffset(offset))
        storage = base;
    else {
        jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), value.payloadGPR());
        storage = value.payloadGPR();
    }
    
    jit.loadValue(
        MacroAssembler::Address(storage, offsetRelativeToBase(offset)), value);

    bool linkedCodeInline = linkCodeInline("property access", jit, stubInfo, [&] (LinkBuffer& linkBuffer) {
        linkBuffer.link(branchToSlowPath, stubInfo.slowPathStartLocation);
    });
    return linkedCodeInline;
}

ALWAYS_INLINE static GPRReg getScratchRegister(StructureStubInfo& stubInfo)
{
    ScratchRegisterAllocator allocator(stubInfo.usedRegisters.toRegisterSet());
    allocator.lock(stubInfo.m_baseGPR);
    allocator.lock(stubInfo.m_valueGPR);
    allocator.lock(stubInfo.m_extraGPR);
    allocator.lock(stubInfo.m_extra2GPR);
#if USE(JSVALUE32_64)
    allocator.lock(stubInfo.m_baseTagGPR);
    allocator.lock(stubInfo.m_valueTagGPR);
    allocator.lock(stubInfo.m_extraTagGPR);
    allocator.lock(stubInfo.m_extra2TagGPR);
#endif
    allocator.lock(stubInfo.m_stubInfoGPR);
    allocator.lock(stubInfo.m_arrayProfileGPR);
    GPRReg scratch = allocator.allocateScratchGPR();
    if (allocator.didReuseRegisters())
        return InvalidGPRReg;
    return scratch;
}

ALWAYS_INLINE static bool hasFreeRegister(StructureStubInfo& stubInfo)
{
    return getScratchRegister(stubInfo) != InvalidGPRReg;
}

bool InlineAccess::canGenerateSelfPropertyReplace(CodeBlock* codeBlock, StructureStubInfo& stubInfo, PropertyOffset offset)
{
    if (!stubInfo.hasConstantIdentifier)
        return false;

    if (codeBlock->useDataIC())
        return true;

    if (isInlineOffset(offset))
        return true;

    return hasFreeRegister(stubInfo);
}

bool InlineAccess::generateSelfPropertyReplace(CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure, PropertyOffset offset)
{
    if (!stubInfo.hasConstantIdentifier)
        return false;

    ASSERT(canGenerateSelfPropertyReplace(codeBlock, stubInfo, offset));

    if (codeBlock->useDataIC()) {
        // These dynamic slots get filled in by StructureStubInfo. Nothing else to do.
        return true;
    }

    CCallHelpers jit;

    GPRReg base = stubInfo.m_baseGPR;
    JSValueRegs value = stubInfo.valueRegs();

    auto branchToSlowPath = jit.patchableBranch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(base, JSCell::structureIDOffset()),
        MacroAssembler::TrustedImm32(bitwise_cast<uint32_t>(structure->id())));

    GPRReg storage;
    if (isInlineOffset(offset))
        storage = base;
    else {
        storage = getScratchRegister(stubInfo);
        ASSERT(storage != InvalidGPRReg);
        jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), storage);
    }

    jit.storeValue(
        value, MacroAssembler::Address(storage, offsetRelativeToBase(offset)));

    bool linkedCodeInline = linkCodeInline("property replace", jit, stubInfo, [&] (LinkBuffer& linkBuffer) {
        linkBuffer.link(branchToSlowPath, stubInfo.slowPathStartLocation);
    });
    return linkedCodeInline;
}

bool InlineAccess::isCacheableArrayLength(CodeBlock* codeBlock, StructureStubInfo& stubInfo, JSArray* array)
{
    ASSERT(array->indexingType() & IsArray);

    if (!stubInfo.hasConstantIdentifier)
        return false;

    if (codeBlock->useDataIC())
        return false;

    if (!hasFreeRegister(stubInfo))
        return false;

    return !hasAnyArrayStorage(array->indexingType()) && array->indexingType() != ArrayClass;
}

bool InlineAccess::generateArrayLength(CodeBlock* codeBlock, StructureStubInfo& stubInfo, JSArray* array)
{
    ASSERT_UNUSED(codeBlock, !codeBlock->useDataIC());
    ASSERT_UNUSED(codeBlock, isCacheableArrayLength(codeBlock, stubInfo, array));

    if (!stubInfo.hasConstantIdentifier)
        return false;

    CCallHelpers jit;

    GPRReg base = stubInfo.m_baseGPR;
    JSValueRegs value = stubInfo.valueRegs();
    GPRReg scratch = getScratchRegister(stubInfo);

    jit.load8(CCallHelpers::Address(base, JSCell::indexingTypeAndMiscOffset()), scratch);
    jit.and32(CCallHelpers::TrustedImm32(IndexingTypeMask), scratch);
    auto branchToSlowPath = jit.patchableBranch32(
        CCallHelpers::NotEqual, scratch, CCallHelpers::TrustedImm32(array->indexingType()));
    jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), value.payloadGPR());
    jit.load32(CCallHelpers::Address(value.payloadGPR(), ArrayStorage::lengthOffset()), value.payloadGPR());
    jit.boxInt32(value.payloadGPR(), value);

    bool linkedCodeInline = linkCodeInline("array length", jit, stubInfo, [&] (LinkBuffer& linkBuffer) {
        linkBuffer.link(branchToSlowPath, stubInfo.slowPathStartLocation);
    });
    return linkedCodeInline;
}

bool InlineAccess::isCacheableStringLength(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    if (!stubInfo.hasConstantIdentifier)
        return false;

    if (codeBlock->useDataIC())
        return false;

    return hasFreeRegister(stubInfo);
}

bool InlineAccess::generateStringLength(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    ASSERT_UNUSED(codeBlock, !codeBlock->useDataIC());
    ASSERT_UNUSED(codeBlock, isCacheableStringLength(codeBlock, stubInfo));

    if (!stubInfo.hasConstantIdentifier)
        return false;

    CCallHelpers jit;

    GPRReg base = stubInfo.m_baseGPR;
    JSValueRegs value = stubInfo.valueRegs();
    GPRReg scratch = getScratchRegister(stubInfo);

    auto branchToSlowPath = jit.patchableBranch8(
        CCallHelpers::NotEqual,
        CCallHelpers::Address(base, JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(StringType));

    jit.loadPtr(CCallHelpers::Address(base, JSString::offsetOfValue()), scratch);
    auto isRope = jit.branchIfRopeStringImpl(scratch);
    jit.load32(CCallHelpers::Address(scratch, StringImpl::lengthMemoryOffset()), value.payloadGPR());
    auto done = jit.jump();

    isRope.link(&jit);
    jit.load32(CCallHelpers::Address(base, JSRopeString::offsetOfLength()), value.payloadGPR());

    done.link(&jit);
    jit.boxInt32(value.payloadGPR(), value);

    bool linkedCodeInline = linkCodeInline("string length", jit, stubInfo, [&] (LinkBuffer& linkBuffer) {
        linkBuffer.link(branchToSlowPath, stubInfo.slowPathStartLocation);
    });
    return linkedCodeInline;
}


bool InlineAccess::generateSelfInAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure)
{
    CCallHelpers jit;

    if (!stubInfo.hasConstantIdentifier)
        return false;

    if (codeBlock->useDataIC()) {
        // These dynamic slots get filled in by StructureStubInfo. Nothing else to do.
        return true;
    }

    GPRReg base = stubInfo.m_baseGPR;
    JSValueRegs value = stubInfo.valueRegs();

    auto branchToSlowPath = jit.patchableBranch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(base, JSCell::structureIDOffset()),
        MacroAssembler::TrustedImm32(bitwise_cast<uint32_t>(structure->id())));
    jit.boxBoolean(true, value);

    bool linkedCodeInline = linkCodeInline("in access", jit, stubInfo, [&] (LinkBuffer& linkBuffer) {
        linkBuffer.link(branchToSlowPath, stubInfo.slowPathStartLocation);
    });
    return linkedCodeInline;
}

void InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo, CodeLocationLabel<JITStubRoutinePtrTag> target)
{
    if (codeBlock->useDataIC()) {
        stubInfo.m_codePtr = target;
        return;
    }

    CCallHelpers::emitJITCodeOver(stubInfo.startLocation.retagged<JSInternalPtrTag>(), scopedLambda<void(CCallHelpers&)>([&](CCallHelpers& jit) {
        // We don't need a nop sled here because nobody should be jumping into the middle of an IC.
        auto jump = jit.jump();
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(jump, target);
        });
    }), "InlineAccess: linking constant jump");
}

void InlineAccess::rewireStubAsJumpInAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo, CodeLocationLabel<JITStubRoutinePtrTag> target)
{
    if (codeBlock->useDataIC()) {
        stubInfo.m_codePtr = target;
        stubInfo.m_inlineAccessBaseStructureID.clear(); // Clear out the inline access code.
        return;
    }

    CCallHelpers::emitJITCodeOver(stubInfo.startLocation.retagged<JSInternalPtrTag>(), scopedLambda<void(CCallHelpers&)>([&](CCallHelpers& jit) {
        // We don't need a nop sled here because nobody should be jumping into the middle of an IC.
        auto jump = jit.jump();
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(jump, target);
        });
    }), "InlineAccess: linking constant jump");
}

void InlineAccess::resetStubAsJumpInAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    if (codeBlock->useDataIC()) {
        stubInfo.m_codePtr = stubInfo.slowPathStartLocation;
        stubInfo.m_inlineAccessBaseStructureID.clear(); // Clear out the inline access code.
        return;
    }

    CCallHelpers::emitJITCodeOver(stubInfo.startLocation.retagged<JSInternalPtrTag>(), scopedLambda<void(CCallHelpers&)>([&](CCallHelpers& jit) {
        // We don't need a nop sled here because nobody should be jumping into the middle of an IC.
        auto jump = jit.jump();
        auto slowPathStartLocation = stubInfo.slowPathStartLocation;
        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
            linkBuffer.link(jump, slowPathStartLocation);
        });
    }), "InlineAccess: linking constant jump");
}

void InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, stubInfo.slowPathStartLocation);
}

} // namespace JSC

#endif // ENABLE(JIT)
