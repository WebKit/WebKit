/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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
#include "GetByIdStatus.h"

#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "ComplexGetStatus.h"
#include "GetterSetterAccessCase.h"
#include "ICStatusUtils.h"
#include "InterpreterInlines.h"
#include "IntrinsicGetterAccessCase.h"
#include "JSCInlines.h"
#include "JSScope.h"
#include "LLIntData.h"
#include "LowLevelInterpreter.h"
#include "ModuleNamespaceAccessCase.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"
#include <wtf/ListDump.h>

namespace JSC {
namespace DOMJIT {
class GetterSetter;
}

bool GetByIdStatus::appendVariant(const GetByIdVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

GetByIdStatus GetByIdStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex, UniquedStringImpl* uid)
{
    VM& vm = *profiledBlock->vm();
    
    auto instruction = profiledBlock->instructions().at(bytecodeIndex);

    StructureID structureID;
    switch (instruction->opcodeID()) {
    case op_get_by_id: {
        auto& metadata = instruction->as<OpGetById>().metadata(profiledBlock);
        // FIXME: We should not just bail if we see a get_by_id_proto_load.
        // https://bugs.webkit.org/show_bug.cgi?id=158039
        if (metadata.m_mode != GetByIdMode::Default)
            return GetByIdStatus(NoInformation, false);
        structureID = metadata.m_modeMetadata.defaultMode.structure;
        break;
    }
    case op_get_by_id_direct:
        structureID = instruction->as<OpGetByIdDirect>().metadata(profiledBlock).m_structure;
        break;
    case op_try_get_by_id: {
        // FIXME: We should not just bail if we see a try_get_by_id.
        // https://bugs.webkit.org/show_bug.cgi?id=158039
        return GetByIdStatus(NoInformation, false);
    }

    default: {
        ASSERT_NOT_REACHED();
        return GetByIdStatus(NoInformation, false);
    }
    }

    if (!structureID)
        return GetByIdStatus(NoInformation, false);

    Structure* structure = vm.heap.structureIDTable().get(structureID);

    if (structure->takesSlowPathInDFGForImpureProperty())
        return GetByIdStatus(NoInformation, false);

    unsigned attributes;
    PropertyOffset offset = structure->getConcurrently(uid, attributes);
    if (!isValidOffset(offset))
        return GetByIdStatus(NoInformation, false);
    if (attributes & PropertyAttribute::CustomAccessorOrValue)
        return GetByIdStatus(NoInformation, false);

    return GetByIdStatus(Simple, false, GetByIdVariant(StructureSet(structure), offset));
}

GetByIdStatus GetByIdStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, unsigned bytecodeIndex, UniquedStringImpl* uid, ExitFlag didExit, CallLinkStatus::ExitSiteData callExitSiteData)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);

    GetByIdStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfoWithoutExitSiteFeedback(
        locker, profiledBlock, map.get(CodeOrigin(bytecodeIndex)).stubInfo, uid,
        callExitSiteData);
    
    if (didExit)
        return result.slowVersion();
#else
    UNUSED_PARAM(map);
    UNUSED_PARAM(didExit);
    UNUSED_PARAM(callExitSiteData);
#endif

    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex, uid);
    
    return result;
}

#if ENABLE(DFG_JIT)
GetByIdStatus GetByIdStatus::computeForStubInfo(const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, CodeOrigin codeOrigin, UniquedStringImpl* uid)
{
    GetByIdStatus result = GetByIdStatus::computeForStubInfoWithoutExitSiteFeedback(
        locker, profiledBlock, stubInfo, uid,
        CallLinkStatus::computeExitSiteData(profiledBlock, codeOrigin.bytecodeIndex));

    if (!result.takesSlowPath() && hasBadCacheExitSite(profiledBlock, codeOrigin.bytecodeIndex))
        return result.slowVersion();
    return result;
}
#endif // ENABLE(DFG_JIT)

