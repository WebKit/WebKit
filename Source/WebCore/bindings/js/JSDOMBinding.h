/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2013 Apple Inc. All rights reserved.
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

#include "JSDOMGlobalObject.h"
#include "JSDOMWrapper.h"
#include "DOMWrapperWorld.h"
#include "ScriptWrappable.h"
#include "ScriptWrappableInlines.h"
#include "WebCoreTypedArrayController.h"
#include <cstddef>
#include <heap/HeapInlines.h>
#include <heap/SlotVisitorInlines.h>
#include <heap/Weak.h>
#include <heap/WeakInlines.h>
#include <runtime/AuxiliaryBarrierInlines.h>
#include <runtime/Error.h>
#include <runtime/IteratorOperations.h>
#include <runtime/JSArray.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSCellInlines.h>
#include <runtime/JSObjectInlines.h>
#include <runtime/JSTypedArrays.h>
#include <runtime/Lookup.h>
#include <runtime/ObjectConstructor.h>
#include <runtime/StructureInlines.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/TypedArrays.h>
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

class CachedScript;
class DOMWindow;
class Frame;
class URL;
class Node;

struct ExceptionCodeWithMessage;

typedef int ExceptionCode;

struct ExceptionDetails {
    String message;
    int lineNumber { 0 };
    int columnNumber { 0 };
    String sourceURL;
};

// Base class for all constructor objects in the JSC bindings.
class DOMConstructorObject : public JSDOMObject {
public:
    typedef JSDOMObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | JSC::ImplementsHasInstance | JSC::ImplementsDefaultHasInstance | JSC::TypeOfShouldCallGetCallData;
    static JSC::Structure* createStructure(JSC::VM&, JSC::JSGlobalObject*, JSC::JSValue);

protected:
    DOMConstructorObject(JSC::Structure*, JSDOMGlobalObject&);

    static String className(const JSObject*);
    static JSC::CallType getCallData(JSCell*, JSC::CallData&);
};

class DOMConstructorJSBuiltinObject : public DOMConstructorObject {
public:
    typedef DOMConstructorObject Base;

protected:
    DOMConstructorJSBuiltinObject(JSC::Structure*, JSDOMGlobalObject&);
    static void visitChildren(JSC::JSCell*, JSC::SlotVisitor&);

    JSC::JSFunction* initializeFunction();
    void setInitializeFunction(JSC::VM&, JSC::JSFunction&);

private:
    JSC::WriteBarrier<JSC::JSFunction> m_initializeFunction;
};

DOMWindow& callerDOMWindow(JSC::ExecState*);
DOMWindow& activeDOMWindow(JSC::ExecState*);
DOMWindow& firstDOMWindow(JSC::ExecState*);

WEBCORE_EXPORT JSC::EncodedJSValue reportDeprecatedGetterError(JSC::ExecState&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT void reportDeprecatedSetterError(JSC::ExecState&, const char* interfaceName, const char* attributeName);

void throwNotSupportedError(JSC::ExecState&, JSC::ThrowScope&, const char* message);
void throwInvalidStateError(JSC::ExecState&, JSC::ThrowScope&, const char* message);
void throwArrayElementTypeError(JSC::ExecState&, JSC::ThrowScope&);
void throwAttributeTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName, const char* expectedType);
WEBCORE_EXPORT void throwSequenceTypeError(JSC::ExecState&, JSC::ThrowScope&);
WEBCORE_EXPORT bool throwSetterTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT void throwNonFiniteTypeError(JSC::ExecState&, JSC::ThrowScope&);
void throwSecurityError(JSC::ExecState&, JSC::ThrowScope&, const String& message);

WEBCORE_EXPORT JSC::EncodedJSValue throwArgumentMustBeEnumError(JSC::ExecState&, JSC::ThrowScope&, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedValues);
JSC::EncodedJSValue throwArgumentMustBeFunctionError(JSC::ExecState&, JSC::ThrowScope&, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName);
WEBCORE_EXPORT JSC::EncodedJSValue throwArgumentTypeError(JSC::ExecState&, JSC::ThrowScope&, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedType);
JSC::EncodedJSValue throwConstructorScriptExecutionContextUnavailableError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName);

