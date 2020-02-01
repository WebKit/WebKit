/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "GetByStatus.h"

#include "BytecodeStructs.h"
#include "CacheableIdentifierInlines.h"
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

bool GetByStatus::appendVariant(const GetByIdVariant& variant)
{
    return appendICStatusVariant(m_variants, variant);
}

GetByStatus GetByStatus::computeFromLLInt(CodeBlock* profiledBlock, BytecodeIndex bytecodeIndex)
{
    VM& vm = profiledBlock->vm();
    
    auto instruction = profiledBlock->instructions().at(bytecodeIndex.offset());

    StructureID structureID;
    const Identifier* identifier = nullptr;
    switch (instruction->opcodeID()) {
    case op_get_by_id: {
        auto& metadata = instruction->as<OpGetById>().metadata(profiledBlock);
        // FIXME: We should not just bail if we see a get_by_id_proto_load.
        // https://bugs.webkit.org/show_bug.cgi?id=158039
        if (metadata.m_modeMetadata.mode != GetByIdMode::Default)
            return GetByStatus(NoInformation, false);
        structureID = metadata.m_modeMetadata.defaultMode.structureID;

        identifier = &(profiledBlock->identifier(instruction->as<OpGetById>().m_property));
        break;
    }
    case op_get_by_id_direct:
        structureID = instruction->as<OpGetByIdDirect>().metadata(profiledBlock).m_structureID;
        identifier = &(profiledBlock->identifier(instruction->as<OpGetByIdDirect>().m_property));
        break;
    case op_try_get_by_id: {
        // FIXME: We should not just bail if we see a try_get_by_id.
        // https://bugs.webkit.org/show_bug.cgi?id=158039
        return GetByStatus(NoInformation, false);
    }

    case op_get_by_val:
        return GetByStatus(NoInformation, false);

    default: {
        ASSERT_NOT_REACHED();
        return GetByStatus(NoInformation, false);
    }
    }

    if (!structureID)
        return GetByStatus(NoInformation, false);

    Structure* structure = vm.heap.structureIDTable().get(structureID);

    if (structure->takesSlowPathInDFGForImpureProperty())
        return GetByStatus(NoInformation, false);

    unsigned attributes;
    PropertyOffset offset = structure->getConcurrently(identifier->impl(), attributes);
    if (!isValidOffset(offset))
        return GetByStatus(NoInformation, false);
    if (attributes & PropertyAttribute::CustomAccessorOrValue)
        return GetByStatus(NoInformation, false);

    GetByStatus result(Simple, false);
    result.appendVariant(GetByIdVariant(nullptr, StructureSet(structure), offset));
    return result;
}

GetByStatus GetByStatus::computeFor(CodeBlock* profiledBlock, ICStatusMap& map, BytecodeIndex bytecodeIndex, ExitFlag didExit, CallLinkStatus::ExitSiteData callExitSiteData)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);

    GetByStatus result;

#if ENABLE(DFG_JIT)
    result = computeForStubInfoWithoutExitSiteFeedback(
        locker, profiledBlock, map.get(CodeOrigin(bytecodeIndex)).stubInfo, callExitSiteData);
    
    if (didExit)
        return result.slowVersion();
#else
    UNUSED_PARAM(map);
    UNUSED_PARAM(didExit);
    UNUSED_PARAM(callExitSiteData);
#endif

    if (!result)
        return computeFromLLInt(profiledBlock, bytecodeIndex);
    
    return result;
}

