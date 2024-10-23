/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InjectedBundlePage.h"

#include "ActivateFonts.h"
#include "InjectedBundle.h"
#include "StringFunctions.h"
#include "WPTFunctions.h"
#include "WebCoreTestSupport.h"
#include <cmath>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/RegularExpression.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleBackForwardListItem.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundleFramePrivate.h>
#include <WebKit/WKBundleHitTestResult.h>
#include <WebKit/WKBundleNodeHandlePrivate.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKURLRequest.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

#if USE(CF)
#include "WebArchiveDumpSupport.h"
#include <wtf/text/cf/StringConcatenateCF.h>
#endif

namespace WTF {

template<> class StringTypeAdapter<WKStringRef> {
public:
    StringTypeAdapter(WKStringRef);
    unsigned length() const { return m_string ? WKStringGetLength(m_string) : 0; }
    bool is8Bit() const { return !m_string; }
    template<typename CharacterType> void writeTo(CharacterType*) const;

private:
    WKStringRef m_string;
};

inline StringTypeAdapter<WKStringRef>::StringTypeAdapter(WKStringRef string)
    : m_string { string }
{
}

template<> inline void StringTypeAdapter<WKStringRef>::writeTo<LChar>(LChar*) const
{
}

template<> inline void StringTypeAdapter<WKStringRef>::writeTo<UChar>(UChar* destination) const
{
    if (m_string)
        WKStringGetCharacters(m_string, reinterpret_cast<WKChar*>(destination), WKStringGetLength(m_string));
}

}

namespace WTR {

static double numericWindowProperty(WKBundleFrameRef frame, const char* name)
{
    auto context = WKBundleFrameGetJavaScriptContext(frame);
    return numericProperty(context, JSContextGetGlobalObject(context), name);
}

static WTF::String dumpPath(JSGlobalContextRef context, JSObjectRef nodeValue)
{
    auto name = toWTFString(stringProperty(context, nodeValue, "nodeName"));
    if (auto parentNode = objectProperty(context, nodeValue, "parentNode"))
        return makeString(name, " > "_s, dumpPath(context, parentNode));
    return name;
}

static WTF::String dumpPath(WKBundlePageRef page, WKBundleScriptWorldRef world, WKBundleNodeHandleRef node)
{
    if (!node)
        return "(null)"_s;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);
    ALLOW_DEPRECATED_DECLARATIONS_END

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSValueRef nodeValue = WKBundleFrameGetJavaScriptWrapperForNodeForWorld(frame, node, world);
    ASSERT(JSValueIsObject(context, nodeValue));
    JSObjectRef nodeObject = (JSObjectRef)nodeValue;

    return dumpPath(context, nodeObject);
}

static WTF::String string(WKBundlePageRef page, WKBundleScriptWorldRef world, WKBundleRangeHandleRef rangeRef)
{
    if (!rangeRef)
        return "(null)"_s;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto frame = WKBundlePageGetMainFrame(page);
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    auto rangeValue = WKBundleFrameGetJavaScriptWrapperForRangeForWorld(frame, rangeRef, world);
    ASSERT(JSValueIsObject(context, rangeValue));
    auto rangeObject = (JSObjectRef)rangeValue;

    return makeString("range from "_s,
        numericProperty(context, rangeObject, "startOffset"),
        " of "_s,
        dumpPath(context, objectProperty(context, rangeObject, "startContainer")),
        " to "_s,
        numericProperty(context, rangeObject, "endOffset"),
        " of "_s,
        dumpPath(context, objectProperty(context, rangeObject, "endContainer"))
    );
}

static WTF::String styleDecToStr(WKBundleCSSStyleDeclarationRef)
{
    // DumpRenderTree calls -[DOMCSSStyleDeclaration description], which just dumps class name and object address.
    // No existing tests actually hit this code path at the time of this writing, because WebCore doesn't call
    // the editing client if the styling operation source is CommandFromDOM or CommandFromDOMWithUserInterface.
    return "<DOMCSSStyleDeclaration ADDRESS>"_s;
}

static WTF::String string(WKSecurityOriginRef origin)
{
    return makeString('{', adoptWK(WKSecurityOriginCopyProtocol(origin)).get(), ", "_s, adoptWK(WKSecurityOriginCopyHost(origin)).get(), ", "_s, WKSecurityOriginGetPort(origin), '}');
}

static WTF::String string(WKBundleFrameRef frame)
{
    auto name = adoptWK(WKBundleFrameCopyName(frame));
    bool isMain = WKBundleFrameIsMainFrame(frame);
    if (WKStringIsEmpty(name.get()))
        return isMain ? "main frame"_s : "frame (anonymous)"_s;
    return makeString(isMain ? "main frame \""_s : "frame \""_s, name.get(), '"');
}

static inline bool isLocalFileScheme(WKStringRef scheme)
{
    return WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "file");
}

static const char divider = '/';

WTF::String pathSuitableForTestResult(WKURLRef fileURL)
{
    if (!fileURL)
        return "(null)"_s;

    auto schemeString = adoptWK(WKURLCopyScheme(fileURL));
    if (!isLocalFileScheme(schemeString.get()))
        return toWTFString(adoptWK(WKURLCopyString(fileURL)));

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto mainFrameURL = adoptWK(WKBundleFrameCopyURL(mainFrame));
    if (!mainFrameURL)
        mainFrameURL = adoptWK(WKBundleFrameCopyProvisionalURL(mainFrame));

    String pathString = toWTFString(adoptWK(WKURLCopyPath(fileURL)));
    String mainFrameURLPathString = toWTFString(adoptWK(WKURLCopyPath(mainFrameURL.get())));
    auto basePath = StringView(mainFrameURLPathString).left(mainFrameURLPathString.reverseFind(divider) + 1);
    
    if (!basePath.isEmpty() && pathString.startsWith(basePath))
        return pathString.substring(basePath.length());
    return toWTFString(adoptWK(WKURLCopyLastPathComponent(fileURL))); // We lose some information here, but it's better than exposing a full path, which is always machine specific.
}

static HashMap<uint64_t, String>& assignedUrlsCache()
{
    static NeverDestroyed<HashMap<uint64_t, String>> cache;
    return cache.get();
}

static inline void dumpResourceURL(uint64_t identifier, StringBuilder& stringBuilder)
{
    if (assignedUrlsCache().contains(identifier))
        stringBuilder.append(assignedUrlsCache().get(identifier));
    else
        stringBuilder.append("<unknown>"_s);
}

static HashMap<WKBundlePageRef, InjectedBundlePage*>& bundlePageMap()
{
    static NeverDestroyed<HashMap<WKBundlePageRef, InjectedBundlePage*>> map;
    return map.get();
}

