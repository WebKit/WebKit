/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSFileSystemDirectoryHandleIterator.h"

#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "JSDOMOperation.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>

namespace WebCore {

using namespace JSC;

static JSC_DECLARE_HOST_FUNCTION(jsFileSystemDirectoryHandleIterator_onPromiseSettled);
static JSC_DECLARE_HOST_FUNCTION(jsFileSystemDirectoryHandleIterator_onPromiseFulfilled);
static JSC_DECLARE_HOST_FUNCTION(jsFileSystemDirectoryHandleIterator_onPromiseRejected);

JSC_ANNOTATE_HOST_FUNCTION(JSFileSystemDirectoryHandleIteratorPrototypeNext, JSFileSystemDirectoryHandleIteratorPrototype::next);

template<>
const JSC::ClassInfo JSFileSystemDirectoryHandleIteratorBase::s_info = { "Directory Handle Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFileSystemDirectoryHandleIteratorBase) };
const JSC::ClassInfo JSFileSystemDirectoryHandleIterator::s_info = { "Directory Handle Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFileSystemDirectoryHandleIterator) };

template<>
const JSC::ClassInfo JSFileSystemDirectoryHandleIteratorPrototype::s_info = { "Directory Handle Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFileSystemDirectoryHandleIteratorPrototype) };

GCClient::IsoSubspace* JSFileSystemDirectoryHandleIterator::subspaceForImpl(VM& vm)
{
    JSLockHolder apiLocker(vm);
    return WebCore::subspaceForImpl<JSFileSystemDirectoryHandleIterator, UseCustomHeapCellType::No>(vm,
        [] (auto& spaces) { return spaces.m_clientSubspaceForFileSystemDirectoryHandleIterator.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_clientSubspaceForFileSystemDirectoryHandleIterator = WTFMove(space); },
        [] (auto& spaces) { return spaces.m_subspaceForFileSystemDirectoryHandleIterator.get(); },
        [] (auto& spaces, auto&& space) { spaces.m_subspaceForFileSystemDirectoryHandleIterator = WTFMove(space); }
    );
}

inline JSC::EncodedJSValue jsFileSystemDirectoryHandleIterator_onPromiseSettledBody(JSGlobalObject* globalObject, JSC::CallFrame*, JSFileSystemDirectoryHandleIterator* castedThis)
{
    return castedThis->settle(globalObject);
}

JSC_DEFINE_HOST_FUNCTION(jsFileSystemDirectoryHandleIterator_onPromiseSettled, (JSGlobalObject* globalObject, JSC::CallFrame* callFrame))
{
    return IDLOperation<JSFileSystemDirectoryHandleIterator>::call<jsFileSystemDirectoryHandleIterator_onPromiseSettledBody>(*globalObject, *callFrame, "onPromiseSettled");
}

JSBoundFunction* JSFileSystemDirectoryHandleIterator::createOnSettledFunction(JSC::JSGlobalObject* globalObject)
{
    JSC::VM& vm = globalObject->vm();
    auto onSettled = JSC::JSFunction::create(vm, globalObject, 0, String(), jsFileSystemDirectoryHandleIterator_onPromiseSettled, ImplementationVisibility::Public);
    return JSC::JSBoundFunction::create(vm, globalObject, onSettled, this, nullptr, 1, nullptr);
}

inline JSC::EncodedJSValue jsFileSystemDirectoryHandleIterator_onPromiseFulfilledBody(JSGlobalObject* globalObject, JSC::CallFrame* callFrame, JSFileSystemDirectoryHandleIterator* castedThis)
{
    return castedThis->fulfill(globalObject, callFrame->argument(0));
}

JSC_DEFINE_HOST_FUNCTION(jsFileSystemDirectoryHandleIterator_onPromiseFulfilled, (JSGlobalObject* globalObject, JSC::CallFrame* callFrame))
{
    return IDLOperation<JSFileSystemDirectoryHandleIterator>::call<jsFileSystemDirectoryHandleIterator_onPromiseFulfilledBody>(*globalObject, *callFrame, "onPromiseFulfilled");
}

JSBoundFunction* JSFileSystemDirectoryHandleIterator::createOnFulfilledFunction(JSC::JSGlobalObject* globalObject)
{
    JSC::VM& vm = globalObject->vm();
    auto onFulfilled = JSC::JSFunction::create(vm, globalObject, 1, String(), jsFileSystemDirectoryHandleIterator_onPromiseFulfilled, ImplementationVisibility::Public);
    return JSC::JSBoundFunction::create(vm, globalObject, onFulfilled, this, nullptr, 1, nullptr);
}

inline JSC::EncodedJSValue jsFileSystemDirectoryHandleIterator_onPromiseRejectedBody(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame, JSFileSystemDirectoryHandleIterator* castedThis)
{
    return castedThis->reject(globalObject, callFrame->argument(0));
}

JSC_DEFINE_HOST_FUNCTION(jsFileSystemDirectoryHandleIterator_onPromiseRejected, (JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame))
{
    return IDLOperation<JSFileSystemDirectoryHandleIterator>::call<jsFileSystemDirectoryHandleIterator_onPromiseRejectedBody>(*globalObject, *callFrame, "onPromiseRejected");
}

JSBoundFunction* JSFileSystemDirectoryHandleIterator::createOnRejectedFunction(JSC::JSGlobalObject* globalObject)
{
    JSC::VM& vm = globalObject->vm();
    auto onSettled = JSC::JSFunction::create(vm, globalObject, 0, String(), jsFileSystemDirectoryHandleIterator_onPromiseRejected, ImplementationVisibility::Public);
    return JSC::JSBoundFunction::create(vm, globalObject, onSettled, this, nullptr, 1, nullptr);
}

} // namespace WebCore
