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

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "DataReference.h"
#include "NPRemoteObjectMap.h"
#include "NPRuntimeUtilities.h"
#include "NPVariantData.h"
#include "PluginController.h"
#include "PluginControllerProxyMessages.h"
#include "PluginCreationParameters.h"
#include "PluginProcessConnection.h"
#include "PluginProcessConnectionManager.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebKeyboardEvent.h"
#include "WebMouseEvent.h"
#include "WebProcess.h"
#include "WebProcessConnectionMessages.h"
#include "WebWheelEvent.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {
using namespace WebCore;

static uint64_t generatePluginInstanceID()
{
    static uint64_t uniquePluginInstanceID;
    return ++uniquePluginInstanceID;
}

Ref<PluginProxy> PluginProxy::create(uint64_t pluginProcessToken)
{
    return adoptRef(*new PluginProxy(pluginProcessToken));
}

PluginProxy::PluginProxy(uint64_t pluginProcessToken)
    : Plugin(PluginProxyType)
    , m_pluginProcessToken(pluginProcessToken)
    , m_pluginInstanceID(generatePluginInstanceID())
{
}

PluginProxy::~PluginProxy()
{
}

void PluginProxy::pluginProcessCrashed()
{
    controller()->pluginProcessCrashed();
}

bool PluginProxy::initialize(const Parameters& parameters)
{
    ASSERT(!m_connection);
    m_connection = WebProcess::singleton().pluginProcessConnectionManager().getPluginProcessConnection(m_pluginProcessToken);
    
    if (!m_connection)
        return false;
    
    // Add the plug-in proxy before creating the plug-in; it needs to be in the map because CreatePlugin
    // can call back out to the plug-in proxy.
    m_connection->addPluginProxy(this);

    // Ask the plug-in process to create a plug-in.
    m_pendingPluginCreationParameters = makeUnique<PluginCreationParameters>();

    m_pendingPluginCreationParameters->pluginInstanceID = m_pluginInstanceID;
    m_pendingPluginCreationParameters->windowNPObjectID = windowNPObjectID();
    m_pendingPluginCreationParameters->parameters = parameters;
    m_pendingPluginCreationParameters->userAgent = controller()->userAgent();
    m_pendingPluginCreationParameters->contentsScaleFactor = contentsScaleFactor();
    m_pendingPluginCreationParameters->isPrivateBrowsingEnabled = controller()->isPrivateBrowsingEnabled();
    m_pendingPluginCreationParameters->isMuted = controller()->isMuted();
    m_pendingPluginCreationParameters->artificialPluginInitializationDelayEnabled = controller()->artificialPluginInitializationDelayEnabled();
    m_pendingPluginCreationParameters->isAcceleratedCompositingEnabled = controller()->isAcceleratedCompositingEnabled();

    if (!canInitializeAsynchronously())
        return initializeSynchronously();

    // Remember that we tried to create this plug-in asynchronously in case we need to create it synchronously later.
    m_waitingOnAsynchronousInitialization = true;
    PluginCreationParameters creationParameters(*m_pendingPluginCreationParameters.get());
    m_connection->connection()->send(Messages::WebProcessConnection::CreatePluginAsynchronously(creationParameters), m_pluginInstanceID);
    return true;
}

bool PluginProxy::canInitializeAsynchronously() const
{
    return controller()->asynchronousPluginInitializationEnabled() && (m_connection->supportsAsynchronousPluginInitialization() || controller()->asynchronousPluginInitializationEnabledForAllPlugins());
}

bool PluginProxy::initializeSynchronously()
{
    ASSERT(m_pendingPluginCreationParameters);

    m_pendingPluginCreationParameters->asynchronousCreationIncomplete = m_waitingOnAsynchronousInitialization;
    bool result = false;
    bool wantsWheelEvents = false;
    uint32_t remoteLayerClientID = 0;
    
    PluginCreationParameters parameters(*m_pendingPluginCreationParameters.get());

    if (!m_connection->connection()->sendSync(Messages::WebProcessConnection::CreatePlugin(parameters), Messages::WebProcessConnection::CreatePlugin::Reply(result, wantsWheelEvents, remoteLayerClientID), 0) || !result)
        didFailToCreatePluginInternal();
    else
        didCreatePluginInternal(wantsWheelEvents, remoteLayerClientID);
    
    return result;
}