String makeGetterTypeErrorMessage(const char* interfaceName, const char* attributeName);
String makeThisTypeErrorMessage(const char* interfaceName, const char* attributeName);

WEBCORE_EXPORT JSC::EncodedJSValue throwGetterTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT JSC::EncodedJSValue throwThisTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* functionName);

WEBCORE_EXPORT JSC::Structure* getCachedDOMStructure(JSDOMGlobalObject&, const JSC::ClassInfo*);
WEBCORE_EXPORT JSC::Structure* cacheDOMStructure(JSDOMGlobalObject&, JSC::Structure*, const JSC::ClassInfo*);

template<typename WrapperClass> JSC::Structure* getDOMStructure(JSC::VM&, JSDOMGlobalObject&);
template<typename WrapperClass> JSC::Structure* deprecatedGetDOMStructure(JSC::ExecState*);
template<typename WrapperClass> JSC::JSObject* getDOMPrototype(JSC::VM&, JSC::JSGlobalObject*);

void callFunctionWithCurrentArguments(JSC::ExecState&, JSC::JSObject& thisObject, JSC::JSFunction&);

template<typename JSClass> JSC::EncodedJSValue createJSBuiltin(JSC::ExecState&, JSC::JSFunction& initializeFunction, JSDOMGlobalObject&);

JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld&, JSC::ArrayBuffer*);
void* wrapperKey(JSC::ArrayBuffer*);

JSDOMObject* getInlineCachedWrapper(DOMWrapperWorld&, void*);
JSDOMObject* getInlineCachedWrapper(DOMWrapperWorld&, ScriptWrappable*);
JSC::JSArrayBuffer* getInlineCachedWrapper(DOMWrapperWorld&, JSC::ArrayBuffer*);

bool setInlineCachedWrapper(DOMWrapperWorld&, void*, JSDOMObject*, JSC::WeakHandleOwner*);
bool setInlineCachedWrapper(DOMWrapperWorld&, ScriptWrappable*, JSDOMObject* wrapper, JSC::WeakHandleOwner* wrapperOwner);
bool setInlineCachedWrapper(DOMWrapperWorld&, JSC::ArrayBuffer*, JSC::JSArrayBuffer* wrapper, JSC::WeakHandleOwner* wrapperOwner);

bool clearInlineCachedWrapper(DOMWrapperWorld&, void*, JSDOMObject*);
bool clearInlineCachedWrapper(DOMWrapperWorld&, ScriptWrappable*, JSDOMObject* wrapper);
bool clearInlineCachedWrapper(DOMWrapperWorld&, JSC::ArrayBuffer*, JSC::JSArrayBuffer* wrapper);

template<typename DOMClass, typename T> Ref<DOMClass> inline castDOMObjectForWrapperCreation(Ref<T>&& object) { return static_reference_cast<DOMClass>(WTFMove(object)); }
#define CREATE_DOM_WRAPPER(globalObject, className, object) createWrapper<JS##className, className>(globalObject, castDOMObjectForWrapperCreation<className>(object))

template<typename DOMClass> JSC::JSObject* getCachedWrapper(DOMWrapperWorld&, DOMClass&);
template<typename DOMClass> inline JSC::JSObject* getCachedWrapper(DOMWrapperWorld& world, Ref<DOMClass>& object) { return getCachedWrapper(world, object.get()); }
template<typename DOMClass, typename WrapperClass> void cacheWrapper(DOMWrapperWorld&, DOMClass*, WrapperClass*);
template<typename DOMClass, typename WrapperClass> void uncacheWrapper(DOMWrapperWorld&, DOMClass*, WrapperClass*);
template<typename WrapperClass, typename DOMClass> WrapperClass* createWrapper(JSDOMGlobalObject*, Ref<DOMClass>&&);

template<typename DOMClass> JSC::JSValue wrap(JSC::ExecState*, JSDOMGlobalObject*, DOMClass&);

void addImpureProperty(const AtomicString&);

const JSC::HashTable& getHashTableForGlobalData(JSC::VM&, const JSC::HashTable& staticTable);

