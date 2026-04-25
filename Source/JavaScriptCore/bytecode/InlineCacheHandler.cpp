/*
 * Copyright (C) 2014-2022, 2026 Apple Inc. All rights reserved.
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
#include "InlineCacheHandler.h"

#if ENABLE(JIT)

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "GetterSetterAccessCase.h"
#include "InlineCacheCompiler.h"
#include "InstanceOfAccessCase.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "ModuleNamespaceAccessCase.h"
#include "PropertyInlineCache.h"
#include "SharedJITStubSet.h"

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InlineCacheHandler);
WTF_MAKE_TZONE_ALLOCATED_IMPL(InlineCacheHandlerWithJSCall);

void InlineCacheHandler::dump(PrintStream& out) const
{
    if (m_callTarget)
        out.print(m_callTarget);
}


InlineCacheHandler::InlineCacheHandler()
{
    disableThreadingChecks();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
InlineCacheHandler::InlineCacheHandler(bool makesJSCalls, Ref<InlineCacheHandler>&& previous, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&& watchpoint, CacheType cacheType)
    : m_callTarget(stubRoutine->code().code().template retagged<JITStubRoutinePtrTag>())
    , m_jumpTarget(CodePtr<NoPtrTag> { m_callTarget.retagged<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>())
    , m_cacheType(cacheType)
    , m_makesJSCalls(makesJSCalls)
    , m_next(WTF::move(previous))
    , m_stubRoutine(WTF::move(stubRoutine))
    , m_watchpoint(WTF::move(watchpoint))
{
    disableThreadingChecks();
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

InlineCacheHandlerWithJSCall::InlineCacheHandlerWithJSCall(Ref<InlineCacheHandler>&& previous, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&& watchpoint, CacheType cacheType)
    : InlineCacheHandler(true, WTF::move(previous), WTF::move(stubRoutine), WTF::move(watchpoint), cacheType)
{
}

void InlineCacheHandler::operator delete(InlineCacheHandler* handler, std::destroying_delete_t)
{
    if (auto* withJSCall = dynamicDowncast<InlineCacheHandlerWithJSCall>(handler)) {
        std::destroy_at(withJSCall);
        InlineCacheHandlerWithJSCall::freeAfterDestruction(withJSCall);
    } else {
        std::destroy_at(handler);
        InlineCacheHandler::freeAfterDestruction(handler);
    }
}

Ref<InlineCacheHandler> InlineCacheHandler::create(Ref<InlineCacheHandler>&& previous, CodeBlock* codeBlock, PropertyInlineCache& propertyCache, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&& watchpoint, unsigned callLinkInfoCount)
{
    VM& vm = codeBlock->vm();
    if (callLinkInfoCount) {
        auto result = adoptRef(*new InlineCacheHandlerWithJSCall(WTF::move(previous), WTF::move(stubRoutine), WTF::move(watchpoint), CacheType::Unset));
        result->m_callLinkInfo.initialize(vm, codeBlock, CallLinkInfo::CallType::Call, propertyCache.codeOrigin);
        result->m_uid = propertyCache.identifier().uid();
        return result;
    }
    auto result = adoptRef(*new InlineCacheHandler(false, WTF::move(previous), WTF::move(stubRoutine), WTF::move(watchpoint), CacheType::Unset));
    result->m_uid = propertyCache.identifier().uid();
    return result;
}

Ref<InlineCacheHandler> InlineCacheHandler::createPreCompiled(Ref<InlineCacheHandler>&& previous, CodeBlock* codeBlock, PropertyInlineCache& propertyCache, Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&& watchpoint, AccessCase& accessCase, CacheType cacheType)
{
    bool makesJSCalls = JSC::doesJSCalls(accessCase.m_type);
    VM& vm = codeBlock->vm();
    Ref<InlineCacheHandler> result = [&]() -> Ref<InlineCacheHandler> {
        if (makesJSCalls) {
            auto handler = adoptRef(*new InlineCacheHandlerWithJSCall(WTF::move(previous), WTF::move(stubRoutine), WTF::move(watchpoint), cacheType));
            handler->m_callLinkInfo.initialize(vm, codeBlock, CallLinkInfo::CallType::Call, propertyCache.codeOrigin);
            return handler;
        }
        return adoptRef(*new InlineCacheHandler(false, WTF::move(previous), WTF::move(stubRoutine), WTF::move(watchpoint), cacheType));
    }();

    result->m_structureID = accessCase.structureID();
    result->m_offset = accessCase.offset();
    result->m_uid = propertyCache.identifier().uid();
    if (!result->m_uid)
        result->m_uid = accessCase.uid();
    switch (accessCase.m_type) {
    case AccessCase::Load:
    case AccessCase::GetGetter:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::IndexedUndefinedKeyLoad:
    case AccessCase::IndexedNullKeyLoad:
    case AccessCase::IndexedTrueKeyLoad:
    case AccessCase::IndexedFalseKeyLoad: {
        result->u.s1.m_holder = nullptr;
        if (auto* holder = accessCase.tryGetAlternateBase())
            result->u.s1.m_holder = holder;
        break;
    }
    case AccessCase::ProxyObjectLoad: {
        result->u.s1.m_holder = accessCase.identifier().cell();
        break;
    }
    case AccessCase::Delete:
    case AccessCase::SetPrivateBrand: {
        result->u.s2.m_newStructureID = accessCase.newStructureID();
        break;
    }
    case AccessCase::Transition:
    case AccessCase::IndexedUndefinedKeyTransition:
    case AccessCase::IndexedNullKeyTransition:
    case AccessCase::IndexedTrueKeyTransition:
    case AccessCase::IndexedFalseKeyTransition: {
        result->u.s2.m_newStructureID = accessCase.newStructureID();
        result->u.s2.m_newSize = accessCase.newStructure()->outOfLineCapacity() * sizeof(JSValue);
        result->u.s2.m_oldSize = accessCase.structure()->outOfLineCapacity() * sizeof(JSValue);
        break;
    }
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomValueSetter: {
        result->u.s1.m_holder = nullptr;
        Structure* currStructure = accessCase.structure();
        if (auto* holder = accessCase.tryGetAlternateBase()) {
            currStructure = holder->structure();
            result->u.s1.m_holder = holder;
        }
        result->u.s1.m_globalObject = currStructure->realm();
        result->u.s1.m_customAccessor = accessCase.as<GetterSetterAccessCase>().customAccessor().taggedPtr();
        break;
    }
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss: {
        result->u.s1.m_holder = accessCase.as<InstanceOfAccessCase>().prototype();
        break;
    }
    case AccessCase::ModuleNamespaceLoad: {
        auto& derived = accessCase.as<ModuleNamespaceAccessCase>();
        result->u.s3.m_moduleNamespaceObject = derived.moduleNamespaceObject();
        result->u.s3.m_moduleVariableSlot = &derived.moduleEnvironment()->variableAt(derived.scopeOffset());
        break;
    }
    case AccessCase::CheckPrivateBrand: {
        break;
    }
    default:
        break;
    }

    return result;
}

Ref<InlineCacheHandler> InlineCacheHandler::createNonHandlerSlowPath(CodePtr<JITStubRoutinePtrTag> slowPath)
{
    auto result = adoptRef(*new InlineCacheHandler);
    result->m_callTarget = slowPath;
    result->m_jumpTarget = slowPath;
    return result;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
Ref<InlineCacheHandler> InlineCacheHandler::createSlowPath(VM& vm, AccessType accessType)
{
    auto result = adoptRef(*new InlineCacheHandler);
    auto codeRef = InlineCacheCompiler::generateSlowPathCode(vm, accessType);
    result->m_callTarget = codeRef.code().template retagged<JITStubRoutinePtrTag>();
    result->m_jumpTarget = CodePtr<NoPtrTag> { codeRef.retaggedCode<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>();
    return result;
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

Ref<InlineCacheHandler> InlineCacheCompiler::generateSlowPathHandler(VM& vm, AccessType accessType)
{
    ASSERT(!isCompilationThread());
    if (auto handler = vm.m_sharedJITStubs->getSlowPathHandler(accessType))
        return handler.releaseNonNull();
    auto handler = InlineCacheHandler::createSlowPath(vm, accessType);
    vm.m_sharedJITStubs->setSlowPathHandler(accessType, handler);
    return handler;
}

template<typename Visitor>
void InlineCacheHandler::propagateTransitions(Visitor& visitor) const
{
    if (m_accessCase)
        m_accessCase->propagateTransitions(visitor);
}

template void InlineCacheHandler::propagateTransitions(AbstractSlotVisitor&) const;
template void InlineCacheHandler::propagateTransitions(SlotVisitor&) const;

template<typename Visitor>
void InlineCacheHandler::visitAggregateImpl(Visitor& visitor)
{
    if (m_accessCase)
        m_accessCase->visitAggregate(visitor);
}
DEFINE_VISIT_AGGREGATE(InlineCacheHandler);

void InlineCacheHandler::aboutToDie()
{
    if (m_stubRoutine)
        m_stubRoutine->aboutToDie();
    // A reference to InlineCacheHandler may keep it alive later than the CodeBlock that "owns" this
    // watchpoint but the watchpoint must not fire after the CodeBlock has finished destruction,
    // so clear the watchpoint eagerly.
    m_watchpoint.reset();
}

bool InlineCacheHandler::visitWeak(VM& vm)
{
    bool isValid = true;
    if (auto* withJSCall = dynamicDowncast<InlineCacheHandlerWithJSCall>(*this))
        withJSCall->m_callLinkInfo.visitWeak(vm);

    if (m_accessCase)
        isValid &= m_accessCase->visitWeak(vm);

    if (m_stubRoutine)
        isValid &= m_stubRoutine->visitWeak(vm);

    return isValid;
}

void InlineCacheHandler::addOwner(CodeBlock* codeBlock)
{
    if (!m_stubRoutine)
        return;
    m_stubRoutine->addOwner(codeBlock);
}

void InlineCacheHandler::removeOwner(CodeBlock* codeBlock)
{
    if (!m_stubRoutine)
        return;
    m_stubRoutine->removeOwner(codeBlock);
}

} // namespace JSC

#endif // ENABLE(JIT)