void PluginProxy::didCreatePlugin(bool wantsWheelEvents, uint32_t remoteLayerClientID, CompletionHandler<void()>&& completionHandler)
{
    // We might have tried to create the plug-in sychronously while waiting on the asynchronous reply,
    // in which case we should ignore this message.
    if (!m_waitingOnAsynchronousInitialization)
        return completionHandler();

    didCreatePluginInternal(wantsWheelEvents, remoteLayerClientID);
    completionHandler();
}

void PluginProxy::didFailToCreatePlugin(CompletionHandler<void()>&& completionHandler)
{
    // We might have tried to create the plug-in sychronously while waiting on the asynchronous reply,
    // in which case we should ignore this message.
    if (!m_waitingOnAsynchronousInitialization)
        return completionHandler();

    didFailToCreatePluginInternal();
    completionHandler();
}

void PluginProxy::didCreatePluginInternal(bool wantsWheelEvents, uint32_t remoteLayerClientID)
{
    m_wantsWheelEvents = wantsWheelEvents;
    m_remoteLayerClientID = remoteLayerClientID;
    m_isStarted = true;
    controller()->didInitializePlugin();

    // Whether synchronously or asynchronously, this plug-in was created and we shouldn't need to remember
    // anything about how.
    m_pendingPluginCreationParameters = nullptr;
    m_waitingOnAsynchronousInitialization = false;
}

void PluginProxy::didFailToCreatePluginInternal()
{
    // Calling out to the connection and the controller could potentially cause the plug-in proxy to go away, so protect it here.
    Ref<PluginProxy> protect(*this);

    m_connection->removePluginProxy(this);
    controller()->didFailToInitializePlugin();

    // Whether synchronously or asynchronously, this plug-in failed to create and we shouldn't need to remember
    // anything about how.
    m_pendingPluginCreationParameters = nullptr;
    m_waitingOnAsynchronousInitialization = false;
}

void PluginProxy::destroy()
{
    m_isStarted = false;

    if (!m_connection)
        return;

    // Although this message is sent synchronously, the Plugin process replies immediately (before performing any tasks) so this is only waiting for
    // confirmation that the Plugin process received the DestroyPlugin message.
    m_connection->connection()->sendSync(Messages::WebProcessConnection::DestroyPlugin(m_pluginInstanceID, m_waitingOnAsynchronousInitialization), Messages::WebProcessConnection::DestroyPlugin::Reply(), 0, 1_s);
    m_connection->removePluginProxy(this);
}

void PluginProxy::paint(GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    if (!needsBackingStore() || !m_backingStore)
        return;

    if (!m_pluginBackingStoreContainsValidData) {
        m_connection->connection()->sendSync(Messages::PluginControllerProxy::PaintEntirePlugin(), Messages::PluginControllerProxy::PaintEntirePlugin::Reply(), m_pluginInstanceID);
    
        // Blit the plug-in backing store into our own backing store.
        auto graphicsContext = m_backingStore->createGraphicsContext();
        if (graphicsContext) {
            graphicsContext->applyDeviceScaleFactor(contentsScaleFactor());
            graphicsContext->setCompositeOperation(CompositeOperator::Copy);

            m_pluginBackingStore->paint(*graphicsContext, contentsScaleFactor(), IntPoint(), pluginBounds());

            m_pluginBackingStoreContainsValidData = true;
        }
    }

    m_backingStore->paint(graphicsContext, contentsScaleFactor(), dirtyRect.location(), dirtyRect);

    if (m_waitingForPaintInResponseToUpdate) {
        m_waitingForPaintInResponseToUpdate = false;
        m_connection->connection()->send(Messages::PluginControllerProxy::DidUpdate(), m_pluginInstanceID);
    }
}

