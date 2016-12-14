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

#include "DOMWrapperWorld.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWrapper.h"
#include "JSDynamicDowncast.h"
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
class DeferredPromise;
class DOMWindow;
class Frame;
class URL;
class Node;

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
WEBCORE_EXPORT JSC::EncodedJSValue throwRequiredMemberTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* memberName, const char* dictionaryName, const char* expectedType);
JSC::EncodedJSValue throwConstructorScriptExecutionContextUnavailableError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName);

String makeGetterTypeErrorMessage(const char* interfaceName, const char* attributeName);
String makeThisTypeErrorMessage(const char* interfaceName, const char* attributeName);

WEBCORE_EXPORT JSC::EncodedJSValue throwGetterTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT JSC::EncodedJSValue throwThisTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* functionName);

WEBCORE_EXPORT JSC::EncodedJSValue rejectPromiseWithGetterTypeError(JSC::ExecState&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT JSC::EncodedJSValue rejectPromiseWithThisTypeError(DeferredPromise&, const char* interfaceName, const char* operationName);
WEBCORE_EXPORT JSC::EncodedJSValue rejectPromiseWithThisTypeError(JSC::ExecState&, const char* interfaceName, const char* operationName);

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

template<typename DOMClass> JSC::JSObject* getCachedWrapper(DOMWrapperWorld&, DOMClass&);
template<typename DOMClass> inline JSC::JSObject* getCachedWrapper(DOMWrapperWorld& world, Ref<DOMClass>& object) { return getCachedWrapper(world, object.get()); }
template<typename DOMClass, typename WrapperClass> void cacheWrapper(DOMWrapperWorld&, DOMClass*, WrapperClass*);
template<typename DOMClass, typename WrapperClass> void uncacheWrapper(DOMWrapperWorld&, DOMClass*, WrapperClass*);
template<typename DOMClass, typename T> auto createWrapper(JSDOMGlobalObject*, Ref<T>&&) -> typename std::enable_if<std::is_same<DOMClass, T>::value, typename JSDOMWrapperConverterTraits<DOMClass>::WrapperClass*>::type;
template<typename DOMClass, typename T> auto createWrapper(JSDOMGlobalObject*, Ref<T>&&) -> typename std::enable_if<!std::is_same<DOMClass, T>::value, typename JSDOMWrapperConverterTraits<DOMClass>::WrapperClass*>::type;

template<typename DOMClass> JSC::JSValue wrap(JSC::ExecState*, JSDOMGlobalObject*, DOMClass&);

template<typename JSClass> JSClass& castThisValue(JSC::ExecState&);

void addImpureProperty(const AtomicString&);

const JSC::HashTable& getHashTableForGlobalData(JSC::VM&, const JSC::HashTable& staticTable);

String retrieveErrorMessage(JSC::ExecState&, JSC::VM&, JSC::JSValue exception, JSC::CatchScope&);
WEBCORE_EXPORT void reportException(JSC::ExecState*, JSC::JSValue exception, CachedScript* = nullptr);
WEBCORE_EXPORT void reportException(JSC::ExecState*, JSC::Exception*, CachedScript* = nullptr, ExceptionDetails* = nullptr);
void reportCurrentException(JSC::ExecState*);

JSC::JSValue createDOMException(JSC::ExecState&, Exception&&);
JSC::JSValue createDOMException(JSC::ExecState*, ExceptionCode, const String&);

// Convert a DOM implementation exception into a JavaScript exception in the execution state.
void propagateException(JSC::ExecState&, JSC::ThrowScope&, Exception&&);
void setDOMException(JSC::ExecState*, JSC::ThrowScope&, ExceptionCode);

// Slower versions of the above for use when the caller doesn't have a ThrowScope.
void propagateException(JSC::ExecState&, Exception&&);
WEBCORE_EXPORT void setDOMException(JSC::ExecState*, ExceptionCode);

// Implementation details of the above.
WEBCORE_EXPORT void propagateExceptionSlowPath(JSC::ExecState&, JSC::ThrowScope&, Exception&&);
WEBCORE_EXPORT void setDOMExceptionSlow(JSC::ExecState*, JSC::ThrowScope&, ExceptionCode);

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

// Validates that the passed object is a sequence type per section 4.1.13 of the WebIDL spec.
JSC::JSObject* toJSSequence(JSC::ExecState&, JSC::JSValue, unsigned& length);

JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, JSC::ArrayBuffer&);
JSC::JSValue toJS(JSC::ExecState*, JSC::JSGlobalObject*, JSC::ArrayBufferView&);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, JSC::ArrayBuffer*);
JSC::JSValue toJS(JSC::ExecState*, JSC::JSGlobalObject*, JSC::ArrayBufferView*);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, Ref<T>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, RefPtr<T>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const Vector<T>&);
template<typename T> JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const Vector<RefPtr<T>>&);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const JSC::PrivateName&);

