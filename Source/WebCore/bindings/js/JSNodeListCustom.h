/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "JSDOMBinding.h"
#include "JSNodeList.h"
#include "StaticNodeList.h"

namespace WebCore {

class JSStaticNodeList final : public JSNodeList {
public:
    using Base = JSNodeList;
    static constexpr unsigned StructureFlags = Base::Base::StructureFlags | JSC::SlowPutArrayStorageVectorPropertiesAreReadOnly;
    static JSStaticNodeList* create(JSC::Structure* structure, JSDOMGlobalObject* globalObject, Ref<StaticNodeList>&& impl)
    {
        JSStaticNodeList* ptr = new (NotNull, JSC::allocateCell<JSStaticNodeList>(globalObject->vm())) JSStaticNodeList(structure, *globalObject, WTFMove(impl));
        ptr->finishCreation(globalObject->vm());
        return ptr;
    }

    DECLARE_INFO;

    static JSC::JSObject* createPrototype(JSC::VM&, JSDOMGlobalObject&);
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info(), JSC::NonArrayWithAlwaysSlowPutContiguous);
    }
protected:
    JSStaticNodeList(JSC::Structure*, JSDOMGlobalObject&, Ref<StaticNodeList>&&);

    void finishCreation(JSC::VM&);
};

template<> struct JSDOMWrapperConverterTraits<StaticNodeList> {
    using WrapperClass = JSStaticNodeList;
    using ToWrappedReturnType = StaticNodeList*;
};

WEBCORE_EXPORT JSC::JSValue createWrapper(JSDOMGlobalObject&, Ref<NodeList>&&);

ALWAYS_INLINE JSC::JSValue toJS(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, NodeList& nodeList)
{
    if (auto wrapper = getCachedWrapper(globalObject->world(), nodeList))
        return wrapper;
    return createWrapper(*globalObject, nodeList);
}

} // namespace WebCore
