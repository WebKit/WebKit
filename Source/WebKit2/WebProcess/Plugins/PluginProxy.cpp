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
#include "PluginProxy.h"

#if ENABLE(PLUGIN_PROCESS)

#include "DataReference.h"
#include "NPRemoteObjectMap.h"
#include "NPRuntimeUtilities.h"
#include "NPVariantData.h"
#include "PluginController.h"
#include "PluginControllerProxyMessages.h"
#include "PluginProcessConnection.h"
#include "PluginProcessConnectionManager.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebEvent.h"
#include "WebProcessConnectionMessages.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generatePluginInstanceID()
{
    static uint64_t uniquePluginInstanceID;
    return ++uniquePluginInstanceID;
}

PassRefPtr<PluginProxy> PluginProxy::create(const String& pluginPath)
{
    return adoptRef(new PluginProxy(pluginPath));
}

PluginProxy::PluginProxy(const String& pluginPath)
    : m_pluginPath(pluginPath)
    , m_pluginInstanceID(generatePluginInstanceID())
    , m_pluginController(0)
    , m_pluginBackingStoreContainsValidData(false)
    , m_isStarted(false)
    , m_waitingForPaintInResponseToUpdate(false)
    , m_remoteLayerClientID(0)
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

    ASSERT(!m_connection);
    m_connection = PluginProcessConnectionManager::shared().getPluginProcessConnection(m_pluginPath);
    
    if (!m_connection)
        return false;
    
    // Add the plug-in proxy before creating the plug-in; it needs to be in the map because CreatePlugin
    // can call back out to the plug-in proxy.
    m_connection->addPluginProxy(this);

    // Ask the plug-in process to create a plug-in.
    bool result = false;

    uint32_t remoteLayerClientID = 0;
    if (!m_connection->connection()->sendSync(Messages::WebProcessConnection::CreatePlugin(m_pluginInstanceID, parameters, pluginController->userAgent(), pluginController->isPrivateBrowsingEnabled(), pluginController->isAcceleratedCompositingEnabled()), Messages::WebProcessConnection::CreatePlugin::Reply(result, remoteLayerClientID), 0) || !result) {
        m_connection->removePluginProxy(this);
        return false;
    }

    m_remoteLayerClientID = remoteLayerClientID;
    m_isStarted = true;

    return true;
}

void PluginProxy::destroy()
{
    ASSERT(m_isStarted);

    m_connection->connection()->sendSync(Messages::WebProcessConnection::DestroyPlugin(m_pluginInstanceID), Messages::WebProcessConnection::DestroyPlugin::Reply(), 0);

    m_isStarted = false;
    m_pluginController = 0;

    m_connection->removePluginProxy(this);
}

void PluginProxy::paint(GraphicsContext* graphicsContext, const IntRect& dirtyRect)
{
    if (!needsBackingStore() || !m_backingStore)
        return;

    if (!m_pluginBackingStoreContainsValidData) {
        m_connection->connection()->sendSync(Messages::PluginControllerProxy::PaintEntirePlugin(), Messages::PluginControllerProxy::PaintEntirePlugin::Reply(), m_pluginInstanceID);
    
        // Blit the plug-in backing store into our own backing store.
        OwnPtr<WebCore::GraphicsContext> graphicsContext = m_backingStore->createGraphicsContext();
        graphicsContext->setCompositeOperation(CompositeCopy);

        m_pluginBackingStore->paint(*graphicsContext, IntPoint(), IntRect(0, 0, m_frameRect.width(), m_frameRect.height()));

        m_pluginBackingStoreContainsValidData = true;
    }

    IntRect dirtyRectInPluginCoordinates = dirtyRect;
    dirtyRectInPluginCoordinates.move(-m_frameRect.x(), -m_frameRect.y());

    m_backingStore->paint(*graphicsContext, dirtyRect.location(), dirtyRectInPluginCoordinates);

    if (m_waitingForPaintInResponseToUpdate) {
        m_waitingForPaintInResponseToUpdate = false;
        m_connection->connection()->send(Messages::PluginControllerProxy::DidUpdate(), m_pluginInstanceID);
        return;
    }
}

