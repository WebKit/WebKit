/*
 * Copyright (C) 2018 Igalia S.L.
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
#include "JSAPIWrapperGlobalObject.h"

#include "JSCInlines.h"
#include "JSCallbackObject.h"
#include "Structure.h"
#include <wtf/NeverDestroyed.h>

class JSAPIWrapperGlobalObjectHandleOwner final : public JSC::WeakHandleOwner {
public:
    void finalize(JSC::Handle<JSC::Unknown>, void*) final;
};

static JSAPIWrapperGlobalObjectHandleOwner* jsAPIWrapperGlobalObjectHandleOwner()
{
    static NeverDestroyed<JSAPIWrapperGlobalObjectHandleOwner> jsWrapperGlobalObjectHandleOwner;
    return &jsWrapperGlobalObjectHandleOwner.get();
}

void JSAPIWrapperGlobalObjectHandleOwner::finalize(JSC::Handle<JSC::Unknown> handle, void*)
{
    auto* wrapperObject = static_cast<JSC::JSAPIWrapperGlobalObject*>(handle.get().asCell());
    if (!wrapperObject->wrappedObject())
        return;

    JSC::Heap::heap(wrapperObject)->releaseSoon(std::unique_ptr<JSC::JSCGLibWrapperObject>(wrapperObject->wrappedObject()));
    JSC::WeakSet::deallocate(JSC::WeakImpl::asWeakImpl(handle.slot()));
}

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(callJSAPIWrapperGlobalObjectCallbackObject);
static JSC_DECLARE_HOST_FUNCTION(constructJSAPIWrapperGlobalObjectCallbackObject);
static JSC_DECLARE_CUSTOM_GETTER(callbackGetterJSAPIWrapperGlobalObjectCallbackObject);
static JSC_DECLARE_CUSTOM_GETTER(staticFunctionGetterJSAPIWrapperGlobalObjectCallbackObject);

template <> const ClassInfo JSCallbackObject<JSAPIWrapperGlobalObject>::s_info = { "JSAPIWrapperGlobalObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCallbackObject) };
template<> const bool JSCallbackObject<JSAPIWrapperGlobalObject>::needsDestruction = true;

template <>
RawNativeFunction JSCallbackObject<JSAPIWrapperGlobalObject>::getCallFunction()
{
    return callJSAPIWrapperGlobalObjectCallbackObject;
}

template <>
RawNativeFunction JSCallbackObject<JSAPIWrapperGlobalObject>::getConstructFunction()
{
    return constructJSAPIWrapperGlobalObjectCallbackObject;
}

template <>
PropertySlot::GetValueFunc JSCallbackObject<JSAPIWrapperGlobalObject>::getCallbackGetter()
{
    return callbackGetterJSAPIWrapperGlobalObjectCallbackObject;
}

template <>
PropertySlot::GetValueFunc JSCallbackObject<JSAPIWrapperGlobalObject>::getStaticFunctionGetter()
{
    return staticFunctionGetterJSAPIWrapperGlobalObjectCallbackObject;
}

template <>
IsoSubspace* JSCallbackObject<JSAPIWrapperGlobalObject>::subspaceForImpl(VM& vm, SubspaceAccess mode)
{
    switch (mode) {
    case SubspaceAccess::OnMainThread:
        return vm.callbackAPIWrapperGlobalObjectSpace<SubspaceAccess::OnMainThread>();
    case SubspaceAccess::Concurrently:
        return vm.callbackAPIWrapperGlobalObjectSpace<SubspaceAccess::Concurrently>();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

template <>
Structure* JSCallbackObject<JSAPIWrapperGlobalObject>::createStructure(VM& vm, JSGlobalObject*, JSValue proto)
{
    return Structure::create(vm, nullptr, proto, TypeInfo(GlobalObjectType, StructureFlags), &s_info);
}

template<>
JSCallbackObject<JSAPIWrapperGlobalObject>* JSCallbackObject<JSAPIWrapperGlobalObject>::create(VM& vm, JSClassRef classRef, Structure* structure)
{
    JSCallbackObject<JSAPIWrapperGlobalObject>* callbackObject = new (NotNull, allocateCell<JSCallbackObject<JSAPIWrapperGlobalObject>>(vm.heap)) JSCallbackObject(vm, classRef, structure);
    callbackObject->finishCreation(vm);
    return callbackObject;
}

JSC_DEFINE_HOST_FUNCTION(callJSAPIWrapperGlobalObjectCallbackObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSCallbackObject<JSAPIWrapperGlobalObject>::callImpl(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(constructJSAPIWrapperGlobalObjectCallbackObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSCallbackObject<JSAPIWrapperGlobalObject>::constructImpl(globalObject, callFrame);
}

JSC_DEFINE_CUSTOM_GETTER(callbackGetterJSAPIWrapperGlobalObjectCallbackObject, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    return JSCallbackObject<JSAPIWrapperGlobalObject>::callbackGetterImpl(globalObject, thisValue, propertyName);
}

JSC_DEFINE_CUSTOM_GETTER(staticFunctionGetterJSAPIWrapperGlobalObjectCallbackObject, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    return JSCallbackObject<JSAPIWrapperGlobalObject>::staticFunctionGetterImpl(globalObject, thisValue, propertyName);
}

JSAPIWrapperGlobalObject::JSAPIWrapperGlobalObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSAPIWrapperGlobalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    WeakSet::allocate(this, jsAPIWrapperGlobalObjectHandleOwner(), 0); // Balanced in JSAPIWrapperGlobalObjectHandleOwner::finalize.
}

void JSAPIWrapperGlobalObject::visitChildren(JSCell* cell, JSC::SlotVisitor& visitor)
{
    Base::visitChildren(cell, visitor);
}

} // namespace JSC