bool PluginProxy::supportsSnapshotting() const
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool isSupported = false;
    if (m_connection && !m_connection->connection()->sendSync(Messages::PluginControllerProxy::SupportsSnapshotting(), Messages::PluginControllerProxy::SupportsSnapshotting::Reply(isSupported), m_pluginInstanceID))
        return false;

    return isSupported;
}

RefPtr<ShareableBitmap> PluginProxy::snapshot()
{
    ShareableBitmap::Handle snapshotStoreHandle;
    m_connection->connection()->sendSync(Messages::PluginControllerProxy::Snapshot(), Messages::PluginControllerProxy::Snapshot::Reply(snapshotStoreHandle), m_pluginInstanceID);

    if (snapshotStoreHandle.isNull())
        return nullptr;

    return ShareableBitmap::create(snapshotStoreHandle);
}

bool PluginProxy::isTransparent()
{
    // This should never be called from the web process.
    ASSERT_NOT_REACHED();
    return false;
}

bool PluginProxy::wantsWheelEvents()
{
    return m_wantsWheelEvents;
}

void PluginProxy::geometryDidChange()
{
    ASSERT(m_isStarted);

    ShareableBitmap::Handle pluginBackingStoreHandle;

    if (updateBackingStore()) {
        // Create a new plug-in backing store.
        m_pluginBackingStore = ShareableBitmap::createShareable(m_backingStore->size(), { });
        if (!m_pluginBackingStore)
            return;

        // Create a handle to the plug-in backing store so we can send it over.
        if (!m_pluginBackingStore->createHandle(pluginBackingStoreHandle)) {
            m_pluginBackingStore = nullptr;
            return;
        }

        m_pluginBackingStoreContainsValidData = false;
    }

    m_connection->connection()->send(Messages::PluginControllerProxy::GeometryDidChange(m_pluginSize, m_clipRect, m_pluginToRootViewTransform, contentsScaleFactor(), pluginBackingStoreHandle), m_pluginInstanceID, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void PluginProxy::geometryDidChange(const IntSize& pluginSize, const IntRect& clipRect, const AffineTransform& pluginToRootViewTransform)
{
    if (pluginSize == m_pluginSize && m_clipRect == clipRect && m_pluginToRootViewTransform == pluginToRootViewTransform) {
        // Nothing to do.
        return;
    }
    
    m_pluginSize = pluginSize;
    m_clipRect = clipRect;
    m_pluginToRootViewTransform = pluginToRootViewTransform;

    geometryDidChange();
}

void PluginProxy::visibilityDidChange(bool isVisible)
{
    ASSERT(m_isStarted);
    m_connection->connection()->send(Messages::PluginControllerProxy::VisibilityDidChange(isVisible), m_pluginInstanceID);
}

void PluginProxy::frameDidFinishLoading(uint64_t requestID)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::FrameDidFinishLoading(requestID), m_pluginInstanceID);
}

void PluginProxy::frameDidFail(uint64_t requestID, bool wasCancelled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::FrameDidFail(requestID, wasCancelled), m_pluginInstanceID);
}

void PluginProxy::didEvaluateJavaScript(uint64_t requestID, const WTF::String& result)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::DidEvaluateJavaScript(requestID, result), m_pluginInstanceID);
}

void PluginProxy::streamWillSendRequest(uint64_t streamID, const URL& requestURL, const URL& responseURL, int responseStatus)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamWillSendRequest(streamID, requestURL.string(), responseURL.string(), responseStatus), m_pluginInstanceID);
}

void PluginProxy::streamDidReceiveResponse(uint64_t streamID, const URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& /* suggestedFileName */)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidReceiveResponse(streamID, responseURL.string(), streamLength, lastModifiedTime, mimeType, headers), m_pluginInstanceID);
}
                                           
