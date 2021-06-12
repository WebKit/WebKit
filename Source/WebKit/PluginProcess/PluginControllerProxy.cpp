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
#include "PluginControllerProxy.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NPObjectProxy.h"
#include "NPRemoteObjectMap.h"
#include "NPRuntimeUtilities.h"
#include "NPVariantData.h"
#include "NetscapePlugin.h"
#include "PluginCreationParameters.h"
#include "PluginProcess.h"
#include "PluginProxyMessages.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessConnection.h"
#include "WebProcessConnectionMessages.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/IdentifierRep.h>
#include <WebCore/NotImplemented.h>
#include <wtf/SetForScope.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "LayerHostingContext.h"
#endif

namespace WebKit {
using namespace WebCore;

PluginControllerProxy::PluginControllerProxy(WebProcessConnection* connection, const PluginCreationParameters& creationParameters)
    : m_connection(connection)
    , m_pluginInstanceID(creationParameters.pluginInstanceID)
    , m_userAgent(creationParameters.userAgent)
    , m_isPrivateBrowsingEnabled(creationParameters.isPrivateBrowsingEnabled)
    , m_isMuted(creationParameters.isMuted)
    , m_isAcceleratedCompositingEnabled(creationParameters.isAcceleratedCompositingEnabled)
    , m_isInitializing(false)
    , m_isVisible(false)
    , m_isWindowVisible(false)
    , m_paintTimer(RunLoop::main(), this, &PluginControllerProxy::paint)
    , m_pluginDestructionProtectCount(0)
    , m_pluginDestroyTimer(RunLoop::main(), this, &PluginControllerProxy::destroy)
    , m_waitingForDidUpdate(false)
    , m_pluginCanceledManualStreamLoad(false)
#if PLATFORM(COCOA)
    , m_isComplexTextInputEnabled(false)
#endif
    , m_contentsScaleFactor(creationParameters.contentsScaleFactor)
    , m_windowNPObject(0)
    , m_pluginElementNPObject(0)
    , m_visiblityActivity("Plugin is visible.")
{
}

PluginControllerProxy::~PluginControllerProxy()
{
    ASSERT(!m_plugin);

    if (m_windowNPObject)
        releaseNPObject(m_windowNPObject);

    if (m_pluginElementNPObject)
        releaseNPObject(m_pluginElementNPObject);
}

void PluginControllerProxy::setInitializationReply(Messages::WebProcessConnection::CreatePlugin::DelayedReply&& reply)
{
    ASSERT(!m_initializationReply);
    m_initializationReply = WTFMove(reply);
}

Messages::WebProcessConnection::CreatePlugin::DelayedReply PluginControllerProxy::takeInitializationReply()
{
    return std::exchange(m_initializationReply, nullptr);
}

bool PluginControllerProxy::initialize(const PluginCreationParameters& creationParameters)
{
    ASSERT(!m_plugin);

    ASSERT(!m_isInitializing);
    m_isInitializing = true; // Cannot use SetForScope here, because this object can be deleted before the function returns.

    m_plugin = NetscapePlugin::create(PluginProcess::singleton().netscapePluginModule());
    if (!m_plugin) {
        // This will delete the plug-in controller proxy object.
        m_connection->removePluginControllerProxy(this, 0);
        return false;
    }

    if (creationParameters.windowNPObjectID)
        m_windowNPObject = m_connection->npRemoteObjectMap()->createNPObjectProxy(creationParameters.windowNPObjectID, m_plugin.get());

    bool returnValue = m_plugin->initialize(*this, creationParameters.parameters);

    if (!returnValue) {
        // Get the plug-in so we can pass it to removePluginControllerProxy. The pointer is only
        // used as an identifier so it's OK to just get a weak reference.
        Plugin* plugin = m_plugin.get();
        
        m_plugin = nullptr;

        // This will delete the plug-in controller proxy object.
        m_connection->removePluginControllerProxy(this, plugin);
        return false;
    }

    platformInitialize(creationParameters);

    m_isInitializing = false;
    return true;
}

void PluginControllerProxy::destroy()
{
    ASSERT(m_plugin);

    // FIXME: Consider removing m_pluginDestructionProtectCount and always use inSendSync here.
    if (m_pluginDestructionProtectCount || m_connection->connection()->inSendSync()) {
        // We have plug-in code on the stack so we can't destroy it right now.
        // Destroy it later.
        m_pluginDestroyTimer.startOneShot(0_s);
        return;
    }

    // Get the plug-in so we can pass it to removePluginControllerProxy. The pointer is only
    // used as an identifier so it's OK to just get a weak reference.
    Plugin* plugin = m_plugin.get();

    m_plugin->destroyPlugin();
    m_plugin = nullptr;

    platformDestroy();

    // This will delete the plug-in controller proxy object.
    m_connection->removePluginControllerProxy(this, plugin);
}

bool PluginControllerProxy::wantsWheelEvents() const
{
    return m_plugin->wantsWheelEvents();
}

void PluginControllerProxy::paint()
{
    ASSERT(!m_dirtyRect.isEmpty());
    m_paintTimer.stop();

    if (!m_backingStore)
        return;

    IntRect dirtyRect = m_dirtyRect;
    m_dirtyRect = IntRect();

    ASSERT(m_plugin);

    // Create a graphics context.
    auto graphicsContext = m_backingStore->createGraphicsContext();
    if (!graphicsContext)
        return;

#if PLATFORM(COCOA)
    // FIXME: We should really call applyDeviceScaleFactor instead of scale, but that ends up calling into WKSI
    // which we currently don't have initiated in the plug-in process.
    graphicsContext->scale(m_contentsScaleFactor);
#endif

    if (m_plugin->isTransparent())
        graphicsContext->clearRect(dirtyRect);

    m_plugin->paint(*graphicsContext, dirtyRect);

    m_connection->connection()->send(Messages::PluginProxy::Update(dirtyRect), m_pluginInstanceID);
}

void PluginControllerProxy::startPaintTimer()
{
    // Check if we should start the timer.
    
    if (m_dirtyRect.isEmpty())
        return;

    // FIXME: Check clip rect.
    
    if (m_paintTimer.isActive())
        return;

    if (m_waitingForDidUpdate)
        return;

    // Start the timer.
    m_paintTimer.startOneShot(0_s);

    m_waitingForDidUpdate = true;
}

void PluginControllerProxy::invalidate(const IntRect& rect)
{
    IntRect dirtyRect = rect;

    // Make sure that the dirty rect is not greater than the plug-in itself.
    dirtyRect.intersect(IntRect(IntPoint(), m_pluginSize));
    m_dirtyRect.unite(dirtyRect);

    startPaintTimer();
}

String PluginControllerProxy::userAgent()
{
    return m_userAgent;
}

void PluginControllerProxy::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    m_connection->connection()->send(Messages::PluginProxy::LoadURL(requestID, method, urlString, target, headerFields, httpBody, allowPopups), m_pluginInstanceID);
}

