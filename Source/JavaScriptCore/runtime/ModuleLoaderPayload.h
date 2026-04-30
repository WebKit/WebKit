/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "JSCell.h"
#include "JSPromise.h"

namespace JSC {

// Wraps the dynamic-import target promise for top-level dynamic loadModule. Acts as the
// host-defined "payload" passed back via FinishLoadingImportedModule, and additionally
// holds the AND-join state used to combine loadPromise and statePromise.
class ModuleLoaderPayload final : public JSCell {
    friend class LLIntOffsetsExtractor;
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.moduleLoaderPayloadSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);
    static ModuleLoaderPayload* create(VM&, JSPromise*);

    JSPromise* promise() const { return m_promise.get(); }

    JSValue fulfillment() const { return m_fulfillment.get(); }
    void setFulfillment(VM& vm, JSValue value) { m_fulfillment.set(vm, this, value); }

    bool decrementRemaining()
    {
        ASSERT(m_remainingFulfillments > 0);
        return !--m_remainingFulfillments;
    }

private:
    ModuleLoaderPayload(VM&, Structure*, JSPromise*);

    void finishCreation(VM&);

    WriteBarrier<JSPromise> m_promise;
    WriteBarrier<Unknown> m_fulfillment;
    uint8_t m_remainingFulfillments { 2 };
};

} // namespace JSC
