/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "JSObject.h"
#include "WasmLimits.h"
#include "WasmTable.h"
#include "WebAssemblyWrapperFunction.h"
#include "WebAssemblyFunction.h"
#include <wtf/MallocPtr.h>
#include <wtf/Ref.h>

namespace JSC {

class JSWebAssemblyTable final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyTableSpace<mode>();
    }

    static JSWebAssemblyTable* tryCreate(JSGlobalObject*, VM&, Structure*, Ref<Wasm::Table>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

    static bool isValidLength(uint32_t length) { return Wasm::Table::isValidLength(length); }
    Optional<uint32_t> maximum() const { return m_table->maximum(); }
    uint32_t length() const { return m_table->length(); }
    uint32_t allocatedLength() const { return m_table->allocatedLength(length()); }
    bool grow(uint32_t delta) WARN_UNUSED_RETURN;
    JSValue get(uint32_t);
    void set(uint32_t, WebAssemblyFunction*);
    void set(uint32_t, WebAssemblyWrapperFunction*);
    void set(uint32_t, JSValue);
    void clear(uint32_t);

    Wasm::Table* table() { return m_table.ptr(); }

private:
    JSWebAssemblyTable(VM&, Structure*, Ref<Wasm::Table>&&);
    void finishCreation(VM&);
    static void visitChildren(JSCell*, SlotVisitor&);

    Ref<Wasm::Table> m_table;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