void PluginControllerProxy::continueStreamLoad(uint64_t streamID)
{
    m_connection->connection()->send(Messages::PluginProxy::ContinueStreamLoad(streamID), m_pluginInstanceID);
}

void PluginControllerProxy::cancelStreamLoad(uint64_t streamID)
{
    m_connection->connection()->send(Messages::PluginProxy::CancelStreamLoad(streamID), m_pluginInstanceID);
}

void PluginControllerProxy::cancelManualStreamLoad()
{
    m_pluginCanceledManualStreamLoad = true;

    m_connection->connection()->send(Messages::PluginProxy::CancelManualStreamLoad(), m_pluginInstanceID);
}

NPObject* PluginControllerProxy::windowScriptNPObject()
{
    if (!m_windowNPObject)
        return 0;

    retainNPObject(m_windowNPObject);
    return m_windowNPObject;
}

NPObject* PluginControllerProxy::pluginElementNPObject()
{
    if (!m_pluginElementNPObject) {
        uint64_t pluginElementNPObjectID = 0;

        if (!m_connection->connection()->sendSync(Messages::PluginProxy::GetPluginElementNPObject(), Messages::PluginProxy::GetPluginElementNPObject::Reply(pluginElementNPObjectID), m_pluginInstanceID))
            return 0;

        if (!pluginElementNPObjectID)
            return 0;

        m_pluginElementNPObject = m_connection->npRemoteObjectMap()->createNPObjectProxy(pluginElementNPObjectID, m_plugin.get());
        ASSERT(m_pluginElementNPObject);
    }

    retainNPObject(m_pluginElementNPObject);
    return m_pluginElementNPObject;
}