#if ENABLE(JIT)
GetByStatus::GetByStatus(StubInfoSummary summary, StructureStubInfo& stubInfo)
    : m_wasSeenInJIT(true)
{
    switch (summary) {
    case StubInfoSummary::NoInformation:
        m_state = NoInformation;
        return;
    case StubInfoSummary::Simple:
    case StubInfoSummary::MakesCalls:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    case StubInfoSummary::TakesSlowPath:
        m_state = stubInfo.tookSlowPath ? ObservedTakesSlowPath : LikelyTakesSlowPath;
        return;
    case StubInfoSummary::TakesSlowPathAndMakesCalls:
        m_state = stubInfo.tookSlowPath ? ObservedSlowPathAndMakesCalls : MakesCalls;
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

GetByStatus::GetByStatus(const ModuleNamespaceAccessCase& accessCase)
    : m_moduleNamespaceData(Box<ModuleNamespaceData>::create(ModuleNamespaceData { accessCase.moduleNamespaceObject(), accessCase.moduleEnvironment(), accessCase.scopeOffset(), accessCase.identifier() }))
    , m_state(ModuleNamespace)
    , m_wasSeenInJIT(true)
{
}

GetByStatus GetByStatus::computeForStubInfoWithoutExitSiteFeedback(
    const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, StructureStubInfo* stubInfo, CallLinkStatus::ExitSiteData callExitSiteData)
{
    StubInfoSummary summary = StructureStubInfo::summary(profiledBlock->vm(), stubInfo);
    if (!isInlineable(summary))
        return GetByStatus(summary, *stubInfo);
    
    // Finally figure out if we can derive an access strategy.
    GetByStatus result;
    result.m_state = Simple;
    result.m_wasSeenInJIT = true; // This is interesting for bytecode dumping only.
    switch (stubInfo->cacheType()) {
    case CacheType::Unset:
        return GetByStatus(NoInformation);
        
    case CacheType::GetByIdSelf: {
        Structure* structure = stubInfo->u.byIdSelf.baseObjectStructure.get();
        if (structure->takesSlowPathInDFGForImpureProperty())
            return GetByStatus(JSC::slowVersion(summary), *stubInfo);
        CacheableIdentifier identifier = stubInfo->getByIdSelfIdentifier();
        UniquedStringImpl* uid = identifier.uid();
        RELEASE_ASSERT(uid);
        GetByIdVariant variant(WTFMove(identifier));
        unsigned attributes;
        variant.m_offset = structure->getConcurrently(uid, attributes);
        if (!isValidOffset(variant.m_offset))
            return GetByStatus(JSC::slowVersion(summary), *stubInfo);
        if (attributes & PropertyAttribute::CustomAccessorOrValue)
            return GetByStatus(JSC::slowVersion(summary), *stubInfo);
        
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
                return GetByStatus(access.as<ModuleNamespaceAccessCase>());
            default:
                break;
            }
        }

        for (unsigned listIndex = 0; listIndex < list->size(); ++listIndex) {
            const AccessCase& access = list->at(listIndex);
            if (access.viaProxy())
                return GetByStatus(JSC::slowVersion(summary), *stubInfo);

            if (access.usesPolyProto())
                return GetByStatus(JSC::slowVersion(summary), *stubInfo);

            if (!access.requiresIdentifierNameMatch()) {
                // FIXME: We could use this for indexed loads in the future. This is pretty solid profiling
                // information, and probably better than ArrayProfile when it's available.
                // https://bugs.webkit.org/show_bug.cgi?id=204215
                return GetByStatus(JSC::slowVersion(summary), *stubInfo);
            }
            
            Structure* structure = access.structure();
            if (!structure) {
                // The null structure cases arise due to array.length and string.length. We have no way
                // of creating a GetByIdVariant for those, and we don't really have to since the DFG
                // handles those cases in FixupPhase using value profiling. That's a bit awkward - we
                // shouldn't have to use value profiling to discover something that the AccessCase
                // could have told us. But, it works well enough. So, our only concern here is to not
                // crash on null structure.
                return GetByStatus(JSC::slowVersion(summary), *stubInfo);
            }
            
            ComplexGetStatus complexGetStatus = ComplexGetStatus::computeFor(
                structure, access.conditionSet(), access.uid());
             
            switch (complexGetStatus.kind()) {
            case ComplexGetStatus::ShouldSkip:
                continue;
                 
            case ComplexGetStatus::TakesSlowPath:
                return GetByStatus(JSC::slowVersion(summary), *stubInfo);
                 
            case ComplexGetStatus::Inlineable: {
                std::unique_ptr<CallLinkStatus> callLinkStatus;
                JSFunction* intrinsicFunction = nullptr;
                FunctionPtr<OperationPtrTag> customAccessorGetter;
                std::unique_ptr<DOMAttributeAnnotation> domAttribute;
                bool haveDOMAttribute = false;

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
                    callLinkStatus = makeUnique<CallLinkStatus>();
                    if (CallLinkInfo* callLinkInfo = access.as<GetterSetterAccessCase>().callLinkInfo()) {
                        *callLinkStatus = CallLinkStatus::computeFor(
                            locker, profiledBlock, *callLinkInfo, callExitSiteData);
                    }
                    break;
                }
                case AccessCase::CustomAccessorGetter: {
                    customAccessorGetter = access.as<GetterSetterAccessCase>().customAccessor();
                    if (!access.as<GetterSetterAccessCase>().domAttribute())
                        return GetByStatus(JSC::slowVersion(summary), *stubInfo);
                    domAttribute = WTF::makeUnique<DOMAttributeAnnotation>(*access.as<GetterSetterAccessCase>().domAttribute());
                    haveDOMAttribute = true;
                    result.m_state = Custom;
                    break;
                }
                default: {
                    // FIXME: It would be totally sweet to support more of these at some point in the
                    // future. https://bugs.webkit.org/show_bug.cgi?id=133052
                    return GetByStatus(JSC::slowVersion(summary), *stubInfo);
                } }

                ASSERT((AccessCase::Miss == access.type() || access.isCustom()) == (access.offset() == invalidOffset));
                GetByIdVariant variant(access.identifier(), StructureSet(structure), complexGetStatus.offset(),
                    complexGetStatus.conditionSet(), WTFMove(callLinkStatus),
                    intrinsicFunction,
                    customAccessorGetter,
                    WTFMove(domAttribute));

                if (!result.appendVariant(variant))
                    return GetByStatus(JSC::slowVersion(summary), *stubInfo);

                if (haveDOMAttribute) {
                    // Give up when custom accesses are not merged into one.
                    if (result.numVariants() != 1)
                        return GetByStatus(JSC::slowVersion(summary), *stubInfo);
                } else {
                    // Give up when custom access and simple access are mixed.
                    if (result.m_state == Custom)
                        return GetByStatus(JSC::slowVersion(summary), *stubInfo);
                }
                break;
            } }
        }
        
        return result;
    }
        
    default:
        return GetByStatus(JSC::slowVersion(summary), *stubInfo);
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return GetByStatus();
}