JSC::JSValue toJSIterator(JSC::ExecState&, JSDOMGlobalObject&, JSC::JSValue);
template<typename T> JSC::JSValue toJSIterator(JSC::ExecState&, JSDOMGlobalObject&, const T&);
JSC::JSValue toJSIteratorEnd(JSC::ExecState&);

RefPtr<JSC::ArrayBufferView> toPossiblySharedArrayBufferView(JSC::JSValue);
RefPtr<JSC::Int8Array> toPossiblySharedInt8Array(JSC::JSValue);
RefPtr<JSC::Int16Array> toPossiblySharedInt16Array(JSC::JSValue);
RefPtr<JSC::Int32Array> toPossiblySharedInt32Array(JSC::JSValue);
RefPtr<JSC::Uint8Array> toPossiblySharedUint8Array(JSC::JSValue);
RefPtr<JSC::Uint8ClampedArray> toPossiblySharedUint8ClampedArray(JSC::JSValue);
RefPtr<JSC::Uint16Array> toPossiblySharedUint16Array(JSC::JSValue);
RefPtr<JSC::Uint32Array> toPossiblySharedUint32Array(JSC::JSValue);
RefPtr<JSC::Float32Array> toPossiblySharedFloat32Array(JSC::JSValue);
RefPtr<JSC::Float64Array> toPossiblySharedFloat64Array(JSC::JSValue);

RefPtr<JSC::ArrayBufferView> toUnsharedArrayBufferView(JSC::JSValue);
RefPtr<JSC::Int8Array> toUnsharedInt8Array(JSC::JSValue);
RefPtr<JSC::Int16Array> toUnsharedInt16Array(JSC::JSValue);
RefPtr<JSC::Int32Array> toUnsharedInt32Array(JSC::JSValue);
RefPtr<JSC::Uint8Array> toUnsharedUint8Array(JSC::JSValue);
RefPtr<JSC::Uint8ClampedArray> toUnsharedUint8ClampedArray(JSC::JSValue);
RefPtr<JSC::Uint16Array> toUnsharedUint16Array(JSC::JSValue);
RefPtr<JSC::Uint32Array> toUnsharedUint32Array(JSC::JSValue);
RefPtr<JSC::Float32Array> toUnsharedFloat32Array(JSC::JSValue);
RefPtr<JSC::Float64Array> toUnsharedFloat64Array(JSC::JSValue);