PassRefPtr<ShareableBitmap> PluginProxy::snapshot()
{
    ShareableBitmap::Handle snapshotStoreHandle;
    m_connection->connection()->sendSync(Messages::PluginControllerProxy::Snapshot(), Messages::PluginControllerProxy::Snapshot::Reply(snapshotStoreHandle), m_pluginInstanceID);

    RefPtr<ShareableBitmap> snapshotBuffer = ShareableBitmap::create(snapshotStoreHandle);
    return snapshotBuffer.release();
}

bool PluginProxy::isTransparent()
{
    // This should never be called from the web process.
    ASSERT_NOT_REACHED();
    return false;
}

void PluginProxy::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect)
{
    ASSERT(m_isStarted);

    m_frameRect = frameRect;

    if (!needsBackingStore()) {
        ShareableBitmap::Handle pluginBackingStoreHandle;
        m_connection->connection()->send(Messages::PluginControllerProxy::GeometryDidChange(frameRect, clipRect, pluginBackingStoreHandle), m_pluginInstanceID, CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply);
        return;
    }

    bool didUpdateBackingStore = false;
    if (!m_backingStore) {
        m_backingStore = ShareableBitmap::create(frameRect.size(), ShareableBitmap::SupportsAlpha);
        didUpdateBackingStore = true;
    } else if (frameRect.size() != m_backingStore->size()) {
        // The backing store already exists, just resize it.
        if (!m_backingStore->resize(frameRect.size()))
            return;

        didUpdateBackingStore = true;
    }

    ShareableBitmap::Handle pluginBackingStoreHandle;

    if (didUpdateBackingStore) {
        // Create a new plug-in backing store.
        m_pluginBackingStore = ShareableBitmap::createShareable(frameRect.size(), ShareableBitmap::SupportsAlpha);
        if (!m_pluginBackingStore)
            return;

        // Create a handle to the plug-in backing store so we can send it over.
        if (!m_pluginBackingStore->createHandle(pluginBackingStoreHandle)) {
            m_pluginBackingStore = nullptr;
            return;
        }

        m_pluginBackingStoreContainsValidData = false;
    }

    m_connection->connection()->send(Messages::PluginControllerProxy::GeometryDidChange(frameRect, clipRect, pluginBackingStoreHandle), m_pluginInstanceID, CoreIPC::DispatchMessageEvenWhenWaitingForSyncReply);
}

void PluginProxy::frameDidFinishLoading(uint64_t requestID)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::FrameDidFinishLoading(requestID), m_pluginInstanceID);
}

void PluginProxy::frameDidFail(uint64_t requestID, bool wasCancelled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::FrameDidFail(requestID, wasCancelled), m_pluginInstanceID);
}

void PluginProxy::didEvaluateJavaScript(uint64_t requestID, const WTF::String& requestURLString, const WTF::String& result)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::DidEvaluateJavaScript(requestID, requestURLString, result), m_pluginInstanceID);
}

void PluginProxy::streamDidReceiveResponse(uint64_t streamID, const KURL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidReceiveResponse(streamID, responseURL.string(), streamLength, lastModifiedTime, mimeType, headers), m_pluginInstanceID);
}
                                           
void PluginProxy::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidReceiveData(streamID, CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(bytes), length)), m_pluginInstanceID);
}

void PluginProxy::streamDidFinishLoading(uint64_t streamID)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidFinishLoading(streamID), m_pluginInstanceID);
}

void PluginProxy::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidFail(streamID, wasCancelled), m_pluginInstanceID);
}