bool PluginControllerProxy::evaluate(NPObject* npObject, const String& scriptString, NPVariant* result, bool allowPopups)
{
    PluginDestructionProtector protector(this);

    NPVariant npObjectAsNPVariant;
    OBJECT_TO_NPVARIANT(npObject, npObjectAsNPVariant);

    // Send the NPObject over as an NPVariantData.
    NPVariantData npObjectAsNPVariantData = m_connection->npRemoteObjectMap()->npVariantToNPVariantData(npObjectAsNPVariant, m_plugin.get());

    bool returnValue = false;
    NPVariantData resultData;

    if (!m_connection->connection()->sendSync(Messages::PluginProxy::Evaluate(npObjectAsNPVariantData, scriptString, allowPopups), Messages::PluginProxy::Evaluate::Reply(returnValue, resultData), m_pluginInstanceID))
        return false;

    if (!returnValue)
        return false;

    *result = m_connection->npRemoteObjectMap()->npVariantDataToNPVariant(resultData, m_plugin.get());
    return true;
}

void PluginControllerProxy::setPluginIsPlayingAudio(bool pluginIsPlayingAudio)
{
    m_connection->connection()->send(Messages::PluginProxy::SetPluginIsPlayingAudio(pluginIsPlayingAudio), m_pluginInstanceID);
}

void PluginControllerProxy::setStatusbarText(const String& statusbarText)
{
    m_connection->connection()->send(Messages::PluginProxy::SetStatusbarText(statusbarText), m_pluginInstanceID);
}

bool PluginControllerProxy::isAcceleratedCompositingEnabled()
{
    return m_isAcceleratedCompositingEnabled;
}

void PluginControllerProxy::pluginProcessCrashed()
{
    // This should never be called from here.
    ASSERT_NOT_REACHED();
}

void PluginControllerProxy::didInitializePlugin()
{
    // This should only be called on the plugin in the web process.
    ASSERT_NOT_REACHED();
}

void PluginControllerProxy::didFailToInitializePlugin()
{
    // This should only be called on the plugin in the web process.
    ASSERT_NOT_REACHED();
}

float PluginControllerProxy::contentsScaleFactor()
{
    return m_contentsScaleFactor;
}

String PluginControllerProxy::proxiesForURL(const String& urlString)
{
    String proxyString;

    if (!m_connection->connection()->sendSync(Messages::PluginProxy::ProxiesForURL(urlString), Messages::PluginProxy::ProxiesForURL::Reply(proxyString), m_pluginInstanceID))
        return String();

    return proxyString;
}

String PluginControllerProxy::cookiesForURL(const String& urlString)
{
    String cookieString;

    if (!m_connection->connection()->sendSync(Messages::PluginProxy::CookiesForURL(urlString), Messages::PluginProxy::CookiesForURL::Reply(cookieString), m_pluginInstanceID))
        return String();

    return cookieString;
}

void PluginControllerProxy::setCookiesForURL(const String& urlString, const String& cookieString)
{
    m_connection->connection()->send(Messages::PluginProxy::SetCookiesForURL(urlString, cookieString), m_pluginInstanceID);
}

bool PluginControllerProxy::isPrivateBrowsingEnabled()
{
    return m_isPrivateBrowsingEnabled;
}

bool PluginControllerProxy::getAuthenticationInfo(const ProtectionSpace& protectionSpace, String& username, String& password)
{
    bool returnValue;
    if (!m_connection->connection()->sendSync(Messages::PluginProxy::GetAuthenticationInfo(protectionSpace), Messages::PluginProxy::GetAuthenticationInfo::Reply(returnValue, username, password), m_pluginInstanceID))
        return false;

    return returnValue;
}

void PluginControllerProxy::protectPluginFromDestruction()
{
    m_pluginDestructionProtectCount++;
}

void PluginControllerProxy::unprotectPluginFromDestruction()
{
    ASSERT(m_pluginDestructionProtectCount);

    m_pluginDestructionProtectCount--;
}

void PluginControllerProxy::frameDidFinishLoading(uint64_t requestID)
{
    m_plugin->frameDidFinishLoading(requestID);
}

void PluginControllerProxy::frameDidFail(uint64_t requestID, bool wasCancelled)
{
    m_plugin->frameDidFail(requestID, wasCancelled);
}

