/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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

#include "IterationModeMetadata.h"
#include "JSObject.h"

namespace JSC {

class JSAsyncFromSyncIterator final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.asyncFromSyncIteratorSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSAsyncFromSyncIterator* create(VM&, Structure*, JSObject* syncIterator, JSValue nextMethod, IterationMode);

    JSObject* syncIterator() const { return asObject(m_syncIterator.pointer()); }
    JSValue nextMethod() const { return m_nextMethod.get(); }
    IterationMode iterationMode() const { return m_syncIterator.type(); }

    void setTarget(VM& vm, JSObject* target, bool closeSyncIteratorOnRejection)
    {
        m_target.setPointer(target);
        m_target.setType(closeSyncIteratorOnRejection);
        vm.writeBarrier(this, target);
    }

    std::tuple<JSObject*, bool> extractTarget()
    {
        auto* target = m_target.pointer();
        bool result = m_target.type();
        m_target = { };
        return { target, result };
    }

    JSValue cachedDriverResult() const { return m_cachedResult.get(); }
    void setCachedDriverResult(VM& vm, JSObject* result) { m_cachedResult.set(vm, this, result); }

private:
    JSAsyncFromSyncIterator(VM& vm, Structure* structure, JSObject* syncIterator, JSValue nextMethod, IterationMode mode)
        : Base(vm, structure)
        , m_syncIterator(syncIterator, mode)
        , m_nextMethod(nextMethod, WriteBarrierEarlyInit)
        , m_target(nullptr, false)
        , m_cachedResult(jsUndefined(), WriteBarrierEarlyInit)
    {
    }

    DECLARE_DEFAULT_FINISH_CREATION;

    CompactPointerTuple<JSObject*, IterationMode> m_syncIterator;
    WriteBarrier<Unknown> m_nextMethod;
    CompactPointerTuple<JSObject*, bool> m_target;
    WriteBarrier<Unknown> m_cachedResult;
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSAsyncFromSyncIterator);

void driveAsyncFromSyncIteratorWithDriver(JSGlobalObject*, JSAsyncFromSyncIterator*, JSObject* driver);
JSValue asyncFromSyncIteratorNext(JSGlobalObject*, JSAsyncFromSyncIterator*, JSValue argument);

} // namespace JSC
