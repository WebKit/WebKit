/*
 * Copyright (C) 2020 Apple, Inc. All rights reserved.
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

#include "JSInternalFieldObjectImpl.h"

namespace JSC {

class JSFinalizationRegistry final : public JSInternalFieldObjectImpl<1> {
public:
    using Base = JSInternalFieldObjectImpl<1>;

    enum class Field : uint8_t { 
        Callback,
    };

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, inlineCapacity == 0U);
        return sizeof(JSFinalizationRegistry);
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.finalizationRegistrySpace<mode>();
    }

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    JSObject* callback() const { return jsCast<JSObject*>(internalField(Field::Callback).get()); }

    static JSFinalizationRegistry* create(VM&, Structure*, JSObject* callback);
    static JSFinalizationRegistry* createWithInitialValues(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    void runFinalizationCleanup(JSGlobalObject*);

    DECLARE_EXPORT_INFO;

    void finalizeUnconditionally(VM&);
    static void visitChildren(JSCell*, SlotVisitor&);
    static void destroy(JSCell*);
    static constexpr bool needsDestruction = true;

    JSValue takeDeadHoldingsValue();

    bool unregister(VM&, JSObject* token);
    // token should be a JSObject or undefined.
    void registerTarget(VM&, JSObject* target, JSValue holdings, JSValue token);

    JS_EXPORT_PRIVATE size_t liveCount(const Locker<JSCellLock>&);
    JS_EXPORT_PRIVATE size_t deadCount(const Locker<JSCellLock>&);

private:
    JSFinalizationRegistry(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSObject* callback);

    static String toStringName(const JSObject*, JSGlobalObject*);

    struct Registration {
        JSObject* target;
        WriteBarrier<Unknown> holdings;
    };

    using LiveRegistrations = Vector<Registration>;
    // We don't need the target anymore since we know it's dead.
    using DeadRegistrations = Vector<WriteBarrier<Unknown>>;

    // Note that we don't bother putting a write barrier on the key or target because they are weakly referenced.
    HashMap<JSObject*, LiveRegistrations> m_liveRegistrations;
    HashMap<JSObject*, DeadRegistrations> m_deadRegistrations;
    // We use a separate list for no unregister values instead of a special key in the tables above because the HashMap has a tendency to reallocate under us when iterating...
    LiveRegistrations m_noUnregistrationLive;
    DeadRegistrations m_noUnregistrationDead;
};

} // namespace JSC


