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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/JSCell.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/WebAssemblyCompileOptions.h>

namespace JSC {

class JSPromise;

class JSWebAssemblyStreamingContext final : public JSCell {
public:
    using Base = JSCell;

    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;
    static constexpr DestructionMode needsDestruction = NeedsDestruction;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyStreamingContextSpace<mode>();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSWebAssemblyStreamingContext* create(VM&, JSPromise*, JSObject* importObject, std::optional<WebAssemblyCompileOptions>&&);

    JSPromise* promise() const { return m_promise.get(); }
    JSObject* importObject() const { return m_importObject.get(); }
    std::optional<WebAssemblyCompileOptions> takeCompileOptions() { return std::exchange(m_compileOptions, std::nullopt); }

    ~JSWebAssemblyStreamingContext();

private:
    JSWebAssemblyStreamingContext(VM&, Structure*, JSPromise*, JSObject* importObject, std::optional<WebAssemblyCompileOptions>&&);
    static void NODELETE destroy(JSCell*);

    WriteBarrier<JSPromise> m_promise;
    WriteBarrier<JSObject> m_importObject;
    std::optional<WebAssemblyCompileOptions> m_compileOptions;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
