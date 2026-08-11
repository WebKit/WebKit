/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "JSTypedArrayViewConstructor.h"

#include "GetterSetter.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(constructTypedArrayView);
static JSC_DECLARE_HOST_FUNCTION(typedArrayConstructorOf);

JSTypedArrayViewConstructor::JSTypedArrayViewConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, constructTypedArrayView, constructTypedArrayView)
{
}

const ClassInfo JSTypedArrayViewConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTypedArrayViewConstructor) };

void JSTypedArrayViewConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, JSTypedArrayViewPrototype* prototype)
{
    Base::finishCreation(vm, 0, "TypedArray"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, globalObject->typedArraySpeciesGetterSetter(), PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->of, typedArrayConstructorOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->from, typedArrayConstructorFromCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    globalObject->installTypedArrayConstructorSpeciesWatchpoint(this);
}

Structure* JSTypedArrayViewConstructor::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

JSC_DEFINE_HOST_FUNCTION(constructTypedArrayView, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "%TypedArray% should not be called directly"_s);
}

template<typename ViewClass>
static ALWAYS_INLINE void typedArrayOfSetElements(JSGlobalObject* globalObject, ViewClass* result, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned length = callFrame->argumentCount();
    for (unsigned index = 0; index < length; ++index) {
        result->setIndex(globalObject, index, callFrame->uncheckedArgument(index));
        RETURN_IF_EXCEPTION(scope, void());
    }
}

template<typename ViewClass>
static ALWAYS_INLINE ViewClass* typedArrayOfFast(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned length = callFrame->argumentCount();

    Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, /* isResizableOrGrowableShared */ false);
    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    typedArrayOfSetElements<ViewClass>(globalObject, result, callFrame);
    return result;
}

// https://tc39.es/ecma262/#sec-%typedarray%.of
JSC_DEFINE_HOST_FUNCTION(typedArrayConstructorOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t length = callFrame->argumentCount();
    JSValue thisValue = callFrame->thisValue();

    if (!thisValue.isConstructor()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "TypedArray.of requires |this| to be a constructor"_s);

#define JSC_TYPED_ARRAY_OF_FAST(name) \
    if (thisValue == globalObject->typedArrayConstructorConcurrently(Type##name)) \
        RELEASE_AND_RETURN(scope, JSValue::encode(typedArrayOfFast<JS##name##Array>(globalObject, callFrame)));
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_TYPED_ARRAY_OF_FAST)
#undef JSC_TYPED_ARRAY_OF_FAST

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());
    JSObject* constructed = construct(globalObject, thisValue, args, "TypedArray.of requires |this| to be a constructor"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSArrayBufferView* view = validateTypedArray(globalObject, constructed);
    RETURN_IF_EXCEPTION(scope, { });
    if (view->length() < length) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "TypedArray.of constructed typed array of insufficient length"_s);

    switch (view->type()) {
#define JSC_TYPED_ARRAY_OF_SET(name) \
    case name##ArrayType: { \
        scope.release(); \
        typedArrayOfSetElements(globalObject, uncheckedDowncast<JS##name##Array>(view), callFrame); \
        return JSValue::encode(view); \
    }
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_TYPED_ARRAY_OF_SET)
#undef JSC_TYPED_ARRAY_OF_SET
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }
}

} // namespace JSC