GetByStatus GetByStatus::computeFor(
    CodeBlock* profiledBlock, ICStatusMap& baselineMap,
    ICStatusContextStack& icContextStack, CodeOrigin codeOrigin)
{
    BytecodeIndex bytecodeIndex = codeOrigin.bytecodeIndex();
    CallLinkStatus::ExitSiteData callExitSiteData = CallLinkStatus::computeExitSiteData(profiledBlock, bytecodeIndex);
    ExitFlag didExit = hasBadCacheExitSite(profiledBlock, bytecodeIndex);

    for (ICStatusContext* context : icContextStack) {
        ICStatus status = context->get(codeOrigin);
        
        auto bless = [&] (const GetByStatus& result) -> GetByStatus {
            if (!context->isInlined(codeOrigin)) {
                // Merge with baseline result, which also happens to contain exit data for both
                // inlined and not-inlined.
                GetByStatus baselineResult = computeFor(
                    profiledBlock, baselineMap, bytecodeIndex, didExit,
                    callExitSiteData);
                baselineResult.merge(result);
                return baselineResult;
            }
            if (didExit.isSet(ExitFromInlined))
                return result.slowVersion();
            return result;
        };
        
        if (status.stubInfo) {
            GetByStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeForStubInfoWithoutExitSiteFeedback(
                    locker, context->optimizedCodeBlock, status.stubInfo, callExitSiteData);
            }
            if (result.isSet())
                return bless(result);
        }
        
        if (status.getStatus)
            return bless(*status.getStatus);
    }
    
    return computeFor(profiledBlock, baselineMap, bytecodeIndex, didExit, callExitSiteData);
}

GetByStatus GetByStatus::computeFor(const StructureSet& set, UniquedStringImpl* uid)
{
    // For now we only handle the super simple self access case. We could handle the
    // prototype case in the future.
    //
    // Note that this code is also used for GetByIdDirect since this function only looks
    // into direct properties. When supporting prototype chains, we should split this for
    // GetById and GetByIdDirect.
    
    if (set.isEmpty())
        return GetByStatus();

    if (parseIndex(*uid))
        return GetByStatus(LikelyTakesSlowPath);
    
    GetByStatus result;
    result.m_state = Simple;
    result.m_wasSeenInJIT = false;
    for (unsigned i = 0; i < set.size(); ++i) {
        Structure* structure = set[i];
        if (structure->typeInfo().overridesGetOwnPropertySlot() && structure->typeInfo().type() != GlobalObjectType)
            return GetByStatus(LikelyTakesSlowPath);
        
        if (!structure->propertyAccessesAreCacheable())
            return GetByStatus(LikelyTakesSlowPath);
        
        unsigned attributes;
        PropertyOffset offset = structure->getConcurrently(uid, attributes);
        if (!isValidOffset(offset))
            return GetByStatus(LikelyTakesSlowPath); // It's probably a prototype lookup. Give up on life for now, even though we could totally be way smarter about it.
        if (attributes & PropertyAttribute::Accessor)
            return GetByStatus(MakesCalls); // We could be smarter here, like strength-reducing this to a Call.
        if (attributes & PropertyAttribute::CustomAccessorOrValue)
            return GetByStatus(LikelyTakesSlowPath);
        
        if (!result.appendVariant(GetByIdVariant(nullptr, structure, offset)))
            return GetByStatus(LikelyTakesSlowPath);
    }
    
    return result;
}
#endif // ENABLE(JIT)