WEBCORE_EXPORT void reportException(JSC::ExecState*, JSC::JSValue exception, CachedScript* = nullptr);
WEBCORE_EXPORT void reportException(JSC::ExecState*, JSC::Exception*, CachedScript* = nullptr, ExceptionDetails* = nullptr);
void reportCurrentException(JSC::ExecState*);

JSC::JSValue createDOMException(JSC::ExecState*, ExceptionCode);
JSC::JSValue createDOMException(JSC::ExecState*, ExceptionCode, const String&);

// Convert a DOM implementation exception code into a JavaScript exception in the execution state.
WEBCORE_EXPORT void setDOMException(JSC::ExecState*, ExceptionCode);
void setDOMException(JSC::ExecState*, const ExceptionCodeWithMessage&);

JSC::JSValue jsString(JSC::ExecState*, const URL&); // empty if the URL is null

JSC::JSValue jsStringOrNull(JSC::ExecState*, const String&); // null if the string is null
JSC::JSValue jsStringOrNull(JSC::ExecState*, const URL&); // null if the URL is null

JSC::JSValue jsStringOrUndefined(JSC::ExecState*, const String&); // undefined if the string is null
JSC::JSValue jsStringOrUndefined(JSC::ExecState*, const URL&); // undefined if the URL is null

// See JavaScriptCore for explanation: Should be used for any string that is already owned by another
// object, to let the engine know that collecting the JSString wrapper is unlikely to save memory.
JSC::JSValue jsOwnedStringOrNull(JSC::ExecState*, const String&);

String propertyNameToString(JSC::PropertyName);

AtomicString propertyNameToAtomicString(JSC::PropertyName);

String valueToStringTreatingNullAsEmptyString(JSC::ExecState*, JSC::JSValue);
String valueToStringWithUndefinedOrNullCheck(JSC::ExecState*, JSC::JSValue); // null if the value is null or undefined

WEBCORE_EXPORT String valueToUSVString(JSC::ExecState*, JSC::JSValue);
String valueToUSVStringTreatingNullAsEmptyString(JSC::ExecState*, JSC::JSValue);
String valueToUSVStringWithUndefinedOrNullCheck(JSC::ExecState*, JSC::JSValue);

template<typename T> JSC::JSValue toNullableJSNumber(Optional<T>); // null if the optional is null

int32_t finiteInt32Value(JSC::JSValue, JSC::ExecState*, bool& okay);

// The following functions convert values to integers as per the WebIDL specification.
// The conversion fails if the value cannot be converted to a number or, if EnforceRange is specified,
// the value is outside the range of the destination integer type.

enum IntegerConversionConfiguration { NormalConversion, EnforceRange, Clamp };

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

// Returns a Date instance for the specified value, or NaN if the date is not a number.
JSC::JSValue jsDateOrNaN(JSC::ExecState*, double);

// Returns a Date instance for the specified value, or null if the value is NaN or infinity.
JSC::JSValue jsDateOrNull(JSC::ExecState*, double);

// NaN if the value can't be converted to a date.
double valueToDate(JSC::ExecState*, JSC::JSValue);

// Validates that the passed object is a sequence type per section 4.1.13 of the WebIDL spec.
JSC::JSObject* toJSSequence(JSC::ExecState&, JSC::JSValue, unsigned& length);

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, JSC::ArrayBuffer*);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, JSC::ArrayBufferView*);
JSC::JSValue toJS(JSC::ExecState*, JSC::JSGlobalObject*, JSC::ArrayBufferView*);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, Ref<T>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, RefPtr<T>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const Vector<T>&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const Vector<RefPtr<T>>&);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const String&);

JSC::JSValue toJSIterator(JSC::ExecState&, JSDOMGlobalObject&, JSC::JSValue);
template<typename T> JSC::JSValue toJSIterator(JSC::ExecState&, JSDOMGlobalObject&, const T&);

JSC::JSValue toJSIteratorEnd(JSC::ExecState&);

template<typename T, size_t inlineCapacity> JSC::JSValue jsArray(JSC::ExecState*, JSDOMGlobalObject*, const Vector<T, inlineCapacity>&);
template<typename T, size_t inlineCapacity> JSC::JSValue jsArray(JSC::ExecState*, JSDOMGlobalObject*, const Vector<T, inlineCapacity>*);
template<typename T, size_t inlineCapacity> JSC::JSValue jsFrozenArray(JSC::ExecState*, JSDOMGlobalObject*, const Vector<T, inlineCapacity>&);

