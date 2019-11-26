/*
 * Copyright (C) 2018 Igalia S.L.
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#include "InternalFunction.h"
#include "JSObjectRef.h"
#include <glib-object.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _JSCClass JSCClass;

namespace JSC {

class JSCCallbackFunction final : public InternalFunction {
    friend struct APICallbackFunction;
public:
    typedef InternalFunction Base;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.jscCallbackFunctionSpace<mode>();
    }

    enum class Type {
        Function,
        Method,
        Constructor
    };

    static JSCCallbackFunction* create(VM&, JSGlobalObject*, const String& name, Type, JSCClass*, GRefPtr<GClosure>&&, GType, Optional<Vector<GType>>&&);
    static constexpr bool needsDestruction = true;
    static void destroy(JSCell*);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

    DECLARE_INFO;

    JSValueRef call(JSContextRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    JSObjectRef construct(JSContextRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

private:
    JSCCallbackFunction(VM&, Structure*, Type, JSCClass*, GRefPtr<GClosure>&&, GType, Optional<Vector<GType>>&&);

    JSObjectCallAsFunctionCallback functionCallback() { return m_functionCallback; }
    JSObjectCallAsConstructorCallback constructCallback() { return m_constructCallback; }

    JSObjectCallAsFunctionCallback m_functionCallback;
    JSObjectCallAsConstructorCallback m_constructCallback;
    Type m_type;
    GRefPtr<JSCClass> m_class;
    GRefPtr<GClosure> m_closure;
    GType m_returnType;
    Optional<Vector<GType>> m_parameters;
};

} // namespace JSC
