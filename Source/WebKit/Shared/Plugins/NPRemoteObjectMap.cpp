/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "NPRemoteObjectMap.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NPObjectMessageReceiver.h"
#include "NPObjectProxy.h"
#include "NPRuntimeUtilities.h"
#include "NPVariantData.h"

namespace WebKit {

static uint64_t generateNPObjectID()
{
    static uint64_t generateNPObjectID;
    return ++generateNPObjectID;
}

Ref<NPRemoteObjectMap> NPRemoteObjectMap::create(IPC::Connection* connection)
{
    return adoptRef(*new NPRemoteObjectMap(connection));
}

NPRemoteObjectMap::NPRemoteObjectMap(IPC::Connection* connection)
    : m_connection(connection)
{
}

NPRemoteObjectMap::~NPRemoteObjectMap()
{
    ASSERT(m_npObjectProxies.isEmpty());
    ASSERT(m_registeredNPObjects.isEmpty());
}

NPObject* NPRemoteObjectMap::createNPObjectProxy(uint64_t remoteObjectID, Plugin* plugin)
{
    NPObjectProxy* npObjectProxy = NPObjectProxy::create(this, plugin, remoteObjectID);

    m_npObjectProxies.add(npObjectProxy);

    return npObjectProxy;
}

void NPRemoteObjectMap::npObjectProxyDestroyed(NPObject* npObject)
{
    NPObjectProxy* npObjectProxy = NPObjectProxy::toNPObjectProxy(npObject);
    ASSERT(m_npObjectProxies.contains(npObjectProxy));

    m_npObjectProxies.remove(npObjectProxy);
}

uint64_t NPRemoteObjectMap::registerNPObject(NPObject* npObject, Plugin* plugin)
{
    uint64_t npObjectID = generateNPObjectID();
    m_registeredNPObjects.set(npObjectID, makeUnique<NPObjectMessageReceiver>(this, plugin, npObjectID, npObject).release());

    return npObjectID;
}

void NPRemoteObjectMap::unregisterNPObject(uint64_t npObjectID)
{
    m_registeredNPObjects.remove(npObjectID);
}

static uint64_t remoteNPObjectID(Plugin* plugin, NPObject* npObject)
{
    if (!NPObjectProxy::isNPObjectProxy(npObject))
        return 0;

    NPObjectProxy* npObjectProxy = NPObjectProxy::toNPObjectProxy(npObject);
    if (npObjectProxy->plugin() != plugin)
        return 0;

    return npObjectProxy->npObjectID();
}

NPVariantData NPRemoteObjectMap::npVariantToNPVariantData(const NPVariant& variant, Plugin* plugin)
{
    switch (variant.type) {
    case NPVariantType_Void:
        return NPVariantData::makeVoid();

    case NPVariantType_Null:
        return NPVariantData::makeNull();

    case NPVariantType_Bool:
        return NPVariantData::makeBool(variant.value.boolValue);

    case NPVariantType_Int32:
        return NPVariantData::makeInt32(variant.value.intValue);

    case NPVariantType_Double:
        return NPVariantData::makeDouble(variant.value.doubleValue);

    case NPVariantType_String:
        return NPVariantData::makeString(variant.value.stringValue.UTF8Characters, variant.value.stringValue.UTF8Length);

    case NPVariantType_Object: {
        NPObject* npObject = variant.value.objectValue;

        if (uint64_t npObjectID = remoteNPObjectID(plugin, npObject)) {
            // FIXME: Under some circumstances, this might leak the NPObjectProxy object. 
            // Figure out how to avoid that.
            retainNPObject(npObject);
            return NPVariantData::makeRemoteNPObjectID(npObjectID);
        }

        uint64_t npObjectID = registerNPObject(npObject, plugin);
        return NPVariantData::makeLocalNPObjectID(npObjectID);
    }

    }

    ASSERT_NOT_REACHED();
    return NPVariantData::makeVoid();
}

NPVariant NPRemoteObjectMap::npVariantDataToNPVariant(const NPVariantData& npVariantData, Plugin* plugin)
{
    NPVariant npVariant;

    switch (npVariantData.type()) {
    case NPVariantData::Void:
        VOID_TO_NPVARIANT(npVariant);
        break;
    case NPVariantData::Null:
        NULL_TO_NPVARIANT(npVariant);
        break;
    case NPVariantData::Bool:
        BOOLEAN_TO_NPVARIANT(npVariantData.boolValue(), npVariant);
        break;
    case NPVariantData::Int32:
        INT32_TO_NPVARIANT(npVariantData.int32Value(), npVariant);
        break;
    case NPVariantData::Double:
        DOUBLE_TO_NPVARIANT(npVariantData.doubleValue(), npVariant);
        break;
    case NPVariantData::String: {
        NPString npString = createNPString(npVariantData.stringValue());
        STRINGN_TO_NPVARIANT(npString.UTF8Characters, npString.UTF8Length, npVariant);
        break;
    }
    case NPVariantData::LocalNPObjectID: {
        uint64_t npObjectID = npVariantData.localNPObjectIDValue();
        ASSERT(npObjectID);

        NPObjectMessageReceiver* npObjectMessageReceiver = m_registeredNPObjects.get(npObjectID);
        if (!npObjectMessageReceiver) {
            ASSERT_NOT_REACHED();
            VOID_TO_NPVARIANT(npVariant);
            break;
        }

        NPObject* npObject = npObjectMessageReceiver->npObject();
        ASSERT(npObject);

        retainNPObject(npObject);
        OBJECT_TO_NPVARIANT(npObject, npVariant);
        break;
    }
    case NPVariantData::RemoteNPObjectID: {
        NPObject* npObjectProxy = createNPObjectProxy(npVariantData.remoteNPObjectIDValue(), plugin);
        OBJECT_TO_NPVARIANT(npObjectProxy, npVariant);
        break;
    }
    }

    return npVariant;
}

void NPRemoteObjectMap::pluginDestroyed(Plugin* plugin)
{
    // Gather and delete the receivers associated with this plug-in.
    Vector<NPObjectMessageReceiver*> receivers;
    for (auto* receiver : m_registeredNPObjects.values()) {
        if (receiver->plugin() == plugin)
            receivers.append(receiver);
    }
    for (auto* receiver : receivers)
        delete receiver;

    // Invalidate and remove all proxies associated with this plug-in.
    Vector<NPObjectProxy*> proxies;
    for (auto* proxy : m_npObjectProxies) {
        if (proxy->plugin() == plugin)
            proxies.append(proxy);
    }
    for (auto* proxy : proxies) {
        proxy->invalidate();
        ASSERT(m_npObjectProxies.contains(proxy));
        m_npObjectProxies.remove(proxy);
    }
}

bool NPRemoteObjectMap::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    NPObjectMessageReceiver* messageReceiver = m_registeredNPObjects.get(decoder.destinationID());
    if (!messageReceiver)
        return false;

    return messageReceiver->didReceiveSyncNPObjectMessageReceiverMessage(connection, decoder, replyEncoder);
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