InjectedBundlePage::InjectedBundlePage(WKBundlePageRef page)
    : m_page(page)
    , m_world(adoptWK(WKBundleScriptWorldCreateWorld()))
{
    ASSERT(!bundlePageMap().contains(page));
    bundlePageMap().set(page, this);

    WKBundlePageLoaderClientV9 loaderClient = {
        { 9, this },
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        didFinishDocumentLoadForFrame,
        didFinishLoadForFrame,
        didFailLoadWithErrorForFrame,
        didSameDocumentNavigationForFrame,
        didReceiveTitleForFrame,
        0, // didFirstLayoutForFrame
        0, // didFirstVisuallyNonEmptyLayoutForFrame
        0, // didRemoveFrameFromHierarchy
        didDisplayInsecureContentForFrame,
        didRunInsecureContentForFrame,
        didClearWindowForFrame,
        didCancelClientRedirectForFrame,
        willPerformClientRedirectForFrame,
        didHandleOnloadEventsForFrame,
        0, // didLayoutForFrame
        0, // didNewFirstVisuallyNonEmptyLayout_unavailable
        0, // didDetectXSSForFrame
        0, // shouldGoToBackForwardListItem
        0, // didCreateGlobalObjectForFrame
        0, // willDisconnectDOMWindowExtensionFromGlobalObject
        0, // didReconnectDOMWindowExtensionToGlobalObject
        0, // willDestroyGlobalObjectForDOMWindowExtension
        didFinishProgress, // didFinishProgress
        0, // shouldForceUniversalAccessFromLocalURL
        0, // didReceiveIntentForFrame
        0, // registerIntentServiceForFrame
        0, // didLayout
        0, // featuresUsedInPage
        0, // willLoadURLRequest
        0, // willLoadDataRequest
        0, // willDestroyFrame_unavailable
        0, // userAgentForURL
        willInjectUserScriptForFrame
    };
    WKBundlePageSetPageLoaderClient(m_page, &loaderClient.base);

    WKBundlePageResourceLoadClientV1 resourceLoadClient = {
        { 1, this },
        didInitiateLoadForResource,
        willSendRequestForFrame,
        didReceiveResponseForResource,
        didReceiveContentLengthForResource,
        didFinishLoadForResource,
        didFailLoadForResource,
        shouldCacheResponse,
        0 // shouldUseCredentialStorage
    };
    WKBundlePageSetResourceLoadClient(m_page, &resourceLoadClient.base);

    WKBundlePageUIClientV2 uiClient = {
        { 2, this },
        willAddMessageToConsole,
        willSetStatusbarText,
        willRunJavaScriptAlert,
        willRunJavaScriptConfirm,
        willRunJavaScriptPrompt,
        0, /*mouseDidMoveOverElement*/
        0, /*pageDidScroll*/
        0, /*paintCustomOverhangArea*/
        0, /*shouldGenerateFileForUpload*/
        0, /*generateFileForUpload*/
        0, /*shouldRubberBandInDirection*/
        0, /*statusBarIsVisible*/
        0, /*menuBarIsVisible*/
        0, /*toolbarsAreVisible*/
        0, /*didReachApplicationCacheOriginQuota*/
        didExceedDatabaseQuota,
        0, /*plugInStartLabelTitle*/
        0, /*plugInStartLabelSubtitle*/
        0, /*plugInExtraStyleSheet*/
        0, /*plugInExtraScript*/
    };
    WKBundlePageSetUIClient(m_page, &uiClient.base);

    WKBundlePageEditorClientV1 editorClient = {
        { 1, this },
        shouldBeginEditing,
        shouldEndEditing,
        shouldInsertNode,
        shouldInsertText,
        shouldDeleteRange,
        shouldChangeSelectedRange,
        shouldApplyStyle,
        didBeginEditing,
        didEndEditing,
        didChange,
        didChangeSelection,
        0, /* willWriteToPasteboard */
        0, /* getPasteboardDataForRange */
        0, /* didWriteToPasteboard */
        0, /* performTwoStepDrop */
    };
    WKBundlePageSetEditorClient(m_page, &editorClient.base);

#if ENABLE(FULLSCREEN_API)
    WKBundlePageFullScreenClientV1 fullScreenClient = {
        { 1, this },
        supportsFullScreen,
        enterFullScreenForElement,
        exitFullScreenForElement,
        beganEnterFullScreen,
        beganExitFullScreen,
        closeFullScreen,
    };
    WKBundlePageSetFullScreenClient(m_page, &fullScreenClient.base);
#endif
}

InjectedBundlePage::~InjectedBundlePage()
{
    ASSERT(bundlePageMap().contains(m_page));
    bundlePageMap().remove(m_page);
}

void InjectedBundlePage::prepare()
{
    WKBundlePageClearMainFrameName(m_page);

    WKBundleClearHistoryForTesting(m_page);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundleFrameClearOpener(WKBundlePageGetMainFrame(m_page));
    ALLOW_DEPRECATED_DECLARATIONS_END

    WKBundlePageSetTracksRepaints(m_page, false);
    
    // Force consistent "responsive" behavior for WebPage::eventThrottlingDelay() for testing. Tests can override via internals.
    WKEventThrottlingBehavior behavior = kWKEventThrottlingBehaviorResponsive;
    WKBundlePageSetEventThrottlingBehaviorOverride(m_page, &behavior);
}

void InjectedBundlePage::resetAfterTest()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    ALLOW_DEPRECATED_DECLARATIONS_END

    // WebKit currently doesn't reset focus even when navigating to a new page. This may or may not be a bug
    // (see <https://bugs.webkit.org/show_bug.cgi?id=138334>), however for tests, we want to start each one with a clean state.
    WKBundleFrameFocus(frame);

    WebCoreTestSupport::resetInternalsObject(WKBundleFrameGetJavaScriptContext(frame));
    assignedUrlsCache().clear();

    // User scripts need to be removed after the test and before loading about:blank, as otherwise they would run in about:blank, and potentially leak results into a subsequest test.
    WKBundlePageRemoveAllUserContent(m_page);

    uninstallFakeHelvetica();

    InjectedBundle::singleton().resetUserScriptInjectedCount();

    m_didCommitMainFrameLoad = false;
}

// Loader Client Callbacks

static void dumpLoadEvent(WKBundleFrameRef frame, ASCIILiteral eventName)
{
    InjectedBundle::singleton().outputText(makeString(string(frame), " - "_s, eventName, '\n'));
}

static String string(WKURLRequestRef request)
{
    auto url = adoptWK(WKURLRequestCopyURL(request));
    auto firstParty = adoptWK(WKURLRequestCopyFirstPartyForCookies(request));
    auto httpMethod = adoptWK(WKURLRequestCopyHTTPMethod(request));
    return makeString("<NSURLRequest URL "_s, pathSuitableForTestResult(url.get()),
        ", main document URL "_s, pathSuitableForTestResult(firstParty.get()),
        ", http method "_s, WKStringIsEmpty(httpMethod.get()) ? "(none)"_s : ""_s, httpMethod.get(), '>');
}