template<typename T, typename JSType> Vector<Ref<T>> toRefNativeArray(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT bool hasIteratorMethod(JSC::ExecState&, JSC::JSValue);

enum SecurityReportingOption { DoNotReportSecurityError, LogSecurityError, ThrowSecurityError };
namespace BindingSecurity {
    template<typename T> T* checkSecurityForNode(JSC::ExecState&, T&);
    template<typename T> T* checkSecurityForNode(JSC::ExecState&, T*);
    template<typename T> ExceptionOr<T*> checkSecurityForNode(JSC::ExecState&, ExceptionOr<T*>&&);
    template<typename T> ExceptionOr<T*> checkSecurityForNode(JSC::ExecState&, ExceptionOr<T&>&&);
    bool shouldAllowAccessToDOMWindow(JSC::ExecState*, DOMWindow&, SecurityReportingOption = LogSecurityError);
    bool shouldAllowAccessToDOMWindow(JSC::ExecState&, DOMWindow&, String& message);
    bool shouldAllowAccessToFrame(JSC::ExecState*, Frame*, SecurityReportingOption = LogSecurityError);
    bool shouldAllowAccessToFrame(JSC::ExecState&, Frame&, String& message);
    bool shouldAllowAccessToNode(JSC::ExecState&, Node*);
};

void printErrorMessageForFrame(Frame*, const String& message);

String propertyNameToString(JSC::PropertyName);
AtomicString propertyNameToAtomicString(JSC::PropertyName);

template<JSC::NativeFunction, int length> JSC::EncodedJSValue nonCachingStaticFunctionGetter(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);


enum class CastedThisErrorBehavior { Throw, ReturnEarly, RejectPromise, Assert };

template<typename JSClass>
struct BindingCaller {
    using AttributeSetterFunction = bool(JSC::ExecState&, JSClass&, JSC::JSValue, JSC::ThrowScope&);
    using AttributeGetterFunction = JSC::JSValue(JSC::ExecState&, JSClass&, JSC::ThrowScope&);
    using OperationCallerFunction = JSC::EncodedJSValue(JSC::ExecState*, JSClass*, JSC::ThrowScope&);
    using PromiseOperationCallerFunction = JSC::EncodedJSValue(JSC::ExecState*, JSClass*, Ref<DeferredPromise>&&, JSC::ThrowScope&);

    static JSClass* castForAttribute(JSC::ExecState&, JSC::EncodedJSValue);
    static JSClass* castForOperation(JSC::ExecState&);

    template<PromiseOperationCallerFunction operationCaller, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::RejectPromise>
    static JSC::EncodedJSValue callPromiseOperation(JSC::ExecState* state, Ref<DeferredPromise>&& promise, const char* operationName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForOperation(*state);
        if (shouldThrow != CastedThisErrorBehavior::Assert && UNLIKELY(!thisObject))
            return rejectPromiseWithThisTypeError(promise.get(), JSClass::info()->className, operationName);
        ASSERT(thisObject);
        ASSERT_GC_OBJECT_INHERITS(thisObject, JSClass::info());
        // FIXME: We should refactor the binding generated code to use references for state and thisObject.
        return operationCaller(state, thisObject, WTFMove(promise), throwScope);
    }

    template<OperationCallerFunction operationCaller, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::Throw>
    static JSC::EncodedJSValue callOperation(JSC::ExecState* state, const char* operationName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForOperation(*state);
        if (shouldThrow != CastedThisErrorBehavior::Assert && UNLIKELY(!thisObject)) {
            if (shouldThrow == CastedThisErrorBehavior::Throw)
                return throwThisTypeError(*state, throwScope, JSClass::info()->className, operationName);
            // For custom promise-returning operations
            return rejectPromiseWithThisTypeError(*state, JSClass::info()->className, operationName);
        }
        ASSERT(thisObject);
        ASSERT_GC_OBJECT_INHERITS(thisObject, JSClass::info());
        // FIXME: We should refactor the binding generated code to use references for state and thisObject.
        return operationCaller(state, thisObject, throwScope);
    }

    template<AttributeSetterFunction setter, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::Throw>
    static bool setAttribute(JSC::ExecState* state, JSC::EncodedJSValue thisValue, JSC::EncodedJSValue encodedValue, const char* attributeName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForAttribute(*state, thisValue);
        if (UNLIKELY(!thisObject))
            return (shouldThrow == CastedThisErrorBehavior::Throw) ? throwSetterTypeError(*state, throwScope, JSClass::info()->className, attributeName) : false;
        return setter(*state, *thisObject, JSC::JSValue::decode(encodedValue), throwScope);
    }

    template<AttributeGetterFunction getter, CastedThisErrorBehavior shouldThrow = CastedThisErrorBehavior::Throw>
    static JSC::EncodedJSValue attribute(JSC::ExecState* state, JSC::EncodedJSValue thisValue, const char* attributeName)
    {
        ASSERT(state);
        auto throwScope = DECLARE_THROW_SCOPE(state->vm());
        auto* thisObject = castForAttribute(*state, thisValue);
        if (UNLIKELY(!thisObject)) {
            if (shouldThrow == CastedThisErrorBehavior::Throw)
                return throwGetterTypeError(*state, throwScope, JSClass::info()->className, attributeName);
            if (shouldThrow == CastedThisErrorBehavior::RejectPromise)
                return rejectPromiseWithGetterTypeError(*state, JSClass::info()->className, attributeName);
            return JSC::JSValue::encode(JSC::jsUndefined());
        }
        return JSC::JSValue::encode(getter(*state, *thisObject, throwScope));
    }
};

// ExceptionOr handling.
void propagateException(JSC::ExecState&, JSC::ThrowScope&, ExceptionOr<void>&&);
template<typename T> JSC::JSValue toJS(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, ExceptionOr<T>&&);
template<typename T> JSC::JSValue toJSNewlyCreated(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, ExceptionOr<T>&& value);

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

template<typename DOMClass, typename T> inline auto createWrapper(JSDOMGlobalObject* globalObject, Ref<T>&& domObject) -> typename std::enable_if<std::is_same<DOMClass, T>::value, typename JSDOMWrapperConverterTraits<DOMClass>::WrapperClass*>::type
{
    using WrapperClass = typename JSDOMWrapperConverterTraits<DOMClass>::WrapperClass;

    ASSERT(!getCachedWrapper(globalObject->world(), domObject));
    auto* domObjectPtr = domObject.ptr();
    auto* wrapper = WrapperClass::create(getDOMStructure<WrapperClass>(globalObject->vm(), *globalObject), globalObject, WTFMove(domObject));
    cacheWrapper(globalObject->world(), domObjectPtr, wrapper);
    return wrapper;
}

template<typename DOMClass, typename T> inline auto createWrapper(JSDOMGlobalObject* globalObject, Ref<T>&& domObject) -> typename std::enable_if<!std::is_same<DOMClass, T>::value, typename JSDOMWrapperConverterTraits<DOMClass>::WrapperClass*>::type
{
    return createWrapper<DOMClass>(globalObject, static_reference_cast<DOMClass>(WTFMove(domObject)));
}

template<typename DOMClass> inline JSC::JSValue wrap(JSC::ExecState* state, JSDOMGlobalObject* globalObject, DOMClass& domObject)
{
    if (auto* wrapper = getCachedWrapper(globalObject->world(), domObject))
        return wrapper;
    return toJSNewlyCreated(state, globalObject, Ref<DOMClass>(domObject));
}

template<typename JSClass> inline JSClass& castThisValue(JSC::ExecState& state)
{
    auto thisValue = state.thisValue();
    auto castedThis = jsDynamicDowncast<JSClass*>(thisValue);
    ASSERT(castedThis);
    return *castedThis;
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
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (lengthValue.isUndefinedOrNull()) {
        throwSequenceTypeError(exec, scope);
        return nullptr;
    }

    length = lengthValue.toUInt32(&exec);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return object;
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

inline RefPtr<JSC::ArrayBufferView> toPossiblySharedArrayBufferView(JSC::JSValue value)
{
    auto* wrapper = jsDynamicDowncast<JSC::JSArrayBufferView*>(value);
    if (!wrapper)
        return nullptr;
    return wrapper->possiblySharedImpl();
}

inline RefPtr<JSC::ArrayBufferView> toUnsharedArrayBufferView(JSC::JSValue value)
{
    auto result = toPossiblySharedArrayBufferView(value);
    if (!result || result->isShared())
        return nullptr;
    return result;
}

inline RefPtr<JSC::Int8Array> toPossiblySharedInt8Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int8Adaptor>(value); }
inline RefPtr<JSC::Int16Array> toPossiblySharedInt16Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int16Adaptor>(value); }
inline RefPtr<JSC::Int32Array> toPossiblySharedInt32Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Int32Adaptor>(value); }
inline RefPtr<JSC::Uint8Array> toPossiblySharedUint8Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8Adaptor>(value); }
inline RefPtr<JSC::Uint8ClampedArray> toPossiblySharedUint8ClampedArray(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint8ClampedAdaptor>(value); }
inline RefPtr<JSC::Uint16Array> toPossiblySharedUint16Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint16Adaptor>(value); }
inline RefPtr<JSC::Uint32Array> toPossiblySharedUint32Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Uint32Adaptor>(value); }
inline RefPtr<JSC::Float32Array> toPossiblySharedFloat32Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float32Adaptor>(value); }
inline RefPtr<JSC::Float64Array> toPossiblySharedFloat64Array(JSC::JSValue value) { return JSC::toPossiblySharedNativeTypedView<JSC::Float64Adaptor>(value); }