JSC::JSValue jsPair(JSC::ExecState&, JSDOMGlobalObject*, JSC::JSValue, JSC::JSValue);
template<typename FirstType, typename SecondType> JSC::JSValue jsPair(JSC::ExecState&, JSDOMGlobalObject*, const FirstType&, const SecondType&);

RefPtr<JSC::ArrayBufferView> toArrayBufferView(JSC::JSValue);
RefPtr<JSC::Int8Array> toInt8Array(JSC::JSValue);
RefPtr<JSC::Int16Array> toInt16Array(JSC::JSValue);
RefPtr<JSC::Int32Array> toInt32Array(JSC::JSValue);
RefPtr<JSC::Uint8Array> toUint8Array(JSC::JSValue);
RefPtr<JSC::Uint8ClampedArray> toUint8ClampedArray(JSC::JSValue);
RefPtr<JSC::Uint16Array> toUint16Array(JSC::JSValue);
RefPtr<JSC::Uint32Array> toUint32Array(JSC::JSValue);
RefPtr<JSC::Float32Array> toFloat32Array(JSC::JSValue);
RefPtr<JSC::Float64Array> toFloat64Array(JSC::JSValue);

template<typename T, typename JSType> Vector<RefPtr<T>> toRefPtrNativeArray(JSC::ExecState*, JSC::JSValue);
template<typename T> Vector<T> toNativeArray(JSC::ExecState&, JSC::JSValue);
template<typename T> Vector<T> toNativeArguments(JSC::ExecState&, size_t startIndex = 0);
bool hasIteratorMethod(JSC::ExecState&, JSC::JSValue);

bool shouldAllowAccessToNode(JSC::ExecState*, Node*);
bool shouldAllowAccessToFrame(JSC::ExecState*, Frame*);
bool shouldAllowAccessToFrame(JSC::ExecState*, Frame*, String& message);
bool shouldAllowAccessToDOMWindow(JSC::ExecState*, DOMWindow&, String& message);

enum SecurityReportingOption {
    DoNotReportSecurityError,
    LogSecurityError, // Legacy behavior.
    ThrowSecurityError
};

class BindingSecurity {
public:
    static bool shouldAllowAccessToNode(JSC::ExecState*, Node*);
    static bool shouldAllowAccessToDOMWindow(JSC::ExecState*, DOMWindow&, SecurityReportingOption = LogSecurityError);
    static bool shouldAllowAccessToFrame(JSC::ExecState*, Frame*, SecurityReportingOption = LogSecurityError);
};

void printErrorMessageForFrame(Frame*, const String& message);

String propertyNameToString(JSC::PropertyName);
AtomicString propertyNameToAtomicString(JSC::PropertyName);

template<JSC::NativeFunction, int length> JSC::EncodedJSValue nonCachingStaticFunctionGetter(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);

// Inline functions and template definitions.

inline JSC::Structure* DOMConstructorObject::createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
{
    return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
}

inline DOMConstructorObject::DOMConstructorObject(JSC::Structure* structure, JSDOMGlobalObject& globalObject)
    : JSDOMObject(structure, globalObject)
{
}

inline String DOMConstructorObject::className(const JSObject*)
{
    return ASCIILiteral("Function");
}

inline DOMConstructorJSBuiltinObject::DOMConstructorJSBuiltinObject(JSC::Structure* structure, JSDOMGlobalObject& globalObject)
    : DOMConstructorObject(structure, globalObject)
{
}

inline JSC::JSFunction* DOMConstructorJSBuiltinObject::initializeFunction()
{
    return m_initializeFunction.get();
}

inline void DOMConstructorJSBuiltinObject::setInitializeFunction(JSC::VM& vm, JSC::JSFunction& function)
{
    m_initializeFunction.set(vm, this, &function);
}

inline JSDOMGlobalObject* deprecatedGlobalObjectForPrototype(JSC::ExecState* exec)
{
    // FIXME: Callers to this function should be using the global object
    // from which the object is being created, instead of assuming the lexical one.
    // e.g. subframe.document.body should use the subframe's global object, not the lexical one.
    return JSC::jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
}

