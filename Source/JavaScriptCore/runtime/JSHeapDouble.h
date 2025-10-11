/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/VM.h>

namespace JSC {

/*
 * This represents a boxed double, which should only be used inside a compressed heap.
 *
 * A compressed heap uses a 4gb address space, and a 32-bit JSValue representation. Hence
 * we must box doubles.
 *
 * We also cannot require that every place that produces a double has an associated VM,
 * so for now we require at most one VM and use a VM singleton.
 *
 * Future work should avoid this by allocating doubles out of a vm-agnostic heap, and
 * tagging double pointers differently in JSValues.
 *
 */

VM& vmForHeapDouble();

class JSHeapDouble final : public JSCell {
public:
    using Base = JSCell;

    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.heapDoubleSpace();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    JS_EXPORT_PRIVATE static JSHeapDouble* createFrom(double value);

    static constexpr size_t offsetOfValue()
    {
        return OBJECT_OFFSETOF(JSHeapDouble, m_value);
    }

    DECLARE_EXPORT_INFO;

    double value() const { return m_value; }
    double toNumber() const { return value(); }
    bool toBoolean() const { return 0.0 < value() || value() < 0.0; }

private:
    JSHeapDouble(VM&, Structure*, double);

    double m_value { 0 };
};

} // namespace JSC
