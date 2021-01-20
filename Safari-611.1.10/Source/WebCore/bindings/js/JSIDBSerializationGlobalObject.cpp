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

#include "config.h"
#include "JSIDBSerializationGlobalObject.h"

#include "WebCoreJSClientData.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

using namespace JSC;

const ClassInfo JSIDBSerializationGlobalObject::s_info = { "JSIDBSerializationGlobalObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSIDBSerializationGlobalObject) };

inline JSIDBSerializationGlobalObject::JSIDBSerializationGlobalObject(VM& vm, Structure* structure, Ref<DOMWrapperWorld>&& impl)
    : Base(vm, structure, WTFMove(impl))
    , m_scriptExecutionContext(EmptyScriptExecutionContext::create(vm))
{
}

JSIDBSerializationGlobalObject* JSIDBSerializationGlobalObject::create(VM& vm, Structure* structure, Ref<DOMWrapperWorld>&& impl)
{
    JSIDBSerializationGlobalObject* ptr =  new (NotNull, allocateCell<JSIDBSerializationGlobalObject>(vm.heap)) JSIDBSerializationGlobalObject(vm, structure, WTFMove(impl));
    ptr->finishCreation(vm);
    return ptr;
}

void JSIDBSerializationGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}

IsoSubspace* JSIDBSerializationGlobalObject::subspaceForImpl(VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->idbSerializationSpace();
}

void JSIDBSerializationGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSIDBSerializationGlobalObject*>(cell)->JSIDBSerializationGlobalObject::~JSIDBSerializationGlobalObject();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