template<typename WrapperClass> inline JSC::Structure* getDOMStructure(JSC::VM& vm, JSDOMGlobalObject& globalObject)
{
    if (JSC::Structure* structure = getCachedDOMStructure(globalObject, WrapperClass::info()))
        return structure;
    return cacheDOMStructure(globalObject, WrapperClass::createStructure(vm, &globalObject, WrapperClass::createPrototype(vm, &globalObject)), WrapperClass::info());
}

template<typename WrapperClass> inline JSC::Structure* deprecatedGetDOMStructure(JSC::ExecState* exec)
{
    // FIXME: This function is wrong. It uses the wrong global object for creating the prototype structure.
    return getDOMStructure<WrapperClass>(exec->vm(), *deprecatedGlobalObjectForPrototype(exec));
}

template<typename WrapperClass> inline JSC::JSObject* getDOMPrototype(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
{
    return JSC::jsCast<JSC::JSObject*>(asObject(getDOMStructure<WrapperClass>(vm, *JSC::jsCast<JSDOMGlobalObject*>(globalObject))->storedPrototype()));
}

template<typename JSClass> inline JSC::EncodedJSValue createJSBuiltin(JSC::ExecState& state, JSC::JSFunction& initializeFunction, JSDOMGlobalObject& globalObject)
{
    JSC::JSObject* object = JSClass::create(getDOMStructure<JSClass>(globalObject.vm(), globalObject), &globalObject);
    callFunctionWithCurrentArguments(state, *object, initializeFunction);
    return JSC::JSValue::encode(object);
}

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld& world, JSC::ArrayBuffer*)
{
    return static_cast<WebCoreTypedArrayController*>(world.vm().m_typedArrayController.get())->wrapperOwner();
}

inline void* wrapperKey(JSC::ArrayBuffer* domObject)
{
    return domObject;
}

inline JSDOMObject* getInlineCachedWrapper(DOMWrapperWorld&, void*) { return nullptr; }
inline bool setInlineCachedWrapper(DOMWrapperWorld&, void*, JSDOMObject*, JSC::WeakHandleOwner*) { return false; }
inline bool clearInlineCachedWrapper(DOMWrapperWorld&, void*, JSDOMObject*) { return false; }

inline JSDOMObject* getInlineCachedWrapper(DOMWrapperWorld& world, ScriptWrappable* domObject)
{
    if (!world.isNormal())
        return nullptr;
    return domObject->wrapper();
}

inline JSC::JSArrayBuffer* getInlineCachedWrapper(DOMWrapperWorld& world, JSC::ArrayBuffer* buffer)
{
    if (!world.isNormal())
        return nullptr;
    return buffer->m_wrapper.get();
}

inline bool setInlineCachedWrapper(DOMWrapperWorld& world, ScriptWrappable* domObject, JSDOMObject* wrapper, JSC::WeakHandleOwner* wrapperOwner)
{
    if (!world.isNormal())
        return false;
    domObject->setWrapper(wrapper, wrapperOwner, &world);
    return true;
}

inline bool setInlineCachedWrapper(DOMWrapperWorld& world, JSC::ArrayBuffer* domObject, JSC::JSArrayBuffer* wrapper, JSC::WeakHandleOwner* wrapperOwner)
{
    if (!world.isNormal())
        return false;
    domObject->m_wrapper = JSC::Weak<JSC::JSArrayBuffer>(wrapper, wrapperOwner, &world);
    return true;
}

inline bool clearInlineCachedWrapper(DOMWrapperWorld& world, ScriptWrappable* domObject, JSDOMObject* wrapper)
{
    if (!world.isNormal())
        return false;
    domObject->clearWrapper(wrapper);
    return true;
}

inline bool clearInlineCachedWrapper(DOMWrapperWorld& world, JSC::ArrayBuffer* domObject, JSC::JSArrayBuffer* wrapper)
{
    if (!world.isNormal())
        return false;
    weakClear(domObject->m_wrapper, wrapper);
    return true;
}

