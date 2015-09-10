/*
 * Copyright (C) 2008, 2012-2015 Apple Inc. All rights reserved.
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
#include "PolymorphicAccess.h"
#include "RegisterSet.h"
#include "SpillRegistersMode.h"
#include "Structure.h"
#include "StructureStubClearingWatchpoint.h"

namespace JSC {

#if ENABLE(JIT)

class PolymorphicAccess;

enum class AccessType : int8_t {
    Get,
    Put,
    In
};

enum class CacheType : int8_t {
    Unset,
    GetByIdSelf,
    PutByIdReplace,
    Stub
};

struct StructureStubInfo {
    StructureStubInfo(AccessType accessType)
        : accessType(accessType)
        , cacheType(CacheType::Unset)
        , seen(false)
        , resetByGC(false)
        , tookSlowPath(false)
        , callSiteIndex(UINT_MAX)
    {
    }

    void initGetByIdSelf(VM& vm, JSCell* owner, Structure* baseObjectStructure, PropertyOffset offset)
    {
        cacheType = CacheType::GetByIdSelf;

        u.byIdSelf.baseObjectStructure.set(vm, owner, baseObjectStructure);
        u.byIdSelf.offset = offset;
    }

    void initPutByIdReplace(VM& vm, JSCell* owner, Structure* baseObjectStructure, PropertyOffset offset)
    {
        cacheType = CacheType::PutByIdReplace;

        u.byIdSelf.baseObjectStructure.set(vm, owner, baseObjectStructure);
        u.byIdSelf.offset = offset;
    }

    void initStub(std::unique_ptr<PolymorphicAccess> stub)
    {
        cacheType = CacheType::Stub;
        u.stub = stub.release();
    }

    MacroAssemblerCodePtr addAccessCase(
        VM&, CodeBlock*, const Identifier&, std::unique_ptr<AccessCase>);

    void reset(CodeBlock*);

    void deref();

    // Check if the stub has weak references that are dead. If it does, then it resets itself,
    // either entirely or just enough to ensure that those dead pointers don't get used anymore.
    void visitWeakReferences(CodeBlock*);
        
    bool seenOnce()
    {
        return seen;
    }

    void setSeen()
    {
        seen = true;
    }
        
    AccessType accessType;
    CacheType cacheType;
    bool seen;
    bool resetByGC : 1;
    bool tookSlowPath : 1;

    CodeOrigin codeOrigin;
    CallSiteIndex callSiteIndex;

    struct {
        unsigned spillMode : 8;
        int8_t baseGPR;
#if USE(JSVALUE32_64)
        int8_t valueTagGPR;
#endif
        int8_t valueGPR;
        RegisterSet usedRegisters;
        int32_t deltaCallToDone;
        int32_t deltaCallToStorageLoad;
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

    union {
        struct {
            WriteBarrierBase<Structure> baseObjectStructure;
            PropertyOffset offset;
        } byIdSelf;
        PolymorphicAccess* stub;
    } u;

    CodeLocationCall callReturnLocation;
};

inline CodeOrigin getStructureStubInfoCodeOrigin(StructureStubInfo& structureStubInfo)
{
    return structureStubInfo.codeOrigin;
}

typedef HashMap<CodeOrigin, StructureStubInfo*, CodeOriginApproximateHash> StubInfoMap;

#else

typedef HashMap<int, void*> StubInfoMap;

#endif // ENABLE(JIT)

} // namespace JSC

#endif // StructureStubInfo_h
