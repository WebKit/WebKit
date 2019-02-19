/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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
#include "PluginProcess.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/IntRect.h>
#include <WebCore/SecurityOrigin.h>
#include <memory>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS CALayer;
#endif

namespace WebCore {
    class HTTPHeaderMap;
    class ProtectionSpace;
}

namespace WebKit {

class ShareableBitmap;
class NPVariantData;
class PluginProcessConnection;

struct PluginCreationParameters;

class PluginProxy : public Plugin {
public:
    static Ref<PluginProxy> create(uint64_t pluginProcessToken, bool isRestartedProcess);
    ~PluginProxy();

    uint64_t pluginInstanceID() const { return m_pluginInstanceID; }
    void pluginProcessCrashed();

    void didReceivePluginProxyMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncPluginProxyMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    bool isBeingAsynchronouslyInitialized() const override { return m_waitingOnAsynchronousInitialization; }

private:
    explicit PluginProxy(uint64_t pluginProcessToken, bool isRestartedProcess);

    // Plugin
    bool initialize(const Parameters&) override;
    bool initializeSynchronously();

    void destroy() override;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    bool supportsSnapshotting() const override;
    RefPtr<ShareableBitmap> snapshot() override;
#if PLATFORM(COCOA)
    PlatformLayer* pluginLayer() override;
#endif
    bool isTransparent() override;
    bool wantsWheelEvents() override;
    void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    void visibilityDidChange(bool isVisible) override;
    void frameDidFinishLoading(uint64_t requestID) override;
    void frameDidFail(uint64_t requestID, bool wasCancelled) override;
    void didEvaluateJavaScript(uint64_t requestID, const String& result) override;
    void streamWillSendRequest(uint64_t streamID, const URL& requestURL, const URL& responseURL, int responseStatus) override;
    void streamDidReceiveResponse(uint64_t streamID, const URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) override;
    void streamDidFinishLoading(uint64_t streamID) override;
    void streamDidFail(uint64_t streamID, bool wasCancelled) override;
    void manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& suggestedFileName) override;
    void manualStreamDidReceiveData(const char* bytes, int length) override;
    void manualStreamDidFinishLoading() override;
    void manualStreamDidFail(bool wasCancelled) override;
    
    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override;
    bool handleMouseEnterEvent(const WebMouseEvent&) override;
    bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    bool handleContextMenuEvent(const WebMouseEvent&) override;
    bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    void setFocus(bool) override;
    bool handleEditingCommand(const String& commandName, const String& argument) override;
    bool isEditingCommandEnabled(const String& commandName) override;
    bool shouldAllowScripting() override { return true; }
    bool shouldAllowNavigationFromDrags() override { return false; }
    
    bool handlesPageScaleFactor() const override;
    bool requiresUnifiedScaleFactor() const override;
    
    NPObject* pluginScriptableNPObject() override;

    void windowFocusChanged(bool) override;
    void windowVisibilityChanged(bool) override;

#if PLATFORM(COCOA)
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) override;
    uint64_t pluginComplexTextInputIdentifier() const override;
    void sendComplexTextInput(const String& textInput) override;
    void setLayerHostingMode(LayerHostingMode) override;
#endif

    void contentsScaleFactorChanged(float) override;
    void storageBlockingStateChanged(bool) override;
    void privateBrowsingStateChanged(bool) override;
    void mutedStateChanged(bool) override;
    bool getFormValue(String& formValue) override;
    bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    WebCore::Scrollbar* horizontalScrollbar() override;
    WebCore::Scrollbar* verticalScrollbar() override;

    unsigned countFindMatches(const String&, WebCore::FindOptions, unsigned) override  { return 0; }
    bool findString(const String&, WebCore::FindOptions, unsigned) override { return false; }

    WebCore::IntPoint convertToRootView(const WebCore::IntPoint&) const override;

    RefPtr<WebCore::SharedBuffer> liveResourceData() const override;
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override { return false; }

    String getSelectionString() const override { return String(); }
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const override { return String(); }
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override { return false; }

    float contentsScaleFactor();
    bool needsBackingStore() const;
    bool updateBackingStore();
    uint64_t windowNPObjectID();
    WebCore::IntRect pluginBounds();

    void geometryDidChange();

    // Message handlers.
    void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups);
    void update(const WebCore::IntRect& paintedRect);
    void proxiesForURL(const String& urlString, CompletionHandler<void(String)>&&);
    void cookiesForURL(const String& urlString, CompletionHandler<void(String)>&&);
    void setCookiesForURL(const String& urlString, const String& cookieString);
    void getAuthenticationInfo(const WebCore::ProtectionSpace&, CompletionHandler<void(bool returnValue, String username, String password)>&&);
    void getPluginElementNPObject(CompletionHandler<void(uint64_t)>&&);
    void evaluate(const NPVariantData& npObjectAsVariantData, const String& scriptString, bool allowPopups, CompletionHandler<void(bool returnValue, NPVariantData&& resultData)>&&);
    void setPluginIsPlayingAudio(bool);
    void continueStreamLoad(uint64_t streamID);
    void cancelStreamLoad(uint64_t streamID);
    void cancelManualStreamLoad();
    void setStatusbarText(const String& statusbarText);
#if PLATFORM(COCOA)
    void pluginFocusOrWindowFocusChanged(bool);
    void setComplexTextInputState(uint64_t);
    void setLayerHostingContextID(uint32_t);
#endif
#if PLATFORM(X11)
    void createPluginContainer(CompletionHandler<void(uint64_t windowID)>&&);
    void windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID);
    void windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID);
#endif

    bool canInitializeAsynchronously() const;

    void didCreatePlugin(bool wantsWheelEvents, uint32_t remoteLayerClientID, CompletionHandler<void()>&&);
    void didFailToCreatePlugin(CompletionHandler<void()>&&);

    void didCreatePluginInternal(bool wantsWheelEvents, uint32_t remoteLayerClientID);
    void didFailToCreatePluginInternal();

    uint64_t m_pluginProcessToken;

    RefPtr<PluginProcessConnection> m_connection;
    uint64_t m_pluginInstanceID;

    WebCore::IntSize m_pluginSize;

    // The clip rect in plug-in coordinates.
    WebCore::IntRect m_clipRect;

    // A transform that can be used to convert from root view coordinates to plug-in coordinates.
    WebCore::AffineTransform m_pluginToRootViewTransform;

    // This is the backing store that we paint when we're told to paint.
    RefPtr<ShareableBitmap> m_backingStore;

    // This is the shared memory backing store that the plug-in paints into. When the plug-in tells us
    // that it's painted something in it, we'll blit from it to our own backing store.
    RefPtr<ShareableBitmap> m_pluginBackingStore;
    
    // Whether all of the plug-in backing store contains valid data.
    bool m_pluginBackingStoreContainsValidData;

    bool m_isStarted;

    // Whether we're called invalidate in response to an update call, and are now waiting for a paint call.
    bool m_waitingForPaintInResponseToUpdate;

    // Whether we should send wheel events to this plug-in or not.
    bool m_wantsWheelEvents;

    // The client ID for the CA layer in the plug-in process. Will be 0 if the plug-in is not a CA plug-in.
    uint32_t m_remoteLayerClientID;
    
    std::unique_ptr<PluginCreationParameters> m_pendingPluginCreationParameters;
    bool m_waitingOnAsynchronousInitialization;

#if PLATFORM(COCOA)
    RetainPtr<CALayer> m_pluginLayer;
#endif

    bool m_isRestartedProcess;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_PLUGIN(PluginProxy, PluginProxyType)

#endif // ENABLE(NETSCAPE_PLUGIN_API)

