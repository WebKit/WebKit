/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#include "JSWrapperObject.h"

namespace JSC {

constexpr PropertyOffset rawJSONObjectRawJSONPropertyOffset = firstOutOfLineOffset;

class JSRawJSONObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    using Base::StructureFlags;

    template<typename, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.rawJSONObjectSpace<mode>();
    }

    static JSRawJSONObject* tryCreate(VM& vm, Structure* structure, JSString* string)
    {
        constexpr bool hasIndexingHeader = false;
        Butterfly* butterfly = Butterfly::tryCreate(vm, nullptr, 0, structure->outOfLineCapacity(), hasIndexingHeader, IndexingHeader(), 0);
        if (!butterfly)
            return nullptr;
        JSRawJSONObject* object = new (NotNull, allocateCell<JSRawJSONObject>(vm)) JSRawJSONObject(vm, structure, butterfly);
        object->finishCreation(vm, string);
        return object;
    }

    DECLARE_EXPORT_INFO;

    JSString* rawJSON(VM&);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

protected:
    JS_EXPORT_PRIVATE void finishCreation(VM&, JSString*);
    JS_EXPORT_PRIVATE JSRawJSONObject(VM&, Structure*, Butterfly*);
};

} // namespace JSC