static String string(WKURLResponseRef response, bool shouldDumpResponseHeaders = false)
{
    auto url = adoptWK(WKURLResponseCopyURL(response));
    if (!url)
        return "(null)"_s;
    if (!shouldDumpResponseHeaders) {
        return makeString("<NSURLResponse "_s, pathSuitableForTestResult(url.get()),
            ", http status code "_s, WKURLResponseHTTPStatusCode(response), '>');
    }
    return makeString("<NSURLResponse "_s, pathSuitableForTestResult(url.get()),
        ", http status code "_s, WKURLResponseHTTPStatusCode(response),
        ", "_s, InjectedBundlePage::responseHeaderCount(response), " headers>"_s);
}

#if !PLATFORM(COCOA)

// FIXME: Implement this for non-Cocoa ports. [GTK][WPE] https://bugs.webkit.org/show_bug.cgi?id=184295
uint64_t InjectedBundlePage::responseHeaderCount(WKURLResponseRef response)
{
    return 0;
}

#endif

static inline void dumpErrorDescriptionSuitableForTestResult(WKErrorRef error, StringBuilder& stringBuilder)
{
    auto errorDomain = toWTFString(adoptWK(WKErrorCopyDomain(error)));
    auto errorCode = WKErrorGetErrorCode(error);

    // We need to do some error mapping here to match the test expectations (Mac error names are expected).
    if (errorDomain == "WebKitNetworkError"_s) {
        errorDomain = "NSURLErrorDomain"_s;
        errorCode = -999;
    }
    if (errorDomain == "WebKitPolicyError"_s)
        errorDomain = "WebKitErrorDomain"_s;

    stringBuilder.append("<NSError domain "_s, errorDomain, ", code "_s, errorCode);
    if (auto url = adoptWK(WKErrorCopyFailingURL(error)))
        stringBuilder.append(", failing URL \""_s, adoptWK(WKURLCopyString(url.get())).get(), '"');
    stringBuilder.append('>');
}

void InjectedBundlePage::didStartProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didStartProvisionalLoadForFrame(frame);
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveServerRedirectForProvisionalLoadForFrame(frame);
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKErrorRef error, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailProvisionalLoadWithErrorForFrame(frame, error);
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didCommitLoadForFrame(frame);
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishLoadForFrame(frame);
}

void InjectedBundlePage::didFinishProgress(WKBundlePageRef, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishProgress();
}

void InjectedBundlePage::willInjectUserScriptForFrame(WKBundlePageRef, WKBundleFrameRef, WKBundleScriptWorldRef, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willInjectUserScriptForFrame();
}

void InjectedBundlePage::didFinishDocumentLoadForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishDocumentLoadForFrame(frame);
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKErrorRef error, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailLoadWithErrorForFrame(frame, error);
}

void InjectedBundlePage::didReceiveTitleForFrame(WKBundlePageRef page, WKStringRef title, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveTitleForFrame(title, frame);
}

void InjectedBundlePage::didClearWindowForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleScriptWorldRef world, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didClearWindowForFrame(frame, world);
}

void InjectedBundlePage::didCancelClientRedirectForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didCancelClientRedirectForFrame(frame);
}

void InjectedBundlePage::willPerformClientRedirectForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKURLRef url, double delay, double date, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willPerformClientRedirectForFrame(page, frame, url, delay, date);
}

void InjectedBundlePage::didSameDocumentNavigationForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKSameDocumentNavigationType type, WKTypeRef*, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didSameDocumentNavigationForFrame(frame, type);
}

void InjectedBundlePage::didHandleOnloadEventsForFrame(WKBundlePageRef page, WKBundleFrameRef frame, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didHandleOnloadEventsForFrame(frame);
}

void InjectedBundlePage::didDisplayInsecureContentForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didDisplayInsecureContentForFrame(frame);
}

void InjectedBundlePage::didRunInsecureContentForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didRunInsecureContentForFrame(frame);
}

void InjectedBundlePage::didInitiateLoadForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKURLRequestRef request, bool pageLoadIsProvisional, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didInitiateLoadForResource(page, frame, identifier, request, pageLoadIsProvisional);
}

WKURLRequestRef InjectedBundlePage::willSendRequestForFrame(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKURLRequestRef request, WKURLResponseRef redirectResponse, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willSendRequestForFrame(page, frame, identifier, request, redirectResponse);
}

void InjectedBundlePage::didReceiveResponseForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKURLResponseRef response, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveResponseForResource(page, frame, identifier, response);
}

void InjectedBundlePage::didReceiveContentLengthForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, uint64_t length, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReceiveContentLengthForResource(page, frame, identifier, length);
}

void InjectedBundlePage::didFinishLoadForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFinishLoadForResource(page, frame, identifier);
}

void InjectedBundlePage::didFailLoadForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKErrorRef error, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didFailLoadForResource(page, frame, identifier, error);
}

bool InjectedBundlePage::shouldCacheResponse(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldCacheResponse(page, frame, identifier);
}

void InjectedBundlePage::didStartProvisionalLoadForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->testURL()) {
        auto testURL = adoptWK(WKBundleFrameCopyProvisionalURL(frame));
        testRunner->setTestURL(testURL.get());
    }

    platformDidStartProvisionalLoadForFrame(frame);

    if (testRunner->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didStartProvisionalLoadForFrame"_s);

    if (!injectedBundle.topLoadingFrame())
        injectedBundle.setTopLoadingFrame(frame);

    if (testRunner->shouldStopProvisionalFrameLoads())
        dumpLoadEvent(frame, "stopping load in didStartProvisionalLoadForFrame callback"_s);
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef frame)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpFrameLoadCallbacks())
        return;

    dumpLoadEvent(frame, "didReceiveServerRedirectForProvisionalLoadForFrame"_s);
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef frame, WKErrorRef error)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    // In case of a COOP process-swap, the old process gets a didFailProvisionalLoadWithErrorForFrame delegate call. We want to ignore
    // this call since it causes the test to dump its output too eagerly, before the test has had a chance to run in the new process.
    if (WKErrorGetErrorCode(error) == kWKErrorCodeFrameLoadInterruptedByPolicyChange && WKBundleFrameIsMainFrame(frame) && !m_didCommitMainFrameLoad && injectedBundle.page() == this)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks()) {
        dumpLoadEvent(frame, "didFailProvisionalLoadWithError"_s);
        auto code = WKErrorGetErrorCode(error);
        if (code == kWKErrorCodeCannotShowURL)
            dumpLoadEvent(frame, "(ErrorCodeCannotShowURL)"_s);
        else if (code == kWKErrorCodeFrameLoadBlockedByContentBlocker)
            dumpLoadEvent(frame, "(kWKErrorCodeFrameLoadBlockedByContentBlocker)"_s);
    }

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundleFrameRef frame)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (WKBundleFrameIsMainFrame(frame))
        m_didCommitMainFrameLoad = true;

    if (!testRunner->shouldDumpFrameLoadCallbacks())
        return;

    dumpLoadEvent(frame, "didCommitLoadForFrame"_s);
}

