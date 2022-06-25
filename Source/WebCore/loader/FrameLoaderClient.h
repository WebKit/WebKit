/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

#include "FrameIdentifier.h"
#include "FrameLoaderTypes.h"
#include "LayoutMilestone.h"
#include "LinkIcon.h"
#include "PageIdentifier.h"
#include "RegistrableDomain.h"
#include "ResourceLoaderIdentifier.h"
#include <wtf/Expected.h>
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
class HTMLFormElement;
class HTMLFrameOwnerElement;
class HTMLPlugInElement;
class HistoryItem;
class IntSize;
class LegacyPreviewLoaderClient;
class MessageEvent;
class NavigationAction;
class Page;
class ProtectionSpace;
class RegistrableDomain;
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
enum class UsedLegacyTLS : bool;

struct StringWithDirection;

typedef Function<void (PolicyAction, PolicyCheckIdentifier)> FramePolicyFunction;

class WEBCORE_EXPORT FrameLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // An inline function cannot be the first non-abstract virtual function declared
    // in the class as it results in the vtable being generated as a weak symbol.
    // This hurts performance (in Mac OS X at least, when loading frameworks), so we
    // don't want to do it in WebKit.
    virtual bool hasHTMLView() const;

    virtual ~FrameLoaderClient() = default;

    virtual bool hasWebView() const = 0; // mainly for assertions

    virtual void makeRepresentation(DocumentLoader*) = 0;

    virtual std::optional<PageIdentifier> pageID() const = 0;
    virtual std::optional<FrameIdentifier> frameID() const = 0;

#if PLATFORM(IOS_FAMILY)
    // Returns true if the client forced the layout.
    virtual bool forceLayoutOnRestoreFromBackForwardCache() = 0;
#endif
    virtual void forceLayoutForNonHTML() = 0;

    virtual void setCopiesOnScroll() = 0;

    virtual void detachedFromParent2() = 0;
    virtual void detachedFromParent3() = 0;

    virtual void assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&) = 0;

    virtual void dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&, const ResourceResponse& redirectResponse) = 0;
    virtual bool shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier) = 0;
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge&) = 0;
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, ResourceLoaderIdentifier, const ProtectionSpace&) = 0;
#endif

#if PLATFORM(IOS_FAMILY)
    virtual RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, ResourceLoaderIdentifier) = 0;
#endif

    virtual void dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&) = 0;
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int dataLength) = 0;
    virtual void dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier) = 0;
    virtual void dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&) = 0;
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
    virtual void dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<UsedLegacyTLS>) = 0;
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading) = 0;
    virtual void dispatchDidFailLoad(const ResourceError&) = 0;
    virtual void dispatchDidFinishDocumentLoad() = 0;
    virtual void dispatchDidFinishLoad() = 0;
    virtual void dispatchDidExplicitOpen(const URL&, const String& /* mimeType */) { }
#if ENABLE(DATA_DETECTION)
    virtual void dispatchDidFinishDataDetection(NSArray *detectionResults) = 0;
#endif

    virtual void dispatchDidLayout() { }
    virtual void dispatchDidReachLayoutMilestone(OptionSet<LayoutMilestone>) { }
    virtual void dispatchDidReachVisuallyNonEmptyState() { }

    virtual Frame* dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy) = 0;
    virtual void dispatchShow() = 0;

    virtual void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, PolicyCheckIdentifier, const String& downloadAttribute, FramePolicyFunction&&) = 0;
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

    virtual void committedLoad(DocumentLoader*, const SharedBuffer&) = 0;
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

    virtual ResourceError cancelledError(const ResourceRequest&) const = 0;
    virtual ResourceError blockedError(const ResourceRequest&) const = 0;
    virtual ResourceError blockedByContentBlockerError(const ResourceRequest&) const = 0;
    virtual ResourceError cannotShowURLError(const ResourceRequest&) const = 0;
    virtual ResourceError interruptedForPolicyChangeError(const ResourceRequest&) const = 0;
#if ENABLE(CONTENT_FILTERING)
    virtual ResourceError blockedByContentFilterError(const ResourceRequest&) const = 0;
#endif

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) const = 0;
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) const = 0;
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) const = 0;

    virtual bool shouldFallBack(const ResourceError&) const = 0;

    virtual bool canHandleRequest(const ResourceRequest&) const = 0;
    virtual bool canShowMIMEType(const String& MIMEType) const = 0;
    virtual bool canShowMIMETypeAsHTML(const String& MIMEType) const = 0;
    virtual bool representationExistsForURLScheme(StringView URLScheme) const = 0;
    virtual String generatedMIMETypeForURLScheme(StringView URLScheme) const = 0;

    virtual void frameLoadCompleted() = 0;
    virtual void saveViewStateToItem(HistoryItem&) = 0;
    virtual void restoreViewState() = 0;
    virtual void provisionalLoadStarted() = 0;
    virtual void didFinishLoad() = 0;
    virtual void prepareForDataSourceReplacement() = 0;

    virtual Ref<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) = 0;
    virtual void updateCachedDocumentLoader(DocumentLoader&) = 0;
    virtual void setTitle(const StringWithDirection&, const URL&) = 0;

    virtual String userAgent(const URL&) const = 0;

    virtual String overrideContentSecurityPolicy() const { return String(); }

    virtual void savePlatformDataToCachedFrame(CachedFrame*) = 0;
    virtual void transitionToCommittedFromCachedFrame(CachedFrame*) = 0;
