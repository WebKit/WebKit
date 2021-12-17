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

#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class HTTPHeaderMap;
class IntRect;
class ProtectionSpace;
}

namespace WebKit {

class PluginController : public CanMakeWeakPtr<PluginController> {
public:
    // Loads the given URL and associates it with the request ID.
    // 
    // If a target is specified, then the URL will be loaded in the window or frame that the target refers to.
    // Once the URL finishes loading, Plugin::frameDidFinishLoading will be called with the given requestID. If the URL
    // fails to load, Plugin::frameDidFailToLoad will be called.
    //
    // If the URL is a JavaScript URL, the JavaScript code will be evaluated and the result sent back using Plugin::didEvaluateJavaScript.
    virtual void loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, 
                         const WebCore::HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups) = 0;

    // Returns the contents scale factor.
    virtual float contentsScaleFactor() = 0;

    // Called when the a plug-in instance is successfully initialized, either synchronously or asynchronously.
    virtual void didInitializePlugin() = 0;
    
protected:
    virtual ~PluginController() { }
};

} // namespace WebKit
