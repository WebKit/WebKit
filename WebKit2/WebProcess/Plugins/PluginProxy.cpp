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

#include "PluginProxy.h"

#include "BackingStore.h"
#include "NotImplemented.h"
#include "PluginController.h"
#include "PluginControllerProxyMessages.h"
#include "PluginProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessConnectionMessages.h"

using namespace WebCore;

namespace WebKit {

static uint64_t generatePluginInstanceID()
{
    static uint64_t uniquePluginInstanceID;
    return ++uniquePluginInstanceID;
}

PassRefPtr<PluginProxy> PluginProxy::create(PassRefPtr<PluginProcessConnection> connection)
{
    return adoptRef(new PluginProxy(connection));
}

PluginProxy::PluginProxy(PassRefPtr<PluginProcessConnection> connection)
    : m_connection(connection)
    , m_pluginInstanceID(generatePluginInstanceID())
    , m_pluginController(0)
    , m_isStarted(false)

{
}

PluginProxy::~PluginProxy()
{
}

void PluginProxy::pluginProcessCrashed()
{
    if (m_pluginController)
        m_pluginController->pluginProcessCrashed();
}

bool PluginProxy::initialize(PluginController* pluginController, const Parameters& parameters)
{
    ASSERT(!m_pluginController);
    ASSERT(pluginController);

    m_pluginController = pluginController;

    // Ask the plug-in process to create a plug-in.
    bool result = false;

    if (!m_connection->connection()->sendSync(Messages::WebProcessConnection::CreatePlugin(m_pluginInstanceID, parameters),
                                              Messages::WebProcessConnection::CreatePlugin::Reply(result),
                                              0, CoreIPC::Connection::NoTimeout))
        return false;

    if (!result)
        return false;

    m_isStarted = true;
    m_connection->addPluginProxy(this);

    return true;
}

void PluginProxy::destroy()
{
    ASSERT(m_isStarted);

    m_connection->connection()->sendSync(Messages::WebProcessConnection::DestroyPlugin(m_pluginInstanceID),
                                         Messages::WebProcessConnection::DestroyPlugin::Reply(),
                                         0, CoreIPC::Connection::NoTimeout);

    m_isStarted = false;
    m_connection->removePluginProxy(this);
}

void PluginProxy::paint(GraphicsContext*, const IntRect& dirtyRect)
{
    notImplemented();
}

#if PLATFORM(MAC)
PlatformLayer* PluginProxy::pluginLayer()
{
    notImplemented();
    return 0;
}
#endif

void PluginProxy::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect)
{
    ASSERT(m_isStarted);

    m_frameRect = frameRect;

    bool didUpdateBackingStore = false;
    if (!m_backingStore) {
        m_backingStore = BackingStore::create(frameRect.size());
        didUpdateBackingStore = true;
    } else if (frameRect.size() != m_backingStore->size()) {
        // The backing store already exists, just resize it.
        if (!m_backingStore->resize(frameRect.size()))
            return;

        didUpdateBackingStore = true;
    }

    SharedMemory::Handle pluginBackingStoreHandle;

    if (didUpdateBackingStore) {
        // Create a new plug-in backing store.
        m_pluginBackingStore = BackingStore::createSharable(frameRect.size());
        if (!m_pluginBackingStore)
            return;

        // Create a handle to the plug-in backing store so we can send it over.
        if (!m_pluginBackingStore->createHandle(pluginBackingStoreHandle)) {
            m_pluginBackingStore.clear();
            return;
        }
    }

    m_connection->connection()->send(Messages::PluginControllerProxy::GeometryDidChange(frameRect, clipRect, pluginBackingStoreHandle), m_pluginInstanceID);
}

void PluginProxy::frameDidFinishLoading(uint64_t requestID)
{
    notImplemented();
}

void PluginProxy::frameDidFail(uint64_t requestID, bool wasCancelled)
{
    notImplemented();
}

void PluginProxy::didEvaluateJavaScript(uint64_t requestID, const WTF::String& requestURLString, const WTF::String& result)
{
    notImplemented();
}

void PluginProxy::streamDidReceiveResponse(uint64_t streamID, const KURL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers)
{
    notImplemented();
}
                                           
void PluginProxy::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
    notImplemented();
}

void PluginProxy::streamDidFinishLoading(uint64_t streamID)
{
    notImplemented();
}

void PluginProxy::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    notImplemented();
}

void PluginProxy::manualStreamDidReceiveResponse(const KURL& responseURL, uint32_t streamLength,  uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers)
{
    notImplemented();
}

void PluginProxy::manualStreamDidReceiveData(const char* bytes, int length)
{
    notImplemented();
}

void PluginProxy::manualStreamDidFinishLoading()
{
    notImplemented();
}

void PluginProxy::manualStreamDidFail(bool wasCancelled)
{
    notImplemented();
}

bool PluginProxy::handleMouseEvent(const WebMouseEvent&)
{
    notImplemented();
    return false;
}

bool PluginProxy::handleWheelEvent(const WebWheelEvent&)
{
    notImplemented();
    return false;
}

bool PluginProxy::handleMouseEnterEvent(const WebMouseEvent&)
{
    notImplemented();
    return false;
}

bool PluginProxy::handleMouseLeaveEvent(const WebMouseEvent&)
{
    notImplemented();
    return false;
}

void PluginProxy::setFocus(bool)
{
    notImplemented();
}

NPObject* PluginProxy::pluginScriptableNPObject()
{
    notImplemented();
    return 0;
}

#if PLATFORM(MAC)
void PluginProxy::windowFocusChanged(bool)
{
    notImplemented();
}

void PluginProxy::windowFrameChanged(const IntRect&)
{
    notImplemented();
}

void PluginProxy::windowVisibilityChanged(bool)
{
    notImplemented();
}

#endif

PluginController* PluginProxy::controller()
{
    return 0;
}

void PluginProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
