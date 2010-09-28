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

#include "WebProcessConnection.h"

#include "PluginControllerProxy.h"
#include "PluginProcess.h"
#include "RunLoop.h"

namespace WebKit {

PassRefPtr<WebProcessConnection> WebProcessConnection::create(CoreIPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(new WebProcessConnection(connectionIdentifier));
}

WebProcessConnection::~WebProcessConnection()
{
    ASSERT(m_pluginControllers.isEmpty());
}
    
WebProcessConnection::WebProcessConnection(CoreIPC::Connection::Identifier connectionIdentifier)
{
    m_connection = CoreIPC::Connection::createServerConnection(connectionIdentifier, this, RunLoop::main());
    m_connection->open();
}

void WebProcessConnection::addPluginControllerProxy(PassOwnPtr<PluginControllerProxy> pluginController)
{
    uint64_t pluginInstanceID = pluginController->pluginInstanceID();

    ASSERT(!m_pluginControllers.contains(pluginInstanceID));
    m_pluginControllers.set(pluginInstanceID, pluginController.leakPtr());
}

void WebProcessConnection::destroyPluginControllerProxy(PluginControllerProxy* pluginController)
{
    pluginController->destroy();
    
    // This will delete the plug-in controller proxy object.
    removePluginControllerProxy(pluginController);
}

void WebProcessConnection::removePluginControllerProxy(PluginControllerProxy* pluginController)
{
    {
        ASSERT(m_pluginControllers.contains(pluginController->pluginInstanceID()));

        OwnPtr<PluginControllerProxy> pluginControllerOwnPtr = adoptPtr(m_pluginControllers.take(pluginController->pluginInstanceID()));
        ASSERT(pluginControllerOwnPtr == pluginController);
    }
    
    if (!m_pluginControllers.isEmpty())
        return;

    // The last plug-in went away, close this connection.
    m_connection->invalidate();
    m_connection = 0;

    // This will cause us to be deleted.    
    PluginProcess::shared().removeWebProcessConnection(this);
}

void WebProcessConnection::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    PluginControllerProxy* pluginControllerProxy = m_pluginControllers.get(arguments->destinationID());
    pluginControllerProxy->didReceivePluginControllerProxyMessage(connection, messageID, arguments);
}

CoreIPC::SyncReplyMode WebProcessConnection::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    return didReceiveSyncWebProcessConnectionMessage(connection, messageID, arguments, reply);
}

void WebProcessConnection::didClose(CoreIPC::Connection*)
{
    // The web process crashed. Destroy all the plug-in controllers. Destroying the last plug-in controller
    // will cause the web process connection itself to be destroyed.
    Vector<PluginControllerProxy*> pluginControllers;
    copyValuesToVector(m_pluginControllers, pluginControllers);

    for (size_t i = 0; i < pluginControllers.size(); ++i)
        destroyPluginControllerProxy(pluginControllers[i]);
}

void WebProcessConnection::destroyPlugin(uint64_t pluginInstanceID)
{
    PluginControllerProxy* pluginControllerProxy = m_pluginControllers.get(pluginInstanceID);
    ASSERT(pluginControllerProxy);

    destroyPluginControllerProxy(pluginControllerProxy);
}

void WebProcessConnection::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{
    // FIXME: Implement.
}

void WebProcessConnection::createPlugin(uint64_t pluginInstanceID, const Plugin::Parameters& parameters, bool& result)
{
    OwnPtr<PluginControllerProxy> pluginControllerProxy = PluginControllerProxy::create(this, pluginInstanceID);

    PluginControllerProxy* pluginControllerProxyPtr = pluginControllerProxy.get();

    // Make sure to add the proxy to the map before initializing it, since the plug-in might call out to the web process from 
    // its NPP_New function. This will hand over ownership of the proxy to the web process connection.
    addPluginControllerProxy(pluginControllerProxy.release());

    // Now try to initialize the plug-in.
    result = pluginControllerProxyPtr->initialize(parameters);

    if (!result) {
        // We failed to initialize, remove the plug-in controller. This could cause us to be deleted.
        removePluginControllerProxy(pluginControllerProxyPtr);
    }
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
