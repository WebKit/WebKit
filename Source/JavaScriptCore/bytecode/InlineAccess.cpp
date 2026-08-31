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
#include "PropertyInlineCache.h"
#include "ScratchRegisterAllocator.h"
#include "Structure.h"

namespace JSC {

void InlineAccess::dumpCacheSizesAndCrash()
{
    GPRReg baseGPR = GPRInfo::regT0;
    GPRReg valueGPR = GPRInfo::regT1;
    GPRReg resultGPR = baseGPR;
    {
        CCallHelpers jit;

        GPRReg scratchGPR = valueGPR;
        jit.patchableBranch8(
            CCallHelpers::NotEqual,
            CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()),
            CCallHelpers::TrustedImm32(StringType));

        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratchGPR);
        auto isRope = jit.branchIfRopeStringImpl(scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, StringImpl::lengthMemoryOffset()), resultGPR);
        auto done = jit.jump();

        isRope.link(&jit);
        jit.load32(CCallHelpers::Address(baseGPR, JSRopeString::offsetOfLength()), resultGPR);

        done.link(&jit);
        jit.boxInt32(resultGPR, resultGPR);

        dataLog("string length size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        GPRReg scratchGPR = valueGPR;
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), valueGPR);
        jit.and32(CCallHelpers::TrustedImm32(IsArray | IndexingShapeMask), valueGPR);
        jit.patchableBranch32(
            CCallHelpers::NotEqual, valueGPR, CCallHelpers::TrustedImm32(IsArray | ContiguousShape));
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), valueGPR);
        jit.load32(CCallHelpers::Address(valueGPR, ArrayStorage::lengthOffset()), valueGPR);
        jit.boxInt32(scratchGPR, resultGPR);

        dataLog("array length size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));
        jit.loadPtr(
            CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()),
            valueGPR);
        GPRReg storageGPR = valueGPR;
        jit.loadValue(
            CCallHelpers::Address(storageGPR, 0x000ab21ca), resultGPR);

        dataLog("out of line offset cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));
        jit.loadValue(
            MacroAssembler::Address(baseGPR, 0x000ab21ca), resultGPR);

        dataLog("inline offset cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));

        jit.storeValue(
            resultGPR, MacroAssembler::Address(baseGPR, 0x000ab21ca));

        dataLog("replace cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    {
        CCallHelpers jit;

        jit.patchableBranch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
            MacroAssembler::TrustedImm32(0x000ab21ca));

        jit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), valueGPR);
        jit.storeValue(
            resultGPR,
            MacroAssembler::Address(baseGPR, 120342));

        dataLog("replace out of line cache size: ", jit.m_assembler.buffer().codeSize(), "\n");
    }

    CRASH();
}


ALWAYS_INLINE static bool linkCodeInline(const char* name, CCallHelpers& jit, RepatchingPropertyInlineCache& propertyCache)
{
    if (jit.m_assembler.buffer().codeSize() <= propertyCache.inlineCodeSize()) {
        bool needsBranchCompaction = true;
        LinkBuffer linkBuffer(jit, propertyCache.startLocation, propertyCache.inlineCodeSize(), LinkBuffer::Profile::InlineCache, JITCompilationMustSucceed, needsBranchCompaction);
        ASSERT(linkBuffer.isValid());
        FINALIZE_CODE(linkBuffer, NoPtrTag, ASCIILiteral::fromLiteralUnsafe(name), "InlineAccessType: '%s'", name);
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
        dataLog("real size: ", jit.m_assembler.buffer().codeSize(), " inline size:", propertyCache.inlineCodeSize(), "\n");
        CRASH();
    }

    return false;
}

bool InlineAccess::generateSelfPropertyAccess(PropertyInlineCache& propertyCache, Structure* structure, PropertyOffset offset)
{
    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(propertyCache);
    if (!repatchingIC)
        return false;

    CCallHelpers jit;

    GPRReg base = propertyCache.baseGPR();
    GPRReg value = propertyCache.valueGPR();

    jit.patchableBranch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(base, JSCell::structureIDOffset()),
        MacroAssembler::TrustedImm32(std::bit_cast<uint32_t>(structure->id()))).linkThunk(repatchingIC->slowPathStartLocation, &jit);
    GPRReg storage;
    if (isInlineOffset(offset))
        storage = base;
    else {
        jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), value);
        storage = value;
    }

    jit.loadValue(
        MacroAssembler::Address(storage, offsetRelativeToBase(offset)), value);

    return linkCodeInline("property access", jit, *repatchingIC);
}

ALWAYS_INLINE static GPRReg getScratchRegister(PropertyInlineCache& propertyCache)
{
    ScratchRegisterAllocator allocator(propertyCache.usedRegisters().toRegisterSet());
    auto registers = propertyCache.registers();
    allocator.lock(registers.baseGPR);
    allocator.lock(registers.valueGPR);
    allocator.lock(registers.extraGPR);
    allocator.lock(registers.extra2GPR);
    allocator.lock(registers.propertyCacheGPR);
    allocator.lock(registers.arrayProfileGPR);
    GPRReg scratch = allocator.allocateScratchGPR();
    if (allocator.didReuseRegisters())
        return InvalidGPRReg;
    return scratch;
}

ALWAYS_INLINE static bool hasFreeRegister(PropertyInlineCache& propertyCache)
{
    return getScratchRegister(propertyCache) != InvalidGPRReg;
}

