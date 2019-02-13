/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Connection.h"
#include "Plugin.h"
#include "PluginController.h"
#include "PluginControllerProxyMessages.h"
#include "ShareableBitmap.h"
#include "WebProcessConnectionMessages.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/UserActivity.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>

namespace IPC {
    class DataReference;
}

namespace WebKit {

class LayerHostingContext;
class ShareableBitmap;
class WebProcessConnection;
struct PluginCreationParameters;

class PluginControllerProxy : PluginController {
    WTF_MAKE_NONCOPYABLE(PluginControllerProxy);

public:
    PluginControllerProxy(WebProcessConnection*, const PluginCreationParameters&);
    ~PluginControllerProxy();

    uint64_t pluginInstanceID() const { return m_pluginInstanceID; }

    bool initialize(const PluginCreationParameters&);
    void destroy();

    void didReceivePluginControllerProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncPluginControllerProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    bool wantsWheelEvents() const;

#if PLATFORM(COCOA)
    uint32_t remoteLayerClientID() const;
#endif

    PluginController* asPluginController() { return this; }

    bool isInitializing() const { return m_isInitializing; }
    
    void setInitializationReply(Messages::WebProcessConnection::CreatePlugin::DelayedReply&&);
    Messages::WebProcessConnection::CreatePlugin::DelayedReply takeInitializationReply();

private:
    void startPaintTimer();
    void paint();

    // PluginController
    void invalidate(const WebCore::IntRect&) override;
    String userAgent() override;
    void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups) override;
    void continueStreamLoad(uint64_t streamID) override;
    void cancelStreamLoad(uint64_t streamID) override;
    void cancelManualStreamLoad() override;
    NPObject* windowScriptNPObject() override;
    NPObject* pluginElementNPObject() override;
    bool evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups) override;
    void setPluginIsPlayingAudio(bool) override;
    void setStatusbarText(const String&) override;
    bool isAcceleratedCompositingEnabled() override;
    void pluginProcessCrashed() override;
    void didInitializePlugin() override;
    void didFailToInitializePlugin() override;

#if PLATFORM(COCOA)
    void pluginFocusOrWindowFocusChanged(bool) override;
    void setComplexTextInputState(PluginComplexTextInputState) override;
    const WTF::MachSendRight& compositingRenderServerPort() override;
#endif

    float contentsScaleFactor() override;
    String proxiesForURL(const String&) override;
    String cookiesForURL(const String&) override;
    void setCookiesForURL(const String& urlString, const String& cookieString) override;
    bool isPrivateBrowsingEnabled() override;
    bool isMuted() const override { return m_isMuted; }
    bool getAuthenticationInfo(const WebCore::ProtectionSpace&, String& username, String& password) override;
    void protectPluginFromDestruction() override;
    void unprotectPluginFromDestruction() override;
#if PLATFORM(X11)
    uint64_t createPluginContainer() override;
    void windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID) override;
    void windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID) override;
