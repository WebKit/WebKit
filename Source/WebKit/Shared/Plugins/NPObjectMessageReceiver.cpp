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
#include "NPObjectMessageReceiver.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NPIdentifierData.h"
#include "NPRemoteObjectMap.h"
#include "NPRuntimeUtilities.h"
#include "NPVariantData.h"
#include "Plugin.h"
#include "PluginController.h"

namespace WebKit {

NPObjectMessageReceiver::NPObjectMessageReceiver(NPRemoteObjectMap* npRemoteObjectMap, Plugin* plugin, uint64_t npObjectID, NPObject* npObject)
    : m_npRemoteObjectMap(npRemoteObjectMap)
    , m_plugin(plugin)
    , m_npObjectID(npObjectID)
    , m_npObject(npObject)
{
    retainNPObject(m_npObject);
}

NPObjectMessageReceiver::~NPObjectMessageReceiver()
{
    m_npRemoteObjectMap->unregisterNPObject(m_npObjectID);

    releaseNPObject(m_npObject);
}

void NPObjectMessageReceiver::deallocate(CompletionHandler<void()>&& completionHandler)
{
    delete this;
    completionHandler();
}

void NPObjectMessageReceiver::hasMethod(const NPIdentifierData& methodNameData, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->hasMethod)
        return completionHandler(false);

    completionHandler(m_npObject->_class->hasMethod(m_npObject, methodNameData.createNPIdentifier()));
}

void NPObjectMessageReceiver::invoke(const NPIdentifierData& methodNameData, const Vector<NPVariantData>& argumentsData, CompletionHandler<void(bool, NPVariantData&&)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->invoke)
        return completionHandler(false, { });

    Vector<NPVariant> arguments;
    for (size_t i = 0; i < argumentsData.size(); ++i)
        arguments.append(m_npRemoteObjectMap->npVariantDataToNPVariant(argumentsData[i], m_plugin));

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    PluginController::PluginDestructionProtector protector(m_plugin->controller());

    NPVariantData resultData;
    bool returnValue = m_npObject->_class->invoke(m_npObject, methodNameData.createNPIdentifier(), arguments.data(), arguments.size(), &result);
    if (returnValue) {
        // Convert the NPVariant to an NPVariantData.
        resultData = m_npRemoteObjectMap->npVariantToNPVariantData(result, m_plugin);
    }

    // Release all arguments.
    for (size_t i = 0; i < argumentsData.size(); ++i)
        releaseNPVariantValue(&arguments[i]);
    
    // And release the result.
    releaseNPVariantValue(&result);
    completionHandler(returnValue, WTFMove(resultData));
}

void NPObjectMessageReceiver::invokeDefault(const Vector<NPVariantData>& argumentsData, CompletionHandler<void(bool, NPVariantData&&)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->invokeDefault)
        return completionHandler(false, { });

    Vector<NPVariant> arguments;
    for (size_t i = 0; i < argumentsData.size(); ++i)
        arguments.append(m_npRemoteObjectMap->npVariantDataToNPVariant(argumentsData[i], m_plugin));

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    PluginController::PluginDestructionProtector protector(m_plugin->controller());

    NPVariantData resultData;
    bool returnValue = m_npObject->_class->invokeDefault(m_npObject, arguments.data(), arguments.size(), &result);
    if (returnValue) {
        // Convert the NPVariant to an NPVariantData.
        resultData = m_npRemoteObjectMap->npVariantToNPVariantData(result, m_plugin);
    }

    // Release all arguments.
    for (size_t i = 0; i < argumentsData.size(); ++i)
        releaseNPVariantValue(&arguments[i]);
    
    // And release the result.
    releaseNPVariantValue(&result);
    completionHandler(returnValue, WTFMove(resultData));
}

void NPObjectMessageReceiver::hasProperty(const NPIdentifierData& propertyNameData, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->hasProperty)
        return completionHandler(false);

    completionHandler(m_npObject->_class->hasProperty(m_npObject, propertyNameData.createNPIdentifier()));
}

void NPObjectMessageReceiver::getProperty(const NPIdentifierData& propertyNameData, CompletionHandler<void(bool, NPVariantData&&)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->getProperty)
        return completionHandler(false, { });

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    PluginController::PluginDestructionProtector protector(m_plugin->controller());

    bool returnValue = m_npObject->_class->getProperty(m_npObject, propertyNameData.createNPIdentifier(), &result);
    if (!returnValue)
        return completionHandler(false, { });


    NPVariantData resultData = m_npRemoteObjectMap->npVariantToNPVariantData(result, m_plugin);

    releaseNPVariantValue(&result);
    completionHandler(true, WTFMove(resultData));
}

void NPObjectMessageReceiver::setProperty(const NPIdentifierData& propertyNameData, const NPVariantData& propertyValueData, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->setProperty)
        return completionHandler(false);

    NPVariant propertyValue = m_npRemoteObjectMap->npVariantDataToNPVariant(propertyValueData, m_plugin);

    PluginController::PluginDestructionProtector protector(m_plugin->controller());

    bool returnValue = m_npObject->_class->setProperty(m_npObject, propertyNameData.createNPIdentifier(), &propertyValue);

    releaseNPVariantValue(&propertyValue);
    completionHandler(returnValue);
}

void NPObjectMessageReceiver::removeProperty(const NPIdentifierData& propertyNameData, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !m_npObject->_class->removeProperty)
        return completionHandler(false);

    completionHandler(m_npObject->_class->removeProperty(m_npObject, propertyNameData.createNPIdentifier()));
}

void NPObjectMessageReceiver::enumerate(CompletionHandler<void(bool, Vector<NPIdentifierData>&&)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !NP_CLASS_STRUCT_VERSION_HAS_ENUM(m_npObject->_class) || !m_npObject->_class->enumerate)
        return completionHandler(false, { });

    NPIdentifier* identifiers = 0;
    uint32_t identifierCount = 0;

    bool returnValue = m_npObject->_class->enumerate(m_npObject, &identifiers, &identifierCount);
    if (!returnValue)
        return completionHandler(false, { });

    Vector<WebKit::NPIdentifierData> identifiersData;
    for (uint32_t i = 0; i < identifierCount; ++i)
        identifiersData.append(NPIdentifierData::fromNPIdentifier(identifiers[i]));

    npnMemFree(identifiers);
    completionHandler(true, WTFMove(identifiersData));
}

void NPObjectMessageReceiver::construct(const Vector<NPVariantData>& argumentsData, CompletionHandler<void(bool, NPVariantData&&)>&& completionHandler)
{
    if (m_plugin->isBeingDestroyed() || !NP_CLASS_STRUCT_VERSION_HAS_CTOR(m_npObject->_class) || !m_npObject->_class->construct)
        return completionHandler(false, { });

    Vector<NPVariant> arguments;
    for (size_t i = 0; i < argumentsData.size(); ++i)
        arguments.append(m_npRemoteObjectMap->npVariantDataToNPVariant(argumentsData[i], m_plugin));

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    PluginController::PluginDestructionProtector protector(m_plugin->controller());

    bool returnValue = m_npObject->_class->construct(m_npObject, arguments.data(), arguments.size(), &result);
    NPVariantData resultData;
    if (returnValue)
        resultData = m_npRemoteObjectMap->npVariantToNPVariantData(result, m_plugin);

    for (size_t i = 0; i < argumentsData.size(); ++i)
        releaseNPVariantValue(&arguments[i]);
    
    releaseNPVariantValue(&result);
    completionHandler(returnValue, WTFMove(resultData));
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)

