/*
 * Copyright (C) 2025 Codeblog CORP.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "VM.h"

namespace JSC {

class JSPromiseCombinatorsGlobalContext;

class JSPromiseCombinatorsContext final : public JSCell {
public:
    using Base = JSCell;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.promiseCombinatorsContextSpace<mode>();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSPromiseCombinatorsContext* create(VM&, JSPromiseCombinatorsGlobalContext*, uint64_t index);

    JSPromiseCombinatorsGlobalContext* globalContext() const { return m_globalContext.get(); }
    uint64_t index() const { return m_index; }

private:
    JSPromiseCombinatorsContext(VM& vm, Structure* structure, JSPromiseCombinatorsGlobalContext* globalContext, uint64_t index)
        : Base(vm, structure)
        , m_globalContext(globalContext, WriteBarrierEarlyInit)
        , m_index(index)
    {
    }

    WriteBarrier<JSPromiseCombinatorsGlobalContext> m_globalContext;
    uint64_t m_index { };
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseCombinatorsContext);

} // namespace JSC