#endif
    
    // Message handlers.
    void frameDidFinishLoading(uint64_t requestID);
    void frameDidFail(uint64_t requestID, bool wasCancelled);
    void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform, float contentsScaleFactor, const ShareableBitmap::Handle& backingStoreHandle);
    void visibilityDidChange(bool isVisible);
    void didEvaluateJavaScript(uint64_t requestID, const String& result);
    void streamWillSendRequest(uint64_t streamID, const String& requestURLString, const String& redirectResponseURLString, uint32_t redirectResponseStatusCode);
    void streamDidReceiveResponse(uint64_t streamID, const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers);
    void streamDidReceiveData(uint64_t streamID, const IPC::DataReference& data);
    void streamDidFinishLoading(uint64_t streamID);
    void streamDidFail(uint64_t streamID, bool wasCancelled);
    void manualStreamDidReceiveResponse(const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers);
    void manualStreamDidReceiveData(const IPC::DataReference& data);
    void manualStreamDidFinishLoading();
    void manualStreamDidFail(bool wasCancelled);
    void handleMouseEvent(const WebMouseEvent&);
    void handleWheelEvent(const WebWheelEvent&, CompletionHandler<void(bool handled)>&&);
    void handleMouseEnterEvent(const WebMouseEvent&, CompletionHandler<void(bool handled)>&&);
    void handleMouseLeaveEvent(const WebMouseEvent&, CompletionHandler<void(bool handled)>&&);
    void handleKeyboardEvent(const WebKeyboardEvent&, CompletionHandler<void(bool handled)>&&);
    void handleEditingCommand(const String&, const String&, CompletionHandler<void(bool handled)>&&);
    void isEditingCommandEnabled(const String&, CompletionHandler<void(bool)>&&);
    void handlesPageScaleFactor(CompletionHandler<void(bool)>&&);
    void requiresUnifiedScaleFactor(CompletionHandler<void(bool)>&&);
    void paintEntirePlugin(CompletionHandler<void()>&&);
    void supportsSnapshotting(CompletionHandler<void(bool)>&&);
    void snapshot(CompletionHandler<void(ShareableBitmap::Handle&&)>);
    void setFocus(bool);
    void didUpdate();
    void getPluginScriptableNPObject(CompletionHandler<void(uint64_t pluginScriptableNPObjectID)>&&);

    void windowFocusChanged(bool);
    void windowVisibilityChanged(bool);
    void updateVisibilityActivity();

#if PLATFORM(COCOA)
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates);
    void sendComplexTextInput(const String& textInput);
    void setLayerHostingMode(uint32_t);

    void updateLayerHostingContext(LayerHostingMode);
#endif

    void storageBlockingStateChanged(bool);
    void privateBrowsingStateChanged(bool);
    void mutedStateChanged(bool);
    void getFormValue(CompletionHandler<void(bool returnValue, String&& formValue)>&&);

    void platformInitialize(const PluginCreationParameters&);
    void platformDestroy();
    void platformGeometryDidChange();

    WebProcessConnection* m_connection;
    uint64_t m_pluginInstanceID;

    String m_userAgent;
    bool m_storageBlockingEnabled;
    bool m_isPrivateBrowsingEnabled;
    bool m_isMuted;
    bool m_isAcceleratedCompositingEnabled;
    bool m_isInitializing;
    bool m_isVisible;
    bool m_isWindowVisible;

    Messages::WebProcessConnection::CreatePlugin::DelayedReply m_initializationReply;

    RefPtr<Plugin> m_plugin;

    WebCore::IntSize m_pluginSize;

    // The dirty rect in plug-in coordinates.
    WebCore::IntRect m_dirtyRect;

    // The paint timer, used for coalescing painting.
    RunLoop::Timer<PluginControllerProxy> m_paintTimer;
    
    // A counter used to prevent the plug-in from being destroyed.
    unsigned m_pluginDestructionProtectCount;

    // A timer that we use to prevent destruction of the plug-in while plug-in
    // code is on the stack.
    RunLoop::Timer<PluginControllerProxy> m_pluginDestroyTimer;

    // Whether we're waiting for the plug-in proxy in the web process to draw the contents of its
    // backing store into the web process backing store.
    bool m_waitingForDidUpdate;

    // Whether the plug-in has canceled the manual stream load.
    bool m_pluginCanceledManualStreamLoad;

#if PLATFORM(COCOA)
    // Whether complex text input is enabled for this plug-in.
    bool m_isComplexTextInputEnabled;

    // For CA plug-ins, this holds the information needed to export the layer hierarchy to the UI process.
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
#endif

    // The contents scale factor of this plug-in.
    float m_contentsScaleFactor;
    
    // The backing store that this plug-in draws into.
    RefPtr<ShareableBitmap> m_backingStore;

    // The window NPObject.
    NPObject* m_windowNPObject;

    // The plug-in element NPObject.
    NPObject* m_pluginElementNPObject;

    // Hold an activity when the plugin is visible to prevent throttling.
    UserActivity m_visiblityActivity;
};

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