inline RefPtr<JSC::Int8Array> toUnsharedInt8Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int8Adaptor>(value); }
inline RefPtr<JSC::Int16Array> toUnsharedInt16Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int16Adaptor>(value); }
inline RefPtr<JSC::Int32Array> toUnsharedInt32Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Int32Adaptor>(value); }
inline RefPtr<JSC::Uint8Array> toUnsharedUint8Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8Adaptor>(value); }
inline RefPtr<JSC::Uint8ClampedArray> toUnsharedUint8ClampedArray(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint8ClampedAdaptor>(value); }
inline RefPtr<JSC::Uint16Array> toUnsharedUint16Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint16Adaptor>(value); }
inline RefPtr<JSC::Uint32Array> toUnsharedUint32Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Uint32Adaptor>(value); }
inline RefPtr<JSC::Float32Array> toUnsharedFloat32Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float32Adaptor>(value); }
inline RefPtr<JSC::Float64Array> toUnsharedFloat64Array(JSC::JSValue value) { return JSC::toUnsharedNativeTypedView<JSC::Float64Adaptor>(value); }

template<typename T, typename JST> inline Vector<Ref<T>> toRefNativeArray(JSC::ExecState& state, JSC::JSValue value)
{
    JSC::VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject()) {
        throwSequenceTypeError(state, scope);
        return { };
    }

    Vector<Ref<T>> result;
    forEachInIterable(&state, value, [&result](JSC::VM& vm, JSC::ExecState* state, JSC::JSValue jsValue) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (jsValue.inherits(JST::info())) {
            auto* object = JST::toWrapped(jsValue);
            ASSERT(object);
            result.append(*object);
        } else
            throwArrayElementTypeError(*state, scope);
    });
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

