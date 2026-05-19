/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSObject.h"
#include "JSScope.h"

namespace JSC {

class StrictEvalActivation final : public JSObject {
public:
    using Base = JSObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesPut;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.strictEvalActivationSpace<mode>();
    }

    static StrictEvalActivation* create(VM& vm, Structure* structure, JSObject* currentScope)
    {
        StrictEvalActivation* scope = new (NotNull, allocateCell<StrictEvalActivation>(vm)) StrictEvalActivation(vm, structure, currentScope);
        scope->finishCreation(vm);
        return scope;
    }

    static bool NODELETE deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);
    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;
    DECLARE_VISIT_CHILDREN;

    JS_EXPORT_PRIVATE JSObject* ensureBindingStorage(JSGlobalObject*);
    JSObject* bindingStorage() const { return m_bindingStorage.get(); }
    JSObject* next() const { return m_next.get(); }

private:
    StrictEvalActivation(VM&, Structure*, JSObject*);

    WriteBarrier<JSObject> m_bindingStorage;
    WriteBarrier<JSObject> m_next;

public:
    static constexpr ptrdiff_t offsetOfNext() { return OBJECT_OFFSETOF(StrictEvalActivation, m_next); }
};

static_assert(StrictEvalActivation::offsetOfNext() == scopeChainNextOffset, "StrictEvalActivation::m_next must live at scopeChainNextOffset");

} // namespace JSC
