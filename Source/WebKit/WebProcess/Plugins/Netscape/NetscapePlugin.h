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

#include "NetscapePluginModule.h"
#include "Plugin.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WTF {
class MachSendRight;
}

namespace WebCore {
class HTTPHeaderMap;
class ProtectionSpace;
class SharedBuffer;
}

namespace WebKit {

class NetscapePluginStream;
class NetscapePluginUnix;

class NetscapePlugin : public Plugin {
public:
    static RefPtr<NetscapePlugin> create(RefPtr<NetscapePluginModule>&&);
    virtual ~NetscapePlugin();

    static RefPtr<NetscapePlugin> fromNPP(NPP);

    // In-process NetscapePlugins don't support asynchronous initialization.
    bool isBeingAsynchronouslyInitialized() const override { return false; }

#if PLATFORM(COCOA)
    NPError setDrawingModel(NPDrawingModel);
    NPError setEventModel(NPEventModel);
    NPBool convertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double& destX, double& destY, NPCoordinateSpace destSpace);
    NPError popUpContextMenu(NPMenu*);

    void setPluginReturnsNonretainedLayer(bool pluginReturnsNonretainedLayer) { m_pluginReturnsNonretainedLayer = pluginReturnsNonretainedLayer; }
    void setPluginWantsLegacyCocoaTextInput(bool pluginWantsLegacyCocoaTextInput) { m_pluginWantsLegacyCocoaTextInput = pluginWantsLegacyCocoaTextInput; }

    bool hasHandledAKeyDownEvent() const { return m_hasHandledAKeyDownEvent; }

    const WTF::MachSendRight& compositingRenderServerPort();

    // Computes an affine transform from the given coordinate space to the screen coordinate space.
    bool getScreenTransform(NPCoordinateSpace sourceSpace, WebCore::AffineTransform&);
#endif

#if PLUGIN_ARCHITECTURE(UNIX)
    const WebCore::IntRect& frameRectInWindowCoordinates() const { return m_frameRectInWindowCoordinates; }
#endif
    const WebCore::IntRect& clipRect() const { return m_clipRect; }
    const WebCore::IntSize& size() const { return m_pluginSize; }

    PluginQuirks quirks() const { return m_pluginModule->pluginQuirks(); }

    void invalidate(const NPRect*);
    static const char* userAgent(NPP);
    void loadURL(const String& method, const String& urlString, const String& target, const WebCore::HTTPHeaderMap& headerFields,
                 const Vector<uint8_t>& httpBody, bool sendNotification, void* notificationData);
    NPError destroyStream(NPStream*, NPReason);
    void setIsWindowed(bool);
    void setIsTransparent(bool);
    void setStatusbarText(const String&);
    static void setException(const String&);
    bool evaluate(NPObject*, const String&scriptString, NPVariant* result);
    bool isPrivateBrowsingEnabled();
    bool isMuted() const;
    bool isWindowed() const { return m_isWindowed; }
    bool isVisible() const { return m_isVisible; }

    static void setSetExceptionFunction(void (*)(const String&));

    // These return retained objects.
    NPObject* windowScriptNPObject();
    NPObject* pluginElementNPObject();

    void cancelStreamLoad(NetscapePluginStream*);
    void removePluginStream(NetscapePluginStream*);

    bool isAcceleratedCompositingEnabled();

    void pushPopupsEnabledState(bool enabled);
    void popPopupsEnabledState();

    void pluginThreadAsyncCall(void (*function)(void*), void* userData);

    unsigned scheduleTimer(unsigned interval, bool repeat, void (*timerFunc)(NPP, unsigned timerID));
    void unscheduleTimer(unsigned timerID);

    double contentsScaleFactor();
    String proxiesForURL(const String& urlString);
    String cookiesForURL(const String& urlString);
    void setCookiesForURL(const String& urlString, const String& cookieString);
    bool getAuthenticationInfo(const WebCore::ProtectionSpace&, String& username, String& password);

    void setIsPlayingAudio(bool);

    void registerRedirect(NetscapePluginStream*, const URL& requestURL, int redirectResponseStatus, void* notificationData);
    void urlRedirectResponse(void* notifyData, bool allow);

