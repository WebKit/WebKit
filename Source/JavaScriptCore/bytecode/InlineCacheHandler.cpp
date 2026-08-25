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
#include "LLIntData.h"
#include "ModuleNamespaceAccessCase.h"
#include "PropertyInlineCache.h"
#include "SharedJITStubSet.h"

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InlineCacheHandler);
WTF_MAKE_TZONE_ALLOCATED_IMPL(InlineCacheHandlerWithJSCall);

static CodePtr<JITStubRoutinePtrTag> llintJumpTargetForCallTarget(CodePtr<JITStubRoutinePtrTag> callTarget)
{
    return CodePtr<NoPtrTag> { callTarget.retagged<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>();
}

// Picks the LLInt opcode that reimplements this handler node. Shapes that call JS, allocate, or
// call into C++ resolve to llint_ic_skip_handler and only run once Baseline compiles the site.
// Mirrors the precompiled-thunk selection in compileOneAccessCaseHandler(); only consulted for
// nodes that picked a precompiled thunk, so shapes with extra runtime conditions are already out.
static OpcodeID llintICHandlerOpcode(AccessType accessType, const AccessCase& accessCase)
{
    bool own = !accessCase.tryGetAlternateBase();
    auto* uid = accessCase.uid();
    bool symbolKey = uid && uid->isSymbol();

    // A transition that changes out-of-line capacity has to allocate a butterfly.
    auto isNonAllocatingTransition = [&] {
        return accessCase.newStructure()->outOfLineCapacity() == accessCase.structure()->outOfLineCapacity();
    };

    switch (accessType) {
    // base = a0, cache = a1, result = r0.
    case AccessType::GetById:
    case AccessType::GetByIdDirect:
    case AccessType::GetPrivateNameById:
        switch (accessCase.m_type) {
        case AccessCase::GetGetter:
        case AccessCase::Load:
            return own ? llint_get_by_id_self_handler : llint_get_by_id_prototype_handler;
        case AccessCase::Miss:
            return llint_get_by_id_miss_handler;
        default:
            break;
        }
        break;

    // value = a0, base = a1, cache = a2.
    case AccessType::PutByIdStrict:
    case AccessType::PutByIdSloppy:
    case AccessType::PutByIdDirectStrict:
    case AccessType::PutByIdDirectSloppy:
    case AccessType::DefinePrivateNameById:
    case AccessType::SetPrivateNameById:
        switch (accessCase.m_type) {
        case AccessCase::Replace:
            return llint_put_by_id_replace_handler;
        case AccessCase::Transition:
            if (isNonAllocatingTransition())
                return llint_put_by_id_transition_handler;
            break;
        default:
            break;
        }
        break;

    // base = a0, cache = a1, result = r0.
    case AccessType::InById:
        switch (accessCase.m_type) {
        case AccessCase::InHit:
            return llint_in_by_id_hit_handler;
        case AccessCase::InMiss:
            return llint_in_by_id_miss_handler;
        default:
            break;
        }
        break;

    // base = a0, cache = a1, result = r0.
    case AccessType::DeleteByIdStrict:
    case AccessType::DeleteByIdSloppy:
        switch (accessCase.m_type) {
        case AccessCase::Delete:
            return llint_del_by_id_delete_handler;
        case AccessCase::DeleteNonConfigurable:
            return llint_del_by_id_non_configurable_handler;
        case AccessCase::DeleteMiss:
            return llint_del_by_id_miss_handler;
        default:
            break;
        }
        break;

    // base = a0, property = a1, cache = a2, array profile = a3, result = r0.
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
        switch (accessCase.m_type) {
        case AccessCase::GetGetter:
        case AccessCase::Load:
            if (symbolKey)
                return own ? llint_get_by_val_symbol_key_self_handler : llint_get_by_val_symbol_key_prototype_handler;
            return own ? llint_get_by_val_string_key_self_handler : llint_get_by_val_string_key_prototype_handler;
        case AccessCase::Miss:
            return symbolKey ? llint_get_by_val_symbol_key_miss_handler : llint_get_by_val_string_key_miss_handler;
        case AccessCase::IndexedUndefinedKeyLoad:
            return own ? llint_get_by_val_undefined_key_self_handler : llint_get_by_val_undefined_key_prototype_handler;
        case AccessCase::IndexedUndefinedKeyMiss:
            return llint_get_by_val_undefined_key_miss_handler;
        case AccessCase::IndexedNullKeyLoad:
            return own ? llint_get_by_val_null_key_self_handler : llint_get_by_val_null_key_prototype_handler;
        case AccessCase::IndexedNullKeyMiss:
            return llint_get_by_val_null_key_miss_handler;
        case AccessCase::IndexedTrueKeyLoad:
            return own ? llint_get_by_val_true_key_self_handler : llint_get_by_val_true_key_prototype_handler;
        case AccessCase::IndexedTrueKeyMiss:
            return llint_get_by_val_true_key_miss_handler;
        case AccessCase::IndexedFalseKeyLoad:
            return own ? llint_get_by_val_false_key_self_handler : llint_get_by_val_false_key_prototype_handler;
        case AccessCase::IndexedFalseKeyMiss:
            return llint_get_by_val_false_key_miss_handler;
        default:
            break;
        }
        break;

    // base = a0, property = a1, value = a2, cache = a3, array profile = a4.
    case AccessType::PutByValStrict:
    case AccessType::PutByValSloppy:
    case AccessType::PutByValDirectStrict:
    case AccessType::PutByValDirectSloppy:
    case AccessType::DefinePrivateNameByVal:
    case AccessType::SetPrivateNameByVal:
        switch (accessCase.m_type) {
        case AccessCase::Replace:
            return symbolKey ? llint_put_by_val_symbol_key_replace_handler : llint_put_by_val_string_key_replace_handler;
        case AccessCase::Transition:
            if (isNonAllocatingTransition())
                return symbolKey ? llint_put_by_val_symbol_key_transition_handler : llint_put_by_val_string_key_transition_handler;
            break;
        case AccessCase::IndexedUndefinedKeyReplace:
            return llint_put_by_val_undefined_key_replace_handler;
        case AccessCase::IndexedUndefinedKeyTransition:
            if (isNonAllocatingTransition())
                return llint_put_by_val_undefined_key_transition_handler;
            break;
        case AccessCase::IndexedNullKeyReplace:
            return llint_put_by_val_null_key_replace_handler;
        case AccessCase::IndexedNullKeyTransition:
            if (isNonAllocatingTransition())
                return llint_put_by_val_null_key_transition_handler;
            break;
        case AccessCase::IndexedTrueKeyReplace:
            return llint_put_by_val_true_key_replace_handler;
        case AccessCase::IndexedTrueKeyTransition:
            if (isNonAllocatingTransition())
                return llint_put_by_val_true_key_transition_handler;
            break;
        case AccessCase::IndexedFalseKeyReplace:
            return llint_put_by_val_false_key_replace_handler;
        case AccessCase::IndexedFalseKeyTransition:
            if (isNonAllocatingTransition())
                return llint_put_by_val_false_key_transition_handler;
            break;
        default:
            break;
        }
        break;

    // base = a0, property = a1, cache = a2, array profile = a3, result = r0.
    case AccessType::InByVal:
    case AccessType::HasPrivateName:
    case AccessType::HasPrivateBrand:
        switch (accessCase.m_type) {
        case AccessCase::InHit:
            return symbolKey ? llint_in_by_val_symbol_key_hit_handler : llint_in_by_val_string_key_hit_handler;
        case AccessCase::InMiss:
            return symbolKey ? llint_in_by_val_symbol_key_miss_handler : llint_in_by_val_string_key_miss_handler;
        default:
            break;
        }
        break;

    // base = a0, property = a1, cache = a2, result = r0.
    case AccessType::DeleteByValStrict:
    case AccessType::DeleteByValSloppy:
        switch (accessCase.m_type) {
        case AccessCase::Delete:
            return symbolKey ? llint_del_by_val_symbol_key_delete_handler : llint_del_by_val_string_key_delete_handler;
        case AccessCase::DeleteNonConfigurable:
            return symbolKey ? llint_del_by_val_symbol_key_non_configurable_handler : llint_del_by_val_string_key_non_configurable_handler;
        case AccessCase::DeleteMiss:
            return symbolKey ? llint_del_by_val_symbol_key_miss_handler : llint_del_by_val_string_key_miss_handler;
        default:
            break;
        }
        break;

    // base = a0, brand = a1, cache = a2.
    case AccessType::CheckPrivateBrand:
        if (accessCase.m_type == AccessCase::CheckPrivateBrand)
            return llint_check_private_brand_handler;
        break;
    case AccessType::SetPrivateBrand:
        if (accessCase.m_type == AccessCase::SetPrivateBrand)
            return llint_set_private_brand_handler;
        break;

    // Not wired up for LLInt yet.
    case AccessType::GetByIdWithThis:
    case AccessType::GetByValWithThis:
    case AccessType::InstanceOf:
        break;
    }

    return llint_ic_skip_handler;
}

void InlineCacheHandler::setLLIntTargets(OpcodeID opcodeID)
{
    m_llintCallTarget = LLInt::getCodePtr<JITStubRoutinePtrTag>(opcodeID);
    m_llintJumpTarget = llintJumpTargetForCallTarget(m_llintCallTarget);
}

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
    : m_next(WTF::move(previous))
    , m_callTarget(stubRoutine->code().code().template retagged<JITStubRoutinePtrTag>())
    , m_jumpTarget(CodePtr<NoPtrTag> { m_callTarget.retagged<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>())
    , m_cacheType(cacheType)
    , m_makesJSCalls(makesJSCalls)
    , m_stubRoutine(WTF::move(stubRoutine))
    , m_watchpoint(WTF::move(watchpoint))
{
    // createPreCompiled() overrides this once it knows the AccessCase. Bespoke stubs keep it.
    setLLIntTargets(llint_ic_skip_handler);
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

    if (!makesJSCalls)
        result->setLLIntTargets(llintICHandlerOpcode(propertyCache.accessType, accessCase));

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
    // Terminal node of every handler chain. LLInt tail-jumps into the compiled slow path from
    // here, which is safe precisely because it never returns to another handler.
    result->setLLIntTargets(llint_ic_generic_handler);
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
