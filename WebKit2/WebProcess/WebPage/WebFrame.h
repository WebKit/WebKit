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

#ifndef WebFrame_h
#define WebFrame_h

#include "APIObject.h"
#include "ImmutableArray.h"
#include "WebFrameLoaderClient.h"
#include <JavaScriptCore/JSBase.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/PolicyChecker.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
    class Frame;
    class HTMLFrameOwnerElement;
}

namespace WebKit {

class InjectedBundleNodeHandle;
class InjectedBundleRangeHandle;
class InjectedBundleScriptWorld;
class WebPage;

class WebFrame : public APIObject {
public:
    static const Type APIType = TypeBundleFrame;

    static PassRefPtr<WebFrame> createMainFrame(WebPage*);
    static PassRefPtr<WebFrame> createSubframe(WebPage*, const String& frameName, WebCore::HTMLFrameOwnerElement*);
    ~WebFrame();

    // Called when the FrameLoaderClient (and therefore the WebCore::Frame) is being torn down.
    void invalidate();

    WebPage* page() const;
    WebCore::Frame* coreFrame() const { return m_coreFrame; }

    uint64_t frameID() const { return m_frameID; }

    uint64_t setUpPolicyListener(WebCore::FramePolicyFunction);
    void invalidatePolicyListener();
    void didReceivePolicyDecision(uint64_t listenerID, WebCore::PolicyAction);

    String source() const;

    // WKBundleFrame API and SPI functions
    bool isMainFrame() const;
    String name() const;
    String url() const;
    String innerText() const;
    PassRefPtr<ImmutableArray> childFrames();
    JSValueRef computedStyleIncludingVisitedInfo(JSObjectRef element);
    JSGlobalContextRef jsContext();
    JSGlobalContextRef jsContextForWorld(InjectedBundleScriptWorld*);

    JSValueRef jsWrapperForWorld(InjectedBundleNodeHandle*, InjectedBundleScriptWorld*);
    JSValueRef jsWrapperForWorld(InjectedBundleRangeHandle*, InjectedBundleScriptWorld*);

    static String counterValue(JSObjectRef element);
    static String markerText(JSObjectRef element);

    unsigned numberOfActiveAnimations();
    bool pauseAnimationOnElementWithId(const String& animationName, const String& elementID, double time);

    unsigned pendingUnloadCount();

    // Simple listener class used by plug-ins to know when frames finish or fail loading.
    class LoadListener {
    public:
        virtual ~LoadListener() { }

        virtual void didFinishLoad(WebFrame*) = 0;
        virtual void didFailLoad(WebFrame*, bool wasCancelled) = 0;
    };
    void setLoadListener(LoadListener* loadListener) { m_loadListener = loadListener; }
    LoadListener* loadListener() const { return m_loadListener; }

private:
    static PassRefPtr<WebFrame> create(WebPage*, const String& frameName, WebCore::HTMLFrameOwnerElement*);
    WebFrame(WebPage*, const String& frameName, WebCore::HTMLFrameOwnerElement*);

    virtual Type type() const { return APIType; }

    WebCore::Frame* m_coreFrame;

    uint64_t m_policyListenerID;
    WebCore::FramePolicyFunction m_policyFunction;

    WebFrameLoaderClient m_frameLoaderClient;
    LoadListener* m_loadListener;
    
    uint64_t m_frameID;
};

} // namespace WebKit

#endif // WebFrame_h
