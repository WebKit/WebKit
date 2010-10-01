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

#ifndef PluginController_h
#define PluginController_h

#include <wtf/Forward.h>

struct NPObject;
typedef struct _NPVariant NPVariant;

namespace WebCore {
    class HTTPHeaderMap;
    class IntRect;
    class KURL;
}

namespace WebKit {

class PluginController {
public:
    // Tells the controller that the plug-in wants the given rect to be repainted. The rect is in the plug-in's coordinate system.
    virtual void invalidate(const WebCore::IntRect&) = 0;

    // Returns the user agent string.
    virtual String userAgent() = 0;

    // Loads the given URL and associates it with the request ID.
    // 
    // If a target is specified, then the URL will be loaded in the window or frame that the target refers to.
    // Once the URL finishes loading, Plugin::frameDidFinishLoading will be called with the given requestID. If the URL
    // fails to load, Plugin::frameDidFailToLoad will be called.
    //
    // If the URL is a JavaScript URL, the JavaScript code will be evaluated and the result sent back using Plugin::didEvaluateJavaScript.
    virtual void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, 
                         const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups) = 0;

    /// Cancels the load of a stream that was requested by loadURL.
    virtual void cancelStreamLoad(uint64_t streamID) = 0;

    // Cancels the load of the manual stream.
    virtual void cancelManualStreamLoad() = 0;

    // Get the NPObject that corresponds to the window JavaScript object. Returns a retained object.
    virtual NPObject* windowScriptNPObject() = 0;

    // Get the NPObject that corresponds to the plug-in's element. Returns a retained object.
    virtual NPObject* pluginElementNPObject() = 0;

    // Evaluates the given script string in the context of the given NPObject.
    virtual bool evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups) = 0;

    // Set the statusbar text.
    virtual void setStatusbarText(const String&) = 0;

#if USE(ACCELERATED_COMPOSITING)
    // Return whether accelerated compositing is enabled.
    virtual bool isAcceleratedCompositingEnabled() = 0;
#endif

    // Tells the controller that the plug-in process has crashed.
    virtual void pluginProcessCrashed() = 0;
    
#if PLATFORM(WIN)
    // The window to use as the parent of the plugin's window.
    virtual HWND nativeParentWindow() = 0;
#endif

    // Returns the proxies for the given URL or null on failure.
    virtual String proxiesForURL(const String&) = 0;

    // Returns the cookies for the given URL or null on failure.
    virtual String cookiesForURL(const String&) = 0;

    // Sets the cookies for the given URL.
    virtual void setCookiesForURL(const String& urlString, const String& cookieString) = 0;
    
protected:
    virtual ~PluginController() { }
};

} // namespace WebKit

#endif // PluginController_h
