/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#if USE(PLUGIN_HOST_PROCESS)

#import "ProxyInstance.h"

#import "NetscapePluginHostProxy.h"
#import "NetscapePluginInstanceProxy.h"
#import <WebCore/npruntime_impl.h>

extern "C" {
#import "WebKitPluginHost.h"
}

using namespace JSC;
using namespace JSC::Bindings;
using namespace std;

namespace WebKit {

class ProxyClass : public JSC::Bindings::Class {
private:
    virtual MethodList methodsNamed(const Identifier&, Instance*) const;
    virtual Field* fieldNamed(const Identifier&, Instance*) const;
};

MethodList ProxyClass::methodsNamed(const Identifier& identifier, Instance* instance) const
{
    return static_cast<ProxyInstance*>(instance)->methodsNamed(identifier);
}

Field* ProxyClass::fieldNamed(const Identifier& identifier, Instance* instance) const
{
    return static_cast<ProxyInstance*>(instance)->fieldNamed(identifier);
}

static ProxyClass* proxyClass()
{
    DEFINE_STATIC_LOCAL(ProxyClass, proxyClass, ());
    return &proxyClass;
}
    
class ProxyField : public JSC::Bindings::Field {
public:
    ProxyField(uint64_t serverIdentifier)
        : m_serverIdentifier(serverIdentifier)
    {
    }
    
    uint64_t serverIdentifier() const { return m_serverIdentifier; }

private:
    virtual JSValuePtr valueFromInstance(ExecState*, const Instance*) const;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValuePtr) const;
    
    uint64_t m_serverIdentifier;
};

JSValuePtr ProxyField::valueFromInstance(ExecState* exec, const Instance* instance) const
{
    return static_cast<const ProxyInstance*>(instance)->fieldValue(exec, this);
}
    
void ProxyField::setValueToInstance(ExecState* exec, const Instance* instance, JSValuePtr value) const
{
    static_cast<const ProxyInstance*>(instance)->setFieldValue(exec, this, value);
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

ProxyInstance::ProxyInstance(PassRefPtr<RootObject> rootObject, NetscapePluginInstanceProxy* instanceProxy, uint32_t objectID)
    : Instance(rootObject)
    , m_instanceProxy(instanceProxy)
    , m_objectID(objectID)
{
    m_instanceProxy->addInstance(this);
}

ProxyInstance::~ProxyInstance()
{
    deleteAllValues(m_fields);
    deleteAllValues(m_methods);

    if (!m_instanceProxy)
        return;
    
    m_instanceProxy->removeInstance(this);

    invalidate();
}
    
JSC::Bindings::Class *ProxyInstance::getClass() const
{
    return proxyClass();
}

JSValuePtr ProxyInstance::invoke(JSC::ExecState* exec, InvokeType type, uint64_t identifier, const JSC::ArgList& args)
{
    RetainPtr<NSData*> arguments(m_instanceProxy->marshalValues(exec, args));
    
    if (_WKPHNPObjectInvoke(m_instanceProxy->hostProxy()->port(), m_instanceProxy->pluginID(), m_objectID,
                            type, identifier, (char*)[arguments.get() bytes], [arguments.get() length]) != KERN_SUCCESS)
        return jsUndefined();
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanAndDataReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>();
    if (!reply.get() || !reply->m_returnValue)
        return jsUndefined();
    
    return m_instanceProxy->demarshalValue(exec, (char*)CFDataGetBytePtr(reply->m_result.get()), CFDataGetLength(reply->m_result.get()));
}

JSValuePtr ProxyInstance::invokeMethod(ExecState* exec, const MethodList& methodList, const ArgList& args)
{
    ASSERT(methodList.size() == 1);

    ProxyMethod* method = static_cast<ProxyMethod*>(methodList[0]);

    return invoke(exec, Invoke, method->serverIdentifier(), args);
}

bool ProxyInstance::supportsInvokeDefaultMethod() const
{
    if (_WKPHNPObjectHasInvokeDefaultMethod(m_instanceProxy->hostProxy()->port(),
                                            m_instanceProxy->pluginID(),
                                            m_objectID) != KERN_SUCCESS)
        return false;
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanReply>();
    if (reply.get() && reply->m_result)
        return true;
        
    return false;
}
    
JSValuePtr ProxyInstance::invokeDefaultMethod(ExecState* exec, const ArgList& args)
{
    return invoke(exec, InvokeDefault, 0, args);
}

bool ProxyInstance::supportsConstruct() const
{
    if (_WKPHNPObjectHasConstructMethod(m_instanceProxy->hostProxy()->port(),
                                        m_instanceProxy->pluginID(),
                                        m_objectID) != KERN_SUCCESS)
        return false;
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanReply>();
    if (reply.get() && reply->m_result)
        return true;
        
    return false;
}
    
JSValuePtr ProxyInstance::invokeConstruct(ExecState* exec, const ArgList& args)
{
    return invoke(exec, Construct, 0, args);
}

JSValuePtr ProxyInstance::defaultValue(ExecState* exec, PreferredPrimitiveType hint) const
{
    if (hint == PreferString)
        return stringValue(exec);
    if (hint == PreferNumber)
        return numberValue(exec);
    return valueOf(exec);
}

JSValuePtr ProxyInstance::stringValue(ExecState* exec) const
{
    // FIXME: Implement something sensible.
    return jsString(exec, "");
}

JSValuePtr ProxyInstance::numberValue(ExecState* exec) const
{
    // FIXME: Implement something sensible.
    return jsNumber(exec, 0);
}

JSValuePtr ProxyInstance::booleanValue() const
{
    // FIXME: Implement something sensible.
    return jsBoolean(false);
}

JSValuePtr ProxyInstance::valueOf(ExecState* exec) const
{
    return stringValue(exec);
}

MethodList ProxyInstance::methodsNamed(const Identifier& identifier)
{
    if (Method* method = m_methods.get(identifier.ustring().rep())) {
        MethodList methodList;
        methodList.append(method);
        return methodList;
    }

    uint64_t methodName = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(identifier.ascii()));
    if (_WKPHNPObjectHasMethod(m_instanceProxy->hostProxy()->port(),
                               m_instanceProxy->pluginID(),
                               m_objectID, methodName) != KERN_SUCCESS)
        return MethodList();
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanReply>();
    if (reply.get() && reply->m_result) {
        Method* method = new ProxyMethod(methodName);
        
        m_methods.set(identifier.ustring().rep(), method);
    
        MethodList methodList;
        methodList.append(method);
        return methodList;
    }

    return MethodList();
}

