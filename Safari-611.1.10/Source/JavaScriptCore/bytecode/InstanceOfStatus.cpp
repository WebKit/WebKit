/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InstanceOfStatus.h"

#include "ICStatusUtils.h"
#include "InstanceOfAccessCase.h"
#include "JSCellInlines.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"

namespace JSC {

void InstanceOfStatus::appendVariant(const InstanceOfVariant& variant)
{
    appendICStatusVariant(m_variants, variant);
}

InstanceOfStatus InstanceOfStatus::computeFor(
    CodeBlock* codeBlock, ICStatusMap& infoMap, BytecodeIndex bytecodeIndex)
{
    ConcurrentJSLocker locker(codeBlock->m_lock);
    
    InstanceOfStatus result;
#if ENABLE(DFG_JIT)
    result = computeForStubInfo(locker, codeBlock->vm(), infoMap.get(CodeOrigin(bytecodeIndex)).stubInfo);

    if (!result.takesSlowPath()) {
        UnlinkedCodeBlock* unlinkedCodeBlock = codeBlock->unlinkedCodeBlock();
        ConcurrentJSLocker locker(unlinkedCodeBlock->m_lock);
        // We also check for BadType here in case this is "primitive instanceof Foo".
        if (unlinkedCodeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCache))
            || unlinkedCodeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadConstantCache))
            || unlinkedCodeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadType)))
            return TakesSlowPath;
    }
#else
    UNUSED_PARAM(infoMap);
    UNUSED_PARAM(bytecodeIndex);
#endif
    
    return result;
}

#if ENABLE(DFG_JIT)
InstanceOfStatus InstanceOfStatus::computeForStubInfo(const ConcurrentJSLocker&, VM& vm, StructureStubInfo* stubInfo)
{
    // FIXME: We wouldn't have to bail for nonCell if we taught MatchStructure how to handle non
    // cells. If we fixed that then we wouldn't be able to use summary();
    // https://bugs.webkit.org/show_bug.cgi?id=185784
    StubInfoSummary summary = StructureStubInfo::summary(vm, stubInfo);
    if (!isInlineable(summary))
        return InstanceOfStatus(summary);
    
    if (stubInfo->cacheType() != CacheType::Stub)
        return TakesSlowPath; // This is conservative. It could be that we have no information.
    
    PolymorphicAccess* list = stubInfo->u.stub;
    InstanceOfStatus result;
    for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
        const AccessCase& access = list->at(listIndex);
        
        if (access.type() == AccessCase::InstanceOfGeneric)
            return TakesSlowPath;
        
        if (!access.conditionSet().structuresEnsureValidity())
            return TakesSlowPath;
        
        result.appendVariant(InstanceOfVariant(
            access.structure(),
            access.conditionSet(),
            access.as<InstanceOfAccessCase>().prototype(),
            access.type() == AccessCase::InstanceOfHit));
    }
    
    return result;
}
#endif // ENABLE(DFG_JIT)

JSObject* InstanceOfStatus::commonPrototype() const
{
    JSObject* prototype = nullptr;
    for (const InstanceOfVariant& variant : m_variants) {
        if (!prototype) {
            prototype = variant.prototype();
            continue;
        }
        if (prototype != variant.prototype())
            return nullptr;
    }
    return prototype;
}

void InstanceOfStatus::filter(const StructureSet& structureSet)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, structureSet);
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

} // namespace JSC

