/*
 * Copyright (C) 2019-2022 Apple, Inc. All rights reserved.
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

#include "JSObject.h"

namespace JSC {

class JSWeakObjectRef final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    DECLARE_EXPORT_INFO;

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSWeakObjectRef* create(VM& vm, Structure* structure, JSCell* target)
    {
        JSWeakObjectRef* instance = new (NotNull, allocateCell<JSWeakObjectRef>(vm)) JSWeakObjectRef(vm, structure);
        instance->finishCreation(vm, target);
        return instance;
    }

    JSCell* deref(VM& vm)
    {
        if (m_value && vm.currentWeakRefVersion() != m_lastAccessVersion) {
            m_lastAccessVersion = vm.currentWeakRefVersion();
            // Perform a GC barrier here so we rescan this object and keep the object alive if we wouldn't otherwise.
            vm.writeBarrier(this);
        }

        return m_value.get();
    }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.weakObjectRefSpace<mode>();
    }

    void finalizeUnconditionally(VM&, CollectionScope);
    DECLARE_VISIT_CHILDREN;

private:
    JSWeakObjectRef(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSCell* value);

    uintptr_t m_lastAccessVersion;
    WriteBarrier<JSCell> m_value;
};

} // namespace JSC