Field* ProxyInstance::fieldNamed(const Identifier& identifier)
{
    if (Field* field = m_fields.get(identifier.ustring().rep()))
        return field;
    
    uint64_t propertyName = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(identifier.ascii()));
    if (_WKPHNPObjectHasProperty(m_instanceProxy->hostProxy()->port(),
                                 m_instanceProxy->pluginID(),
                                 m_objectID, propertyName) != KERN_SUCCESS)
        return 0;
        
    auto_ptr<NetscapePluginInstanceProxy::BooleanReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanReply>();
    if (reply.get() && reply->m_result) {
        Field* field = new ProxyField(propertyName);
        
        m_fields.set(identifier.ustring().rep(), field);
    
        return field;
    }
    
    return 0;
}

JSC::JSValuePtr ProxyInstance::fieldValue(ExecState* exec, const Field* field) const
{
    uint64_t serverIdentifier = static_cast<const ProxyField*>(field)->serverIdentifier();
    
    if (_WKPHNPObjectGetProperty(m_instanceProxy->hostProxy()->port(),
                                 m_instanceProxy->pluginID(),
                                 m_objectID, serverIdentifier) != KERN_SUCCESS)
        return jsUndefined();
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanAndDataReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>();
    if (!reply.get() || !reply->m_returnValue)
        return jsUndefined();
    
    return m_instanceProxy->demarshalValue(exec, (char*)CFDataGetBytePtr(reply->m_result.get()), CFDataGetLength(reply->m_result.get()));
}
    
void ProxyInstance::setFieldValue(ExecState* exec, const Field* field, JSValuePtr value) const
{
    uint64_t serverIdentifier = static_cast<const ProxyField*>(field)->serverIdentifier();
    
    data_t valueData;
    mach_msg_type_number_t valueLength;

    m_instanceProxy->marshalValue(exec, value, valueData, valueLength);
    kern_return_t kr = _WKPHNPObjectSetProperty(m_instanceProxy->hostProxy()->port(),
                                                m_instanceProxy->pluginID(),
                                                m_objectID, serverIdentifier, valueData, valueLength);
    mig_deallocate(reinterpret_cast<vm_address_t>(valueData), valueLength);
    if (kr != KERN_SUCCESS)
        return;
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanReply> reply = m_instanceProxy->waitForReply<NetscapePluginInstanceProxy::BooleanReply>();
}

void ProxyInstance::invalidate()
{
    if (NetscapePluginHostProxy* hostProxy = m_instanceProxy->hostProxy())
        _WKPHNPObjectRelease(hostProxy->port(),
                             m_instanceProxy->pluginID(), m_objectID);
    m_instanceProxy = 0;
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)

