/*
 * Copyright (C) 2008, 2012-2016 Apple Inc. All rights reserved.
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

#ifndef StructureStubInfo_h
#define StructureStubInfo_h

#include "CodeOrigin.h"
#include "Instruction.h"
#include "JITStubRoutine.h"
#include "MacroAssembler.h"
#include "ObjectPropertyConditionSet.h"
#include "Opcode.h"
#include "Options.h"
#include "RegisterSet.h"
#include "Structure.h"
#include "StructureStubClearingWatchpoint.h"

namespace JSC {

#if ENABLE(JIT)

class AccessCase;
class AccessGenerationResult;
class PolymorphicAccess;

enum class AccessType : int8_t {
    Get,
    GetPure,
    Put,
    In
};

enum class CacheType : int8_t {
    Unset,
    GetByIdSelf,
    PutByIdReplace,
    Stub
};

class StructureStubInfo {
    WTF_MAKE_NONCOPYABLE(StructureStubInfo);
    WTF_MAKE_FAST_ALLOCATED;
public:
    StructureStubInfo(AccessType);
    ~StructureStubInfo();

    void initGetByIdSelf(CodeBlock*, Structure* baseObjectStructure, PropertyOffset);
    void initPutByIdReplace(CodeBlock*, Structure* baseObjectStructure, PropertyOffset);
    void initStub(CodeBlock*, std::unique_ptr<PolymorphicAccess>);

    AccessGenerationResult addAccessCase(CodeBlock*, const Identifier&, std::unique_ptr<AccessCase>);

    void reset(CodeBlock*);

    void deref();
    void aboutToDie();

    // Check if the stub has weak references that are dead. If it does, then it resets itself,
    // either entirely or just enough to ensure that those dead pointers don't get used anymore.
    void visitWeakReferences(CodeBlock*);
        
    ALWAYS_INLINE bool considerCaching()
    {
        everConsidered = true;
        if (!countdown) {
            // Check if we have been doing repatching too frequently. If so, then we should cool off
            // for a while.
            willRepatch();
            if (repatchCount > Options::repatchCountForCoolDown()) {
                // We've been repatching too much, so don't do it now.
                repatchCount = 0;
                // The amount of time we require for cool-down depends on the number of times we've
                // had to cool down in the past. The relationship is exponential. The max value we
                // allow here is 2^256 - 2, since the slow paths may increment the count to indicate
                // that they'd like to temporarily skip patching just this once.
                countdown = WTF::leftShiftWithSaturation(
                    static_cast<uint8_t>(Options::initialCoolDownCount()),
                    numberOfCoolDowns,
                    static_cast<uint8_t>(std::numeric_limits<uint8_t>::max() - 1));
                willCoolDown();
                return false;
            }
            return true;
        }
        countdown--;
        return false;
    }

    ALWAYS_INLINE void willRepatch()
    {
        WTF::incrementWithSaturation(repatchCount);
    }

    ALWAYS_INLINE void willCoolDown()
    {
        WTF::incrementWithSaturation(numberOfCoolDowns);
    }

    CodeLocationCall callReturnLocation;

    CodeOrigin codeOrigin;
    CallSiteIndex callSiteIndex;

    bool containsPC(void* pc) const;

    union {
        struct {
            WriteBarrierBase<Structure> baseObjectStructure;
            PropertyOffset offset;
        } byIdSelf;
        PolymorphicAccess* stub;
    } u;

    struct {
        int8_t baseGPR;
#if USE(JSVALUE32_64)
        int8_t valueTagGPR;
        int8_t baseTagGPR;
#endif
        int8_t valueGPR;
        RegisterSet usedRegisters;
        int32_t deltaCallToDone;
        int32_t deltaCallToJump;
        int32_t deltaCallToSlowCase;
        int32_t deltaCheckImmToCall;
#if USE(JSVALUE64)
        int32_t deltaCallToLoadOrStore;
#else
        int32_t deltaCallToTagLoadOrStore;
        int32_t deltaCallToPayloadLoadOrStore;
#endif
    } patch;

    AccessType accessType;
    CacheType cacheType;
    uint8_t countdown; // We repatch only when this is zero. If not zero, we decrement.
    uint8_t repatchCount;
    uint8_t numberOfCoolDowns;
    bool resetByGC : 1;
    bool tookSlowPath : 1;
    bool everConsidered : 1;
};

inline CodeOrigin getStructureStubInfoCodeOrigin(StructureStubInfo& structureStubInfo)
{
    return structureStubInfo.codeOrigin;
}

#else

class StructureStubInfo;

#endif // ENABLE(JIT)

typedef HashMap<CodeOrigin, StructureStubInfo*, CodeOriginApproximateHash> StubInfoMap;

} // namespace JSC

#endif // StructureStubInfo_h