#if ENABLE(JIT)
GetByIdStatus::GetByIdStatus(const ModuleNamespaceAccessCase& accessCase)
    : m_state(ModuleNamespace)
    , m_wasSeenInJIT(true)
    , m_moduleNamespaceObject(accessCase.moduleNamespaceObject())
    , m_moduleEnvironment(accessCase.moduleEnvironment())
    , m_scopeOffset(accessCase.scopeOffset())
{
}

GetByIdStatus GetByIdStatus::computeForStubInfoWithoutExitSiteFeedback(
    const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, UniquedStringImpl* uid,
    CallLinkStatus::ExitSiteData callExitSiteData)
{
    StubInfoSummary summary = StructureStubInfo::summary(stubInfo);
    if (!isInlineable(summary))
        return GetByIdStatus(summary);
    
    // Finally figure out if we can derive an access strategy.
    GetByIdStatus result;
    result.m_state = Simple;
    result.m_wasSeenInJIT = true; // This is interesting for bytecode dumping only.
    switch (stubInfo->cacheType) {
    case CacheType::Unset:
        return GetByIdStatus(NoInformation);
        
    case CacheType::GetByIdSelf: {
        Structure* structure = stubInfo->u.byIdSelf.baseObjectStructure.get();
        if (structure->takesSlowPathInDFGForImpureProperty())
            return GetByIdStatus(JSC::slowVersion(summary));
        unsigned attributes;
        GetByIdVariant variant;
        variant.m_offset = structure->getConcurrently(uid, attributes);
        if (!isValidOffset(variant.m_offset))
            return GetByIdStatus(JSC::slowVersion(summary));
        if (attributes & PropertyAttribute::CustomAccessorOrValue)
            return GetByIdStatus(JSC::slowVersion(summary));
        
        variant.m_structureSet.add(structure);
        bool didAppend = result.appendVariant(variant);
        ASSERT_UNUSED(didAppend, didAppend);
        return result;
    }
        
    case CacheType::Stub: {
        PolymorphicAccess* list = stubInfo->u.stub;
        if (list->size() == 1) {
            const AccessCase& access = list->at(0);
            switch (access.type()) {
            case AccessCase::ModuleNamespaceLoad:
                return GetByIdStatus(access.as<ModuleNamespaceAccessCase>());
            default:
                break;
            }
        }

        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);
            if (access.viaProxy())
                return GetByIdStatus(JSC::slowVersion(summary));

            if (access.usesPolyProto())
                return GetByIdStatus(JSC::slowVersion(summary));
            
            Structure* structure = access.structure();
            if (!structure) {
                // The null structure cases arise due to array.length and string.length. We have no way
                // of creating a GetByIdVariant for those, and we don't really have to since the DFG
                // handles those cases in FixupPhase using value profiling. That's a bit awkward - we
                // shouldn't have to use value profiling to discover something that the AccessCase
                // could have told us. But, it works well enough. So, our only concern here is to not
                // crash on null structure.
                return GetByIdStatus(JSC::slowVersion(summary));
            }
            
            ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(
                structure, access.conditionSet(), uid);
             
            switch (complexGetStatus.kind()) {
            case ComplexGetStatus::ShouldSkip:
                continue;
                 
            case ComplexGetStatus::TakesSlowPath:
                return GetByIdStatus(JSC::slowVersion(summary));
                 
            case ComplexGetStatus::Inlineable: {
                std::unique_ptr<CallLinkStatus> callLinkStatus;
                JSFunction* intrinsicFunction = nullptr;
                FunctionPtr<OperationPtrTag> customAccessorGetter;
                Optional<DOMAttributeAnnotation> domAttribute;

                switch (access.type()) {
                case AccessCase::Load:
                case AccessCase::GetGetter:
                case AccessCase::Miss: {
                    break;
                }
                case AccessCase::IntrinsicGetter: {
                    intrinsicFunction = access.as<IntrinsicGetterAccessCase>().intrinsicFunction();
                    break;
                }
                case AccessCase::Getter: {
                    callLinkStatus = std::make_unique<CallLinkStatus>();
                    if (CallLinkInfo* callLinkInfo = access.as<GetterSetterAccessCase>().callLinkInfo()) {
                        *callLinkStatus = CallLinkStatus::computeFor(
                            locker, profiledBlock, *callLinkInfo, callExitSiteData);
                    }
                    break;
                }
                case AccessCase::CustomAccessorGetter: {
                    customAccessorGetter = access.as<GetterSetterAccessCase>().customAccessor();
                    domAttribute = access.as<GetterSetterAccessCase>().domAttribute();
                    if (!domAttribute)
                        return GetByIdStatus(JSC::slowVersion(summary));
                    result.m_state = Custom;
                    break;
                }
                default: {
                    // FIXME: It would be totally sweet to support more of these at some point in the
                    // future. https://bugs.webkit.org/show_bug.cgi?id=133052
                    return GetByIdStatus(JSC::slowVersion(summary));
                } }

                ASSERT((AccessCase::Miss == access.type()) == (access.offset() == invalidOffset));
                GetByIdVariant variant(
                    StructureSet(structure), complexGetStatus.offset(),
                    complexGetStatus.conditionSet(), WTFMove(callLinkStatus),
                    intrinsicFunction,
                    customAccessorGetter,
                    domAttribute);

                if (!result.appendVariant(variant))
                    return GetByIdStatus(JSC::slowVersion(summary));

                if (domAttribute) {
                    // Give up when cutom accesses are not merged into one.
                    if (result.numVariants() != 1)
                        return GetByIdStatus(JSC::slowVersion(summary));
                } else {
                    // Give up when custom access and simple access are mixed.
                    if (result.m_state == Custom)
                        return GetByIdStatus(JSC::slowVersion(summary));
                }
                break;
            } }
        }
        
        return result;
    }
        
    default:
        return GetByIdStatus(JSC::slowVersion(summary));
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return GetByIdStatus();
}