    // Member functions for calling into the plug-in.
    NPError NPP_New(NPMIMEType pluginType, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData*);
    NPError NPP_Destroy(NPSavedData**);
    NPError NPP_SetWindow(NPWindow*);
    NPError NPP_NewStream(NPMIMEType, NPStream*, NPBool seekable, uint16_t* stype);
    NPError NPP_DestroyStream(NPStream*, NPReason);
    void NPP_StreamAsFile(NPStream*, const char* filename);
    int32_t NPP_WriteReady(NPStream*);
    int32_t NPP_Write(NPStream*, int32_t offset, int32_t len, void* buffer);
    int16_t NPP_HandleEvent(void* event);
    void NPP_URLNotify(const char* url, NPReason, void* notifyData);
    bool NPP_URLRedirectNotify(const char* url, int32_t status, void* notifyData);
    NPError NPP_GetValue(NPPVariable, void *value);
    NPError NPP_SetValue(NPNVariable, void *value);

    // Convert the given point from plug-in coordinates to root view coordinates.
    WebCore::IntPoint convertToRootView(const WebCore::IntPoint&) const override;

private:
    explicit NetscapePlugin(Ref<NetscapePluginModule>&&);

    void callSetWindow();
    void callSetWindowInvisible();
    bool shouldLoadSrcURL();
    NetscapePluginStream* streamFromID(uint64_t streamID);
    void stopAllStreams();
    bool allowPopups() const;

    const char* userAgent();

    void platformPreInitialize();
    bool platformPostInitialize();
    void platformDestroy();
    bool platformInvalidate(const WebCore::IntRect&);
    void platformGeometryDidChange();
    void platformVisibilityDidChange();
    void platformPaint(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect, bool isSnapshot = false);

    bool platformHandleMouseEvent(const WebMouseEvent&);
    bool platformHandleWheelEvent(const WebWheelEvent&);
    bool platformHandleMouseEnterEvent(const WebMouseEvent&);
    bool platformHandleMouseLeaveEvent(const WebMouseEvent&);
    bool platformHandleKeyboardEvent(const WebKeyboardEvent&);
    void platformSetFocus(bool);

    static bool wantsPluginRelativeNPWindowCoordinates();

    // Plugin
    bool initialize(const Parameters&) override;
    void destroy() override;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
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
    void streamDidReceiveResponse(uint64_t streamID, const URL& responseURL, uint32_t streamLength,
                                          uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) override;
    void streamDidFinishLoading(uint64_t streamID) override;
    void streamDidFail(uint64_t streamID, bool wasCancelled) override;
    void manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength, 
                                                uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
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
    bool isEditingCommandEnabled(const String&) override;

    bool shouldAllowScripting() override;
    bool shouldAllowNavigationFromDrags() override;
    
    bool handlesPageScaleFactor() const override;

    NPObject* pluginScriptableNPObject() override;
    
    unsigned countFindMatches(const String&, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool findString(const String&, WebCore::FindOptions, unsigned maxMatchCount) override;

    void windowFocusChanged(bool) override;
    void windowVisibilityChanged(bool) override;

#if PLATFORM(COCOA)
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) override;

    uint64_t pluginComplexTextInputIdentifier() const override;
    void sendComplexTextInput(const String& textInput) override;
    void setLayerHostingMode(LayerHostingMode) override;

    void pluginFocusOrWindowFocusChanged();
    void setComplexTextInputEnabled(bool);

    void updatePluginLayer();
#endif

    void contentsScaleFactorChanged(float) override;
    void storageBlockingStateChanged(bool) override;
    void privateBrowsingStateChanged(bool) override;
    bool getFormValue(String& formValue) override;
    bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    WebCore::Scrollbar* horizontalScrollbar() override;
    WebCore::Scrollbar* verticalScrollbar() override;

    bool supportsSnapshotting() const override;

    // Convert the given point from root view coordinates to plug-in coordinates. Returns false if the point can't be
    // converted (if the transformation matrix isn't invertible).
    bool convertFromRootView(const WebCore::IntPoint& pointInRootViewCoordinates, WebCore::IntPoint& pointInPluginCoordinates);