void PluginProxy::streamDidReceiveData(uint64_t streamID, const uint8_t* bytes, int length)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidReceiveData(streamID, IPC::DataReference(bytes, length)), m_pluginInstanceID);
}

void PluginProxy::streamDidFinishLoading(uint64_t streamID)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidFinishLoading(streamID), m_pluginInstanceID);
}

void PluginProxy::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StreamDidFail(streamID, wasCancelled), m_pluginInstanceID);
}

void PluginProxy::manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength,  uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& /* suggestedFileName */)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::ManualStreamDidReceiveResponse(responseURL.string(), streamLength, lastModifiedTime, mimeType, headers), m_pluginInstanceID);
}

void PluginProxy::manualStreamDidReceiveData(const uint8_t* bytes, int length)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::ManualStreamDidReceiveData(IPC::DataReference(bytes, length)), m_pluginInstanceID);
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
    if (m_waitingOnAsynchronousInitialization)
        return false;

    m_connection->connection()->send(Messages::PluginControllerProxy::HandleMouseEvent(mouseEvent), m_pluginInstanceID);
    return true;
}

bool PluginProxy::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleWheelEvent(wheelEvent), Messages::PluginControllerProxy::HandleWheelEvent::Reply(handled), m_pluginInstanceID))
        return false;

    return handled;
}

bool PluginProxy::handleMouseEnterEvent(const WebMouseEvent& mouseEnterEvent)
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleMouseEnterEvent(mouseEnterEvent), Messages::PluginControllerProxy::HandleMouseEnterEvent::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

bool PluginProxy::handleMouseLeaveEvent(const WebMouseEvent& mouseLeaveEvent)
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleMouseLeaveEvent(mouseLeaveEvent), Messages::PluginControllerProxy::HandleMouseLeaveEvent::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

bool PluginProxy::handleContextMenuEvent(const WebMouseEvent&)
{
    // We don't know if the plug-in has handled mousedown event by displaying a context menu, so we never want WebKit to show a default one.
    return true;
}

bool PluginProxy::handleKeyboardEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleKeyboardEvent(keyboardEvent), Messages::PluginControllerProxy::HandleKeyboardEvent::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

void PluginProxy::setFocus(bool hasFocus)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::SetFocus(hasFocus), m_pluginInstanceID);
}

bool PluginProxy::handleEditingCommand(const String& commandName, const String& argument)
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandleEditingCommand(commandName, argument), Messages::PluginControllerProxy::HandleEditingCommand::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}
    
bool PluginProxy::isEditingCommandEnabled(const String& commandName)
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool enabled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::IsEditingCommandEnabled(commandName), Messages::PluginControllerProxy::IsEditingCommandEnabled::Reply(enabled), m_pluginInstanceID))
        return false;
    
    return enabled;
}
    
bool PluginProxy::handlesPageScaleFactor() const
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool handled = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::HandlesPageScaleFactor(), Messages::PluginControllerProxy::HandlesPageScaleFactor::Reply(handled), m_pluginInstanceID))
        return false;
    
    return handled;
}

bool PluginProxy::requiresUnifiedScaleFactor() const
{
    if (m_waitingOnAsynchronousInitialization)
        return false;

    bool required = false;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::RequiresUnifiedScaleFactor(), Messages::PluginControllerProxy::RequiresUnifiedScaleFactor::Reply(required), m_pluginInstanceID))
        return false;
    
    return required;
}

NPObject* PluginProxy::pluginScriptableNPObject()
{
    // Sending the synchronous Messages::PluginControllerProxy::GetPluginScriptableNPObject message can cause us to dispatch an
    // incoming synchronous message that ends up destroying the PluginProxy object.
    PluginController::PluginDestructionProtector protector(controller());

    uint64_t pluginScriptableNPObjectID = 0;
    
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::GetPluginScriptableNPObject(), Messages::PluginControllerProxy::GetPluginScriptableNPObject::Reply(pluginScriptableNPObjectID), m_pluginInstanceID))
        return 0;

    if (!pluginScriptableNPObjectID)
        return 0;

    return m_connection->npRemoteObjectMap()->createNPObjectProxy(pluginScriptableNPObjectID, this);
}

