/*
 * Copyright (C) 2003-2019 Apple Inc.  All rights reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "c_instance.h"

#include "CRuntimeObject.h"
#include "IdentifierRep.h"
#include "JSDOMBinding.h"
#include "c_class.h"
#include "c_runtime.h"
#include "c_utility.h"
#include "npruntime_impl.h"
#include "runtime_method.h"
#include "runtime_root.h"
#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace JSC {
namespace Bindings {

static String& globalExceptionString()
{
    static NeverDestroyed<String> exceptionStr;
    return exceptionStr;
}

void CInstance::setGlobalException(String exception)
{
    globalExceptionString() = exception;
}

void CInstance::moveGlobalExceptionToExecState(JSGlobalObject* lexicalGlobalObject)
{
    if (globalExceptionString().isNull())
        return;

    {
        VM& vm = lexicalGlobalObject->vm();
        JSLockHolder lock(vm);
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, globalExceptionString()));
    }

    globalExceptionString() = String();
}

CInstance::CInstance(NPObject* o, RefPtr<RootObject>&& rootObject)
    : Instance(WTFMove(rootObject))
{
    _object = _NPN_RetainObject(o);
    _class = 0;
}

CInstance::~CInstance()
{
    _NPN_ReleaseObject(_object);
}

RuntimeObject* CInstance::newRuntimeObject(JSGlobalObject* lexicalGlobalObject)
{
    // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object.
    return CRuntimeObject::create(lexicalGlobalObject->vm(), WebCore::deprecatedGetDOMStructure<CRuntimeObject>(lexicalGlobalObject), this);
}

Class *CInstance::getClass() const
{
    if (!_class)
        _class = CClass::classForIsA(_object->_class);
    return _class;
}

bool CInstance::supportsInvokeDefaultMethod() const
{
    return _object->_class->invokeDefault;
}

class CRuntimeMethod final : public RuntimeMethod {
public:
    using Base = RuntimeMethod;

    static CRuntimeMethod* create(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const String& name, Bindings::Method* method)
    {
        VM& vm = globalObject->vm();
        // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
        // We need to pass in the right global object for "i".
        Structure* domStructure = WebCore::deprecatedGetDOMStructure<CRuntimeMethod>(lexicalGlobalObject);
        CRuntimeMethod* runtimeMethod = new (NotNull, allocateCell<CRuntimeMethod>(vm.heap)) CRuntimeMethod(vm, domStructure, method);
        runtimeMethod->finishCreation(vm, name);
        return runtimeMethod;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    CRuntimeMethod(VM& vm, Structure* structure, Bindings::Method* method)
        : RuntimeMethod(vm, structure, method)
    {
    }

    void finishCreation(VM& vm, const String& name)
    {
        Base::finishCreation(vm, name);
        ASSERT(inherits(vm, info()));
    }
};

const ClassInfo CRuntimeMethod::s_info = { "CRuntimeMethod", &RuntimeMethod::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(CRuntimeMethod) };

JSValue CInstance::getMethod(JSGlobalObject* lexicalGlobalObject, PropertyName propertyName)
{
    Method* method = getClass()->methodNamed(propertyName, this);
    return CRuntimeMethod::create(lexicalGlobalObject, lexicalGlobalObject, propertyName.publicName(), method);
}

JSValue CInstance::invokeMethod(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame, RuntimeMethod* runtimeMethod)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!asObject(runtimeMethod)->inherits<CRuntimeMethod>(vm))
        return throwTypeError(lexicalGlobalObject, scope, "Attempt to invoke non-plug-in method on plug-in object."_s);

    CMethod* method = static_cast<CMethod*>(runtimeMethod->method());
    ASSERT(method);

    NPIdentifier ident = method->identifier();
    if (!_object->_class->hasMethod(_object, ident))
        return jsUndefined();

    unsigned count = callFrame->argumentCount();
    Vector<NPVariant, 8> cArgs(count);

    unsigned i;
    for (i = 0; i < count; i++)
        convertValueToNPVariant(lexicalGlobalObject, callFrame->uncheckedArgument(i), &cArgs[i]);

    // Invoke the 'C' method.
    bool retval = true;
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);

    {
        JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject);
        ASSERT(globalExceptionString().isNull());
        retval = _object->_class->invoke(_object, ident, cArgs.data(), count, &resultVariant);
        moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    if (!retval)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."_s));

    for (i = 0; i < count; i++)
        _NPN_ReleaseVariantValue(&cArgs[i]);

    JSValue resultValue = convertNPVariantToValue(lexicalGlobalObject, &resultVariant, m_rootObject.get());
    _NPN_ReleaseVariantValue(&resultVariant);
    return resultValue;
}


JSValue CInstance::invokeDefaultMethod(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!_object->_class->invokeDefault)
        return jsUndefined();

    unsigned count = callFrame->argumentCount();
    Vector<NPVariant, 8> cArgs(count);

    unsigned i;
    for (i = 0; i < count; i++)
        convertValueToNPVariant(lexicalGlobalObject, callFrame->uncheckedArgument(i), &cArgs[i]);

    // Invoke the 'C' method.
    bool retval = true;
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);
    {
        JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject);
        ASSERT(globalExceptionString().isNull());
        retval = _object->_class->invokeDefault(_object, cArgs.data(), count, &resultVariant);
        moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    if (!retval)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."_s));

    for (i = 0; i < count; i++)
        _NPN_ReleaseVariantValue(&cArgs[i]);

    JSValue resultValue = convertNPVariantToValue(lexicalGlobalObject, &resultVariant, m_rootObject.get());
    _NPN_ReleaseVariantValue(&resultVariant);
    return resultValue;
}

bool CInstance::supportsConstruct() const
{
    return _object->_class->construct;
}

JSValue CInstance::invokeConstruct(JSGlobalObject* lexicalGlobalObject, CallFrame*, const ArgList& args)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!_object->_class->construct)
        return jsUndefined();

    unsigned count = args.size();
    Vector<NPVariant, 8> cArgs(count);

    unsigned i;
    for (i = 0; i < count; i++)
        convertValueToNPVariant(lexicalGlobalObject, args.at(i), &cArgs[i]);

    // Invoke the 'C' method.
    bool retval = true;
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);
    {
        JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject);
        ASSERT(globalExceptionString().isNull());
        retval = _object->_class->construct(_object, cArgs.data(), count, &resultVariant);
        moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    if (!retval)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."_s));

    for (i = 0; i < count; i++)
        _NPN_ReleaseVariantValue(&cArgs[i]);

    JSValue resultValue = convertNPVariantToValue(lexicalGlobalObject, &resultVariant, m_rootObject.get());
    _NPN_ReleaseVariantValue(&resultVariant);
    return resultValue;
}

JSValue CInstance::defaultValue(JSGlobalObject* lexicalGlobalObject, PreferredPrimitiveType hint) const
{
    if (hint == PreferString)
        return stringValue(lexicalGlobalObject);
    if (hint == PreferNumber)
        return numberValue(lexicalGlobalObject);
    return valueOf(lexicalGlobalObject);
}

JSValue CInstance::stringValue(JSGlobalObject* lexicalGlobalObject) const
{
    JSValue value;
    if (toJSPrimitive(lexicalGlobalObject, "toString", value))
        return value;

    // Fallback to default implementation.
    return jsNontrivialString(lexicalGlobalObject->vm(), "NPObject"_s);
}

JSValue CInstance::numberValue(JSGlobalObject*) const
{
    // FIXME: Implement something sensible.
    return jsNumber(0);
}

JSValue CInstance::booleanValue() const
{
    // As per ECMA 9.2.
    return jsBoolean(getObject());
}

JSValue CInstance::valueOf(JSGlobalObject* lexicalGlobalObject) const
{
    JSValue value;
    if (toJSPrimitive(lexicalGlobalObject, "valueOf", value))
        return value;

    // Fallback to default implementation.
    return stringValue(lexicalGlobalObject);
}

bool CInstance::toJSPrimitive(JSGlobalObject* lexicalGlobalObject, const char* name, JSValue& resultValue) const
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    NPIdentifier ident = _NPN_GetStringIdentifier(name);
    if (!_object->_class->hasMethod(_object, ident))
        return false;

    // Invoke the 'C' method.
    bool retval = true;
    NPVariant resultVariant;
    VOID_TO_NPVARIANT(resultVariant);

    {
        JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject);
        ASSERT(globalExceptionString().isNull());
        retval = _object->_class->invoke(_object, ident, 0, 0, &resultVariant);
        moveGlobalExceptionToExecState(lexicalGlobalObject);
    }

    if (!retval)
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Error calling method on NPObject."_s));

    resultValue = convertNPVariantToValue(lexicalGlobalObject, &resultVariant, m_rootObject.get());
    _NPN_ReleaseVariantValue(&resultVariant);
    return true;
}

void CInstance::getPropertyNames(JSGlobalObject* lexicalGlobalObject, PropertyNameArray& nameArray)
{
    if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(_object->_class) || !_object->_class->enumerate)
        return;

    uint32_t count;
    NPIdentifier* identifiers;

    {
        JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject);
        ASSERT(globalExceptionString().isNull());
        bool ok = _object->_class->enumerate(_object, &identifiers, &count);
        moveGlobalExceptionToExecState(lexicalGlobalObject);
        if (!ok)
            return;
    }

    VM& vm = lexicalGlobalObject->vm();
    for (uint32_t i = 0; i < count; i++) {
        IdentifierRep* identifier = static_cast<IdentifierRep*>(identifiers[i]);

        if (identifier->isString())
            nameArray.add(identifierFromNPIdentifier(lexicalGlobalObject, identifier->string()));
        else
            nameArray.add(Identifier::from(vm, identifier->number()));
    }

    // FIXME: This should really call NPN_MemFree but that's in WebKit
    free(identifiers);
}

}
}

#endif // ENABLE(NETSCAPE_PLUGIN_API)
