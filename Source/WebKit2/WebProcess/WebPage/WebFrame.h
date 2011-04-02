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
    class KURL;
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
    void didReceivePolicyDecision(uint64_t listenerID, WebCore::PolicyAction, uint64_t downloadID);

    void startDownload(const WebCore::ResourceRequest&);
    void convertHandleToDownload(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceRequest& initialRequest, const WebCore::ResourceResponse&);

    String source() const;
    String contentsAsString() const;
    String selectionAsString() const;

    WebCore::IntSize size() const;

    // WKBundleFrame API and SPI functions
    bool isMainFrame() const;
    String name() const;
    String url() const;
    String innerText() const;
    bool isFrameSet() const;
    PassRefPtr<ImmutableArray> childFrames();
    JSValueRef computedStyleIncludingVisitedInfo(JSObjectRef element);
    JSGlobalContextRef jsContext();
    JSGlobalContextRef jsContextForWorld(InjectedBundleScriptWorld*);
    WebCore::IntRect contentBounds() const;
    WebCore::IntRect visibleContentBounds() const;
    WebCore::IntRect visibleContentBoundsExcludingScrollbars() const;
    WebCore::IntSize scrollOffset() const;
    bool hasHorizontalScrollbar() const;
    bool hasVerticalScrollbar() const;
    bool getDocumentBackgroundColor(double* red, double* green, double* blue, double* alpha);

    static WebFrame* frameForContext(JSContextRef);

    JSValueRef jsWrapperForWorld(InjectedBundleNodeHandle*, InjectedBundleScriptWorld*);
    JSValueRef jsWrapperForWorld(InjectedBundleRangeHandle*, InjectedBundleScriptWorld*);

    static String counterValue(JSObjectRef element);
    static String markerText(JSObjectRef element);

    unsigned numberOfActiveAnimations() const;
    bool pauseAnimationOnElementWithId(const String& animationName, const String& elementID, double time);
    void suspendAnimations();
    void resumeAnimations();
    String layerTreeAsText() const;
    
    unsigned pendingUnloadCount() const;
    
    bool allowsFollowingLink(const WebCore::KURL&) const;

    String provisionalURL() const;
    String suggestedFilenameForResourceWithURL(const WebCore::KURL&) const;
    String mimeTypeForResourceWithURL(const WebCore::KURL&) const;

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
    static PassRefPtr<WebFrame> create();
    WebFrame();

    void init(WebPage*, const String& frameName, WebCore::HTMLFrameOwnerElement*);

    virtual Type type() const { return APIType; }

    WebCore::Frame* m_coreFrame;

    uint64_t m_policyListenerID;
    WebCore::FramePolicyFunction m_policyFunction;
    uint64_t m_policyDownloadID;

    WebFrameLoaderClient m_frameLoaderClient;
    LoadListener* m_loadListener;
    
    uint64_t m_frameID;
};

} // namespace WebKit

#endif // WebFrame_h
