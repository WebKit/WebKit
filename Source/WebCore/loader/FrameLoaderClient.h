/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FrameLoaderTypes.h"
#include "LayoutMilestone.h"
#include "LinkIcon.h"
#include <functional>
#include <wtf/Forward.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifest.h"
#endif

#if ENABLE(CONTENT_FILTERING)
#include "ContentFilterUnblockHandler.h"
#endif

#if PLATFORM(COCOA)
#ifdef __OBJC__ 
#import <Foundation/Foundation.h>
typedef id RemoteAXObjectRef;
#else
typedef void* RemoteAXObjectRef;
#endif
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSArray;
OBJC_CLASS NSCachedURLResponse;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSView;
#endif

namespace PAL {
class SessionID;
}

namespace WebCore {

class AuthenticationChallenge;
class CachedFrame;
class CachedResourceRequest;
class Color;
class DOMWindow;
class DOMWindowExtension;
class DOMWrapperWorld;
class DocumentLoader;
class Element;
class FormState;
class Frame;
class FrameLoader;
class FrameNetworkingContext;
class HTMLAppletElement;
class HTMLFormElement;
class HTMLFrameOwnerElement;
class HTMLPlugInElement;
class HistoryItem;
class IntSize;
class MessageEvent;
class NavigationAction;
class Page;
class PluginViewBase;
class PreviewLoaderClient;
class ProtectionSpace;
class RTCPeerConnectionHandler;
class ResourceError;
class ResourceHandle;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class SharedBuffer;
class SubstituteData;
class Widget;

enum class LockBackForwardList : bool;
enum class PolicyDecisionMode;

struct StringWithDirection;

typedef WTF::Function<void (PolicyAction, PolicyCheckIdentifier)> FramePolicyFunction;

class WEBCORE_EXPORT FrameLoaderClient {
public:
    // An inline function cannot be the first non-abstract virtual function declared
    // in the class as it results in the vtable being generated as a weak symbol.
    // This hurts performance (in Mac OS X at least, when loading frameworks), so we
    // don't want to do it in WebKit.
    virtual bool hasHTMLView() const;

    virtual ~FrameLoaderClient() = default;

    virtual void frameLoaderDestroyed() = 0;

    virtual bool hasWebView() const = 0; // mainly for assertions

    virtual void makeRepresentation(DocumentLoader*) = 0;

    virtual Optional<uint64_t> pageID() const = 0;
    virtual Optional<uint64_t> frameID() const = 0;
    virtual PAL::SessionID sessionID() const = 0;

#if PLATFORM(IOS_FAMILY)
    // Returns true if the client forced the layout.
    virtual bool forceLayoutOnRestoreFromPageCache() = 0;
#endif
    virtual void forceLayoutForNonHTML() = 0;

    virtual void setCopiesOnScroll() = 0;

    virtual void detachedFromParent2() = 0;
    virtual void detachedFromParent3() = 0;

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&) = 0;

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse) = 0;
    virtual bool shouldUseCredentialStorage(DocumentLoader*, unsigned long identifier) = 0;
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) = 0;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long identifier, const ProtectionSpace&) = 0;
#endif

#if PLATFORM(IOS_FAMILY)
    virtual RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, unsigned long identifier) = 0;
#endif

    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&) = 0;
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int dataLength) = 0;
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier) = 0;
    virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&) = 0;
    virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length) = 0;

    virtual void dispatchDidDispatchOnloadEvents() = 0;
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() = 0;
    virtual void dispatchDidChangeProvisionalURL() { }
    virtual void dispatchDidCancelClientRedirect() = 0;
    virtual void dispatchWillPerformClientRedirect(const URL&, double interval, WallTime fireDate, LockBackForwardList) = 0;
    virtual void dispatchDidChangeMainDocument() { }
    virtual void dispatchWillChangeDocument(const URL&, const URL&) { }
    virtual void dispatchDidNavigateWithinPage() { }
    virtual void dispatchDidChangeLocationWithinPage() = 0;
    virtual void dispatchDidPushStateWithinPage() = 0;
    virtual void dispatchDidReplaceStateWithinPage() = 0;
    virtual void dispatchDidPopStateWithinPage() = 0;
    virtual void dispatchWillClose() = 0;
    virtual void dispatchDidReceiveIcon() { }
    virtual void dispatchDidStartProvisionalLoad() = 0;
    virtual void dispatchDidReceiveTitle(const StringWithDirection&) = 0;
    virtual void dispatchDidCommitLoad(Optional<HasInsecureContent>) = 0;
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) = 0;
    virtual void dispatchDidFailLoad(const ResourceError&) = 0;
    virtual void dispatchDidFinishDocumentLoad() = 0;
    virtual void dispatchDidFinishLoad() = 0;