void InjectedBundlePage::didFinishProgress()
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpProgressFinishedCallback())
        return;

    injectedBundle.outputText("postProgressFinishedNotification\n"_s);
}

void InjectedBundlePage::willInjectUserScriptForFrame()
{
    InjectedBundle::singleton().increaseUserScriptInjectedCount();
}

enum FrameNamePolicy { ShouldNotIncludeFrameName, ShouldIncludeFrameName };

static void dumpFrameScrollPosition(WKBundleFrameRef frame, StringBuilder& stringBuilder, FrameNamePolicy shouldIncludeFrameName = ShouldNotIncludeFrameName)
{
    double x = numericWindowProperty(frame, "pageXOffset");
    double y = numericWindowProperty(frame, "pageYOffset");
    if (std::abs(x) <= 0.00000001 && std::abs(y) <= 0.00000001)
        return;
    if (shouldIncludeFrameName)
        stringBuilder.append("frame '"_s, adoptWK(WKBundleFrameCopyName(frame)).get(), "' "_s);
    stringBuilder.append("scrolled to "_s, x, ',', y, '\n');
}

static void dumpDescendantFrameScrollPositions(WKBundleFrameRef frame, StringBuilder& stringBuilder)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto childFrames = adoptWK(WKBundleFrameCopyChildFrames(frame));
    ALLOW_DEPRECATED_DECLARATIONS_END
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        dumpFrameScrollPosition(subframe, stringBuilder, ShouldIncludeFrameName);
        dumpDescendantFrameScrollPositions(subframe, stringBuilder);
    }
}

void InjectedBundlePage::dumpAllFrameScrollPositions(StringBuilder& stringBuilder)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    ALLOW_DEPRECATED_DECLARATIONS_END
    dumpFrameScrollPosition(frame, stringBuilder);
    dumpDescendantFrameScrollPositions(frame, stringBuilder);
}

void InjectedBundlePage::dumpAllFramesText(StringBuilder& stringBuilder)
{
    constexpr bool includeSubframes { true };
    stringBuilder.append(toWTFString(adoptWK(WKBundlePageCopyFrameTextForTesting(m_page, includeSubframes))));
}

void InjectedBundlePage::dumpDOMAsWebArchive(WKBundleFrameRef frame, StringBuilder& stringBuilder)
{
#if USE(CF)
    auto wkData = adoptWK(WKBundleFrameCopyWebArchive(frame));
    auto cfData = adoptCF(CFDataCreate(0, WKDataGetBytes(wkData.get()), WKDataGetSize(wkData.get())));
    stringBuilder.append(WebCoreTestSupport::createXMLStringFromWebArchiveData(cfData.get()).get());
#endif
}

void InjectedBundlePage::dump(bool forceRepaint)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (forceRepaint) {
        // Force a paint before dumping. This matches DumpRenderTree on Windows. (DumpRenderTree on Mac
        // does this at a slightly different time.) See <http://webkit.org/b/55469> for details.
        WKBundlePageForceRepaint(m_page);
    }
    WKBundlePageFlushPendingEditorStateUpdate(m_page);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto urlRef = adoptWK(WKBundleFrameCopyURL(frame));
    if (!urlRef)
        return;
    String url = toWTFString(adoptWK(WKURLCopyString(urlRef.get())));
    auto mimeType = adoptWK(WKBundleFrameCopyMIMETypeForResourceWithURL(frame, urlRef.get()));
    if (url.find("dumpAsText/"_s) != notFound || WKStringIsEqualToUTF8CString(mimeType.get(), "text/plain"))
        testRunner->dumpAsText(false);

    StringBuilder stringBuilder;

    switch (testRunner->whatToDump()) {
    case WhatToDump::RenderTree: {
        if (testRunner->isPrinting())
            stringBuilder.append(adoptWK(WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(m_page)).get());
        else
            stringBuilder.append(adoptWK(WKBundlePageCopyRenderTreeExternalRepresentation(m_page, testRunner->renderTreeDumpOptions())).get());
        break;
    }
    case WhatToDump::MainFrameText: {
        constexpr bool includeSubframes { false };
        stringBuilder.append(toWTFString(adoptWK(WKBundlePageCopyFrameTextForTesting(m_page, includeSubframes))));
        break;
    }
    case WhatToDump::AllFramesText:
        dumpAllFramesText(stringBuilder);
        break;
    case WhatToDump::Audio:
        break;
    case WhatToDump::DOMAsWebArchive:
        dumpDOMAsWebArchive(frame, stringBuilder);
        break;
    }

    if (testRunner->shouldDumpAllFrameScrollPositions())
        dumpAllFrameScrollPositions(stringBuilder);
    else if (testRunner->shouldDumpMainFrameScrollPosition()) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        dumpFrameScrollPosition(WKBundlePageGetMainFrame(m_page), stringBuilder);
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    if (testRunner->shouldDumpBackForwardListsForAllWindows())
        injectedBundle.dumpBackForwardListsForAllPages(stringBuilder);

    if (injectedBundle.shouldDumpPixels() && testRunner->shouldDumpPixels()) {
        bool shouldCreateSnapshot = testRunner->isPrinting();
        if (shouldCreateSnapshot) {
            WKSnapshotOptions options = kWKSnapshotOptionsShareable;
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            WKRect snapshotRect = WKBundleFrameGetVisibleContentBounds(WKBundlePageGetMainFrame(m_page));
            ALLOW_DEPRECATED_DECLARATIONS_END

            if (testRunner->isPrinting())
                options |= kWKSnapshotOptionsPrinting;
            else {
                options |= kWKSnapshotOptionsInViewCoordinates;
                if (testRunner->shouldDumpSelectionRect())
                    options |= kWKSnapshotOptionsPaintSelectionRectangle;
            }

            injectedBundle.setPixelResult(adoptWK(WKBundlePageCreateSnapshotWithOptions(m_page, snapshotRect, options)).get());
        } else
            injectedBundle.setPixelResultIsPending(true);

        if (WKBundlePageIsTrackingRepaints(m_page) && !testRunner->isPrinting())
            injectedBundle.setRepaintRects(adoptWK(WKBundlePageCopyTrackedRepaintRects(m_page)).get());
    }

    injectedBundle.outputText(stringBuilder.toString(), InjectedBundle::IsFinalTestOutput::Yes);
    injectedBundle.done(forceRepaint);
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundleFrameRef frame)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didFinishLoadForFrame"_s);

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundleFrameRef frame, WKErrorRef)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didFailLoadWithError"_s);

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    StringBuilder stringBuilder;
    if (testRunner->shouldDumpFrameLoadCallbacks())
        stringBuilder.append(string(frame), " - didReceiveTitle: "_s, title, '\n');
    if (testRunner->shouldDumpTitleChanges())
        stringBuilder.append("TITLE CHANGED: '"_s, title, "'\n"_s);
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didClearWindowForFrame(WKBundleFrameRef frame, WKBundleScriptWorldRef world)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    auto context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);

    if (WKBundleScriptWorldNormalWorld() != world) {
        setGlobalObjectProperty(context, "__worldID", TestRunner::worldIDForWorld(world));
        return;
    }

    testRunner->makeWindowObject(context);
    injectedBundle.gcController()->makeWindowObject(context);
    injectedBundle.eventSendingController()->makeWindowObject(context);
    injectedBundle.textInputController()->makeWindowObject(context);
    injectedBundle.accessibilityController()->makeWindowObject(context);

    WebCoreTestSupport::injectInternalsObject(context);
}