ALWAYS_INLINE void propagateException(JSC::ExecState& state, JSC::ThrowScope& throwScope, Exception&& exception)
{
    if (throwScope.exception())
        return;
    propagateExceptionSlowPath(state, throwScope, WTFMove(exception));
}

ALWAYS_INLINE void setDOMException(JSC::ExecState* exec, JSC::ThrowScope& throwScope, ExceptionCode ec)
{
    if (LIKELY(!ec || throwScope.exception()))
        return;
    setDOMExceptionSlow(exec, throwScope, ec);
}

inline void propagateException(JSC::ExecState& state, JSC::ThrowScope& throwScope, ExceptionOr<void>&& value)
{
    if (UNLIKELY(value.hasException()))
        propagateException(state, throwScope, value.releaseException());
}

template<typename T> inline JSC::JSValue toJS(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, ExceptionOr<T>&& value)
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }
    return toJS(&state, &globalObject, value.releaseReturnValue());
}

template<typename T> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, ExceptionOr<T>&& value)
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }
    return toJSNewlyCreated(&state, &globalObject, value.releaseReturnValue());
}

template<typename T> inline T* BindingSecurity::checkSecurityForNode(JSC::ExecState& state, T& node)
{
    return shouldAllowAccessToNode(state, &node) ? &node : nullptr;
}

template<typename T> inline T* BindingSecurity::checkSecurityForNode(JSC::ExecState& state, T* node)
{
    return shouldAllowAccessToNode(state, node) ? node : nullptr;
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNode(JSC::ExecState& state, ExceptionOr<T*>&& value)
{
    if (value.hasException())
        return value.releaseException();
    return checkSecurityForNode(state, value.releaseReturnValue());
}

template<typename T> inline ExceptionOr<T*> BindingSecurity::checkSecurityForNode(JSC::ExecState& state, ExceptionOr<T&>&& value)
{
    if (value.hasException())
        return value.releaseException();
    return checkSecurityForNode(state, value.releaseReturnValue());
}

} // namespace WebCore