template<typename DOMClass> inline JSC::JSObject* getCachedWrapper(DOMWrapperWorld& world, DOMClass& domObject)
{
    if (auto* wrapper = getInlineCachedWrapper(world, &domObject))
        return wrapper;
    return world.m_wrappers.get(wrapperKey(&domObject));
}

template<typename DOMClass, typename WrapperClass> inline void cacheWrapper(DOMWrapperWorld& world, DOMClass* domObject, WrapperClass* wrapper)
{
    JSC::WeakHandleOwner* owner = wrapperOwner(world, domObject);
    if (setInlineCachedWrapper(world, domObject, wrapper, owner))
        return;
    weakAdd(world.m_wrappers, wrapperKey(domObject), JSC::Weak<JSC::JSObject>(wrapper, owner, &world));
}

template<typename DOMClass, typename WrapperClass> inline void uncacheWrapper(DOMWrapperWorld& world, DOMClass* domObject, WrapperClass* wrapper)
{
    if (clearInlineCachedWrapper(world, domObject, wrapper))
        return;
    weakRemove(world.m_wrappers, wrapperKey(domObject), wrapper);
}

template<typename WrapperClass, typename DOMClass> inline WrapperClass* createWrapper(JSDOMGlobalObject* globalObject, Ref<DOMClass>&& node)
{
    ASSERT(!getCachedWrapper(globalObject->world(), node));
    auto* nodePtr = node.ptr();
    WrapperClass* wrapper = WrapperClass::create(getDOMStructure<WrapperClass>(globalObject->vm(), *globalObject), globalObject, WTFMove(node));
    cacheWrapper(globalObject->world(), nodePtr, wrapper);
    return wrapper;
}

template<typename DOMClass> inline JSC::JSValue wrap(JSC::ExecState* state, JSDOMGlobalObject* globalObject, DOMClass& domObject)
{
    if (auto* wrapper = getCachedWrapper(globalObject->world(), domObject))
        return wrapper;
    return toJSNewlyCreated(state, globalObject, Ref<DOMClass>(domObject));
}

inline int32_t finiteInt32Value(JSC::JSValue value, JSC::ExecState* exec, bool& okay)
{
    double number = value.toNumber(exec);
    okay = std::isfinite(number);
    return JSC::toInt32(number);
}

// Validates that the passed object is a sequence type per section 4.1.13 of the WebIDL spec.
inline JSC::JSObject* toJSSequence(JSC::ExecState& exec, JSC::JSValue value, unsigned& length)
{
    JSC::VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSC::JSObject* object = value.getObject();
    if (!object) {
        throwSequenceTypeError(exec, scope);
        return nullptr;
    }

    JSC::JSValue lengthValue = object->get(&exec, exec.propertyNames().length);
    if (exec.hadException())
        return nullptr;

    if (lengthValue.isUndefinedOrNull()) {
        throwSequenceTypeError(exec, scope);
        return nullptr;
    }

    length = lengthValue.toUInt32(&exec);
    if (exec.hadException())
        return nullptr;

    return object;
}

inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, JSC::ArrayBuffer* buffer)
{
    if (!buffer)
        return JSC::jsNull();
    if (JSC::JSValue result = getCachedWrapper(globalObject->world(), *buffer))
        return result;

    // The JSArrayBuffer::create function will register the wrapper in finishCreation.
    return JSC::JSArrayBuffer::create(exec->vm(), globalObject->arrayBufferStructure(), buffer);
}

inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, JSC::ArrayBufferView* view)
{
    if (!view)
        return JSC::jsNull();
    return view->wrap(exec, globalObject);
}

inline JSC::JSValue toJS(JSC::ExecState* exec, JSC::JSGlobalObject* globalObject, JSC::ArrayBufferView* view)
{
    if (!view)
        return JSC::jsNull();
    return view->wrap(exec, globalObject);
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
    JSC::JSArray* array = constructEmptyArray(exec, nullptr, vector.size());
    if (UNLIKELY(exec->hadException()))
        return JSC::jsUndefined();
    for (size_t i = 0; i < vector.size(); ++i)
        array->putDirectIndex(exec, i, toJS(exec, globalObject, vector[i]));
    return array;
}

