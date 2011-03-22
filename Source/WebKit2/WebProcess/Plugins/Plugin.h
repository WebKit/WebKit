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

#ifndef Plugin_h
#define Plugin_h

#include <WebCore/GraphicsLayer.h>
#include <WebCore/KURL.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

struct NPObject;

namespace CoreIPC {
    class ArgumentEncoder;
    class ArgumentDecoder;
}

namespace WebCore {
    class GraphicsContext;
    class IntRect;
}

namespace WebKit {

class ShareableBitmap;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
    
class PluginController;

class Plugin : public RefCounted<Plugin> {
public:
    struct Parameters {
        WebCore::KURL url;
        Vector<String> names;
        Vector<String> values;
        String mimeType;
        bool loadManually;

        void encode(CoreIPC::ArgumentEncoder*) const;
        static bool decode(CoreIPC::ArgumentDecoder*, Parameters&);
    };

    virtual ~Plugin();
    
    // Initializes the plug-in. If the plug-in fails to initialize this should return false.
    virtual bool initialize(PluginController*, const Parameters&) = 0;

    // Destroys the plug-in.
    virtual void destroy() = 0;

    // Tells the plug-in to paint itself into the given graphics context. The passed-in context and
    // dirty rect are in window coordinates. The context is saved/restored by the caller.
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect) = 0;

    // Tells the plug-in to draw itself into a bitmap, and return that.
    virtual PassRefPtr<ShareableBitmap> snapshot() = 0;

#if PLATFORM(MAC)
    // If a plug-in is using the Core Animation drawing model, this returns its plug-in layer.
    virtual PlatformLayer* pluginLayer() = 0;
#endif
    
    // Returns whether the plug-in is transparent or not.
    virtual bool isTransparent() = 0;

    // Tells the plug-in that either the plug-ins frame rect or its clip rect has changed. Both rects are in window coordinates.
    virtual void geometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect) = 0;

    // Tells the plug-in that a frame load request that the plug-in made by calling PluginController::loadURL has finished.
    virtual void frameDidFinishLoading(uint64_t requestID) = 0;

    // Tells the plug-in that a frame load request that the plug-in made by calling PluginController::loadURL has failed.
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled) = 0;

    // Tells the plug-in that a request to evaluate JavaScript (using PluginController::loadURL) has been fulfilled and passes
    // back the result. If evaluating the script failed, result will be null.
    virtual void didEvaluateJavaScript(uint64_t requestID, const String& requestURLString, const String& result) = 0;

    // Tells the plug-in that a stream has received its HTTP response.
    virtual void streamDidReceiveResponse(uint64_t streamID, const WebCore::KURL& responseURL, uint32_t streamLength, 
                                          uint32_t lastModifiedTime, const String& mimeType, const String& headers) = 0;

    // Tells the plug-in that a stream did receive data.
    virtual void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) = 0;

    // Tells the plug-in that a stream has finished loading.
    virtual void streamDidFinishLoading(uint64_t streamID) = 0;

    // Tells the plug-in that a stream has failed to load, either because of network errors or because the load was cancelled.
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled) = 0;

    // Tells the plug-in that the manual stream has received its HTTP response.
    virtual void manualStreamDidReceiveResponse(const WebCore::KURL& responseURL, uint32_t streamLength, 
                                                uint32_t lastModifiedTime, const String& mimeType, const String& headers) = 0;

    // Tells the plug-in that the manual stream did receive data.
    virtual void manualStreamDidReceiveData(const char* bytes, int length) = 0;

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

    // Tells the plug-in to handle the passed in keyboard event. The plug-in should return true if it processed the event.
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    
    // Tells the plug-in about focus changes.
    virtual void setFocus(bool) = 0;

    // Get the NPObject that corresponds to the plug-in's scriptable object. Returns a retained object.
    virtual NPObject* pluginScriptableNPObject() = 0;

#if PLATFORM(MAC)
    // Tells the plug-in about window focus changes.
    virtual void windowFocusChanged(bool) = 0;

    // Tells the plug-in about window and plug-in frame changes.
    virtual void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) = 0;

    // Tells the plug-in about window visibility changes.
    virtual void windowVisibilityChanged(bool) = 0;

    // Get the per complex text input identifier.
    virtual uint64_t pluginComplexTextInputIdentifier() const = 0;

    // Send the complex text input to the plug-in.
    virtual void sendComplexTextInput(const String& textInput) = 0;
#endif

    // Called when the private browsing state for this plug-in changes.
    virtual void privateBrowsingStateChanged(bool) = 0;

    // Returns the plug-in controller for this plug-in.
    // FIXME: We could just have the controller be a member variable of Plugin.
    virtual PluginController* controller() = 0;

protected:
    Plugin();
};
    
} // namespace WebKit

#endif // Plugin_h
