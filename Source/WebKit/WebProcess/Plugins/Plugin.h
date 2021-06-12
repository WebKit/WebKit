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

#include <WebCore/FindOptions.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
typedef struct objc_object* id;

OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;
#endif

struct NPObject;

namespace IPC {
class Encoder;
class Decoder;
}

namespace WebCore {
class AffineTransform;
class Element;
class FloatPoint;
class FloatSize;
class GraphicsContext;
class IntPoint;
class IntRect;
class IntSize;
class Scrollbar;
class SharedBuffer;
}

namespace WebKit {

class ShareableBitmap;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
    
class PluginController;

enum PluginType {
    PluginProxyType,
    NetscapePluginType,
    PDFPluginType,
};

enum class LayerHostingMode : uint8_t;

class Plugin : public ThreadSafeRefCounted<Plugin> {
public:
    struct Parameters {
        URL url;
        Vector<String> names;
        Vector<String> values;
        String mimeType;
        bool isFullFramePlugin;
        bool shouldUseManualLoader;
#if PLATFORM(COCOA)
        LayerHostingMode layerHostingMode;
#endif

        void encode(IPC::Encoder&) const;
        static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, Parameters&);
    };

    bool initialize(PluginController&, const Parameters&);

    void destroyPlugin();

    bool isBeingDestroyed() const { return m_isBeingDestroyed; }

    PluginController* controller();
    const PluginController* controller() const;

    virtual ~Plugin();

    PluginType type() const { return m_type; }

    bool isPluginProxy() const { return m_type == PluginProxyType; }
    bool isNetscapePlugin() const { return m_type == NetscapePluginType; }
    bool isPDFPlugin() const { return m_type == PDFPluginType; }

private:
    // Initializes the plug-in. If the plug-in fails to initialize this should return false.
    // This is only called by the other initialize overload so it can be made private.
    virtual bool initialize(const Parameters&) = 0;

    // Destroys the plug-in.
    virtual void destroy() = 0;

public:

    // Tells the plug-in to paint itself into the given graphics context. The passed-in context and
    // dirty rect are in window coordinates. The context is saved/restored by the caller.
    virtual void paint(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) = 0;

    // Invalidate native tintable controls. The passed-in context is in window coordinates.
    virtual void updateControlTints(WebCore::GraphicsContext&);

    // Returns whether the plug-in supports snapshotting or not.
    virtual bool supportsSnapshotting() const = 0;

    // Tells the plug-in to draw itself into a bitmap, and return that.
    virtual RefPtr<ShareableBitmap> snapshot() = 0;

#if PLATFORM(COCOA)
    // If a plug-in is using the Core Animation drawing model, this returns its plug-in layer.
    virtual PlatformLayer* pluginLayer() = 0;
#endif

    // Returns whether the plug-in is transparent or not.
    virtual bool isTransparent() = 0;

    // Returns whether we should send wheel events to this plug-in.
    virtual bool wantsWheelEvents() = 0;

