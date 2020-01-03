/*
 * Copyright (C) 2008-2019 Apple Inc. All Rights Reserved.
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

#if USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)

#import "ProxyInstance.h"

#import "NetscapePluginHostProxy.h"
#import "ProxyRuntimeObject.h"
#import <JavaScriptCore/Error.h>
#import <JavaScriptCore/FunctionPrototype.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/PropertyNameArray.h>
#import <WebCore/CommonVM.h>
#import <WebCore/IdentifierRep.h>
#import <WebCore/JSDOMWindow.h>
#import <WebCore/npruntime_impl.h>
#import <WebCore/runtime_method.h>
#import <wtf/NeverDestroyed.h>

extern "C" {
#import "WebKitPluginHost.h"
}

using namespace JSC;
using namespace JSC::Bindings;
using namespace WebCore;

namespace WebKit {

class ProxyClass : public JSC::Bindings::Class {
private:
    virtual Method* methodNamed(PropertyName, Instance*) const;
    virtual Field* fieldNamed(PropertyName, Instance*) const;
};

Method* ProxyClass::methodNamed(PropertyName propertyName, Instance* instance) const
{
    return static_cast<ProxyInstance*>(instance)->methodNamed(propertyName);
}

Field* ProxyClass::fieldNamed(PropertyName propertyName, Instance* instance) const
{
    return static_cast<ProxyInstance*>(instance)->fieldNamed(propertyName);
}

static ProxyClass* proxyClass()
{
    static NeverDestroyed<ProxyClass> proxyClass;
    return &proxyClass.get();
}
    
class ProxyField : public JSC::Bindings::Field {
public:
    ProxyField(uint64_t serverIdentifier)
        : m_serverIdentifier(serverIdentifier)
    {
    }
    
    uint64_t serverIdentifier() const { return m_serverIdentifier; }

private:
    virtual JSValue valueFromInstance(JSGlobalObject*, const Instance*) const;
    virtual bool setValueToInstance(JSGlobalObject*, const Instance*, JSValue) const;
    
    uint64_t m_serverIdentifier;
};

JSValue ProxyField::valueFromInstance(JSGlobalObject* lexicalGlobalObject, const Instance* instance) const
{
    return static_cast<const ProxyInstance*>(instance)->fieldValue(lexicalGlobalObject, this);
}
    
bool ProxyField::setValueToInstance(JSGlobalObject* lexicalGlobalObject, const Instance* instance, JSValue value) const
{
    return static_cast<const ProxyInstance*>(instance)->setFieldValue(lexicalGlobalObject, this, value);
}

class ProxyMethod : public JSC::Bindings::Method {
public:
    ProxyMethod(uint64_t serverIdentifier)
        : m_serverIdentifier(serverIdentifier)
    {
    }

    uint64_t serverIdentifier() const { return m_serverIdentifier; }

private:
    virtual int numParameters() const { return 0; }

    uint64_t m_serverIdentifier;
};

ProxyInstance::ProxyInstance(Ref<RootObject>&& rootObject, NetscapePluginInstanceProxy* instanceProxy, uint32_t objectID)
    : Instance(WTFMove(rootObject))
    , m_instanceProxy(instanceProxy)
    , m_objectID(objectID)
{
    m_instanceProxy->addInstance(this);
}

ProxyInstance::~ProxyInstance()
{
    if (!m_instanceProxy)
        return;
    
    m_instanceProxy->removeInstance(this);

    invalidate();
}
    
RuntimeObject* ProxyInstance::newRuntimeObject(JSGlobalObject* lexicalGlobalObject)
{
    // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object.
    return ProxyRuntimeObject::create(lexicalGlobalObject->vm(), WebCore::deprecatedGetDOMStructure<ProxyRuntimeObject>(lexicalGlobalObject), *this);
}

JSC::Bindings::Class* ProxyInstance::getClass() const
{
    return proxyClass();
}

JSValue ProxyInstance::invoke(JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame, InvokeType type, uint64_t identifier, const ArgList& args)
{
    if (!m_instanceProxy)
        return jsUndefined();

    RetainPtr<NSData> arguments(m_instanceProxy->marshalValues(lexicalGlobalObject, args));

    uint32_t requestID = m_instanceProxy->nextRequestID();

    for (unsigned i = 0; i < args.size(); i++)
        m_instanceProxy->retainLocalObject(args.at(i));

    if (_WKPHNPObjectInvoke(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID, type, identifier, static_cast<char*>(const_cast<void*>([arguments.get() bytes])), [arguments.get() length]) != KERN_SUCCESS) {
        if (m_instanceProxy) {
            for (unsigned i = 0; i < args.size(); i++)
                m_instanceProxy->releaseLocalObject(args.at(i));
        }
        return jsUndefined();
    }
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>(requestID);
    NetscapePluginInstanceProxy::moveGlobalExceptionToExecState(lexicalGlobalObject);

    if (m_instanceProxy) {
        for (unsigned i = 0; i < args.size(); i++)
            m_instanceProxy->releaseLocalObject(args.at(i));
    }

    if (!reply || !reply->m_returnValue)
        return jsUndefined();
    
    return m_instanceProxy->demarshalValue(lexicalGlobalObject, reinterpret_cast<char*>(const_cast<unsigned char*>(CFDataGetBytePtr(reply->m_result.get()))), CFDataGetLength(reply->m_result.get()));
}

class ProxyRuntimeMethod final : public RuntimeMethod {
public:
    using Base = RuntimeMethod;

    static ProxyRuntimeMethod* create(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, const String& name, Bindings::Method* method)
    {
        VM& vm = globalObject->vm();
        // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
        // lexicalGlobalObject-vm() is also likely wrong.
        Structure* domStructure = deprecatedGetDOMStructure<ProxyRuntimeMethod>(lexicalGlobalObject);
        ProxyRuntimeMethod* runtimeMethod = new (allocateCell<ProxyRuntimeMethod>(vm.heap)) ProxyRuntimeMethod(vm, domStructure, method);
        runtimeMethod->finishCreation(vm, name);
        return runtimeMethod;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), &s_info);
    }

    DECLARE_INFO;

private:
    ProxyRuntimeMethod(VM& vm, Structure* structure, Bindings::Method* method)
        : RuntimeMethod(vm, structure, method)
    {
    }

    void finishCreation(VM& vm, const String& name)
    {
        Base::finishCreation(vm, name);
        ASSERT(inherits(vm, info()));
    }
};

const ClassInfo ProxyRuntimeMethod::s_info = { "ProxyRuntimeMethod", &RuntimeMethod::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ProxyRuntimeMethod) };

JSValue ProxyInstance::getMethod(JSC::JSGlobalObject* lexicalGlobalObject, PropertyName propertyName)
{
    Method* method = getClass()->methodNamed(propertyName, this);
    return ProxyRuntimeMethod::create(lexicalGlobalObject, lexicalGlobalObject, propertyName.publicName(), method);
}

JSValue ProxyInstance::invokeMethod(JSGlobalObject* lexicalGlobalObject, JSC::CallFrame* callFrame, JSC::RuntimeMethod* runtimeMethod)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!asObject(runtimeMethod)->inherits<ProxyRuntimeMethod>(vm))
        return throwTypeError(lexicalGlobalObject, scope, "Attempt to invoke non-plug-in method on plug-in object."_s);

    ProxyMethod* method = static_cast<ProxyMethod*>(runtimeMethod->method());
    ASSERT(method);

    return invoke(lexicalGlobalObject, callFrame, Invoke, method->serverIdentifier(), ArgList(callFrame));
}

bool ProxyInstance::supportsInvokeDefaultMethod() const
{
    if (!m_instanceProxy)
        return false;
    
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    if (_WKPHNPObjectHasInvokeDefaultMethod(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID) != KERN_SUCCESS)
        return false;
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    return reply && reply->m_result;
}

JSValue ProxyInstance::invokeDefaultMethod(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame)
{
    return invoke(lexicalGlobalObject, callFrame, InvokeDefault, 0, ArgList(callFrame));
}

bool ProxyInstance::supportsConstruct() const
{
    if (!m_instanceProxy)
        return false;
    
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    if (_WKPHNPObjectHasConstructMethod(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID) != KERN_SUCCESS)
        return false;
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    return reply && reply->m_result;
}
    
JSValue ProxyInstance::invokeConstruct(JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame, const ArgList& args)
{
    return invoke(lexicalGlobalObject, callFrame, Construct, 0, args);
}

JSValue ProxyInstance::defaultValue(JSGlobalObject* lexicalGlobalObject, PreferredPrimitiveType hint) const
{
    if (hint == PreferString)
        return stringValue(lexicalGlobalObject);
    if (hint == PreferNumber)
        return numberValue(lexicalGlobalObject);
    return valueOf(lexicalGlobalObject);
}

JSValue ProxyInstance::stringValue(JSGlobalObject* lexicalGlobalObject) const
{
    // FIXME: Implement something sensible.
    return jsEmptyString(lexicalGlobalObject->vm());
}

JSValue ProxyInstance::numberValue(JSGlobalObject*) const
{
    // FIXME: Implement something sensible.
    return jsNumber(0);
}

JSValue ProxyInstance::booleanValue() const
{
    // FIXME: Implement something sensible.
    return jsBoolean(false);
}

JSValue ProxyInstance::valueOf(JSGlobalObject* lexicalGlobalObject) const
{
    return stringValue(lexicalGlobalObject);
}

void ProxyInstance::getPropertyNames(JSGlobalObject* lexicalGlobalObject, PropertyNameArray& nameArray)
{
    if (!m_instanceProxy)
        return;
    
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    if (_WKPHNPObjectEnumerate(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID) != KERN_SUCCESS)
        return;
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>(requestID);
    NetscapePluginInstanceProxy::moveGlobalExceptionToExecState(lexicalGlobalObject);
    if (!reply || !reply->m_returnValue)
        return;
    
    NSArray *array = [NSPropertyListSerialization propertyListWithData:(__bridge NSData *)reply->m_result.get() options:NSPropertyListImmutable format:nullptr error:nullptr];
    
    VM& vm = lexicalGlobalObject->vm();
    for (NSNumber *number in array) {
        IdentifierRep* identifier = reinterpret_cast<IdentifierRep*>([number longLongValue]);
        if (!IdentifierRep::isValid(identifier))
            continue;

        if (identifier->isString()) {
            const char* str = identifier->string();
            nameArray.add(Identifier::fromString(vm, String::fromUTF8WithLatin1Fallback(str, strlen(str))));
        } else
            nameArray.add(Identifier::from(vm, identifier->number()));
    }
}

Method* ProxyInstance::methodNamed(PropertyName propertyName)
{
    String name(propertyName.publicName());
    if (name.isNull())
        return nullptr;

    if (!m_instanceProxy)
        return nullptr;
    
    // If we already have an entry in the map, use it.
    auto existingMapEntry = m_methods.find(name.impl());
    if (existingMapEntry != m_methods.end())
        return existingMapEntry->value.get();
    
    uint64_t methodName = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(name.ascii().data()));
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    if (_WKPHNPObjectHasMethod(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID, methodName) != KERN_SUCCESS)
        return nullptr;
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    if (!reply)
        return nullptr;

    if (!reply->m_result && !m_instanceProxy->hostProxy()->shouldCacheMissingPropertiesAndMethods())
        return nullptr;

    // Add a new entry to the map unless an entry was added while we were in waitForReply.
    auto mapAddResult = m_methods.add(name.impl(), nullptr);
    if (mapAddResult.isNewEntry && reply->m_result)
        mapAddResult.iterator->value = makeUnique<ProxyMethod>(methodName);

    return mapAddResult.iterator->value.get();
}

Field* ProxyInstance::fieldNamed(PropertyName propertyName)
{
    String name(propertyName.publicName());
    if (name.isNull())
        return nullptr;

    if (!m_instanceProxy)
        return nullptr;
    
    // If we already have an entry in the map, use it.
    auto existingMapEntry = m_fields.find(name.impl());
    if (existingMapEntry != m_fields.end())
        return existingMapEntry->value.get();
    
    uint64_t identifier = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(name.ascii().data()));
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    if (_WKPHNPObjectHasProperty(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID, identifier) != KERN_SUCCESS)
        return nullptr;
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    if (!reply)
        return nullptr;
    
    if (!reply->m_result && !m_instanceProxy->hostProxy()->shouldCacheMissingPropertiesAndMethods())
        return nullptr;
    
    // Add a new entry to the map unless an entry was added while we were in waitForReply.
    auto mapAddResult = m_fields.add(name.impl(), nullptr);
    if (mapAddResult.isNewEntry && reply->m_result)
        mapAddResult.iterator->value = makeUnique<ProxyField>(identifier);
    return mapAddResult.iterator->value.get();
}

JSC::JSValue ProxyInstance::fieldValue(JSGlobalObject* lexicalGlobalObject, const Field* field) const
{
    if (!m_instanceProxy)
        return jsUndefined();
    
    uint64_t serverIdentifier = static_cast<const ProxyField*>(field)->serverIdentifier();
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    if (_WKPHNPObjectGetProperty(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID, serverIdentifier) != KERN_SUCCESS)
        return jsUndefined();
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>(requestID);
    NetscapePluginInstanceProxy::moveGlobalExceptionToExecState(lexicalGlobalObject);
    if (!reply || !reply->m_returnValue)
        return jsUndefined();
    
    return m_instanceProxy->demarshalValue(lexicalGlobalObject, reinterpret_cast<char*>(const_cast<unsigned char*>(CFDataGetBytePtr(reply->m_result.get()))), CFDataGetLength(reply->m_result.get()));
}
    
bool ProxyInstance::setFieldValue(JSGlobalObject* lexicalGlobalObject, const Field* field, JSValue value) const
{
    if (!m_instanceProxy)
        return false;
    
    uint64_t serverIdentifier = static_cast<const ProxyField*>(field)->serverIdentifier();
    uint32_t requestID = m_instanceProxy->nextRequestID();
    
    data_t valueData;
    mach_msg_type_number_t valueLength;

    m_instanceProxy->marshalValue(lexicalGlobalObject, value, valueData, valueLength);
    m_instanceProxy->retainLocalObject(value);
    kern_return_t kr = _WKPHNPObjectSetProperty(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), requestID, m_objectID, serverIdentifier, valueData, valueLength);
    mig_deallocate(reinterpret_cast<vm_address_t>(valueData), valueLength);
    if (m_instanceProxy)
        m_instanceProxy->releaseLocalObject(value);
    if (kr != KERN_SUCCESS)
        return false;
    
    waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    NetscapePluginInstanceProxy::moveGlobalExceptionToExecState(lexicalGlobalObject);
    return true;
}

void ProxyInstance::invalidate()
{
    ASSERT(m_instanceProxy);
    
    if (NetscapePluginHostProxy* hostProxy = m_instanceProxy->hostProxy())
        _WKPHNPObjectRelease(hostProxy->port(), m_instanceProxy->pluginID(), m_objectID);
    m_instanceProxy = nullptr;
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)