    RefPtr<WebCore::SharedBuffer> liveResourceData() const override;

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override { return false; }

    String getSelectionString() const override { return String(); }
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const override { return String(); }
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override { return false; }

    void mutedStateChanged(bool) override;

    void updateNPNPrivateMode();

    uint64_t m_nextRequestID;

    typedef HashMap<uint64_t, std::pair<String, void*>> PendingURLNotifyMap;
    PendingURLNotifyMap m_pendingURLNotifications;

    typedef HashMap<uint64_t, RefPtr<NetscapePluginStream>> StreamsMap;
    StreamsMap m_streams;
    HashMap<void*, std::pair<RefPtr<NetscapePluginStream>, String>> m_redirects;

    Ref<NetscapePluginModule> m_pluginModule;
    NPP_t m_npp;
    NPWindow m_npWindow;

    WebCore::IntSize m_pluginSize;

    // The clip rect in plug-in coordinates.
    WebCore::IntRect m_clipRect;

    // A transform that can be used to convert from root view coordinates to plug-in coordinates.
    WebCore::AffineTransform m_pluginToRootViewTransform;

#if PLUGIN_ARCHITECTURE(UNIX)
    WebCore::IntRect m_frameRectInWindowCoordinates;
#endif

    CString m_userAgent;

    bool m_isStarted;
    bool m_isWindowed;
    bool m_isTransparent;
    bool m_inNPPNew;
    bool m_shouldUseManualLoader;
    bool m_hasCalledSetWindow;
    bool m_isVisible;

    RefPtr<NetscapePluginStream> m_manualStream;
    Vector<bool, 8> m_popupEnabledStates;

    class Timer {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(Timer);

    public:
        typedef void (*TimerFunc)(NPP, uint32_t timerID);

        Timer(NetscapePlugin*, unsigned timerID, unsigned interval, bool repeat, TimerFunc);
        ~Timer();

        void start();
        void stop();

    private:
        void timerFired();

        // This is a weak pointer since Timer objects are destroyed before the NetscapePlugin object itself is destroyed.
        NetscapePlugin* m_netscapePlugin;

        unsigned m_timerID;
        unsigned m_interval;
        bool m_repeat;
        TimerFunc m_timerFunc;

        RunLoop::Timer<Timer> m_timer;
    };
    typedef HashMap<unsigned, std::unique_ptr<Timer>> TimerMap;
    TimerMap m_timers;
    unsigned m_nextTimerID;

    bool m_privateBrowsingState { false };
    bool m_storageBlockingState { false };

#if PLUGIN_ARCHITECTURE(MAC)
    NPDrawingModel m_drawingModel;
    NPEventModel m_eventModel;

    RetainPtr<PlatformLayer> m_pluginLayer;
    bool m_pluginReturnsNonretainedLayer;
    LayerHostingMode m_layerHostingMode;

    NPCocoaEvent* m_currentMouseEvent;

    bool m_pluginHasFocus;
    bool m_windowHasFocus;

    // Whether the plug-in wants to use the legacy Cocoa text input handling that
    // existed in WebKit1, or the updated Cocoa text input handling specified on
    // https://wiki.mozilla.org/NPAPI:CocoaEventModel#Text_Input
    bool m_pluginWantsLegacyCocoaTextInput;

    // Whether complex text input is enabled.
    bool m_isComplexTextInputEnabled;

    // Whether the plug-in has handled a keydown event. This is used to determine
    // if we can tell the plug-in that we support the updated Cocoa text input specification.
    bool m_hasHandledAKeyDownEvent;

    // The number of NPCocoaEventKeyUp events that should be ignored.
    unsigned m_ignoreNextKeyUpEventCounter;

    WebCore::IntRect m_windowFrameInScreenCoordinates;
    WebCore::IntRect m_viewFrameInWindowCoordinates;
#elif PLUGIN_ARCHITECTURE(UNIX)
    std::unique_ptr<NetscapePluginUnix> m_impl;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_PLUGIN(NetscapePlugin, NetscapePluginType)

#endif // ENABLE(NETSCAPE_PLUGIN_API)