void PluginProxy::windowFocusChanged(bool hasFocus)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::WindowFocusChanged(hasFocus), m_pluginInstanceID);
}

void PluginProxy::windowVisibilityChanged(bool isVisible)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::WindowVisibilityChanged(isVisible), m_pluginInstanceID);
}

#if PLATFORM(COCOA)
void PluginProxy::windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::WindowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates), m_pluginInstanceID);
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

void PluginProxy::contentsScaleFactorChanged(float)
{
    geometryDidChange();
}

void PluginProxy::storageBlockingStateChanged(bool isStorageBlockingEnabled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::StorageBlockingStateChanged(isStorageBlockingEnabled), m_pluginInstanceID);
}

void PluginProxy::privateBrowsingStateChanged(bool isPrivateBrowsingEnabled)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::PrivateBrowsingStateChanged(isPrivateBrowsingEnabled), m_pluginInstanceID);
}

void PluginProxy::mutedStateChanged(bool isMuted)
{
    m_connection->connection()->send(Messages::PluginControllerProxy::MutedStateChanged(isMuted), m_pluginInstanceID);
}

bool PluginProxy::getFormValue(String& formValue)
{
    bool returnValue;
    if (!m_connection->connection()->sendSync(Messages::PluginControllerProxy::GetFormValue(), Messages::PluginControllerProxy::GetFormValue::Reply(returnValue, formValue), m_pluginInstanceID))
        return false;

    return returnValue;
}

bool PluginProxy::handleScroll(ScrollDirection, ScrollGranularity)
{
    return false;
}

Scrollbar* PluginProxy::horizontalScrollbar()
{
    return 0;
}

Scrollbar* PluginProxy::verticalScrollbar()
{
    return 0;
}

void PluginProxy::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    controller()->loadURL(requestID, method, urlString, target, headerFields, httpBody, allowPopups);
}

void PluginProxy::proxiesForURL(const String& urlString, CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(controller()->proxiesForURL(urlString));
}

void PluginProxy::cookiesForURL(const String& urlString, CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(controller()->cookiesForURL(urlString));
}

void PluginProxy::setCookiesForURL(const String& urlString, const String& cookieString)
{
    controller()->setCookiesForURL(urlString, cookieString);
}

void PluginProxy::getAuthenticationInfo(const ProtectionSpace& protectionSpace, CompletionHandler<void(bool returnValue, String username, String password)>&& completionHandler)
{
    String username;
    String password;
    bool returnValue = controller()->getAuthenticationInfo(protectionSpace, username, password);
    completionHandler(returnValue, username, password);
}

float PluginProxy::contentsScaleFactor()
{
    return controller()->contentsScaleFactor();
}

bool PluginProxy::updateBackingStore()
{
    if (m_pluginSize.isEmpty() || !needsBackingStore())
        return false;

    IntSize backingStoreSize = m_pluginSize;
    backingStoreSize.scale(contentsScaleFactor());

    if (m_backingStore) {
        if (m_backingStore->size() == backingStoreSize)
            return false;
        m_backingStore = nullptr; // Give malloc a chance to recycle our backing store.
    }

    m_backingStore = ShareableBitmap::create(backingStoreSize, { });
    return !!m_backingStore;
}

uint64_t PluginProxy::windowNPObjectID()
{
    NPObject* windowScriptNPObject = controller()->windowScriptNPObject();
    if (!windowScriptNPObject)
        return 0;

    uint64_t windowNPObjectID = m_connection->npRemoteObjectMap()->registerNPObject(windowScriptNPObject, this);
    releaseNPObject(windowScriptNPObject);

    return windowNPObjectID;
}

