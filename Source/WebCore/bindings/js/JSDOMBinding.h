/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2006, 2008-2009, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *  Copyright (C) 2012 Ericsson AB. All rights reserved.
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "ExceptionOr.h"
#include "JSDOMWrapperCache.h"
#include <cstddef>
#include <heap/HeapInlines.h>
#include <heap/SlotVisitorInlines.h>
#include <runtime/AuxiliaryBarrierInlines.h>
#include <runtime/IteratorOperations.h>
#include <runtime/JSArray.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSCellInlines.h>
#include <runtime/JSObjectInlines.h>
#include <runtime/JSTypedArrays.h>
#include <runtime/Lookup.h>
#include <runtime/ObjectConstructor.h>
#include <runtime/StructureInlines.h>
#include <runtime/WriteBarrier.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

// FIXME: We could make this file a lot easier to read by putting all function declarations at the top,
// and function definitions below, even for template and inline functions.

namespace JSC {
class JSFunction;
}

namespace WebCore {

class URL;

void addImpureProperty(const AtomicString&);

JSC::JSValue jsString(JSC::ExecState*, const URL&); // empty if the URL is null

JSC::JSValue jsStringOrUndefined(JSC::ExecState*, const String&); // undefined if the string is null
JSC::JSValue jsStringOrUndefined(JSC::ExecState*, const URL&); // undefined if the URL is null

// See JavaScriptCore for explanation: Should be used for any string that is already owned by another
// object, to let the engine know that collecting the JSString wrapper is unlikely to save memory.
JSC::JSValue jsOwnedStringOrNull(JSC::ExecState*, const String&);

String propertyNameToString(JSC::PropertyName);
AtomicString propertyNameToAtomicString(JSC::PropertyName);

WEBCORE_EXPORT String identifierToByteString(JSC::ExecState&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToByteString(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT String identifierToUSVString(JSC::ExecState&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToUSVString(JSC::ExecState&, JSC::JSValue);

int32_t finiteInt32Value(JSC::JSValue, JSC::ExecState*, bool& okay);

// The following functions convert values to integers as per the WebIDL specification.
// The conversion fails if the value cannot be converted to a number or, if EnforceRange is specified,
// the value is outside the range of the destination integer type.

WEBCORE_EXPORT int8_t toInt8EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint8_t toUInt8EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int16_t toInt16EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint16_t toUInt16EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int32_t toInt32EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint32_t toUInt32EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int64_t toInt64EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint64_t toUInt64EnforceRange(JSC::ExecState&, JSC::JSValue);

WEBCORE_EXPORT int8_t toInt8Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint8_t toUInt8Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int16_t toInt16Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint16_t toUInt16Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int32_t toInt32Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint32_t toUInt32Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int64_t toInt64Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint64_t toUInt64Clamp(JSC::ExecState&, JSC::JSValue);

WEBCORE_EXPORT int8_t toInt8(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint8_t toUInt8(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int16_t toInt16(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint16_t toUInt16(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int64_t toInt64(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint64_t toUInt64(JSC::ExecState&, JSC::JSValue);

JSC::JSValue jsDate(JSC::ExecState*, double value);

// NaN if the value can't be converted to a date.
double valueToDate(JSC::ExecState*, JSC::JSValue);

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, JSC::ArrayBuffer&);
JSC::JSValue toJS(JSC::ExecState*, JSC::JSGlobalObject*, JSC::ArrayBufferView&);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, JSC::ArrayBuffer*);
JSC::JSValue toJS(JSC::ExecState*, JSC::JSGlobalObject*, JSC::ArrayBufferView*);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, Ref<T>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, RefPtr<T>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const Vector<T>&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const Vector<RefPtr<T>>&);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const JSC::PrivateName&);

RefPtr<JSC::ArrayBufferView> toPossiblySharedArrayBufferView(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Int8Array> toPossiblySharedInt8Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Int16Array> toPossiblySharedInt16Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Int32Array> toPossiblySharedInt32Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint8Array> toPossiblySharedUint8Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint8ClampedArray> toPossiblySharedUint8ClampedArray(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint16Array> toPossiblySharedUint16Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint32Array> toPossiblySharedUint32Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Float32Array> toPossiblySharedFloat32Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Float64Array> toPossiblySharedFloat64Array(JSC::VM&, JSC::JSValue);

RefPtr<JSC::ArrayBufferView> toUnsharedArrayBufferView(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Int8Array> toUnsharedInt8Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Int16Array> toUnsharedInt16Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Int32Array> toUnsharedInt32Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint8Array> toUnsharedUint8Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint8ClampedArray> toUnsharedUint8ClampedArray(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint16Array> toUnsharedUint16Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Uint32Array> toUnsharedUint32Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Float32Array> toUnsharedFloat32Array(JSC::VM&, JSC::JSValue);
RefPtr<JSC::Float64Array> toUnsharedFloat64Array(JSC::VM&, JSC::JSValue);

String propertyNameToString(JSC::PropertyName);
AtomicString propertyNameToAtomicString(JSC::PropertyName);

WEBCORE_EXPORT bool hasIteratorMethod(JSC::ExecState&, JSC::JSValue);

template<JSC::NativeFunction, int length> JSC::EncodedJSValue nonCachingStaticFunctionGetter(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);


// Inline functions and template definitions.

inline int32_t finiteInt32Value(JSC::JSValue value, JSC::ExecState* exec, bool& okay)
{
    double number = value.toNumber(exec);
    okay = std::isfinite(number);
    return JSC::toInt32(number);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer& buffer)
{
    if (auto result = getCachedWrapper(globalObject->world(), buffer))
        return result;

    // The JSArrayBuffer::create function will register the wrapper in finishCreation.
    return JSC::JSArrayBuffer::create(state->vm(), globalObject->arrayBufferStructure(buffer.sharingMode()), &buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView& view)
{
    return view.wrap(state, globalObject);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer* buffer)
{
    if (!buffer)
        return JSC::jsNull();
    return toJS(state, globalObject, *buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView* view)
{
    if (!view)
        return JSC::jsNull();
    return toJS(state, globalObject, *view);
}

template<typename T> inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, Ref<T>&& ptr)
{
    return toJS(exec, globalObject, ptr.get());
}

template<typename T> inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, RefPtr<T>&& ptr)
{
    return toJS(exec, globalObject, ptr.get());
}

template<typename T> inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const Vector<T>& vector)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSC::JSArray* array = constructEmptyArray(exec, nullptr, vector.size());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    for (size_t i = 0; i < vector.size(); ++i) {
        array->putDirectIndex(exec, i, toJS(exec, globalObject, vector[i]));
        RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    }
    return array;
}

template<typename T> inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const Vector<RefPtr<T>>& vector)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSC::JSArray* array = constructEmptyArray(exec, nullptr, vector.size());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    for (size_t i = 0; i < vector.size(); ++i) {
        array->putDirectIndex(exec, i, toJS(exec, globalObject, vector[i].get()));
        RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    }
    return array;
}

inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject*, const JSC::PrivateName& privateName)
{
    return JSC::Symbol::create(exec->vm(), privateName.uid());
}

inline RefPtr<JSC::ArrayBufferView> toPossiblySharedArrayBufferView(JSC::VM& vm, JSC::JSValue value)
{
    auto* wrapper = jsDynamicDowncast<JSC::JSArrayBufferView*>(vm, value);
    if (!wrapper)
        return nullptr;
    return wrapper->possiblySharedImpl();
}

inline RefPtr<JSC::ArrayBufferView> toUnsharedArrayBufferView(JSC::VM& vm, JSC::JSValue value)
{
    auto result = toPossiblySharedArrayBufferView(vm, value);
    if (!result || result->isShared())
        return nullptr;
    return result;
}

inline RefPtr<JSC::Int8Array> toPossiblySharedInt8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int8Adaptor>(vm, value); }
inline RefPtr<JSC::Int16Array> toPossiblySharedInt16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int16Adaptor>(vm, value); }
inline RefPtr<JSC::Int32Array> toPossiblySharedInt32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int32Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8Array> toPossiblySharedUint8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8ClampedArray> toPossiblySharedUint8ClampedArray(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8ClampedAdaptor>(vm, value); }
inline RefPtr<JSC::Uint16Array> toPossiblySharedUint16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint16Adaptor>(vm, value); }
inline RefPtr<JSC::Uint32Array> toPossiblySharedUint32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint32Adaptor>(vm, value); }
inline RefPtr<JSC::Float32Array> toPossiblySharedFloat32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float32Adaptor>(vm, value); }
inline RefPtr<JSC::Float64Array> toPossiblySharedFloat64Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float64Adaptor>(vm, value); }