void InjectedBundlePage::didCancelClientRedirectForFrame(WKBundleFrameRef frame)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didCancelClientRedirectForFrame"_s);

    testRunner->setDidCancelClientRedirect(true);
}

void InjectedBundlePage::willPerformClientRedirectForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKURLRef url, double delay, double date)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpFrameLoadCallbacks())
        return;

    injectedBundle.outputText(makeString(string(frame), " - willPerformClientRedirectToURL: "_s, pathSuitableForTestResult(url), '\n'));
}

void InjectedBundlePage::didSameDocumentNavigationForFrame(WKBundleFrameRef frame, WKSameDocumentNavigationType type)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpFrameLoadCallbacks())
        return;

    if (type != kWKSameDocumentNavigationAnchorNavigation)
        return;

    dumpLoadEvent(frame, "didChangeLocationWithinPageForFrame"_s);
}

void InjectedBundlePage::didFinishDocumentLoadForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didFinishDocumentLoadForFrame"_s);

    if (unsigned pendingFrameUnloadEvents = WKBundleFrameGetPendingUnloadCount(frame))
        injectedBundle.outputText(makeString(string(frame), " - has "_s, pendingFrameUnloadEvents, " onunload handler(s)\n"_s));
}

void InjectedBundlePage::didHandleOnloadEventsForFrame(WKBundleFrameRef frame)
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didHandleOnloadEventsForFrame"_s);
}

void InjectedBundlePage::didDisplayInsecureContentForFrame(WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;
    if (testRunner->shouldDumpFrameLoadCallbacks())
        injectedBundle.outputText("didDisplayInsecureContent\n"_s);
}

void InjectedBundlePage::didRunInsecureContentForFrame(WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFrameLoadCallbacks())
        injectedBundle.outputText("didRunInsecureContent\n"_s);
}

void InjectedBundlePage::didInitiateLoadForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKURLRequestRef request, bool)
{
    if (!InjectedBundle::singleton().isTestRunning())
        return;

    auto url = adoptWK(WKURLRequestCopyURL(request));
    assignedUrlsCache().add(identifier, pathSuitableForTestResult(url.get()));
}

// Resource Load Client Callbacks

static inline bool isLocalHost(WKStringRef host)
{
    return WKStringIsEqualToUTF8CString(host, "127.0.0.1") || WKStringIsEqualToUTF8CString(host, "localhost");
}

static inline bool isHTTPOrHTTPSScheme(WKStringRef scheme)
{
    return WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "http") || WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "https");
}

static inline bool isAllowedHost(WKStringRef host)
{
    return InjectedBundle::singleton().isAllowedHost(host);
}

WKURLRequestRef InjectedBundlePage::willSendRequestForFrame(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKURLRequestRef request, WKURLResponseRef response)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (testRunner && testRunner->shouldDumpResourceLoadCallbacks()) {
        StringBuilder stringBuilder;
        dumpResourceURL(identifier, stringBuilder);
        stringBuilder.append(" - willSendRequest "_s, string(request),
            " redirectResponse "_s, string(response, testRunner->shouldDumpAllHTTPRedirectedResponseHeaders()), '\n');
        injectedBundle.outputText(stringBuilder.toString());
    }

    if (testRunner && testRunner->willSendRequestReturnsNull())
        return nullptr;

    auto redirectURL = adoptWK(WKURLResponseCopyURL(response));
    if (testRunner && testRunner->willSendRequestReturnsNullOnRedirect() && redirectURL) {
        injectedBundle.outputText("Returning null for this redirect\n"_s);
        return nullptr;
    }

    auto url = adoptWK(WKURLRequestCopyURL(request));
    auto host = adoptWK(WKURLCopyHostName(url.get()));
    auto scheme = adoptWK(WKURLCopyScheme(url.get()));
    auto urlString = adoptWK(WKURLCopyString(url.get()));
    if (host && !WKStringIsEmpty(host.get())
        && isHTTPOrHTTPSScheme(scheme.get())
        && !WKStringIsEqualToUTF8CString(host.get(), "255.255.255.255") // Used in some tests that expect to get back an error.
        && !isLocalHost(host.get())) {
        bool mainFrameIsExternal = false;
        if (injectedBundle.isTestRunning()) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(m_page);
            ALLOW_DEPRECATED_DECLARATIONS_END
            auto mainFrameURL = adoptWK(WKBundleFrameCopyURL(mainFrame));
            if (!mainFrameURL || WKStringIsEqualToUTF8CString(adoptWK(WKURLCopyString(mainFrameURL.get())).get(), "about:blank"))
                mainFrameURL = adoptWK(WKBundleFrameCopyProvisionalURL(mainFrame));
            if (mainFrameURL) {
                auto mainFrameHost = adoptWK(WKURLCopyHostName(mainFrameURL.get()));
                auto mainFrameScheme = adoptWK(WKURLCopyScheme(mainFrameURL.get()));
                mainFrameIsExternal = isHTTPOrHTTPSScheme(mainFrameScheme.get()) && !isLocalHost(mainFrameHost.get());
            }
        }
        if (!mainFrameIsExternal && !isAllowedHost(host.get())) {
            auto blockedURL = makeString(urlString.get());
            replace(blockedURL, JSC::Yarr::RegularExpression("\\?key=[-0123456789abcdefABCDEF]+"_s), "?key=GENERATED_KEY"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("&key=[-0123456789abcdefABCDEF]+"_s), "&key=GENERATED_KEY"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("%3Fkey%3D[-0123456789abcdefABCDEF]+"_s), "%3Fkey%3DGENERATED_KEY"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("%26key%3D[-0123456789abcdefABCDEF]+"_s), "%26key%3DGENERATED_KEY"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("%253Fkey%253D[-0123456789abcdefABCDEF]+"_s), "%253Fkey%253DGENERATED_KEY"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("%2526key%253D[-0123456789abcdefABCDEF]+"_s), "%2526key%253DGENERATED_KEY"_s);
            replace(blockedURL, JSC::Yarr::RegularExpression("reportID=[-0123456789abcdefABCDEF]+"_s), "reportID=GENERATED_REPORT_ID"_s);
            injectedBundle.outputText(makeString("Blocked access to external URL "_s, blockedURL, '\n'));
            return nullptr;
        }
    }
    
    if (testRunner) {
        String body = testRunner->willSendRequestHTTPBody();
        if (!body.isEmpty()) {
            CString cBody = body.utf8();
            auto body = adoptWK(WKDataCreate(reinterpret_cast<const unsigned char*>(cBody.data()), cBody.length()));
            return WKURLRequestCopySettingHTTPBody(request, body.get());
        }
    }

    WKRetain(request);
    return request;
}