#if PLATFORM(IOS_FAMILY)
    virtual void didRestoreFrameHierarchyForCachedFrame() = 0;
#endif
    virtual void transitionToCommittedForNewPage() = 0;

    virtual void didRestoreFromBackForwardCache() = 0;

    virtual bool canCachePage() const = 0;
    virtual void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) = 0;

    virtual RefPtr<Frame> createFrame(const AtomString& name, HTMLFrameOwnerElement&) = 0;
    virtual RefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement&, const URL&, const Vector<AtomString>&, const Vector<AtomString>&, const String&, bool loadManually) = 0;
    virtual void redirectDataToPlugin(Widget&) = 0;

    virtual ObjectContentType objectContentType(const URL&, const String& mimeType) = 0;
    virtual String overrideMediaType() const = 0;

    virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) = 0;

    virtual void registerForIconNotification() { }

#if PLATFORM(COCOA)
    // Allow an accessibility object to retrieve a Frame parent if there's no PlatformWidget.
    virtual RemoteAXObjectRef accessibilityRemoteObject() = 0;
    virtual void willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, NSCachedURLResponse*, CompletionHandler<void(NSCachedURLResponse *)>&&) const = 0;
    virtual NSDictionary *dataDetectionContext() { return nullptr; }
#endif

#if USE(CFURLCONNECTION)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    virtual bool shouldCacheResponse(DocumentLoader*, ResourceLoaderIdentifier, const ResourceResponse&, const unsigned char* data, unsigned long long length) = 0;
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
    virtual void dispatchServiceWorkerGlobalObjectAvailable(DOMWrapperWorld&) { }
    virtual void dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(DOMWindowExtension*) { }
    virtual void dispatchDidReconnectDOMWindowExtensionToGlobalObject(DOMWindowExtension*) { }
    virtual void dispatchWillDestroyGlobalObjectForDOMWindowExtension(DOMWindowExtension*) { }

    virtual void willInjectUserScript(DOMWrapperWorld&) { }

#if ENABLE(SERVICE_WORKER)
    virtual void didFinishServiceWorkerPageRegistration(bool success) { UNUSED_PARAM(success); }
#endif

#if ENABLE(WEB_RTC)
    virtual void dispatchWillStartUsingPeerConnectionHandler(RTCPeerConnectionHandler*) { }
#endif

#if ENABLE(WEBGL)
    virtual bool allowWebGL(bool enabledPerSettings) { return enabledPerSettings; }
    virtual WebGLLoadPolicy webGLPolicyForURL(const URL&) const { return WebGLLoadPolicy::WebGLAllowCreation; }
    virtual WebGLLoadPolicy resolveWebGLPolicyForURL(const URL&) const { return WebGLLoadPolicy::WebGLAllowCreation; }
#endif

    virtual void completePageTransitionIfNeeded() { }

    // FIXME (bug 116233): We need to get rid of EmptyFrameLoaderClient completely, then this will no longer be needed.
    virtual bool isEmptyFrameLoaderClient() const { return false; }
    virtual bool isRemoteWorkerFrameLoaderClient() const { return false; }

#if USE(QUICK_LOOK)
    virtual RefPtr<LegacyPreviewLoaderClient> createPreviewLoaderClient(const String&, const String&) = 0;
#endif

#if ENABLE(CONTENT_FILTERING)
    virtual void contentFilterDidBlockLoad(ContentFilterUnblockHandler) { }
#endif

    virtual void prefetchDNS(const String&) = 0;
    virtual void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&&) = 0;

    virtual void didRestoreScrollPosition() { }

    virtual void getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>&) { }

    virtual void didCreateWindow(DOMWindow&) { }

#if ENABLE(APPLICATION_MANIFEST)
    virtual void finishedLoadingApplicationManifest(uint64_t, const std::optional<ApplicationManifest>&) { }
#endif

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    virtual bool hasFrameSpecificStorageAccess() { return false; }
    virtual void didLoadFromRegistrableDomain(RegistrableDomain&&) { }
    virtual Vector<RegistrableDomain> loadedSubresourceDomains() const { return { }; }
#endif

    virtual AllowsContentJavaScript allowsContentJavaScriptFromMostRecentNavigation() const { return AllowsContentJavaScript::Yes; }

#if ENABLE(APP_BOUND_DOMAINS)
    virtual bool shouldEnableInAppBrowserPrivacyProtections() const { return false; }
    virtual void notifyPageOfAppBoundBehavior() { }
#endif

#if ENABLE(PDFKIT_PLUGIN)
    virtual bool shouldUsePDFPlugin(const String&, StringView) const { return false; }
#endif

    virtual bool isParentProcessAFullWebBrowser() const { return false; }

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    virtual void modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>)>&&) const { }
#endif
};

} // namespace WebCore
