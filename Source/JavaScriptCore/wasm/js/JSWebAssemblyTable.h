/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/WasmLimits.h>
#include <JavaScriptCore/WasmTable.h>
#include <JavaScriptCore/WebAssemblyFunction.h>
#include <JavaScriptCore/WebAssemblyWrapperFunction.h>
#include <wtf/Ref.h>

namespace JSC {

class JSWebAssemblyTable final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyTableSpace<mode>();
    }

    static JSWebAssemblyTable* create(VM&, Structure*, Ref<Wasm::Table>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    DECLARE_VISIT_CHILDREN;

    static bool isValidLength(uint32_t length) { return Wasm::Table::isValidLength(length); }
    std::optional<uint32_t> maximum() const { return m_table->maximum(); }
    uint32_t length() const { return m_table->length(); }
    uint32_t allocatedLength() const { return m_table->allocatedLength(length()); }
    std::optional<uint32_t> grow(JSGlobalObject*, uint32_t delta, JSValue defaultValue) WARN_UNUSED_RETURN;
    JSValue get(JSGlobalObject*, uint32_t);
    void set(uint32_t, JSValue);
    void set(JSGlobalObject*, uint32_t, JSValue);
    void clear(uint32_t);
    JSObject* type(JSGlobalObject*);

    Wasm::Table* table() { return m_table.ptr(); }

private:
    JSWebAssemblyTable(VM&, Structure*, Ref<Wasm::Table>&&);
    DECLARE_DEFAULT_FINISH_CREATION;

    const Ref<Wasm::Table> m_table;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