#if ENABLE(DATA_DETECTION)
    virtual void dispatchDidFinishDataDetection(NSArray *detectionResults) = 0;
#endif

    virtual void dispatchDidLayout() { }
    virtual void dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) { }

    virtual Frame* dispatchCreatePage(const NavigationAction&) = 0;
    virtual void dispatchShow() = 0;

    virtual void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, PolicyCheckIdentifier, FramePolicyFunction&&) = 0;
    virtual void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, FormState*, const String& frameName, PolicyCheckIdentifier, FramePolicyFunction&&) = 0;
    virtual void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, const ResourceResponse& redirectResponse, FormState*, PolicyDecisionMode, PolicyCheckIdentifier, FramePolicyFunction&&) = 0;
    virtual void cancelPolicyCheck() = 0;

    virtual void dispatchUnableToImplementPolicy(const ResourceError&) = 0;

    virtual void dispatchWillSendSubmitEvent(Ref<FormState>&&) = 0;
    virtual void dispatchWillSubmitForm(FormState&, CompletionHandler<void()>&&) = 0;

    virtual void revertToProvisionalState(DocumentLoader*) = 0;
    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) = 0;

    virtual void setMainFrameDocumentReady(bool) = 0;

    virtual void startDownload(const ResourceRequest&, const String& suggestedName = String()) = 0;

    virtual void willChangeTitle(DocumentLoader*) = 0;
    virtual void didChangeTitle(DocumentLoader*) = 0;

    virtual void willReplaceMultipartContent() = 0;
    virtual void didReplaceMultipartContent() = 0;

    virtual void committedLoad(DocumentLoader*, const char*, int) = 0;
    virtual void finishedLoading(DocumentLoader*) = 0;
    
    virtual void updateGlobalHistory() = 0;
    virtual void updateGlobalHistoryRedirectLinks() = 0;

    virtual bool shouldGoToHistoryItem(HistoryItem&) const = 0;

    // This frame has set its opener to null, disowning it for the lifetime of the frame.
    // See http://html.spec.whatwg.org/#dom-opener.
    // FIXME: JSC should allow disowning opener. - <https://bugs.webkit.org/show_bug.cgi?id=103913>.
    virtual void didDisownOpener() { }

    // This frame has displayed inactive content (such as an image) from an
    // insecure source.  Inactive content cannot spread to other frames.
    virtual void didDisplayInsecureContent() = 0;

    // The indicated security origin has run active content (such as a
    // script) from an insecure source.  Note that the insecure content can
    // spread to other frames in the same origin.
    virtual void didRunInsecureContent(SecurityOrigin&, const URL&) = 0;
    virtual void didDetectXSS(const URL&, bool didBlockEntirePage) = 0;

    virtual ResourceError cancelledError(const ResourceRequest&) = 0;
    virtual ResourceError blockedError(const ResourceRequest&) = 0;
    virtual ResourceError blockedByContentBlockerError(const ResourceRequest&) = 0;
    virtual ResourceError cannotShowURLError(const ResourceRequest&) = 0;
    virtual ResourceError interruptedForPolicyChangeError(const ResourceRequest&) = 0;
#if ENABLE(CONTENT_FILTERING)
    virtual ResourceError blockedByContentFilterError(const ResourceRequest&) = 0;
#endif

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) = 0;
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) = 0;
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) = 0;

    virtual bool shouldFallBack(const ResourceError&) = 0;

    virtual bool canHandleRequest(const ResourceRequest&) const = 0;
    virtual bool canShowMIMEType(const String& MIMEType) const = 0;
    virtual bool canShowMIMETypeAsHTML(const String& MIMEType) const = 0;
    virtual bool representationExistsForURLScheme(const String& URLScheme) const = 0;
    virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const = 0;

    virtual void frameLoadCompleted() = 0;
    virtual void saveViewStateToItem(HistoryItem&) = 0;
    virtual void restoreViewState() = 0;
    virtual void provisionalLoadStarted() = 0;
    virtual void didFinishLoad() = 0;
    virtual void prepareForDataSourceReplacement() = 0;

    virtual Ref<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) = 0;
    virtual void updateCachedDocumentLoader(DocumentLoader&) = 0;
    virtual void setTitle(const StringWithDirection&, const URL&) = 0;

    virtual String userAgent(const URL&) = 0;

    virtual String overrideContentSecurityPolicy() const { return String(); }
    
    virtual void savePlatformDataToCachedFrame(CachedFrame*) = 0;
    virtual void transitionToCommittedFromCachedFrame(CachedFrame*) = 0;
#if PLATFORM(IOS_FAMILY)
    virtual void didRestoreFrameHierarchyForCachedFrame() = 0;
