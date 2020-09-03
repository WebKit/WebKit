/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "DataView.h"
#include "JSArrayBufferView.h"

namespace JSC {

class JSDataView final : public JSArrayBufferView {
public:
    using Base = JSArrayBufferView;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesAnyFormOfGetPropertyNames;

    static constexpr unsigned elementSize = 1;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.dataViewSpace<mode>();
    }

    JS_EXPORT_PRIVATE static JSDataView* create(
        JSGlobalObject*, Structure*, RefPtr<ArrayBuffer>&&, unsigned byteOffset,
        unsigned byteLength);
    
    // Dummy methods, which don't actually work; these are just in place to
    // placate some template specialization we do elsewhere.
    static JSDataView* createUninitialized(JSGlobalObject*, Structure*, unsigned length);
    static JSDataView* create(JSGlobalObject*, Structure*, unsigned length);
    bool set(JSGlobalObject*, unsigned, JSObject*, unsigned, unsigned length);
    bool setIndex(JSGlobalObject*, unsigned, JSValue);
    
    ArrayBuffer* possiblySharedBuffer() const { return m_buffer; }
    ArrayBuffer* unsharedBuffer() const
    {
        RELEASE_ASSERT(!m_buffer->isShared());
        return m_buffer;
    }
    
    RefPtr<DataView> possiblySharedTypedImpl();
    RefPtr<DataView> unsharedTypedImpl();
    
    static const TypedArrayType TypedArrayStorageType = TypeDataView;

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);
    
    DECLARE_EXPORT_INFO;

private:
    JSDataView(VM&, ConstructionContext&, ArrayBuffer*);

    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);
    static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);
    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);

    static void getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode);

    ArrayBuffer* m_buffer;
};

} // namespace JSC