template<typename T> inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const Vector<RefPtr<T>>& vector)
{
    JSC::JSArray* array = constructEmptyArray(exec, nullptr, vector.size());
    if (UNLIKELY(exec->hadException()))
        return JSC::jsUndefined();
    for (size_t i = 0; i < vector.size(); ++i)
        array->putDirectIndex(exec, i, toJS(exec, globalObject, vector[i].get()));
    return array;
}

inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject*, const String& value)
{
    return jsStringOrNull(exec, value);
}

inline JSC::JSValue toJSIterator(JSC::ExecState& state, JSDOMGlobalObject&, JSC::JSValue value)
{
    return createIteratorResultObject(&state, value, false);
}

template<typename T> inline JSC::JSValue toJSIterator(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const T& value)
{
    return createIteratorResultObject(&state, toJS(&state, &globalObject, value), false);
}

inline JSC::JSValue toJSIteratorEnd(JSC::ExecState& state)
{
    return createIteratorResultObject(&state, JSC::jsUndefined(), true);
}

template<typename T> struct JSValueTraits {
    static JSC::JSValue arrayJSValue(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const T& value)
    {
        return toJS(exec, globalObject, WTF::getPtr(value));
    }
};

template<> struct JSValueTraits<String> {
    static JSC::JSValue arrayJSValue(JSC::ExecState* exec, JSDOMGlobalObject*, const String& value)
    {
        return JSC::jsStringWithCache(exec, value);
    }
};

template<> struct JSValueTraits<double> {
    static JSC::JSValue arrayJSValue(JSC::ExecState*, JSDOMGlobalObject*, const double& value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct JSValueTraits<float> {
    static JSC::JSValue arrayJSValue(JSC::ExecState*, JSDOMGlobalObject*, const float& value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct JSValueTraits<unsigned long> {
    static JSC::JSValue arrayJSValue(JSC::ExecState*, JSDOMGlobalObject*, const unsigned long& value)
    {
        return JSC::jsNumber(value);
    }
};

template<typename T, size_t inlineCapacity> JSC::JSValue jsArray(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const Vector<T, inlineCapacity>& vector)
{
    JSC::MarkedArgumentBuffer list;
    for (auto& element : vector)
        list.append(JSValueTraits<T>::arrayJSValue(exec, globalObject, element));
    return JSC::constructArray(exec, nullptr, globalObject, list);
}

template<typename T, size_t inlineCapacity> inline JSC::JSValue jsArray(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const Vector<T, inlineCapacity>* vector)
{
    if (!vector)
        return JSC::constructEmptyArray(exec, nullptr, globalObject, 0);
    return jsArray(exec, globalObject, *vector);
}

template<typename T, size_t inlineCapacity> JSC::JSValue jsFrozenArray(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, const Vector<T, inlineCapacity>& vector)
{
    JSC::MarkedArgumentBuffer list;
    for (auto& element : vector) {
        list.append(JSValueTraits<T>::arrayJSValue(exec, globalObject, element));
        if (UNLIKELY(exec->hadException()))
            return JSC::jsUndefined();
    }
    auto* array = JSC::constructArray(exec, nullptr, globalObject, list);
    if (UNLIKELY(exec->hadException()))
        return JSC::jsUndefined();
    return JSC::objectConstructorFreeze(exec, array);
}

inline JSC::JSValue jsPair(JSC::ExecState& state, JSDOMGlobalObject* globalObject, JSC::JSValue value1, JSC::JSValue value2)
{
    JSC::MarkedArgumentBuffer args;
    args.append(value1);
    args.append(value2);
    return constructArray(&state, 0, globalObject, args);
}

template<typename FirstType, typename SecondType> inline JSC::JSValue jsPair(JSC::ExecState& state, JSDOMGlobalObject* globalObject, const FirstType& value1, const SecondType& value2)
{
    return jsPair(state, globalObject, toJS(&state, globalObject, value1), toJS(&state, globalObject, value2));
}

inline RefPtr<JSC::ArrayBufferView> toArrayBufferView(JSC::JSValue value)
{
    auto* wrapper = JSC::jsDynamicCast<JSC::JSArrayBufferView*>(value);
    if (!wrapper)
        return nullptr;
    return wrapper->impl();
}

inline RefPtr<JSC::Int8Array> toInt8Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Int8Adaptor>(value); }
inline RefPtr<JSC::Int16Array> toInt16Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Int16Adaptor>(value); }
inline RefPtr<JSC::Int32Array> toInt32Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Int32Adaptor>(value); }
inline RefPtr<JSC::Uint8Array> toUint8Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Uint8Adaptor>(value); }
inline RefPtr<JSC::Uint8ClampedArray> toUint8ClampedArray(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Uint8ClampedAdaptor>(value); }
inline RefPtr<JSC::Uint16Array> toUint16Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Uint16Adaptor>(value); }
inline RefPtr<JSC::Uint32Array> toUint32Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Uint32Adaptor>(value); }
inline RefPtr<JSC::Float32Array> toFloat32Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Float32Adaptor>(value); }
inline RefPtr<JSC::Float64Array> toFloat64Array(JSC::JSValue value) { return JSC::toNativeTypedView<JSC::Float64Adaptor>(value); }