#endif
    virtual void transitionToCommittedForNewPage() = 0;

    virtual void didSaveToPageCache() = 0;
    virtual void didRestoreFromPageCache() = 0;

    virtual void dispatchDidBecomeFrameset(bool) = 0; // Can change due to navigation or DOM modification.

    virtual bool canCachePage() const = 0;
    virtual void convertMainResourceLoadToDownload(DocumentLoader*, PAL::SessionID, const ResourceRequest&, const ResourceResponse&) = 0;

    virtual RefPtr<Frame> createFrame(const URL&, const String& name, HTMLFrameOwnerElement&, const String& referrer) = 0;
    virtual RefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement&, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually) = 0;
    virtual void recreatePlugin(Widget*) = 0;
    virtual void redirectDataToPlugin(Widget&) = 0;

    virtual RefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement&, const URL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues) = 0;

    virtual ObjectContentType objectContentType(const URL&, const String& mimeType) = 0;
    virtual String overrideMediaType() const = 0;

    virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) = 0;

    virtual void registerForIconNotification() { }

#if PLATFORM(COCOA)
    // Allow an accessibility object to retrieve a Frame parent if there's no PlatformWidget.
    virtual RemoteAXObjectRef accessibilityRemoteObject() = 0;
    virtual void willCacheResponse(DocumentLoader*, unsigned long identifier, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) const = 0;
    virtual NSDictionary *dataDetectionContext() { return nullptr; }
#endif

#if USE(CFURLCONNECTION)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    virtual bool shouldCacheResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&, const unsigned char* data, unsigned long long length) = 0;
#endif

    virtual bool shouldAlwaysUsePluginDocument(const String& /*mimeType*/) const { return false; }
    virtual bool shouldLoadMediaElementURL(const URL&) const { return true; }

    virtual void didChangeScrollOffset() { }

    virtual bool allowScript(bool enabledPerSettings) { return enabledPerSettings; }

    // Clients that generally disallow universal access can make exceptions for particular URLs.
    virtual bool shouldForceUniversalAccessFromLocalURL(const URL&) { return false; }

    virtual Ref<FrameNetworkingContext> createNetworkingContext() = 0;

    virtual bool shouldPaintBrokenImage(const URL&) const { return true; }

    virtual void dispatchGlobalObjectAvailable(DOMWrapperWorld&) { }
    virtual void dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(DOMWindowExtension*) { }
    virtual void dispatchDidReconnectDOMWindowExtensionToGlobalObject(DOMWindowExtension*) { }
    virtual void dispatchWillDestroyGlobalObjectForDOMWindowExtension(DOMWindowExtension*) { }

    virtual void willInjectUserScript(DOMWrapperWorld&) { }

#if ENABLE(WEB_RTC)
    virtual void dispatchWillStartUsingPeerConnectionHandler(RTCPeerConnectionHandler*) { }
#endif

#if ENABLE(WEBGL)
    virtual bool allowWebGL(bool enabledPerSettings) { return enabledPerSettings; }
    // Informs the embedder that a WebGL canvas inside this frame received a lost context
    // notification with the given GL_ARB_robustness guilt/innocence code (see Extensions3D.h).
    virtual void didLoseWebGLContext(int) { }
    virtual WebGLLoadPolicy webGLPolicyForURL(const URL&) const { return WebGLAllowCreation; }
    virtual WebGLLoadPolicy resolveWebGLPolicyForURL(const URL&) const { return WebGLAllowCreation; }
#endif

    virtual void forcePageTransitionIfNeeded() { }

    // FIXME (bug 116233): We need to get rid of EmptyFrameLoaderClient completely, then this will no longer be needed.
    virtual bool isEmptyFrameLoaderClient() { return false; }

#if USE(QUICK_LOOK)
    virtual RefPtr<PreviewLoaderClient> createPreviewLoaderClient(const String&, const String&) = 0;
#endif

#if ENABLE(CONTENT_FILTERING)
    virtual void contentFilterDidBlockLoad(ContentFilterUnblockHandler) { }
#endif

    virtual void prefetchDNS(const String&) = 0;

    virtual void didRestoreScrollPosition() { }

    virtual void getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>&) { }
    virtual void finishedLoadingIcon(uint64_t, SharedBuffer*) { }

    virtual void didCreateWindow(DOMWindow&) { }

#if ENABLE(APPLICATION_MANIFEST)
    virtual void finishedLoadingApplicationManifest(uint64_t, const Optional<ApplicationManifest>&) { }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    virtual bool hasFrameSpecificStorageAccess() { return false; }
    virtual void setHasFrameSpecificStorageAccess(bool) { }
#endif
};

} // namespace WebCore
