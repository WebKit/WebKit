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

#ifndef PluginProxy_h
#define PluginProxy_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "Connection.h"
#include "Plugin.h"
#include "PluginProcess.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/IntRect.h>
#include <WebCore/SecurityOrigin.h>
#include <memory>

#if PLATFORM(COCOA)
#include "WebHitTestResult.h"
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

    void didReceivePluginProxyMessage(IPC::Connection&, IPC::MessageDecoder&);
    void didReceiveSyncPluginProxyMessage(IPC::Connection&, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

    bool isBeingAsynchronouslyInitialized() const override { return m_waitingOnAsynchronousInitialization; }

private:
    explicit PluginProxy(uint64_t pluginProcessToken, bool isRestartedProcess);

    // Plugin
    virtual bool initialize(const Parameters&) override;
    bool initializeSynchronously();

    virtual void destroy() override;
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect) override;
    virtual bool supportsSnapshotting() const override;
    virtual PassRefPtr<ShareableBitmap> snapshot() override;
#if PLATFORM(COCOA)
    virtual PlatformLayer* pluginLayer() override;
#endif
    virtual bool isTransparent() override;
    virtual bool wantsWheelEvents() override;
    virtual void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    virtual void visibilityDidChange(bool isVisible) override;
    virtual void frameDidFinishLoading(uint64_t requestID) override;
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled) override;
    virtual void didEvaluateJavaScript(uint64_t requestID, const String& result) override;
    virtual void streamDidReceiveResponse(uint64_t streamID, const WebCore::URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    virtual void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) override;
    virtual void streamDidFinishLoading(uint64_t streamID) override;
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled) override;
    virtual void manualStreamDidReceiveResponse(const WebCore::URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& suggestedFileName) override;
    virtual void manualStreamDidReceiveData(const char* bytes, int length) override;
    virtual void manualStreamDidFinishLoading() override;
    virtual void manualStreamDidFail(bool wasCancelled) override;
    
    virtual bool handleMouseEvent(const WebMouseEvent&) override;
    virtual bool handleWheelEvent(const WebWheelEvent&) override;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) override;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) override;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    virtual void setFocus(bool) override;
    virtual bool handleEditingCommand(const String& commandName, const String& argument) override;
    virtual bool isEditingCommandEnabled(const String& commandName) override;
    virtual bool shouldAllowScripting() override { return true; }
    virtual bool shouldAllowNavigationFromDrags() override { return false; }
    
    virtual bool handlesPageScaleFactor() override;
    
    virtual NPObject* pluginScriptableNPObject() override;

    virtual void windowFocusChanged(bool) override;
    virtual void windowVisibilityChanged(bool) override;

#if PLATFORM(COCOA)
    virtual void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) override;
    virtual uint64_t pluginComplexTextInputIdentifier() const override;
    virtual void sendComplexTextInput(const String& textInput) override;
    virtual void setLayerHostingMode(LayerHostingMode) override;
#endif

    virtual void contentsScaleFactorChanged(float) override;
    virtual void storageBlockingStateChanged(bool) override;
    virtual void privateBrowsingStateChanged(bool) override;
    virtual void mutedStateChanged(bool) override;
    virtual bool getFormValue(String& formValue) override;
    virtual bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    virtual WebCore::Scrollbar* horizontalScrollbar() override;
    virtual WebCore::Scrollbar* verticalScrollbar() override;

    virtual unsigned countFindMatches(const String&, WebCore::FindOptions, unsigned) override  { return 0; }
    virtual bool findString(const String&, WebCore::FindOptions, unsigned) override { return false; }

    virtual WebCore::IntPoint convertToRootView(const WebCore::IntPoint&) const override;

    virtual PassRefPtr<WebCore::SharedBuffer> liveResourceData() const override;
    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override { return false; }

    String getSelectionString() const override { return String(); }
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const override { return String(); }
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override { return false; }

#if PLATFORM(COCOA)
    virtual WebCore::AudioHardwareActivityType audioHardwareActivity() const override;
#endif

    float contentsScaleFactor();
    bool needsBackingStore() const;
    bool updateBackingStore();
    uint64_t windowNPObjectID();
    WebCore::IntRect pluginBounds();

    void geometryDidChange();

    // Message handlers.
    void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups);
    void update(const WebCore::IntRect& paintedRect);
    void proxiesForURL(const String& urlString, String& proxyString);
    void cookiesForURL(const String& urlString, String& cookieString);
    void setCookiesForURL(const String& urlString, const String& cookieString);
    void getAuthenticationInfo(const WebCore::ProtectionSpace&, bool& returnValue, String& username, String& password);
    void getPluginElementNPObject(uint64_t& pluginElementNPObjectID);
    void evaluate(const NPVariantData& npObjectAsVariantData, const String& scriptString, bool allowPopups, bool& returnValue, NPVariantData& resultData);
    void setPluginIsPlayingAudio(bool);
    void cancelStreamLoad(uint64_t streamID);
    void cancelManualStreamLoad();
    void setStatusbarText(const String& statusbarText);
#if PLATFORM(COCOA)
    void pluginFocusOrWindowFocusChanged(bool);
    void setComplexTextInputState(uint64_t);
    void setLayerHostingContextID(uint32_t);
#endif
#if PLUGIN_ARCHITECTURE(X11)
    void createPluginContainer(uint64_t& windowID);
    void windowedPluginGeometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect, uint64_t windowID);
    void windowedPluginVisibilityDidChange(bool isVisible, uint64_t windowID);
#endif

    bool canInitializeAsynchronously() const;

    void didCreatePlugin(bool wantsWheelEvents, uint32_t remoteLayerClientID);
    void didFailToCreatePlugin();

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

#endif // PluginProxy_h