GetByIdStatus GetByIdStatus::computeFor(
    CodeBlock* profiledBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& icContextStack, CodeOrigin codeOrigin, UniquedStringImpl* uid)
{
    CallLinkStatus::ExitSiteData callExitSiteData =
        CallLinkStatus::computeExitSiteData(profiledBlock, codeOrigin.bytecodeIndex);
    ExitFlag didExit = hasBadCacheExitSite(profiledBlock, codeOrigin.bytecodeIndex);
    
    for (ICStatusContext* context : icContextStack) {
        ICStatus status = context->get(codeOrigin);
        
        auto bless = [&] (const GetByIdStatus& result) -> GetByIdStatus {
            if (!context->isInlined(codeOrigin)) {
                // Merge with baseline result, which also happens to contain exit data for both
                // inlined and not-inlined.
                GetByIdStatus baselineResult = computeFor(
                    profiledBlock, baselineMap, codeOrigin.bytecodeIndex, uid, didExit,
                    callExitSiteData);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };
        
        if (status.stubInfo) {
            GetByIdStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfoWithoutExitSiteFeedback(
                    locker, context->optimizedCodeBlock, status.stubInfo, uid, callExitSiteData);
            }
            if (result.isSet())
                return bless(result);
        }
        
        if (status.getStatus)
            return bless(*status.getStatus);
    }
    
    return computeFor(profiledBlock, baselineMap, codeOrigin.bytecodeIndex, uid, didExit, callExitSiteData);
}