bool GetByStatus::makesCalls() const
{
    switch (m_state) {
    case NoInformation:
    case LikelyTakesSlowPath:
    case ObservedTakesSlowPath:
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
    case ObservedSlowPathAndMakesCalls:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();

    return false;
}

GetByStatus GetByStatus::slowVersion() const
{
    if (observedStructureStubInfoSlowPath())
        return GetByStatus(makesCalls() ? ObservedSlowPathAndMakesCalls : ObservedTakesSlowPath, wasSeenInJIT());
    return GetByStatus(makesCalls() ? MakesCalls : LikelyTakesSlowPath, wasSeenInJIT());
}

void GetByStatus::merge(const GetByStatus& other)
{
    if (other.m_state == NoInformation)
        return;
    
    auto mergeSlow = [&] () {
        if (observedStructureStubInfoSlowPath() || other.observedStructureStubInfoSlowPath())
            *this = GetByStatus((makesCalls() || other.makesCalls()) ? ObservedSlowPathAndMakesCalls : ObservedTakesSlowPath);
        else
            *this = GetByStatus((makesCalls() || other.makesCalls()) ? MakesCalls : LikelyTakesSlowPath);
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
        
        if (m_moduleNamespaceData->m_moduleNamespaceObject != other.m_moduleNamespaceData->m_moduleNamespaceObject)
            return mergeSlow();
        
        if (m_moduleNamespaceData->m_moduleEnvironment != other.m_moduleNamespaceData->m_moduleEnvironment)
            return mergeSlow();
        
        if (m_moduleNamespaceData->m_scopeOffset != other.m_moduleNamespaceData->m_scopeOffset)
            return mergeSlow();
        
        return;
        
    case LikelyTakesSlowPath:
    case ObservedTakesSlowPath:
    case MakesCalls:
    case ObservedSlowPathAndMakesCalls:
        return mergeSlow();
    }
    
    RELEASE_ASSERT_NOT_REACHED();
}

void GetByStatus::filter(const StructureSet& set)
{
    if (m_state != Simple)
        return;
    filterICStatusVariants(m_variants, set);
    if (m_variants.isEmpty())
        m_state = NoInformation;
}

void GetByStatus::visitAggregate(SlotVisitor& visitor)
{
    if (isModuleNamespace())
        m_moduleNamespaceData->m_identifier.visitAggregate(visitor);
    for (GetByIdVariant& variant : m_variants)
        variant.visitAggregate(visitor);
}

void GetByStatus::markIfCheap(SlotVisitor& visitor)
{
    for (GetByIdVariant& variant : m_variants)
        variant.markIfCheap(visitor);
}

bool GetByStatus::finalize(VM& vm)
{
    for (GetByIdVariant& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    if (isModuleNamespace()) {
        if (m_moduleNamespaceData->m_moduleNamespaceObject && !vm.heap.isMarked(m_moduleNamespaceData->m_moduleNamespaceObject))
            return false;
        if (m_moduleNamespaceData->m_moduleEnvironment && !vm.heap.isMarked(m_moduleNamespaceData->m_moduleEnvironment))
            return false;
    }
    return true;
}

CacheableIdentifier GetByStatus::singleIdentifier() const
{
    if (isModuleNamespace())
        return m_moduleNamespaceData->m_identifier;

    if (m_variants.isEmpty())
        return nullptr;

    CacheableIdentifier result = m_variants.first().identifier();
    if (!result)
        return nullptr;
    for (size_t i = 1; i < m_variants.size(); ++i) {
        CacheableIdentifier identifier = m_variants[i].identifier();
        if (!identifier)
            return nullptr;
        if (identifier != result)
            return nullptr;
    }
    return result;
}

void GetByStatus::dump(PrintStream& out) const
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
    case LikelyTakesSlowPath:
        out.print("LikelyTakesSlowPath");
        break;
    case ObservedTakesSlowPath:
        out.print("ObservedTakesSlowPath");
        break;
    case MakesCalls:
        out.print("MakesCalls");
        break;
    case ObservedSlowPathAndMakesCalls:
        out.print("ObservedSlowPathAndMakesCalls");
        break;
    }
    out.print(", ", listDump(m_variants), ", seenInJIT = ", m_wasSeenInJIT, ")");
}

} // namespace JSC