IntRect PluginProxy::pluginBounds()
{
    return IntRect(IntPoint(), m_pluginSize);
}

void PluginProxy::getPluginElementNPObject(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    NPObject* pluginElementNPObject = controller()->pluginElementNPObject();
    if (!pluginElementNPObject)
        return completionHandler(0);

    uint64_t pluginElementNPObjectID = m_connection->npRemoteObjectMap()->registerNPObject(pluginElementNPObject, this);
    releaseNPObject(pluginElementNPObject);
    completionHandler(pluginElementNPObjectID);
}

void PluginProxy::evaluate(const NPVariantData& npObjectAsVariantData, const String& scriptString, bool allowPopups, CompletionHandler<void(bool returnValue, NPVariantData&& resultData)>&& completionHandler)
{
    PluginController::PluginDestructionProtector protector(controller());

    NPVariant npObjectAsVariant = m_connection->npRemoteObjectMap()->npVariantDataToNPVariant(npObjectAsVariantData, this);
    if (!NPVARIANT_IS_OBJECT(npObjectAsVariant) || !(NPVARIANT_TO_OBJECT(npObjectAsVariant)))
        return completionHandler(false, { });

    NPVariant result;
    bool returnValue = controller()->evaluate(NPVARIANT_TO_OBJECT(npObjectAsVariant), scriptString, &result, allowPopups);
    if (!returnValue)
        return completionHandler(false, { });

    // Convert the NPVariant to an NPVariantData.
    NPVariantData resultData = m_connection->npRemoteObjectMap()->npVariantToNPVariantData(result, this);
    
    // And release the result.
    releaseNPVariantValue(&result);
    releaseNPVariantValue(&npObjectAsVariant);
    
    completionHandler(returnValue, WTFMove(resultData));
}

void PluginProxy::setPluginIsPlayingAudio(bool pluginIsPlayingAudio)
{
    controller()->setPluginIsPlayingAudio(pluginIsPlayingAudio);
}

void PluginProxy::continueStreamLoad(uint64_t streamID)
{
    controller()->continueStreamLoad(streamID);
}

void PluginProxy::cancelStreamLoad(uint64_t streamID)
{
    controller()->cancelStreamLoad(streamID);
}

void PluginProxy::cancelManualStreamLoad()
{
    controller()->cancelManualStreamLoad();
}

void PluginProxy::setStatusbarText(const String& statusbarText)
{
    controller()->setStatusbarText(statusbarText);
}

#if PLATFORM(X11)
void PluginProxy::createPluginContainer(CompletionHandler<void(uint64_t windowID)>&& completionHandler)
{
    completionHandler(controller()->createPluginContainer());
}

void PluginProxy::windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID)
{
    controller()->windowedPluginGeometryDidChange(frameRect, clipRect, windowID);
}

void PluginProxy::windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID)
{
    controller()->windowedPluginVisibilityDidChange(isVisible, windowID);
}
#endif

void PluginProxy::update(const IntRect& paintedRect)
{
    if (paintedRect == pluginBounds())
        m_pluginBackingStoreContainsValidData = true;

    if (m_backingStore) {
        // Blit the plug-in backing store into our own backing store.
        auto graphicsContext = m_backingStore->createGraphicsContext();
        if (graphicsContext) {
            graphicsContext->applyDeviceScaleFactor(contentsScaleFactor());
            graphicsContext->setCompositeOperation(CompositeOperator::Copy);
            m_pluginBackingStore->paint(*graphicsContext, contentsScaleFactor(), paintedRect.location(), paintedRect);
        }
    }

    // Ask the controller to invalidate the rect for us.
    m_waitingForPaintInResponseToUpdate = true;
    controller()->invalidate(paintedRect);
}

IntPoint PluginProxy::convertToRootView(const IntPoint& point) const
{
    return m_pluginToRootViewTransform.mapPoint(point);
}

RefPtr<WebCore::SharedBuffer> PluginProxy::liveResourceData() const
{
    return nullptr;
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
