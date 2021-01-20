/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "EmptyScriptExecutionContext.h"
#include "JSDOMGlobalObject.h"

namespace WebCore {

class JSIDBSerializationGlobalObject final : public JSDOMGlobalObject {
public:
    using Base = JSDOMGlobalObject;
    static JSIDBSerializationGlobalObject* create(JSC::VM&, JSC::Structure*, Ref<DOMWrapperWorld>&&);

    template<typename, JSC::SubspaceAccess mode>
    static JSC::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        if constexpr (mode == JSC::SubspaceAccess::Concurrently)
            return nullptr;
        return subspaceForImpl(vm);
    }
    static JSC::IsoSubspace* subspaceForImpl(JSC::VM&);

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, 0, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
    }
    static void destroy(JSC::JSCell*);

    ScriptExecutionContext* scriptExecutionContext() const { return m_scriptExecutionContext.ptr(); }

private:
    JSIDBSerializationGlobalObject(JSC::VM&, JSC::Structure*, Ref<DOMWrapperWorld>&&);
    void finishCreation(JSC::VM&);

    Ref<EmptyScriptExecutionContext> m_scriptExecutionContext;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