inline RefPtr<JSC::Int8Array> toUnsharedInt8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int8Adaptor>(vm, value); }
inline RefPtr<JSC::Int16Array> toUnsharedInt16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int16Adaptor>(vm, value); }
inline RefPtr<JSC::Int32Array> toUnsharedInt32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int32Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8Array> toUnsharedUint8Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8Adaptor>(vm, value); }
inline RefPtr<JSC::Uint8ClampedArray> toUnsharedUint8ClampedArray(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8ClampedAdaptor>(vm, value); }
inline RefPtr<JSC::Uint16Array> toUnsharedUint16Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint16Adaptor>(vm, value); }
inline RefPtr<JSC::Uint32Array> toUnsharedUint32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint32Adaptor>(vm, value); }
inline RefPtr<JSC::Float32Array> toUnsharedFloat32Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float32Adaptor>(vm, value); }
inline RefPtr<JSC::Float64Array> toUnsharedFloat64Array(JSC::VM& vm, JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float64Adaptor>(vm, value); }

inline String propertyNameToString(JSC::PropertyName propertyName)
{
    ASSERT(!propertyName.isSymbol());
    return propertyName.uid() ? propertyName.uid() : propertyName.publicName();
}

inline AtomicString propertyNameToAtomicString(JSC::PropertyName propertyName)
{
    return AtomicString(propertyName.uid() ? propertyName.uid() : propertyName.publicName());
}

template<JSC::NativeFunction nativeFunction, int length> JSC::EncodedJSValue nonCachingStaticFunctionGetter(JSC::ExecState* exec, JSC::EncodedJSValue, JSC::PropertyName propertyName)
{
    return JSC::JSValue::encode(JSC::JSFunction::create(exec->vm(), exec->lexicalGlobalObject(), length, propertyName.publicName(), nativeFunction));
}

} // namespace WebCore
