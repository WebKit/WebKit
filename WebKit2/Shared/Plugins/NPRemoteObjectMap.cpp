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

#if ENABLE(PLUGIN_PROCESS)

#include "NPRemoteObjectMap.h"

#include "NPObjectMessageReceiver.h"
#include "NPObjectProxy.h"
#include "NPVariantData.h"
#include "NotImplemented.h"
#include <wtf/OwnPtr.h>

namespace WebKit {

static uint64_t generateNPObjectID()
{
    static uint64_t generateNPObjectID;
    return ++generateNPObjectID;
}

PassRefPtr<NPRemoteObjectMap> NPRemoteObjectMap::create(CoreIPC::Connection* connection)
{
    return adoptRef(new NPRemoteObjectMap(connection));
}

NPRemoteObjectMap::NPRemoteObjectMap(CoreIPC::Connection* connection)
    : m_connection(connection)
{
}

NPRemoteObjectMap::~NPRemoteObjectMap()
{
}

NPObject* NPRemoteObjectMap::createNPObjectProxy(uint64_t remoteObjectID)
{
    return NPObjectProxy::create(this, remoteObjectID);
}

uint64_t NPRemoteObjectMap::registerNPObject(NPObject* npObject)
{
    uint64_t npObjectID = generateNPObjectID();
    m_registeredNPObjects.set(npObjectID, NPObjectMessageReceiver::create(this, npObjectID, npObject).leakPtr());

    return npObjectID;
}

void NPRemoteObjectMap::unregisterNPObject(uint64_t npObjectID)
{
    m_registeredNPObjects.remove(npObjectID);
}

NPVariantData NPRemoteObjectMap::npVariantToNPVariantData(const NPVariant& variant)
{
    switch (variant.type) {
    case NPVariantType_Void:
        return NPVariantData::makeVoid();

    case NPVariantType_Bool:
        return NPVariantData::makeBool(variant.value.boolValue);

    case NPVariantType_Double:
        return NPVariantData::makeDouble(variant.value.doubleValue);
            
    case NPVariantType_Null:
    case NPVariantType_Int32:
    case NPVariantType_String:
    case NPVariantType_Object:
        notImplemented();
        return NPVariantData::makeVoid();
    }

    ASSERT_NOT_REACHED();
    return NPVariantData::makeVoid();
}

NPVariant NPRemoteObjectMap::npVariantDataToNPVariant(const NPVariantData& npVariantData)
{
    NPVariant npVariant;

    switch (npVariantData.type()) {
    case NPVariantData::Void:
        VOID_TO_NPVARIANT(npVariant);
        break;
    case NPVariantData::Bool:
        BOOLEAN_TO_NPVARIANT(npVariantData.boolValue(), npVariant);
        break;
    case NPVariantData::Double:
        DOUBLE_TO_NPVARIANT(npVariantData.doubleValue(), npVariant);
        break;
    }

    return npVariant;
}

void NPRemoteObjectMap::invalidate()
{
    // FIXME: Invalidate NPObjectProxy and NPObjectMessageReceiver objects.
    notImplemented();
}

CoreIPC::SyncReplyMode NPRemoteObjectMap::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    NPObjectMessageReceiver* messageReceiver = m_registeredNPObjects.get(arguments->destinationID());
    if (!messageReceiver)
        return CoreIPC::AutomaticReply;

    return messageReceiver->didReceiveSyncNPObjectMessageReceiverMessage(connection, messageID, arguments, reply);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
