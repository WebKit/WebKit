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

#pragma once

#if ENABLE(JIT)

#include "AccessCase.h"
#include "CallLinkInfo.h"
#include "JITStubRoutine.h"
#include "PropertyInlineCacheClearingWatchpoint.h"
#include <wtf/RefCounted.h>

namespace JSC {

class CodeBlock;
class InlineCacheCompiler;
class InlineCacheHandlerWithJSCall;
class PolymorphicAccessJITStubRoutine;
class PropertyInlineCache;

enum class CacheType : int8_t {
    Unset,
    GetByIdSelf,
    GetByIdPrototype,
    PutByIdReplace,
    InByIdSelf,
    Stub,
    ArrayLength,
    StringLength,
};

class InlineCacheHandler : public RefCounted<InlineCacheHandler> {
    WTF_MAKE_NONCOPYABLE(InlineCacheHandler);
    WTF_MAKE_TZONE_ALLOCATED(InlineCacheHandler);
    friend class InlineCacheCompiler;
    friend class InlineCacheHandlerWithJSCall;
public:
    static Ref<InlineCacheHandler> create(Ref<InlineCacheHandler>&&, CodeBlock*, PropertyInlineCache&, Ref<PolymorphicAccessJITStubRoutine>&&, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&&, unsigned callLinkInfoCount);
    static Ref<InlineCacheHandler> createPreCompiled(Ref<InlineCacheHandler>&&, CodeBlock*, PropertyInlineCache&, Ref<PolymorphicAccessJITStubRoutine>&&, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&&, AccessCase&, CacheType);

    void operator delete(InlineCacheHandler*, std::destroying_delete_t);

    CodePtr<JITStubRoutinePtrTag> callTarget() const { return m_callTarget; }
    CodePtr<JITStubRoutinePtrTag> jumpTarget() const { return m_jumpTarget; }

    void aboutToDie();
    bool containsPC(void* pc) const
    {
        if (!m_stubRoutine)
            return false;

        uintptr_t pcAsInt = std::bit_cast<uintptr_t>(pc);
        return m_stubRoutine->startAddress() <= pcAsInt && pcAsInt <= m_stubRoutine->endAddress();
    }

    // If this returns false then we are requesting a reset of the owning PropertyInlineCache.
    bool visitWeak(VM&);

    void dump(PrintStream&) const;

    static Ref<InlineCacheHandler> createNonHandlerSlowPath(CodePtr<JITStubRoutinePtrTag>);

    void addOwner(CodeBlock*);
    void removeOwner(CodeBlock*);

    PolymorphicAccessJITStubRoutine* stubRoutine() { return m_stubRoutine.get(); }

    InlineCacheHandler* next() const { return m_next.get(); }
    void setNext(RefPtr<InlineCacheHandler>&& next)
    {
        m_next = WTF::move(next);
    }

    AccessCase* accessCase() const { return m_accessCase.get(); }
    void setAccessCase(RefPtr<AccessCase>&& accessCase)
    {
        m_accessCase = WTF::move(accessCase);
    }

    bool makesJSCalls() const { return m_makesJSCalls; }