bool InlineAccess::canGenerateSelfPropertyReplace(PropertyInlineCache& propertyCache, PropertyOffset offset)
{
    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    if (propertyCache.isHandlerIC())
        return false;

    if (isInlineOffset(offset))
        return true;

    return hasFreeRegister(propertyCache);
}

bool InlineAccess::generateSelfPropertyReplace(PropertyInlineCache& propertyCache, Structure* structure, PropertyOffset offset)
{
    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(propertyCache);
    if (!repatchingIC)
        return false;

    ASSERT(canGenerateSelfPropertyReplace(propertyCache, offset));

    CCallHelpers jit;

    GPRReg base = propertyCache.baseGPR();
    GPRReg value = propertyCache.valueGPR();

    jit.patchableBranch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(base, JSCell::structureIDOffset()),
        MacroAssembler::TrustedImm32(std::bit_cast<uint32_t>(structure->id()))).linkThunk(repatchingIC->slowPathStartLocation, &jit);

    GPRReg storage;
    if (isInlineOffset(offset))
        storage = base;
    else {
        storage = getScratchRegister(propertyCache);
        ASSERT(storage != InvalidGPRReg);
        jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), storage);
    }

    jit.storeValue(
        value, MacroAssembler::Address(storage, offsetRelativeToBase(offset)));

    return linkCodeInline("property replace", jit, *repatchingIC);
}

bool InlineAccess::isCacheableArrayLength(PropertyInlineCache& propertyCache, JSArray* array)
{
    ASSERT(array->indexingType() & IsArray);

    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    if (propertyCache.isHandlerIC())
        return propertyCache.preconfiguredCacheType == CacheType::ArrayLength;

    if (!hasFreeRegister(propertyCache))
        return false;

    return !hasAnyArrayStorage(array->indexingType()) && array->indexingType() != ArrayClass;
}

bool InlineAccess::generateArrayLength(PropertyInlineCache& propertyCache, JSArray* array)
{
    ASSERT(isCacheableArrayLength(propertyCache, array));

    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(propertyCache);
    if (!repatchingIC)
        return false;

    CCallHelpers jit;

    GPRReg base = propertyCache.baseGPR();
    GPRReg value = propertyCache.valueGPR();
    GPRReg scratch = getScratchRegister(propertyCache);

    jit.load8(CCallHelpers::Address(base, JSCell::indexingTypeAndMiscOffset()), scratch);
    jit.and32(CCallHelpers::TrustedImm32(IndexingTypeMask), scratch);
    jit.patchableBranch32(
        CCallHelpers::NotEqual, scratch, CCallHelpers::TrustedImm32(array->indexingType())).linkThunk(repatchingIC->slowPathStartLocation, &jit);
    jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), value);
    jit.load32(CCallHelpers::Address(value, ArrayStorage::lengthOffset()), value);
    jit.boxInt32(value, value);

    return linkCodeInline("array length", jit, *repatchingIC);
}

bool InlineAccess::isCacheableStringLength(PropertyInlineCache& propertyCache)
{
    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    if (propertyCache.isHandlerIC())
        return propertyCache.preconfiguredCacheType == CacheType::StringLength;

    return hasFreeRegister(propertyCache);
}

bool InlineAccess::generateStringLength(PropertyInlineCache& propertyCache)
{
    ASSERT(isCacheableStringLength(propertyCache));

    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(propertyCache);
    if (!repatchingIC)
        return false;

    CCallHelpers jit;

    GPRReg base = propertyCache.baseGPR();
    GPRReg value = propertyCache.valueGPR();
    GPRReg scratch = getScratchRegister(propertyCache);

    jit.patchableBranch8(
        CCallHelpers::NotEqual,
        CCallHelpers::Address(base, JSCell::typeInfoTypeOffset()),
        CCallHelpers::TrustedImm32(StringType)).linkThunk(repatchingIC->slowPathStartLocation, &jit);

    jit.loadPtr(CCallHelpers::Address(base, JSString::offsetOfValue()), scratch);
    auto isRope = jit.branchIfRopeStringImpl(scratch);
    jit.load32(CCallHelpers::Address(scratch, StringImpl::lengthMemoryOffset()), value);
    auto done = jit.jump();

    isRope.link(&jit);
    jit.load32(CCallHelpers::Address(base, JSRopeString::offsetOfLength()), value);

    done.link(&jit);
    jit.boxInt32(value, value);

    return linkCodeInline("string length", jit, *repatchingIC);
}


bool InlineAccess::generateSelfInAccess(PropertyInlineCache& propertyCache, Structure* structure)
{
    CCallHelpers jit;

    if (!hasConstantIdentifier(propertyCache.accessType))
        return false;

    auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(propertyCache);
    if (!repatchingIC)
        return false;

    GPRReg base = propertyCache.baseGPR();
    GPRReg value = propertyCache.valueGPR();

    jit.patchableBranch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(base, JSCell::structureIDOffset()),
        MacroAssembler::TrustedImm32(std::bit_cast<uint32_t>(structure->id()))).linkThunk(repatchingIC->slowPathStartLocation, &jit);
    jit.boxBoolean(true, value);

    return linkCodeInline("in access", jit, *repatchingIC);
}

} // namespace JSC

#endif // ENABLE(JIT)
