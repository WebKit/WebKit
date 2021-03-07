/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "JSDestructibleObject.h"
#include "JSObject.h"
#include "WasmMemoryMode.h"
#include "WasmOps.h"
#include <wtf/Bag.h>
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace JSC {

namespace Wasm {
class Module;
struct ModuleInformation;
class Plan;
}

class SymbolTable;
class JSWebAssemblyCodeBlock;
class JSWebAssemblyMemory;

class JSWebAssemblyModule final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyModuleSpace<mode>();
    }

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static JSWebAssemblyModule* createStub(VM&, JSGlobalObject*, Structure*, Expected<RefPtr<Wasm::Module>, String>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    const Wasm::ModuleInformation& moduleInformation() const;
    SymbolTable* exportSymbolTable() const;
    Wasm::SignatureIndex signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const;

    JSWebAssemblyCodeBlock* codeBlock(Wasm::MemoryMode mode);
    void setCodeBlock(VM&, Wasm::MemoryMode, JSWebAssemblyCodeBlock*);

    JS_EXPORT_PRIVATE Wasm::Module& module();

private:
    friend class JSWebAssemblyCodeBlock;

    JSWebAssemblyModule(VM&, Structure*, Ref<Wasm::Module>&&);
    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

    Ref<Wasm::Module> m_module;
    WriteBarrier<SymbolTable> m_exportSymbolTable;
    WriteBarrier<JSWebAssemblyCodeBlock> m_codeBlocks[Wasm::NumberOfMemoryModes];
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