void PluginProxy::manualStreamDidReceiveResponse(const KURL& responseURL, uint32_t streamLength,  uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::ManualStreamDidReceiveResponse(responseURL.string(), streamLength, lastModifiedTime, mimeType, headers), m_pluginInstanceID);
}

void PluginProxy::manualStreamDidReceiveData(const char* bytes, int length)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::ManualStreamDidReceiveData(CoreIPC::DataReference(reinterpret_cast<const uint8_t*>(bytes), length)), m_pluginInstanceID);
}

void PluginProxy::manualStreamDidFinishLoading()
{
    m_connection->connection()->send(Messages::PluginControllerProxy::ManualStreamDidFinishLoading(), m_pluginInstanceID);
}

void PluginProxy::manualStreamDidFail(bool wasCancelled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::ManualStreamDidFail(wasCancelled), m_pluginInstanceID);
}

bool PluginProxy::handleMouseEvent(const WebMouseEvent& mouseEvent)
{
    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleMouseEvent(mouseEvent), Messages::PluginControllerProxy::HandleMouseEvent::Reply(handled), m_pluginInstanceID))
        return false;

    return handled;
}

bool PluginProxy::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleWheelEvent(wheelEvent), Messages::PluginControllerProxy::HandleWheelEvent::Reply(handled), m_pluginInstanceID))
        return false;

    return handled;
}

bool PluginProxy::handleMouseEnterEvent(const WebMouseEvent& mouseEnterEvent)
{
    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleMouseEnterEvent(mouseEnterEvent), Messages::PluginControllerProxy::HandleMouseEnterEvent::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

bool PluginProxy::handleMouseLeaveEvent(const WebMouseEvent& mouseLeaveEvent)
{
    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleMouseLeaveEvent(mouseLeaveEvent), Messages::PluginControllerProxy::HandleMouseLeaveEvent::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

bool PluginProxy::handleKeyboardEvent(const WebKeyboardEvent& keyboardEvent)
{
    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleKeyboardEvent(keyboardEvent), Messages::PluginControllerProxy::HandleKeyboardEvent::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

void PluginProxy::setFocus(bool hasFocus)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::SetFocus(hasFocus), m_pluginInstanceID);
}

NPObject* PluginProxy::pluginScriptableNPObject()
{
    uint64_t pluginScriptableNPObjectID = 0;
    
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::GetPluginScriptableNPObject(), Messages::PluginControllerProxy::GetPluginScriptableNPObject::Reply(pluginScriptableNPObjectID), m_pluginInstanceID))
        return 0;

    if (!pluginScriptableNPObjectID)
        return 0;

    return m_connection->npRemoteObjectMap()->createNPObjectProxy(pluginScriptableNPObjectID, this);
}

#if PLATFORM(MAC)
void PluginProxy::windowFocusChanged(bool hasFocus)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::WindowFocusChanged(hasFocus), m_pluginInstanceID);
}

void PluginProxy::windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::WindowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates), m_pluginInstanceID);
}

void PluginProxy::windowVisibilityChanged(bool isVisible)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::WindowVisibilityChanged(isVisible), m_pluginInstanceID);
}

uint64_t PluginProxy::pluginComplexTextInputIdentifier() const
{
    return m_pluginInstanceID;
}

void PluginProxy::sendComplexTextInput(const String& textInput)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::SendComplexTextInput(textInput), m_pluginInstanceID);
}

#endif

void PluginProxy::privateBrowsingStateChanged(bool isPrivateBrowsingEnabled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::PrivateBrowsingStateChanged(isPrivateBrowsingEnabled), m_pluginInstanceID);
}

PluginController* PluginProxy::controller()
{
    return m_pluginController;
}

void PluginProxy::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    m_pluginController->loadURL(requestID, method, urlString, target, headerFields, httpBody, allowPopups);
}

void PluginProxy::proxiesForURL(const String& urlString, String& proxyString)
{
    proxyString = m_pluginController->proxiesForURL(urlString);
}