void InjectedBundlePage::didReceiveResponseForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKURLResponseRef response)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpResourceLoadCallbacks()) {
        StringBuilder stringBuilder;
        dumpResourceURL(identifier, stringBuilder);
        stringBuilder.append(" - didReceiveResponse "_s, string(response), '\n');
        injectedBundle.outputText(stringBuilder.toString());
    }


    if (!testRunner->shouldDumpResourceResponseMIMETypes())
        return;

    auto url = adoptWK(WKURLResponseCopyURL(response));
    auto urlString = adoptWK(WKURLCopyLastPathComponent(url.get()));
    auto mimeTypeString = adoptWK(WKURLResponseCopyMIMEType(response));

    StringBuilder stringBuilder;
    stringBuilder.append(urlString.get(), " has MIME type "_s, mimeTypeString.get());

    String platformMimeType = platformResponseMimeType(response);
    if (!platformMimeType.isEmpty() && platformMimeType != toWTFString(mimeTypeString)) {
        stringBuilder.append(" but platform response has "_s, platformMimeType);
    }

    stringBuilder.append('\n');

    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didReceiveContentLengthForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t, uint64_t)
{
}

void InjectedBundlePage::didFinishLoadForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpResourceLoadCallbacks())
        return;

    StringBuilder stringBuilder;
    dumpResourceURL(identifier, stringBuilder);
    stringBuilder.append(" - didFinishLoading\n"_s);
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didFailLoadForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier, WKErrorRef error)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpResourceLoadCallbacks())
        return;

    StringBuilder stringBuilder;
    dumpResourceURL(identifier, stringBuilder);
    stringBuilder.append(" - didFailLoadingWithError: "_s);

    dumpErrorDescriptionSuitableForTestResult(error, stringBuilder);
    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());
}

bool InjectedBundlePage::shouldCacheResponse(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    if (!testRunner->shouldDumpWillCacheResponse())
        return true;

    injectedBundle.outputText(makeString(identifier, " - willCacheResponse: called\n"_s));

    // The default behavior is the cache the response.
    return true;
}

// UI Client Callbacks

void InjectedBundlePage::willAddMessageToConsole(WKBundlePageRef page, WKStringRef message, uint32_t /* lineNumber */, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willAddMessageToConsole(message);
}

void InjectedBundlePage::willSetStatusbarText(WKBundlePageRef page, WKStringRef statusbarText, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willSetStatusbarText(statusbarText);
}

void InjectedBundlePage::willRunJavaScriptAlert(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptAlert(message, frame);
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKBundlePageRef page, WKStringRef message, WKBundleFrameRef frame, const void *clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptConfirm(message, frame);
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKBundlePageRef page, WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef frame, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willRunJavaScriptPrompt(message, defaultValue, frame);
}

uint64_t InjectedBundlePage::didExceedDatabaseQuota(WKBundlePageRef page, WKSecurityOriginRef origin, WKStringRef databaseName, WKStringRef databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didExceedDatabaseQuota(origin, databaseName, databaseDisplayName, currentQuotaBytes, currentOriginUsageBytes, currentDatabaseUsageBytes, expectedUsageBytes);
}

static WTF::String stripTrailingSpacesAddNewline(const WTF::String& string)
{
    StringBuilder builder;
    for (auto line : StringView(string).splitAllowingEmptyEntries('\n')) {
        while (line.endsWith(' '))
            line = line.left(line.length() - 1);
        builder.append(line, '\n');
    }
    return builder.toString();
}

static WTF::String addLeadingSpaceStripTrailingSpacesAddNewline(const WTF::String& string)
{
    auto result = stripTrailingSpacesAddNewline(string);
    return (result.isEmpty() || result.startsWith('\n')) ? result : makeString(' ', result);
}

static StringView lastFileURLPathComponent(StringView path)
{
    auto pos = path.find("file://"_s);
    ASSERT(WTF::notFound != pos);

    auto tmpPath = path.substring(pos + 7);
    if (tmpPath.length() < 2) // Keep the lone slash to avoid empty output.
        return tmpPath;

    // Remove the trailing delimiter
    if (tmpPath[tmpPath.length() - 1] == '/')
        tmpPath = tmpPath.left(tmpPath.length() - 1);

    pos = tmpPath.reverseFind('/');
    if (WTF::notFound != pos)
        return tmpPath.substring(pos + 1);

    return tmpPath;
}

void InjectedBundlePage::willAddMessageToConsole(WKStringRef message)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    auto messageString = toWTFString(message);
    messageString = messageString.left(messageString.find(nullCharacter));

    size_t fileProtocolStart = messageString.find("file://"_s);
    if (fileProtocolStart != WTF::notFound) {
        StringView messageStringView { messageString };
        // FIXME: The code below does not handle additional text after url nor multiple urls. This matches DumpRenderTree implementation.
        messageString = makeString(messageStringView.left(fileProtocolStart), lastFileURLPathComponent(messageStringView.substring(fileProtocolStart)));
    }
    messageString = makeString("CONSOLE MESSAGE:"_s, addLeadingSpaceStripTrailingSpacesAddNewline(messageString));
    if (injectedBundle.dumpJSConsoleLogInStdErr())
        injectedBundle.dumpToStdErr(messageString);
    else
        injectedBundle.outputText(messageString);
}

void InjectedBundlePage::willSetStatusbarText(WKStringRef statusbarText)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpStatusCallbacks())
        return;

    injectedBundle.outputText(makeString("UI DELEGATE STATUS CALLBACK: setStatusText:"_s, statusbarText, '\n'));
}

void InjectedBundlePage::willRunJavaScriptAlert(WKStringRef message, WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    injectedBundle.outputText(makeString("ALERT:"_s, addLeadingSpaceStripTrailingSpacesAddNewline(toWTFString(message))));
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKStringRef message, WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    injectedBundle.outputText(makeString("CONFIRM:"_s, addLeadingSpaceStripTrailingSpacesAddNewline(toWTFString(message))));
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef)
{
    InjectedBundle::singleton().outputText(makeString("PROMPT: "_s, message, ", default text:"_s, addLeadingSpaceStripTrailingSpacesAddNewline(toWTFString(defaultValue))));
}