    static constexpr ptrdiff_t offsetOfCallTarget() { return OBJECT_OFFSETOF(InlineCacheHandler, m_callTarget); }
    static constexpr ptrdiff_t offsetOfJumpTarget() { return OBJECT_OFFSETOF(InlineCacheHandler, m_jumpTarget); }
    static constexpr ptrdiff_t offsetOfNext() { return OBJECT_OFFSETOF(InlineCacheHandler, m_next); }
    static constexpr ptrdiff_t offsetOfUid() { return OBJECT_OFFSETOF(InlineCacheHandler, m_uid); }
    static constexpr ptrdiff_t offsetOfStructureID() { return OBJECT_OFFSETOF(InlineCacheHandler, m_structureID); }
    static constexpr ptrdiff_t offsetOfOffset() { return OBJECT_OFFSETOF(InlineCacheHandler, m_offset); }
    static constexpr ptrdiff_t offsetOfNewStructureID() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s2.m_newStructureID); }
    static constexpr ptrdiff_t offsetOfNewSize() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s2.m_newSize); }
    static constexpr ptrdiff_t offsetOfOldSize() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s2.m_oldSize); }
    static constexpr ptrdiff_t offsetOfHolder() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s1.m_holder); }
    static constexpr ptrdiff_t offsetOfGlobalObject() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s1.m_globalObject); }
    static constexpr ptrdiff_t offsetOfCustomAccessor() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s1.m_customAccessor); }
    static constexpr ptrdiff_t offsetOfModuleNamespaceObject() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s3.m_moduleNamespaceObject); }
    static constexpr ptrdiff_t offsetOfModuleVariableSlot() { return OBJECT_OFFSETOF(InlineCacheHandler, u.s3.m_moduleVariableSlot); }

    StructureID structureID() const { return m_structureID; }
    PropertyOffset offset() const { return m_offset; }
    JSCell* holder() const { return u.s1.m_holder; }
    size_t newSize() const { return u.s2.m_newSize; }
    size_t oldSize() const { return u.s2.m_oldSize; }
    StructureID newStructureID() const { return u.s2.m_newStructureID; }

    CacheType cacheType() const { return m_cacheType; }

    DECLARE_VISIT_AGGREGATE;

    // This returns true if it has marked everything it will ever marked. This can be used as an
    // optimization to then avoid calling this method again during the fixpoint.
    template<typename Visitor> void propagateTransitions(Visitor&) const;

protected:
    InlineCacheHandler();
    InlineCacheHandler(bool makesJSCalls, Ref<InlineCacheHandler>&&, Ref<PolymorphicAccessJITStubRoutine>&&, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&&, CacheType);

    static Ref<InlineCacheHandler> createSlowPath(VM&, AccessType);

    CodePtr<JITStubRoutinePtrTag> m_callTarget;
    CodePtr<JITStubRoutinePtrTag> m_jumpTarget;
    StructureID m_structureID { };
    PropertyOffset m_offset { invalidOffset };
    UniquedStringImpl* m_uid { nullptr };
    union {
        struct {
            StructureID m_newStructureID { };
            unsigned m_newSize { };
            unsigned m_oldSize { };
        } s2 { };
        struct {
            JSCell* m_holder;
            JSGlobalObject* m_globalObject;
            void* m_customAccessor;
        } s1;
        struct {
            JSObject* m_moduleNamespaceObject;
            WriteBarrierBase<Unknown>* m_moduleVariableSlot;
        } s3;
    } u;
    CacheType m_cacheType { CacheType::Unset };
    bool m_makesJSCalls { false };
    RefPtr<InlineCacheHandler> m_next;
    RefPtr<PolymorphicAccessJITStubRoutine> m_stubRoutine;
    RefPtr<AccessCase> m_accessCase;
    std::unique_ptr<PropertyInlineCacheClearingWatchpoint> m_watchpoint;
};

class InlineCacheHandlerWithJSCall final : public InlineCacheHandler {
    WTF_MAKE_TZONE_ALLOCATED(InlineCacheHandlerWithJSCall);
    friend class InlineCacheHandler;
    friend class InlineCacheCompiler;
public:
    CallLinkInfo* callLinkInfo(const ConcurrentJSLocker&) { return &m_callLinkInfo; }

    static constexpr ptrdiff_t offsetOfCallLinkInfo() { return OBJECT_OFFSETOF(InlineCacheHandlerWithJSCall, m_callLinkInfo); }

private:
    InlineCacheHandlerWithJSCall(Ref<InlineCacheHandler>&&, Ref<PolymorphicAccessJITStubRoutine>&&, std::unique_ptr<PropertyInlineCacheClearingWatchpoint>&&, CacheType);

    DataOnlyCallLinkInfo m_callLinkInfo;
};

} // namespace JSC

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::InlineCacheHandlerWithJSCall)
    static bool isType(const JSC::InlineCacheHandler& handler) { return handler.makesJSCalls(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(JIT)