template<typename T> struct NativeValueTraits;

template<> struct NativeValueTraits<String> {
    static inline bool nativeValue(JSC::ExecState& exec, JSC::JSValue jsValue, String& indexedValue)
    {
        indexedValue = jsValue.toWTFString(&exec);
        return !exec.hadException();
    }
};

template<> struct NativeValueTraits<unsigned> {
    static inline bool nativeValue(JSC::ExecState& exec, JSC::JSValue jsValue, unsigned& indexedValue)
    {
        indexedValue = jsValue.toUInt32(&exec);
        return !exec.hadException();
    }
};

template<> struct NativeValueTraits<float> {
    static inline bool nativeValue(JSC::ExecState& exec, JSC::JSValue jsValue, float& indexedValue)
    {
        indexedValue = jsValue.toFloat(&exec);
        return !exec.hadException();
    }
};

template<> struct NativeValueTraits<double> {
    static inline bool nativeValue(JSC::ExecState& exec, JSC::JSValue jsValue, double& indexedValue)
    {
        indexedValue = jsValue.toNumber(&exec);
        return !exec.hadException();
    }
};

template<typename T, typename JST> Vector<RefPtr<T>> toRefPtrNativeArray(JSC::ExecState& exec, JSC::JSValue value)
{
    JSC::VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject()) {
        throwSequenceTypeError(exec, scope);
        return Vector<RefPtr<T>>();
    }

    Vector<RefPtr<T>> result;
    forEachInIterable(&exec, value, [&result](JSC::VM& vm, JSC::ExecState* state, JSC::JSValue jsValue) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (jsValue.inherits(JST::info()))
            result.append(JST::toWrapped(jsValue));
        else
            throwArrayElementTypeError(*state, scope);
    });
    return result;
}

template<typename T> Vector<T> toNativeArray(JSC::ExecState& exec, JSC::JSValue value)
{
    JSC::VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject()) {
        throwSequenceTypeError(exec, scope);
        return Vector<T>();
    }

    Vector<T> result;
    forEachInIterable(&exec, value, [&result](JSC::VM&, JSC::ExecState* state, JSC::JSValue jsValue) {
        T convertedValue;
        if (!NativeValueTraits<T>::nativeValue(*state, jsValue, convertedValue))
            return;
        ASSERT(!state->hadException());
        result.append(convertedValue);
    });
    return result;
}

template<typename T> Vector<T> toNativeArguments(JSC::ExecState& exec, size_t startIndex)
{
    size_t length = exec.argumentCount();
    ASSERT(startIndex <= length);

    Vector<T> result;
    result.reserveInitialCapacity(length - startIndex);
    typedef NativeValueTraits<T> TraitsType;

    for (size_t i = startIndex; i < length; ++i) {
        T indexValue;
        if (!TraitsType::nativeValue(exec, exec.uncheckedArgument(i), indexValue))
            return Vector<T>();
        result.uncheckedAppend(indexValue);
    }
    return result;
}

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

template<typename T> inline JSC::JSValue toNullableJSNumber(Optional<T> optionalNumber)
{
    return optionalNumber ? JSC::jsNumber(optionalNumber.value()) : JSC::jsNull();
}

} // namespace WebCore