    // Tells the plug-in that its geometry has changed. The clip rect is in plug-in coordinates, and the affine transform can be used
    // to convert from root view coordinates to plug-in coordinates.
    virtual void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) = 0;

    // Tells the plug-in that it has been explicitly hidden or shown. (Note that this is not called when the plug-in becomes obscured from view on screen.)
    virtual void visibilityDidChange(bool isVisible) = 0;

    // Tells the plug-in that a frame load request that the plug-in made by calling PluginController::loadURL has finished.
    virtual void frameDidFinishLoading(uint64_t requestID) = 0;

    // Tells the plug-in that a frame load request that the plug-in made by calling PluginController::loadURL has failed.
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled) = 0;

    // Tells the plug-in that a request to evaluate JavaScript (using PluginController::loadURL) has been fulfilled and passes
    // back the result. If evaluating the script failed, result will be null.
    virtual void didEvaluateJavaScript(uint64_t requestID, const String& result) = 0;

    // Tells the plug-in that a stream may send an HTTP request.
    virtual void streamWillSendRequest(uint64_t streamID, const URL& requestURL, const URL& responseURL, int responseStatusCode) = 0;

    // Tells the plug-in that a stream has received its HTTP response.
    virtual void streamDidReceiveResponse(uint64_t streamID, const URL& responseURL, uint32_t streamLength, 
                                          uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) = 0;

    // Tells the plug-in that a stream did receive data.
    virtual void streamDidReceiveData(uint64_t streamID, const uint8_t* bytes, int length) = 0;

    // Tells the plug-in that a stream has finished loading.
    virtual void streamDidFinishLoading(uint64_t streamID) = 0;

    // Tells the plug-in that a stream has failed to load, either because of network errors or because the load was cancelled.
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled) = 0;

    // Tells the plug-in that the manual stream has received its HTTP response.
    virtual void manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength, 
                                                uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) = 0;

    // Tells the plug-in that the manual stream did receive data.
    virtual void manualStreamDidReceiveData(const uint8_t* bytes, int length) = 0;

    // Tells the plug-in that a stream has finished loading.
    virtual void manualStreamDidFinishLoading() = 0;
    
    // Tells the plug-in that a stream has failed to load, either because of network errors or because the load was cancelled.
    virtual void manualStreamDidFail(bool wasCancelled) = 0;
    
    // Tells the plug-in to handle the passed in mouse event. The plug-in should return true if it processed the event.
    virtual bool handleMouseEvent(const WebMouseEvent&) = 0;

    // Tells the plug-in to handle the passed in wheel event. The plug-in should return true if it processed the event.
    virtual bool handleWheelEvent(const WebWheelEvent&) = 0;

    // Tells the plug-in to handle the passed in mouse over event. The plug-in should return true if it processed the event.
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) = 0;
    
    // Tells the plug-in to handle the passed in mouse leave event. The plug-in should return true if it processed the event.
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) = 0;

    // Tells the plug-in to handle the passed in context menu event. The plug-in should return true if it processed the event.
    virtual bool handleContextMenuEvent(const WebMouseEvent&) = 0;

    // Tells the plug-in to handle the passed in keyboard event. The plug-in should return true if it processed the event.
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    
    // Tells the plug-in to handle the passed in editing command. The plug-in should return true if it executed the command.
    virtual bool handleEditingCommand(const String& commandName, const String& argument) = 0;
    
    // Ask the plug-in whether it will be able to handle the given editing command.
    virtual bool isEditingCommandEnabled(const String&) = 0;

    // Ask the plug-in whether it should be allowed to execute JavaScript or navigate to JavaScript URLs.
    virtual bool shouldAllowScripting() = 0;

    // Ask the plug-in whether it wants URLs and files dragged onto it to cause navigation.
    virtual bool shouldAllowNavigationFromDrags() = 0;

    // Ask the plug-in whether it wants to override full-page zoom.
    virtual bool handlesPageScaleFactor() const = 0;

    // Tells the plug-in about focus changes.
    virtual void setFocus(bool) = 0;

    // Get the NPObject that corresponds to the plug-in's scriptable object. Returns a retained object.
    virtual NPObject* pluginScriptableNPObject() = 0;

    // Tells the plug-in about window focus changes.
    virtual void windowFocusChanged(bool) = 0;
    
    // Tells the plug-in about window visibility changes.
    virtual void windowVisibilityChanged(bool) = 0;
    
#if PLATFORM(COCOA)
    // Tells the plug-in about window and plug-in frame changes.
    virtual void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) = 0;

    // Get the per complex text input identifier.
    virtual uint64_t pluginComplexTextInputIdentifier() const = 0;

    // Send the complex text input to the plug-in.
    virtual void sendComplexTextInput(const String& textInput) = 0;

    // Tells the plug-in about changes to the layer hosting mode.
    virtual void setLayerHostingMode(LayerHostingMode) = 0;
#endif

    // Tells the plug-in about scale factor changes.
    virtual void contentsScaleFactorChanged(float) = 0;

    // Called when the storage blocking policy for this plug-in changes.
    virtual void storageBlockingStateChanged(bool) = 0;

    // Called when the private browsing state for this plug-in changes.
    virtual void privateBrowsingStateChanged(bool) = 0;

    // Gets the form value representation for the plug-in, letting plug-ins participate in form submission.
    virtual bool getFormValue(String& formValue) = 0;

    // Tells the plug-in that it should scroll. The plug-in should return true if it did scroll.
    virtual bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) = 0;

    // A plug-in can use WebCore scroll bars. Make them known, so that hit testing can find them.
    // FIXME: This code should be in PluginView or its base class, not in individual plug-ins.
    virtual WebCore::Scrollbar* horizontalScrollbar() = 0;
    virtual WebCore::Scrollbar* verticalScrollbar() = 0;

#if PLATFORM(COCOA)
    virtual RetainPtr<PDFDocument> pdfDocumentForPrinting() const { return 0; }
    virtual WebCore::FloatSize pdfDocumentSizeForPrinting() const;
    virtual NSObject *accessibilityObject() const { return 0; }
    virtual id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const { return nullptr; }
#endif

    virtual unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;

    virtual bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;

    virtual WebCore::IntPoint convertToRootView(const WebCore::IntPoint& pointInLocalCoordinates) const;

    virtual RefPtr<WebCore::SharedBuffer> liveResourceData() const = 0;

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) = 0;

    virtual String getSelectionString() const = 0;
    virtual String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const = 0;
    virtual bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const = 0;

    virtual void mutedStateChanged(bool) { }

    virtual bool canCreateTransientPaintingSnapshot() const { return true; }

    virtual bool requiresUnifiedScaleFactor() const { return false; }

    virtual void willDetachRenderer() { }

    virtual bool pluginHandlesContentOffsetForAccessibilityHitTest() const { return false; }

protected:
    Plugin(PluginType);

    PluginType m_type;

    bool m_isBeingDestroyed { false };

private:
    WeakPtr<PluginController> m_pluginController;
};
    
} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_PLUGIN(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ToValueTypeName) \
static bool isType(const WebKit::Plugin& plugin) { return plugin.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