GetByIdStatus GetByIdStatus::computeFor(const StructureSet& set, UniquedStringImpl* uid)
{
    // For now we only handle the super simple self access case. We could handle the
    // prototype case in the future.
    //
    // Note that this code is also used for GetByIdDirect since this function only looks
    // into direct properties. When supporting prototype chains, we should split this for
    // GetById and GetByIdDirect.
    
    if (set.isEmpty())
        return GetByIdStatus();

    if (parseIndex(*uid))
        return GetByIdStatus(TakesSlowPath);
    
    GetByIdStatus result;
    result.m_state = Simple;
    result.m_wasSeenInJIT = false;
    for (unsigned i = 0; i < set.size(); ++i) {
        Structure* structure = set[i];
        if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
            return GetByIdStatus(TakesSlowPath);
        
        if (!structure->propertyAccessesAreCacheable())
            return GetByIdStatus(TakesSlowPath);
        
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(uid, attributes);
        if (!isValidOffset(offset))
            return GetByIdStatus(TakesSlowPath); // It's probably a prototype lookup. Give up on life for now, even though we could totally be way smarter about it.
        if (attributes & PropertyAttribute::Accessor)
            return GetByIdStatus(MakesCalls); // We could be smarter here, like strength-reducing this to a Call.
        if (attributes & PropertyAttribute::CustomAccessorOrValue)
            return GetByIdStatus(TakesSlowPath);
        
        if (!result.appendVariant(GetByIdVariant(structure, offset)))
            return GetByIdStatus(TakesSlowPath);
    }
    
    return result;
}
#endif // ENABLE(JIT)

bool GetByIdStatus::makesCalls() const
{
    switch (m_state) {
    case NoInformation:
    case TakesSlowPath:
    case Custom:
    case ModuleNamespace:
        return false;
    case Simple:
        for (unsigned i = m_variants.size(); i--;) {
            if (m_variants[i].callLinkStatus())
                return true;
        }
        return false;
    case MakesCalls:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();

    return false;
}

GetByIdStatus GetByIdStatus::slowVersion() const
{
    return GetByIdStatus(makesCalls() ? MakesCalls : TakesSlowPath, wasSeenInJIT());
}

void GetByIdStatus::merge(const GetByIdStatus& other)
{
    if (other.m_state == NoInformation)
        return;
    
    auto mergeSlow = [&] () {
        *this = GetByIdStatus((makesCalls() || other.makesCalls()) ? MakesCalls : TakesSlowPath);
    };
    
    switch (m_state) {
    case NoInformation:
        *this = other;
        return;
        
    case Simple:
    case Custom:
        if (m_state != other.m_state)
            return mergeSlow();
        
        for (const GetByIdVariant& otherVariant : other.m_variants) {
            if (!appendVariant(otherVariant))
                return mergeSlow();
        }
        return;
        
    case ModuleNamespace:
        if (other.m_state != ModuleNamespace)
            return mergeSlow();
        
        if (m_moduleNamespaceObject != other.m_moduleNamespaceObject)
            return mergeSlow();
        
        if (m_moduleEnvironment != other.m_moduleEnvironment)
            return mergeSlow();
        
        if (m_scopeOffset != other.m_scopeOffset)
            return mergeSlow();
        
        return;
        
    case TakesSlowPath:
    case MakesCalls:
        return mergeSlow();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void GetByIdStatus::filter(const StructureSet& set)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, set);
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

void GetByIdStatus::markIfCheap(SlotVisitor& visitor)
{
    for (GetByIdVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

bool GetByIdStatus::finalize()
{
    for (GetByIdVariant& variant : m_variants) {
        if (!variant.finalize())
            return false;
    }
    if (m_moduleNamespaceObject && !Heap::isMarked(m_moduleNamespaceObject))
        return false;
    if (m_moduleEnvironment && !Heap::isMarked(m_moduleEnvironment))
        return false;
    return true;
}

void GetByIdStatus::dump(PrintStream& out) const
{
    out.print("(");
    switch (m_state) {
    case NoInformation:
        out.print("NoInformation");
        break;
    case Simple:
        out.print("Simple");
        break;
    case Custom:
        out.print("Custom");
        break;
    case ModuleNamespace:
        out.print("ModuleNamespace");
        break;
    case TakesSlowPath:
        out.print("TakesSlowPath");
        break;
    case MakesCalls:
        out.print("MakesCalls");
        break;
    }
    out.print(", ", listDump(m_variants), ", seenInJIT = ", m_wasSeenInJIT, ")");
}

} // namespace JSC