uint64_t InjectedBundlePage::didExceedDatabaseQuota(WKSecurityOriginRef origin, WKStringRef databaseName, WKStringRef databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    if (testRunner->shouldDumpDatabaseCallbacks())
        injectedBundle.outputText(makeString("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:"_s, string(origin), " database:"_s, databaseName, '\n'));

    uint64_t defaultQuota = 5 * 1024 * 1024;
    double testDefaultQuota = testRunner->databaseDefaultQuota();
    if (testDefaultQuota >= 0)
        defaultQuota = testDefaultQuota;

    unsigned long long newQuota = defaultQuota;

    double maxQuota = testRunner->databaseMaxQuota();
    if (maxQuota >= 0) {
        if (defaultQuota < expectedUsageBytes && expectedUsageBytes <= maxQuota) {
            newQuota = expectedUsageBytes;
            injectedBundle.outputText(makeString("UI DELEGATE DATABASE CALLBACK: increased quota to "_s, newQuota, '\n'));
        }
    }
    return newQuota;
}

// Editor Client Callbacks

bool InjectedBundlePage::shouldBeginEditing(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldBeginEditing(range);
}

bool InjectedBundlePage::shouldEndEditing(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldEndEditing(range);
}

bool InjectedBundlePage::shouldInsertNode(WKBundlePageRef page, WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldInsertNode(node, rangeToReplace, action);
}

bool InjectedBundlePage::shouldInsertText(WKBundlePageRef page, WKStringRef text, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldInsertText(text, rangeToReplace, action);
}

bool InjectedBundlePage::shouldDeleteRange(WKBundlePageRef page, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldDeleteRange(range);
}

bool InjectedBundlePage::shouldChangeSelectedRange(WKBundlePageRef page, WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldChangeSelectedRange(fromRange, toRange, affinity, stillSelecting);
}

bool InjectedBundlePage::shouldApplyStyle(WKBundlePageRef page, WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->shouldApplyStyle(style, range);
}

void InjectedBundlePage::didBeginEditing(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didBeginEditing(notificationName);
}

void InjectedBundlePage::didEndEditing(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didEndEditing(notificationName);
}

void InjectedBundlePage::didChange(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didChange(notificationName);
}

void InjectedBundlePage::didChangeSelection(WKBundlePageRef page, WKStringRef notificationName, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didChangeSelection(notificationName);
}

bool InjectedBundlePage::shouldBeginEditing(WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    if (testRunner->shouldDumpEditingCallbacks())
        injectedBundle.outputText(makeString("EDITING DELEGATE: shouldBeginEditingInDOMRange:"_s, string(m_page, m_world.get(), range), '\n'));
    return testRunner->shouldAllowEditing();
}

bool InjectedBundlePage::shouldEndEditing(WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    if (testRunner->shouldDumpEditingCallbacks())
        injectedBundle.outputText(makeString("EDITING DELEGATE: shouldEndEditingInDOMRange:"_s, string(m_page, m_world.get(), range), '\n'));
    return testRunner->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertNode(WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    static constexpr ASCIILiteral insertactionstring[] = {
        "WebViewInsertActionTyped"_s,
        "WebViewInsertActionPasted"_s,
        "WebViewInsertActionDropped"_s,
    };

    if (testRunner->shouldDumpEditingCallbacks()) {
        injectedBundle.outputText(makeString("EDITING DELEGATE:"_s
            " shouldInsertNode:"_s, dumpPath(m_page, m_world.get(), node),
            " replacingDOMRange:"_s, string(m_page, m_world.get(), rangeToReplace),
            " givenAction:"_s, insertactionstring[action], '\n'));
    }
    return testRunner->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertText(WKStringRef text, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    static constexpr ASCIILiteral insertactionstring[] = {
        "WebViewInsertActionTyped"_s,
        "WebViewInsertActionPasted"_s,
        "WebViewInsertActionDropped"_s,
    };

    if (testRunner->shouldDumpEditingCallbacks()) {
        injectedBundle.outputText(makeString("EDITING DELEGATE:"_s
            " shouldInsertText:"_s, text,
            " replacingDOMRange:"_s, string(m_page, m_world.get(), rangeToReplace),
            " givenAction:"_s, insertactionstring[action], '\n'));
    }
    return testRunner->shouldAllowEditing();
}

bool InjectedBundlePage::shouldDeleteRange(WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    if (testRunner->shouldDumpEditingCallbacks())
        injectedBundle.outputText(makeString("EDITING DELEGATE: shouldDeleteDOMRange:"_s, string(m_page, m_world.get(), range), '\n'));
    return testRunner->shouldAllowEditing();
}

bool InjectedBundlePage::shouldChangeSelectedRange(WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    static constexpr ASCIILiteral affinitystring[] = {
        "NSSelectionAffinityUpstream"_s,
        "NSSelectionAffinityDownstream"_s
    };

    if (testRunner->shouldDumpEditingCallbacks()) {
        injectedBundle.outputText(makeString("EDITING DELEGATE:"_s
            " shouldChangeSelectedDOMRange:"_s, string(m_page, m_world.get(), fromRange),
            " toDOMRange:"_s, string(m_page, m_world.get(), toRange),
            " affinity:"_s, affinitystring[affinity],
            " stillSelecting:"_s, stillSelecting ? "TRUE"_s : "FALSE"_s, '\n'));
    }
    return testRunner->shouldAllowEditing();
}

bool InjectedBundlePage::shouldApplyStyle(WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;

    if (testRunner->shouldDumpEditingCallbacks()) {
        injectedBundle.outputText(makeString("EDITING DELEGATE:"
            " shouldApplyStyle:"_s, styleDecToStr(style),
            " toElementsInDOMRange:"_s, string(m_page, m_world.get(), range), '\n'));
    }
    return testRunner->shouldAllowEditing();
}

void InjectedBundlePage::didBeginEditing(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpEditingCallbacks())
        return;

    injectedBundle.outputText(makeString("EDITING DELEGATE: webViewDidBeginEditing:"_s, notificationName, '\n'));
}

void InjectedBundlePage::didEndEditing(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpEditingCallbacks())
        return;

    injectedBundle.outputText(makeString("EDITING DELEGATE: webViewDidEndEditing:"_s, notificationName, '\n'));
}

void InjectedBundlePage::didChange(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpEditingCallbacks())
        return;

    injectedBundle.outputText(makeString("EDITING DELEGATE: webViewDidChange:"_s, notificationName, '\n'));
}

void InjectedBundlePage::didChangeSelection(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpEditingCallbacks())
        return;

    injectedBundle.outputText(makeString("EDITING DELEGATE: webViewDidChangeSelection:"_s, notificationName, '\n'));
}

#if ENABLE(FULLSCREEN_API)
bool InjectedBundlePage::supportsFullScreen(WKBundlePageRef pageRef, WKFullScreenKeyboardRequestType requestType)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return true;
    if (testRunner->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("supportsFullScreen() == true\n"_s);
    return true;
}