void PluginControllerProxy::geometryDidChange(const IntSize& pluginSize, const IntRect& clipRect, const AffineTransform& pluginToRootViewTransform, float contentsScaleFactor, const ShareableBitmap::Handle& backingStoreHandle)
{
    ASSERT(m_plugin);

    m_pluginSize = pluginSize;

    if (contentsScaleFactor != m_contentsScaleFactor) {
        m_contentsScaleFactor = contentsScaleFactor;
        m_plugin->contentsScaleFactorChanged(m_contentsScaleFactor);
    }

    platformGeometryDidChange();

    if (!backingStoreHandle.isNull()) {
        // Create a new backing store.
        m_backingStore = ShareableBitmap::create(backingStoreHandle);
    }

    m_plugin->geometryDidChange(pluginSize, clipRect, pluginToRootViewTransform);
}

void PluginControllerProxy::visibilityDidChange(bool isVisible)
{
    m_isVisible = isVisible;
    
    ASSERT(m_plugin);
    m_plugin->visibilityDidChange(isVisible);

    updateVisibilityActivity();
}

void PluginControllerProxy::windowFocusChanged(bool hasFocus)
{
    ASSERT(m_plugin);
    m_plugin->windowFocusChanged(hasFocus);
}

void PluginControllerProxy::windowVisibilityChanged(bool isVisible)
{
    m_isWindowVisible = isVisible;

    ASSERT(m_plugin);
    m_plugin->windowVisibilityChanged(isVisible);

    updateVisibilityActivity();
}

void PluginControllerProxy::updateVisibilityActivity()
{
    if (m_isVisible && m_isWindowVisible)
        m_visiblityActivity.start();
    else
        m_visiblityActivity.stop();
}

void PluginControllerProxy::didEvaluateJavaScript(uint64_t requestID, const String& result)
{
    m_plugin->didEvaluateJavaScript(requestID, result);
}

void PluginControllerProxy::streamWillSendRequest(uint64_t streamID, const String& requestURLString, const String& redirectResponseURLString, uint32_t redirectResponseStatusCode)
{
    m_plugin->streamWillSendRequest(streamID, URL({ }, requestURLString), URL({ }, redirectResponseURLString), redirectResponseStatusCode);
}

void PluginControllerProxy::streamDidReceiveResponse(uint64_t streamID, const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers)
{
    m_plugin->streamDidReceiveResponse(streamID, URL({ }, responseURLString), streamLength, lastModifiedTime, mimeType, headers, String());
}

void PluginControllerProxy::streamDidReceiveData(uint64_t streamID, const IPC::DataReference& data)
{
    m_plugin->streamDidReceiveData(streamID, data.data(), data.size());
}

void PluginControllerProxy::streamDidFinishLoading(uint64_t streamID)
{
    m_plugin->streamDidFinishLoading(streamID);
}

void PluginControllerProxy::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    m_plugin->streamDidFail(streamID, wasCancelled);
}

void PluginControllerProxy::manualStreamDidReceiveResponse(const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers)
{
    if (m_pluginCanceledManualStreamLoad)
        return;

    m_plugin->manualStreamDidReceiveResponse(URL({ }, responseURLString), streamLength, lastModifiedTime, mimeType, headers, String());
}

void PluginControllerProxy::manualStreamDidReceiveData(const IPC::DataReference& data)
{
    if (m_pluginCanceledManualStreamLoad)
        return;

    m_plugin->manualStreamDidReceiveData(data.data(), data.size());
}

void PluginControllerProxy::manualStreamDidFinishLoading()
{
    if (m_pluginCanceledManualStreamLoad)
        return;
    
    m_plugin->manualStreamDidFinishLoading();
}

void PluginControllerProxy::manualStreamDidFail(bool wasCancelled)
{
    if (m_pluginCanceledManualStreamLoad)
        return;
    
    m_plugin->manualStreamDidFail(wasCancelled);
}

void PluginControllerProxy::handleMouseEvent(const WebMouseEvent& mouseEvent)
{
    m_plugin->handleMouseEvent(mouseEvent);
}

void PluginControllerProxy::handleWheelEvent(const WebWheelEvent& wheelEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->handleWheelEvent(wheelEvent));
}

void PluginControllerProxy::handleMouseEnterEvent(const WebMouseEvent& mouseEnterEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->handleMouseEnterEvent(mouseEnterEvent));
}

void PluginControllerProxy::handleMouseLeaveEvent(const WebMouseEvent& mouseLeaveEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->handleMouseLeaveEvent(mouseLeaveEvent));
}