void PluginProxy::cookiesForURL(const String& urlString, String& cookieString)
{
    cookieString = m_pluginController->cookiesForURL(urlString);
}

void PluginProxy::setCookiesForURL(const String& urlString, const String& cookieString)
{
    m_pluginController->setCookiesForURL(urlString, cookieString);
}

void PluginProxy::getWindowScriptNPObject(uint64_t& windowScriptNPObjectID)
{
    NPObject* windowScriptNPObject = m_pluginController->windowScriptNPObject();
    if (!windowScriptNPObject) {
        windowScriptNPObjectID = 0;
        return;
    }

    windowScriptNPObjectID = m_connection->npRemoteObjectMap()->registerNPObject(windowScriptNPObject, this);
    releaseNPObject(windowScriptNPObject);
}

void PluginProxy::getPluginElementNPObject(uint64_t& pluginElementNPObjectID)
{
    NPObject* pluginElementNPObject = m_pluginController->pluginElementNPObject();
    if (!pluginElementNPObject) {
        pluginElementNPObjectID = 0;
        return;
    }

    pluginElementNPObjectID = m_connection->npRemoteObjectMap()->registerNPObject(pluginElementNPObject, this);
    releaseNPObject(pluginElementNPObject);
}

void PluginProxy::evaluate(const NPVariantData& npObjectAsVariantData, const String& scriptString, bool allowPopups, bool& returnValue, NPVariantData& resultData)
{
    PluginController::PluginDestructionProtector protector(m_pluginController);

    NPVariant npObjectAsVariant = m_connection->npRemoteObjectMap()->npVariantDataToNPVariant(npObjectAsVariantData, this);
    if (!NPVARIANT_IS_OBJECT(npObjectAsVariant) || !(NPVARIANT_TO_OBJECT(npObjectAsVariant))) {
        returnValue = false;
        return;
    }
        
    NPVariant result;
    returnValue = m_pluginController->evaluate(NPVARIANT_TO_OBJECT(npObjectAsVariant), scriptString, &result, allowPopups);
    if (!returnValue)
        return;

    // Convert the NPVariant to an NPVariantData.
    resultData = m_connection->npRemoteObjectMap()->npVariantToNPVariantData(result, this);
    
    // And release the result.
    releaseNPVariantValue(&result);

    releaseNPVariantValue(&npObjectAsVariant);
}

void PluginProxy::cancelStreamLoad(uint64_t streamID)
{
    m_pluginController->cancelStreamLoad(streamID);
}

void PluginProxy::cancelManualStreamLoad()
{
    m_pluginController->cancelManualStreamLoad();
}

void PluginProxy::setStatusbarText(const String& statusbarText)
{
    m_pluginController->setStatusbarText(statusbarText);
}

#if PLATFORM(MAC)
void PluginProxy::setComplexTextInputEnabled(bool complexTextInputEnabled)
{
    m_pluginController->setComplexTextInputEnabled(complexTextInputEnabled);
}
#endif
    
void PluginProxy::update(const IntRect& paintedRect)
{
    if (paintedRect == m_frameRect)
        m_pluginBackingStoreContainsValidData = true;

    IntRect paintedRectPluginCoordinates = paintedRect;
    paintedRectPluginCoordinates.move(-m_frameRect.x(), -m_frameRect.y());

    if (m_backingStore) {
        // Blit the plug-in backing store into our own backing store.
        OwnPtr<GraphicsContext> graphicsContext = m_backingStore->createGraphicsContext();
        graphicsContext->setCompositeOperation(CompositeCopy);
        m_pluginBackingStore->paint(*graphicsContext, paintedRectPluginCoordinates.location(), 
                                    paintedRectPluginCoordinates);
    }

    // Ask the controller to invalidate the rect for us.
    m_waitingForPaintInResponseToUpdate = true;
    m_pluginController->invalidate(paintedRectPluginCoordinates);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