void InjectedBundlePage::enterFullScreenForElement(WKBundlePageRef pageRef, WKBundleNodeHandleRef elementRef)
{
    ASSERT(bundlePageMap().contains(pageRef));
    if (auto* injectedBundlePage = bundlePageMap().get(pageRef))
        injectedBundlePage->enterFullScreenForElement(elementRef);
}

void InjectedBundlePage::enterFullScreenForElement(WKBundleNodeHandleRef elementRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("enterFullScreenForElement()\n"_s);

    if (m_fullscreenState == EnteringFullscreen)
        return;
    m_fullscreenState = EnteringFullscreen;

    if (!testRunner->hasCustomFullScreenBehavior()) {
        WKBundlePageWillEnterFullScreen(m_page);
        if (m_fullscreenState != EnteringFullscreen)
            return;

        WKBundlePageDidEnterFullScreen(m_page);
        if (m_fullscreenState != EnteringFullscreen)
            return;

    } else
        testRunner->callEnterFullscreenForElementCallback();

    m_fullscreenState = InFullscreen;
}

void InjectedBundlePage::exitFullScreenForElement(WKBundlePageRef pageRef, WKBundleNodeHandleRef elementRef)
{
    ASSERT(bundlePageMap().contains(pageRef));
    if (auto* injectedBundlePage = bundlePageMap().get(pageRef))
        injectedBundlePage->exitFullScreenForElement(elementRef);
}

void InjectedBundlePage::exitFullScreenForElement(WKBundleNodeHandleRef elementRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("exitFullScreenForElement()\n"_s);

    if (m_fullscreenState == ExitingFullscreen)
        return;
    m_fullscreenState = ExitingFullscreen;

    if (!testRunner->hasCustomFullScreenBehavior()) {
        WKBundlePageWillExitFullScreen(m_page);
        if (m_fullscreenState != ExitingFullscreen)
            return;

        WKBundlePageDidExitFullScreen(m_page);
        if (m_fullscreenState != ExitingFullscreen)
            return;
    } else
        testRunner->callExitFullscreenForElementCallback();

    m_fullscreenState = NotInFullscreen;
}

void InjectedBundlePage::beganEnterFullScreen(WKBundlePageRef, WKRect initialRect, WKRect finalRect)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (!testRunner->shouldDumpFullScreenCallbacks())
        return;

    injectedBundle.outputText(makeString("beganEnterFullScreen() - initialRect.size: {"_s,
        initialRect.size.width, ", "_s,
        initialRect.size.height,
        "}, finalRect.size: {"_s,
        finalRect.size.width, ", "_s,
        finalRect.size.height,
        "}\n"_s));
}

void InjectedBundlePage::beganExitFullScreen(WKBundlePageRef, WKRect initialRect, WKRect finalRect)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;
    if (!testRunner->shouldDumpFullScreenCallbacks())
        return;

    injectedBundle.outputText(makeString("beganExitFullScreen() - initialRect.size: {"_s,
        initialRect.size.width, ", "_s,
        initialRect.size.height,
        "}, finalRect.size: {"_s,
        finalRect.size.width, ", "_s,
        finalRect.size.height,
        "}\n"_s));
}

void InjectedBundlePage::closeFullScreen(WKBundlePageRef pageRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner)
        return;

    if (testRunner->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("closeFullScreen()\n"_s);

    if (!testRunner->hasCustomFullScreenBehavior()) {
        WKBundlePageWillExitFullScreen(pageRef);
        WKBundlePageDidExitFullScreen(pageRef);
    }
}
#endif

String InjectedBundlePage::dumpHistory()
{
    return makeString(
        "\n============== Back Forward List ==============\n"_s,
        adoptWK(WKBundlePageDumpHistoryForTesting(m_page, toWK("/LayoutTests/").get())).get(),
        "===============================================\n"_s
    );
}

#if !PLATFORM(COCOA)
void InjectedBundlePage::platformDidStartProvisionalLoadForFrame(WKBundleFrameRef)
{
}

String InjectedBundlePage::platformResponseMimeType(WKURLResponseRef)
{
    return String();
}
#endif

static bool hasTestWaitAttribute(WKBundlePageRef page)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto frame = WKBundlePageGetMainFrame(page);
    ALLOW_DEPRECATED_DECLARATIONS_END
    return frame && hasTestWaitAttribute(WKBundleFrameGetJavaScriptContext(frame));
}

static void dumpAfterWaitAttributeIsRemoved(WKBundlePageRef page)
{
    if (hasTestWaitAttribute(page)) {
        WKRetain(page);
        // Use a 1ms interval between tries to allow lower priority run loop sources with zero delays to run.
        RunLoop::current().dispatchAfter(1_ms, [page] {
            WKBundlePageCallAfterTasksAndTimers(page, [] (void* typelessPage) {
                auto page = static_cast<WKBundlePageRef>(typelessPage);
                dumpAfterWaitAttributeIsRemoved(page);
                WKRelease(page);
            }, const_cast<OpaqueWKBundlePage*>(page));
        });
        return;
    }

    auto& bundle = InjectedBundle::singleton();
    RefPtr testRunner = bundle.testRunner();
    if (!testRunner)
        return;
    if (auto currentPage = bundle.page(); currentPage && currentPage->page() == page)
        currentPage->dump(testRunner->shouldForceRepaint());
}

void InjectedBundlePage::frameDidChangeLocation(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    RefPtr testRunner = injectedBundle.testRunner();
    if (!testRunner) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (frame != injectedBundle.topLoadingFrame())
        return;

    injectedBundle.setTopLoadingFrame(nullptr);

    if (testRunner->shouldDisplayOnLoadFinish()) {
        if (auto page = InjectedBundle::singleton().page())
            WKBundlePageForceRepaint(page->page());
    }

    if (testRunner->shouldWaitUntilDone())
        return;

    if (injectedBundle.shouldProcessWorkQueue()) {
        injectedBundle.processWorkQueue();
        return;
    }

    auto page = InjectedBundle::singleton().page();
    if (!page) {
        injectedBundle.done(testRunner->shouldForceRepaint());
        return;
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (auto frame = WKBundlePageGetMainFrame(page->page()))
        sendTestRenderedEvent(WKBundleFrameGetJavaScriptContext(frame));
    ALLOW_DEPRECATED_DECLARATIONS_END
    dumpAfterWaitAttributeIsRemoved(page->page());
}

void InjectedBundlePage::notifyDone()
{
    if (InjectedBundle::singleton().topLoadingFrame())
        return;
    forceImmediateCompletion();
}

void InjectedBundlePage::forceImmediateCompletion()
{
    RefPtr testRunner = InjectedBundle::singleton().testRunner();
    if (!testRunner)
        return;
    dump(testRunner->shouldForceRepaint());
}

} // namespace WTR
