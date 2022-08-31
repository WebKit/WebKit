/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "MacroAssemblerCodeRef.h"
#include "ProxyableAccessCase.h"

namespace JSC {

class GetterSetterAccessCase final : public ProxyableAccessCase {
public:
    using Base = ProxyableAccessCase;
    friend class AccessCase;

    // This can return null if it hasn't been generated yet. That's
    // actually somewhat likely because of how we do buffering of new cases.
    // CallLinkInfo's ownership is held both by generated code via GCAwareJITStubRoutine and PolymorphicAccess.
    // The ownership relation is PolymorphicAccess -> GCAwareJITStubRoutine -> CallLinkInfo.
    // PolymorphicAccess can be destroyed while GCAwareJITStubRoutine is alive if we are destroying PolymorphicAccess
    // while we are executing GCAwareJITStubRoutine. It is not possible that GetterSetterAccessCase is alive while
    // GCAwareJITStubRoutine is destroyed.
    OptimizingCallLinkInfo* callLinkInfo() const { return m_callLinkInfo; }
    JSObject* customSlotBase() const { return m_customSlotBase.get(); }
    std::optional<DOMAttributeAnnotation> domAttribute() const { return m_domAttribute; }

    void emitDOMJITGetter(AccessGenerationState&, const DOMJIT::GetterSetter*, GPRReg baseForGetGPR);

    static Ref<AccessCase> create(
        VM&, JSCell* owner, AccessType, CacheableIdentifier, PropertyOffset, Structure*,
        const ObjectPropertyConditionSet&, bool viaProxy, WatchpointSet* additionalSet, CodePtr<CustomAccessorPtrTag> customGetter,
        JSObject* customSlotBase, std::optional<DOMAttributeAnnotation>, RefPtr<PolyProtoAccessChain>&&);

    static Ref<AccessCase> create(VM&, JSCell* owner, AccessType, Structure*, CacheableIdentifier, PropertyOffset,
        const ObjectPropertyConditionSet&, RefPtr<PolyProtoAccessChain>&&, bool viaProxy = false,
        CodePtr<CustomAccessorPtrTag> customSetter = nullptr, JSObject* customSlotBase = nullptr);

    CodePtr<CustomAccessorPtrTag> customAccessor() const { return m_customAccessor; }

private:
    GetterSetterAccessCase(VM&, JSCell*, AccessType, CacheableIdentifier, PropertyOffset, Structure*, const ObjectPropertyConditionSet&, bool viaProxy, WatchpointSet* additionalSet, JSObject* customSlotBase, RefPtr<PolyProtoAccessChain>&&);

    GetterSetterAccessCase(const GetterSetterAccessCase&);

    bool hasAlternateBaseImpl() const;
    JSObject* alternateBaseImpl() const;
    void dumpImpl(PrintStream&, CommaPrinter&, Indenter&) const;
    Ref<AccessCase> cloneImpl() const;


    WriteBarrier<JSObject> m_customSlotBase;
    OptimizingCallLinkInfo* m_callLinkInfo { nullptr };
    CodePtr<CustomAccessorPtrTag> m_customAccessor;
    std::optional<DOMAttributeAnnotation> m_domAttribute;
};

} // namespace JSC

#endif // ENABLE(JIT)