void PluginControllerProxy::handleKeyboardEvent(const WebKeyboardEvent& keyboardEvent, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->handleKeyboardEvent(keyboardEvent));
}

void PluginControllerProxy::handleEditingCommand(const String& commandName, const String& argument, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->handleEditingCommand(commandName, argument));
}
    
void PluginControllerProxy::isEditingCommandEnabled(const String& commandName, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->isEditingCommandEnabled(commandName));
}
    
void PluginControllerProxy::handlesPageScaleFactor(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->handlesPageScaleFactor());
}

void PluginControllerProxy::requiresUnifiedScaleFactor(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->requiresUnifiedScaleFactor());
}

void PluginControllerProxy::paintEntirePlugin(CompletionHandler<void()>&& completionHandler)
{
    if (m_pluginSize.isEmpty())
        return completionHandler();

    m_dirtyRect = IntRect(IntPoint(), m_pluginSize);
    paint();
    completionHandler();
}

void PluginControllerProxy::supportsSnapshotting(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(m_plugin->supportsSnapshotting());
}

void PluginControllerProxy::snapshot(CompletionHandler<void(ShareableBitmap::Handle&&)> completionHandler)
{
    ASSERT(m_plugin);
    RefPtr<ShareableBitmap> bitmap = m_plugin->snapshot();
    if (!bitmap)
        return completionHandler({ });

    ShareableBitmap::Handle backingStoreHandle;
    bitmap->createHandle(backingStoreHandle);
    completionHandler(WTFMove(backingStoreHandle));
}

void PluginControllerProxy::setFocus(bool hasFocus)
{
    m_plugin->setFocus(hasFocus);
}

void PluginControllerProxy::didUpdate()
{
    m_waitingForDidUpdate = false;
    startPaintTimer();
}

void PluginControllerProxy::getPluginScriptableNPObject(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    NPObject* pluginScriptableNPObject = m_plugin->pluginScriptableNPObject();
    if (!pluginScriptableNPObject)
        return completionHandler(0);
    
    uint64_t pluginScriptableNPObjectID = m_connection->npRemoteObjectMap()->registerNPObject(pluginScriptableNPObject, m_plugin.get());
    releaseNPObject(pluginScriptableNPObject);
    completionHandler(pluginScriptableNPObjectID);
}

void PluginControllerProxy::storageBlockingStateChanged(bool isStorageBlockingEnabled)
{
    if (m_storageBlockingEnabled != isStorageBlockingEnabled) {
        m_storageBlockingEnabled = isStorageBlockingEnabled;
        m_plugin->storageBlockingStateChanged(m_storageBlockingEnabled);
    }
}

void PluginControllerProxy::privateBrowsingStateChanged(bool isPrivateBrowsingEnabled)
{
    m_isPrivateBrowsingEnabled = isPrivateBrowsingEnabled;

    m_plugin->privateBrowsingStateChanged(isPrivateBrowsingEnabled);
}

void PluginControllerProxy::mutedStateChanged(bool isMuted)
{
    if (m_isMuted == isMuted)
        return;
    
    m_isMuted = isMuted;
    m_plugin->mutedStateChanged(isMuted);
}

void PluginControllerProxy::getFormValue(CompletionHandler<void(bool, String&&)>&& completionHandler)
{
    String formValue;
    bool returnValue = m_plugin->getFormValue(formValue);
    completionHandler(returnValue, WTFMove(formValue));
}

#if PLATFORM(X11)
uint64_t PluginControllerProxy::createPluginContainer()
{
    uint64_t windowID = 0;
    m_connection->connection()->sendSync(Messages::PluginProxy::CreatePluginContainer(), Messages::PluginProxy::CreatePluginContainer::Reply(windowID), m_pluginInstanceID);
    return windowID;
}

void PluginControllerProxy::windowedPluginGeometryDidChange(const IntRect& frameRect, const IntRect& clipRect, uint64_t windowID)
{
    m_connection->connection()->send(Messages::PluginProxy::WindowedPluginGeometryDidChange(frameRect, clipRect, windowID), m_pluginInstanceID);
}

void PluginControllerProxy::windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID)
{
    m_connection->connection()->send(Messages::PluginProxy::WindowedPluginVisibilityDidChange(isVisible, windowID), m_pluginInstanceID);
}
#endif

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
