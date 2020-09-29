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
#include "ReftestFunctions.h"
#include "StringFunctions.h"
#include "WebCoreTestSupport.h"
#include <cmath>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKArray.h>
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleBackForwardListItem.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundleFramePrivate.h>
#include <WebKit/WKBundleHitTestResult.h>
#include <WebKit/WKBundleNavigationAction.h>
#include <WebKit/WKBundleNavigationActionPrivate.h>
#include <WebKit/WKBundleNodeHandlePrivate.h>
#include <WebKit/WKBundlePagePrivate.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKURLRequest.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if USE(CF) && !PLATFORM(WIN_CAIRO) && !USE(DIRECT2D)
#include "WebArchiveDumpSupport.h"
#endif

using namespace std;

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
        return makeString(name, " > ", dumpPath(context, parentNode));
    return name;
}

static WTF::String dumpPath(WKBundlePageRef page, WKBundleScriptWorldRef world, WKBundleNodeHandleRef node)
{
    if (!node)
        return "(null)";

    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSValueRef nodeValue = WKBundleFrameGetJavaScriptWrapperForNodeForWorld(frame, node, world);
    ASSERT(JSValueIsObject(context, nodeValue));
    JSObjectRef nodeObject = (JSObjectRef)nodeValue;

    return dumpPath(context, nodeObject);
}

static WTF::String rangeToStr(WKBundlePageRef page, WKBundleScriptWorldRef world, WKBundleRangeHandleRef rangeRef)
{
    if (!rangeRef)
        return "(null)";

    auto frame = WKBundlePageGetMainFrame(page);
    auto context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    auto rangeValue = WKBundleFrameGetJavaScriptWrapperForRangeForWorld(frame, rangeRef, world);
    ASSERT(JSValueIsObject(context, rangeValue));
    auto rangeObject = (JSObjectRef)rangeValue;

    return makeString("range from ",
        numericProperty(context, rangeObject, "startOffset"),
        " of ",
        dumpPath(context, objectProperty(context, rangeObject, "startContainer")),
        " to ",
        numericProperty(context, rangeObject, "endOffset"),
        " of ",
        dumpPath(context, objectProperty(context, rangeObject, "endContainer"))
    );
}

static WKRetainPtr<WKStringRef> NavigationTypeToString(WKFrameNavigationType type)
{
    switch (type) {
    case kWKFrameNavigationTypeLinkClicked:
        return toWK("link clicked");
    case kWKFrameNavigationTypeFormSubmitted:
        return toWK("form submitted");
    case kWKFrameNavigationTypeBackForward:
        return toWK("back/forward");
    case kWKFrameNavigationTypeReload:
        return toWK("reload");
    case kWKFrameNavigationTypeFormResubmitted:
        return toWK("form resubmitted");
    case kWKFrameNavigationTypeOther:
        return toWK("other");
    }
    return toWK("illegal value");
}

static WTF::String styleDecToStr(WKBundleCSSStyleDeclarationRef style)
{
    // DumpRenderTree calls -[DOMCSSStyleDeclaration description], which just dumps class name and object address.
    // No existing tests actually hit this code path at the time of this writing, because WebCore doesn't call
    // the editing client if the styling operation source is CommandFromDOM or CommandFromDOMWithUserInterface.
    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("<DOMCSSStyleDeclaration ADDRESS>");
    return stringBuilder.toString();
}

static WTF::String string(WKSecurityOriginRef origin)
{
    return makeString('{', toWTFString(adoptWK(WKSecurityOriginCopyProtocol(origin))), ", ", toWTFString(adoptWK(WKSecurityOriginCopyHost(origin))), ", ", WKSecurityOriginGetPort(origin), '}');
}

static WTF::String frameToStr(WKBundleFrameRef frame)
{
    auto name = adoptWK(WKBundleFrameCopyName(frame));
    StringBuilder stringBuilder;
    if (WKBundleFrameIsMainFrame(frame)) {
        if (!WKStringIsEmpty(name.get())) {
            stringBuilder.appendLiteral("main frame \"");
            stringBuilder.append(toWTFString(name));
            stringBuilder.append('"');
        } else
            stringBuilder.appendLiteral("main frame");
    } else {
        if (!WKStringIsEmpty(name.get())) {
            stringBuilder.appendLiteral("frame \"");
            stringBuilder.append(toWTFString(name));
            stringBuilder.append('"');
        }
        else
            stringBuilder.appendLiteral("frame (anonymous)");
    }
    
    return stringBuilder.toString();
}

static inline bool isLocalFileScheme(WKStringRef scheme)
{
    return WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "file");
}

static const char divider = '/';

static inline WTF::String pathSuitableForTestResult(WKURLRef fileUrl)
{
    if (!fileUrl)
        return "(null)";

    auto schemeString = adoptWK(WKURLCopyScheme(fileUrl));
    if (!isLocalFileScheme(schemeString.get()))
        return toWTFString(adoptWK(WKURLCopyString(fileUrl)));

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    auto mainFrameURL = adoptWK(WKBundleFrameCopyURL(mainFrame));
    if (!mainFrameURL)
        mainFrameURL = adoptWK(WKBundleFrameCopyProvisionalURL(mainFrame));

    String pathString = toWTFString(adoptWK(WKURLCopyPath(fileUrl)));
    String mainFrameURLPathString = toWTFString(adoptWK(WKURLCopyPath(mainFrameURL.get())));
    String basePath = mainFrameURLPathString.substring(0, mainFrameURLPathString.reverseFind(divider) + 1);
    
    if (!basePath.isEmpty() && pathString.startsWith(basePath))
        return pathString.substring(basePath.length());
    return toWTFString(adoptWK(WKURLCopyLastPathComponent(fileUrl))); // We lose some information here, but it's better than exposing a full path, which is always machine specific.
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
        stringBuilder.appendLiteral("<unknown>");
}

InjectedBundlePage::InjectedBundlePage(WKBundlePageRef page)
    : m_page(page)
    , m_world(adoptWK(WKBundleScriptWorldCreateWorld()))
{
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
        didDetectXSSForFrame,
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

    WKBundlePagePolicyClientV0 policyClient = {
        { 0, this },
        decidePolicyForNavigationAction,
        decidePolicyForNewWindowAction,
        decidePolicyForResponse,
        unableToImplementPolicy
    };
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBundlePageSetPolicyClient(m_page, &policyClient.base);
ALLOW_DEPRECATED_DECLARATIONS_END

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
        didReachApplicationCacheOriginQuota,
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
}

void InjectedBundlePage::stopLoading()
{
    WKBundlePageStopLoading(m_page);
}

void InjectedBundlePage::prepare()
{
    WKBundlePageClearMainFrameName(m_page);

    WKBundlePageSetPageZoomFactor(m_page, 1);
    WKBundlePageSetTextZoomFactor(m_page, 1);

    WKPoint origin = { 0, 0 };
    WKBundlePageSetScaleAtOrigin(m_page, 1, origin);
    
    WKBundleClearHistoryForTesting(m_page);

    WKBundleFrameClearOpener(WKBundlePageGetMainFrame(m_page));
    
    WKBundlePageSetTracksRepaints(m_page, false);
    
    // Force consistent "responsive" behavior for WebPage::eventThrottlingDelay() for testing. Tests can override via internals.
    WKEventThrottlingBehavior behavior = kWKEventThrottlingBehaviorResponsive;
    WKBundlePageSetEventThrottlingBehaviorOverride(m_page, &behavior);
}

void InjectedBundlePage::resetAfterTest()
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);

    // WebKit currently doesn't reset focus even when navigating to a new page. This may or may not be a bug
    // (see <https://bugs.webkit.org/show_bug.cgi?id=138334>), however for tests, we want to start each one with a clean state.
    WKBundleFrameFocus(frame);

    WebCoreTestSupport::resetInternalsObject(WKBundleFrameGetJavaScriptContext(frame));
    assignedUrlsCache().clear();

    // User scripts need to be removed after the test and before loading about:blank, as otherwise they would run in about:blank, and potentially leak results into a subsequest test.
    WKBundlePageRemoveAllUserContent(m_page);

    uninstallFakeHelvetica();

    InjectedBundle::singleton().resetUserScriptInjectedCount();
}

// Loader Client Callbacks

// String output must be identical to -[WebFrame _drt_descriptionSuitableForTestResult].
static void dumpFrameDescriptionSuitableForTestResult(WKBundleFrameRef frame, StringBuilder& stringBuilder)
{
    auto name = adoptWK(WKBundleFrameCopyName(frame));
    if (WKBundleFrameIsMainFrame(frame)) {
        if (WKStringIsEmpty(name.get())) {
            stringBuilder.appendLiteral("main frame");
            return;
        }

        stringBuilder.appendLiteral("main frame \"");
        stringBuilder.append(toWTFString(name));
        stringBuilder.append('"');
        return;
    }

    if (WKStringIsEmpty(name.get())) {
        stringBuilder.appendLiteral("frame (anonymous)");
        return;
    }

    stringBuilder.appendLiteral("frame \"");
    stringBuilder.append(toWTFString(name));
    stringBuilder.append('"');
}

static void dumpLoadEvent(WKBundleFrameRef frame, const char* eventName)
{
    StringBuilder stringBuilder;
    dumpFrameDescriptionSuitableForTestResult(frame, stringBuilder);
    stringBuilder.appendLiteral(" - ");
    stringBuilder.append(eventName);
    stringBuilder.append('\n');
    InjectedBundle::singleton().outputText(stringBuilder.toString());
}

static inline void dumpRequestDescriptionSuitableForTestResult(WKURLRequestRef request, StringBuilder& stringBuilder)
{
    auto url = adoptWK(WKURLRequestCopyURL(request));
    auto firstParty = adoptWK(WKURLRequestCopyFirstPartyForCookies(request));
    auto httpMethod = adoptWK(WKURLRequestCopyHTTPMethod(request));

    stringBuilder.append("<NSURLRequest URL ", pathSuitableForTestResult(url.get()), ", main document URL ", pathSuitableForTestResult(firstParty.get()), ", http method ");
    if (WKStringIsEmpty(httpMethod.get()))
        stringBuilder.appendLiteral("(none)");
    else
        stringBuilder.append(toWTFString(httpMethod));
    stringBuilder.append('>');
}

static inline void dumpResponseDescriptionSuitableForTestResult(WKURLResponseRef response, StringBuilder& stringBuilder, bool shouldDumpResponseHeaders = false)
{
    auto url = adoptWK(WKURLResponseCopyURL(response));
    if (!url) {
        stringBuilder.appendLiteral("(null)");
        return;
    }
    stringBuilder.appendLiteral("<NSURLResponse ");
    stringBuilder.append(pathSuitableForTestResult(url.get()));
    stringBuilder.appendLiteral(", http status code ");
    stringBuilder.appendNumber(WKURLResponseHTTPStatusCode(response));

    if (shouldDumpResponseHeaders) {
        stringBuilder.appendLiteral(", ");
        stringBuilder.appendNumber(InjectedBundlePage::responseHeaderCount(response));
        stringBuilder.appendLiteral(" headers");
    }
    stringBuilder.append('>');
}

#if !PLATFORM(COCOA)
// FIXME: Implement this for non cocoa ports.
//        [GTK][WPE] https://bugs.webkit.org/show_bug.cgi?id=184295
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
    if (errorDomain == "WebKitNetworkError") {
        errorDomain = "NSURLErrorDomain";
        errorCode = -999;
    }
    if (errorDomain ==  "WebKitPolicyError")
        errorDomain = "WebKitErrorDomain";

    stringBuilder.append("<NSError domain ", errorDomain, ", code ", errorCode);
    if (auto url = adoptWK(WKErrorCopyFailingURL(error)))
        stringBuilder.append(", failing URL \"", toWTFString(adoptWK(WKURLCopyString(url.get()))), '"');
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

void InjectedBundlePage::didDetectXSSForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didDetectXSSForFrame(frame);
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
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->testURL()) {
        auto testURL = adoptWK(WKBundleFrameCopyProvisionalURL(frame));
        injectedBundle.testRunner()->setTestURL(testURL.get());
    }

    platformDidStartProvisionalLoadForFrame(frame);

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didStartProvisionalLoadForFrame");

    if (!injectedBundle.topLoadingFrame())
        injectedBundle.setTopLoadingFrame(frame);

    if (injectedBundle.testRunner()->shouldStopProvisionalFrameLoads())
        dumpLoadEvent(frame, "stopping load in didStartProvisionalLoadForFrame callback");
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    dumpLoadEvent(frame, "didReceiveServerRedirectForProvisionalLoadForFrame");
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef frame, WKErrorRef error)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpLoadEvent(frame, "didFailProvisionalLoadWithError");
        auto code = WKErrorGetErrorCode(error);
        if (code == kWKErrorCodeCannotShowURL)
            dumpLoadEvent(frame, "(ErrorCodeCannotShowURL)");
        else if (code == kWKErrorCodeFrameLoadBlockedByContentBlocker)
            dumpLoadEvent(frame, "(kWKErrorCodeFrameLoadBlockedByContentBlocker)");
    }

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    dumpLoadEvent(frame, "didCommitLoadForFrame");
}

void InjectedBundlePage::didFinishProgress()
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpProgressFinishedCallback())
        return;

    injectedBundle.outputText("postProgressFinishedNotification\n");
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
    if (fabs(x) <= 0.00000001 && fabs(y) <= 0.00000001)
        return;

    if (shouldIncludeFrameName) {
        auto name = adoptWK(WKBundleFrameCopyName(frame));
        stringBuilder.appendLiteral("frame '");
        stringBuilder.append(toWTFString(name));
        stringBuilder.appendLiteral("' ");
    }
    stringBuilder.appendLiteral("scrolled to ");
    stringBuilder.appendNumber(x);
    stringBuilder.append(',');
    stringBuilder.appendNumber(y);
    stringBuilder.append('\n');
}

static void dumpDescendantFrameScrollPositions(WKBundleFrameRef frame, StringBuilder& stringBuilder)
{
    auto childFrames = adoptWK(WKBundleFrameCopyChildFrames(frame));
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        dumpFrameScrollPosition(subframe, stringBuilder, ShouldIncludeFrameName);
        dumpDescendantFrameScrollPositions(subframe, stringBuilder);
    }
}

void InjectedBundlePage::dumpAllFrameScrollPositions(StringBuilder& stringBuilder)
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    dumpFrameScrollPosition(frame, stringBuilder);
    dumpDescendantFrameScrollPositions(frame, stringBuilder);
}

static bool hasDocumentElement(WKBundleFrameRef frame)
{
    auto context = WKBundleFrameGetJavaScriptContext(frame);
    return objectProperty(context, JSContextGetGlobalObject(context), { "document", "documentElement" });
}

static void dumpFrameText(WKBundleFrameRef frame, StringBuilder& builder)
{
    // If the frame doesn't have a document element, its inner text will be an empty string, so
    // we'll end up just appending a single newline below. Since DumpRenderTree didn't append
    // anything in this case, we decided to preserve that behavior.
    if (!hasDocumentElement(frame))
        return;

    // To keep things tidy, strip all trailing spaces: they are not a meaningful part of dumpAsText test output.
    // Breaking the string up into lines lets us efficiently strip and has a side effect of adding a newline after the last line.
    auto text = toWTFString(adoptWK(WKBundleFrameCopyInnerText(frame)));
    for (auto line : StringView(text).splitAllowingEmptyEntries('\n')) {
        while (line.endsWith(' '))
            line = line.substring(0, line.length() - 1);
        builder.append(line, '\n');
    }
}

static void dumpDescendantFramesText(WKBundleFrameRef frame, StringBuilder& stringBuilder)
{
    auto childFrames = adoptWK(WKBundleFrameCopyChildFrames(frame));
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        auto subframeName = adoptWK(WKBundleFrameCopyName(subframe));

        // DumpRenderTree ignores empty frames, so do the same thing here.
        if (!hasDocumentElement(subframe))
            continue;

        stringBuilder.appendLiteral("\n--------\nFrame: '");
        stringBuilder.append(toWTFString(subframeName));
        stringBuilder.appendLiteral("'\n--------\n");

        dumpFrameText(subframe, stringBuilder);
        dumpDescendantFramesText(subframe, stringBuilder);
    }
}

void InjectedBundlePage::dumpAllFramesText(StringBuilder& stringBuilder)
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    dumpFrameText(frame, stringBuilder);
    dumpDescendantFramesText(frame, stringBuilder);
}


void InjectedBundlePage::dumpDOMAsWebArchive(WKBundleFrameRef frame, StringBuilder& stringBuilder)
{
#if USE(CF) && !PLATFORM(WIN_CAIRO) && !USE(DIRECT2D)
    auto wkData = adoptWK(WKBundleFrameCopyWebArchive(frame));
    auto cfData = adoptCF(CFDataCreate(0, WKDataGetBytes(wkData.get()), WKDataGetSize(wkData.get())));
    auto cfString = adoptCF(WebCoreTestSupport::createXMLStringFromWebArchiveData(cfData.get()));
    stringBuilder.append(cfString.get());
#endif
}

void InjectedBundlePage::dump()
{
    auto& injectedBundle = InjectedBundle::singleton();
    ASSERT(injectedBundle.isTestRunning());

    // Force a paint before dumping. This matches DumpRenderTree on Windows. (DumpRenderTree on Mac
    // does this at a slightly different time.) See <http://webkit.org/b/55469> for details.
    WKBundlePageForceRepaint(m_page);
    WKBundlePageFlushPendingEditorStateUpdate(m_page);

    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    auto urlRef = adoptWK(WKBundleFrameCopyURL(frame));
    String url = toWTFString(adoptWK(WKURLCopyString(urlRef.get())));
    auto mimeType = adoptWK(WKBundleFrameCopyMIMETypeForResourceWithURL(frame, urlRef.get()));
    if (url.find("dumpAsText/") != notFound || WKStringIsEqualToUTF8CString(mimeType.get(), "text/plain"))
        injectedBundle.testRunner()->dumpAsText(false);

    StringBuilder stringBuilder;

    switch (injectedBundle.testRunner()->whatToDump()) {
    case WhatToDump::RenderTree: {
        if (injectedBundle.testRunner()->isPrinting())
            stringBuilder.append(toWTFString(adoptWK(WKBundlePageCopyRenderTreeExternalRepresentationForPrinting(m_page)).get()));
        else
            stringBuilder.append(toWTFString(adoptWK(WKBundlePageCopyRenderTreeExternalRepresentation(m_page, injectedBundle.testRunner()->renderTreeDumpOptions())).get()));
        break;
    }
    case WhatToDump::MainFrameText:
        dumpFrameText(WKBundlePageGetMainFrame(m_page), stringBuilder);
        break;
    case WhatToDump::AllFramesText:
        dumpAllFramesText(stringBuilder);
        break;
    case WhatToDump::Audio:
        break;
    case WhatToDump::DOMAsWebArchive:
        dumpDOMAsWebArchive(frame, stringBuilder);
        break;
    }

    if (injectedBundle.testRunner()->shouldDumpAllFrameScrollPositions())
        dumpAllFrameScrollPositions(stringBuilder);
    else if (injectedBundle.testRunner()->shouldDumpMainFrameScrollPosition())
        dumpFrameScrollPosition(WKBundlePageGetMainFrame(m_page), stringBuilder);

    if (injectedBundle.testRunner()->shouldDumpBackForwardListsForAllWindows())
        injectedBundle.dumpBackForwardListsForAllPages(stringBuilder);

    if (injectedBundle.shouldDumpPixels() && injectedBundle.testRunner()->shouldDumpPixels()) {
        bool shouldCreateSnapshot = injectedBundle.testRunner()->isPrinting();
        if (shouldCreateSnapshot) {
            WKSnapshotOptions options = kWKSnapshotOptionsShareable;
            WKRect snapshotRect = WKBundleFrameGetVisibleContentBounds(WKBundlePageGetMainFrame(m_page));

            if (injectedBundle.testRunner()->isPrinting())
                options |= kWKSnapshotOptionsPrinting;
            else {
                options |= kWKSnapshotOptionsInViewCoordinates;
                if (injectedBundle.testRunner()->shouldDumpSelectionRect())
                    options |= kWKSnapshotOptionsPaintSelectionRectangle;
            }

            injectedBundle.setPixelResult(adoptWK(WKBundlePageCreateSnapshotWithOptions(m_page, snapshotRect, options)).get());
        } else
            injectedBundle.setPixelResultIsPending(true);

        if (WKBundlePageIsTrackingRepaints(m_page) && !injectedBundle.testRunner()->isPrinting())
            injectedBundle.setRepaintRects(adoptWK(WKBundlePageCopyTrackedRepaintRects(m_page)).get());
    }

    injectedBundle.outputText(stringBuilder.toString());
    injectedBundle.done();
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didFinishLoadForFrame");

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundleFrameRef frame, WKErrorRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didFailLoadWithError");

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    StringBuilder stringBuilder;
    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame, stringBuilder);
        stringBuilder.appendLiteral(" - didReceiveTitle: ");
        stringBuilder.append(toWTFString(title));
        stringBuilder.append('\n');
    }

    if (injectedBundle.testRunner()->shouldDumpTitleChanges()) {
        stringBuilder.appendLiteral("TITLE CHANGED: '");
        stringBuilder.append(toWTFString(title));
        stringBuilder.appendLiteral("'\n");
    }

    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didClearWindowForFrame(WKBundleFrameRef frame, WKBundleScriptWorldRef world)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    auto context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);

    if (WKBundleScriptWorldNormalWorld() != world) {
        setGlobalObjectProperty(context, "__worldID", TestRunner::worldIDForWorld(world));
        return;
    }

    injectedBundle.testRunner()->makeWindowObject(context);
    injectedBundle.gcController()->makeWindowObject(context);
    injectedBundle.eventSendingController()->makeWindowObject(context);
    injectedBundle.textInputController()->makeWindowObject(context);
#if HAVE(ACCESSIBILITY)
    injectedBundle.accessibilityController()->makeWindowObject(context);
#endif

    WebCoreTestSupport::injectInternalsObject(context);
}

void InjectedBundlePage::didCancelClientRedirectForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didCancelClientRedirectForFrame");

    injectedBundle.testRunner()->setDidCancelClientRedirect(true);
}

void InjectedBundlePage::willPerformClientRedirectForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKURLRef url, double delay, double date)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    StringBuilder stringBuilder;
    dumpFrameDescriptionSuitableForTestResult(frame, stringBuilder);
    stringBuilder.append(" - willPerformClientRedirectToURL: ", pathSuitableForTestResult(url), '\n');
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didSameDocumentNavigationForFrame(WKBundleFrameRef frame, WKSameDocumentNavigationType type)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    if (type != kWKSameDocumentNavigationAnchorNavigation)
        return;

    dumpLoadEvent(frame, "didChangeLocationWithinPageForFrame");
}

void InjectedBundlePage::didFinishDocumentLoadForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didFinishDocumentLoadForFrame");

    unsigned pendingFrameUnloadEvents = WKBundleFrameGetPendingUnloadCount(frame);
    if (pendingFrameUnloadEvents) {
        StringBuilder stringBuilder;
        stringBuilder.append(frameToStr(frame));
        stringBuilder.appendLiteral(" - has ");
        stringBuilder.appendNumber(pendingFrameUnloadEvents);
        stringBuilder.appendLiteral(" onunload handler(s)\n");
        injectedBundle.outputText(stringBuilder.toString());
    }
}

void InjectedBundlePage::didHandleOnloadEventsForFrame(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        dumpLoadEvent(frame, "didHandleOnloadEventsForFrame");
}

void InjectedBundlePage::didDisplayInsecureContentForFrame(WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        injectedBundle.outputText("didDisplayInsecureContent\n");
}

void InjectedBundlePage::didRunInsecureContentForFrame(WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        injectedBundle.outputText("didRunInsecureContent\n");
}

void InjectedBundlePage::didDetectXSSForFrame(WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFrameLoadCallbacks())
        injectedBundle.outputText("didDetectXSS\n");
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
    if (injectedBundle.isTestRunning()
        && injectedBundle.testRunner()->shouldDumpResourceLoadCallbacks()) {
        StringBuilder stringBuilder;
        dumpResourceURL(identifier, stringBuilder);
        stringBuilder.appendLiteral(" - willSendRequest ");
        dumpRequestDescriptionSuitableForTestResult(request, stringBuilder);
        stringBuilder.appendLiteral(" redirectResponse ");
        dumpResponseDescriptionSuitableForTestResult(response, stringBuilder, injectedBundle.testRunner()->shouldDumpAllHTTPRedirectedResponseHeaders());
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }

    if (injectedBundle.isTestRunning() && injectedBundle.testRunner()->willSendRequestReturnsNull())
        return nullptr;

    auto redirectURL = adoptWK(WKURLResponseCopyURL(response));
    if (injectedBundle.isTestRunning() && injectedBundle.testRunner()->willSendRequestReturnsNullOnRedirect() && redirectURL) {
        injectedBundle.outputText("Returning null for this redirect\n");
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
            WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(m_page);
            auto mainFrameURL = adoptWK(WKBundleFrameCopyURL(mainFrame));
            if (!mainFrameURL || WKStringIsEqualToUTF8CString(adoptWK(WKURLCopyString(mainFrameURL.get())).get(), "about:blank"))
                mainFrameURL = adoptWK(WKBundleFrameCopyProvisionalURL(mainFrame));

            auto mainFrameHost = adoptWK(WKURLCopyHostName(mainFrameURL.get()));
            auto mainFrameScheme = adoptWK(WKURLCopyScheme(mainFrameURL.get()));
            mainFrameIsExternal = isHTTPOrHTTPSScheme(mainFrameScheme.get()) && !isLocalHost(mainFrameHost.get());
        }
        if (!mainFrameIsExternal && !isAllowedHost(host.get())) {
            StringBuilder stringBuilder;
            stringBuilder.appendLiteral("Blocked access to external URL ");
            stringBuilder.append(toWTFString(urlString));
            stringBuilder.append('\n');
            injectedBundle.outputText(stringBuilder.toString());
            return nullptr;
        }
    }
    
    if (injectedBundle.isTestRunning()) {
        String body = injectedBundle.testRunner()->willSendRequestHTTPBody();
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
    if (!injectedBundle.isTestRunning())
        return;

    if (injectedBundle.testRunner()->shouldDumpResourceLoadCallbacks()) {
        StringBuilder stringBuilder;
        dumpResourceURL(identifier, stringBuilder);
        stringBuilder.appendLiteral(" - didReceiveResponse ");
        dumpResponseDescriptionSuitableForTestResult(response, stringBuilder);
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }


    if (!injectedBundle.testRunner()->shouldDumpResourceResponseMIMETypes())
        return;

    auto url = adoptWK(WKURLResponseCopyURL(response));
    auto urlString = adoptWK(WKURLCopyLastPathComponent(url.get()));
    auto mimeTypeString = adoptWK(WKURLResponseCopyMIMEType(response));

    StringBuilder stringBuilder;
    stringBuilder.append(toWTFString(urlString));
    stringBuilder.appendLiteral(" has MIME type ");
    stringBuilder.append(toWTFString(mimeTypeString));

    String platformMimeType = platformResponseMimeType(response);
    if (!platformMimeType.isEmpty() && platformMimeType != toWTFString(mimeTypeString)) {
        stringBuilder.appendLiteral(" but platform response has ");
        stringBuilder.append(platformMimeType);
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
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpResourceLoadCallbacks())
        return;

    StringBuilder stringBuilder;
    dumpResourceURL(identifier, stringBuilder);
    stringBuilder.appendLiteral(" - didFinishLoading\n");
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didFailLoadForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier, WKErrorRef error)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpResourceLoadCallbacks())
        return;

    StringBuilder stringBuilder;
    dumpResourceURL(identifier, stringBuilder);
    stringBuilder.appendLiteral(" - didFailLoadingWithError: ");

    dumpErrorDescriptionSuitableForTestResult(error, stringBuilder);
    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());
}

bool InjectedBundlePage::shouldCacheResponse(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    if (!injectedBundle.testRunner()->shouldDumpWillCacheResponse())
        return true;

    StringBuilder stringBuilder;
    stringBuilder.appendNumber(identifier);
    stringBuilder.appendLiteral(" - willCacheResponse: called\n");
    injectedBundle.outputText(stringBuilder.toString());

    // The default behavior is the cache the response.
    return true;
}


// Policy Client Callbacks

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForNavigationAction(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleNavigationActionRef navigationAction, WKURLRequestRef request, WKTypeRef* userData, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->decidePolicyForNavigationAction(page, frame, navigationAction, request, userData);
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForNewWindowAction(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleNavigationActionRef navigationAction, WKURLRequestRef request, WKStringRef frameName, WKTypeRef* userData, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->decidePolicyForNewWindowAction(page, frame, navigationAction, request, frameName, userData);
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForResponse(WKBundlePageRef page, WKBundleFrameRef frame, WKURLResponseRef response, WKURLRequestRef request, WKTypeRef* userData, const void* clientInfo)
{
    return static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->decidePolicyForResponse(page, frame, response, request, userData);
}

void InjectedBundlePage::unableToImplementPolicy(WKBundlePageRef page, WKBundleFrameRef frame, WKErrorRef error, WKTypeRef* userData, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->unableToImplementPolicy(page, frame, error, userData);
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForNavigationAction(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleNavigationActionRef navigationAction, WKURLRequestRef request, WKTypeRef* userData)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return WKBundlePagePolicyActionUse;

    if (injectedBundle.testRunner()->shouldDumpPolicyCallbacks()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral(" - decidePolicyForNavigationAction\n");
        dumpRequestDescriptionSuitableForTestResult(request, stringBuilder);
        stringBuilder.append(" is main frame - ", WKBundleFrameIsMainFrame(frame) ? "yes" : "no");
        stringBuilder.append(" should open URLs externally - ", WKBundleNavigationActionGetShouldOpenExternalURLs(navigationAction) ? "yes" : "no");
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }

    if (!injectedBundle.testRunner()->isPolicyDelegateEnabled())
        return WKBundlePagePolicyActionPassThrough;

    auto url = adoptWK(WKURLRequestCopyURL(request));
    auto urlScheme = adoptWK(WKURLCopyScheme(url.get()));

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("Policy delegate: attempt to load ");
    if (isLocalFileScheme(urlScheme.get()))
        stringBuilder.append(toWTFString(adoptWK(WKURLCopyLastPathComponent(url.get()))));
    else
        stringBuilder.append(toWTFString(adoptWK(WKURLCopyString(url.get()))));
    stringBuilder.appendLiteral(" with navigation type \'");
    stringBuilder.append(toWTFString(NavigationTypeToString(WKBundleNavigationActionGetNavigationType(navigationAction))));
    stringBuilder.appendLiteral("\'");
    auto hitTestResultRef = adoptWK(WKBundleNavigationActionCopyHitTestResult(navigationAction));
    if (hitTestResultRef) {
        auto nodeHandleRef = adoptWK(WKBundleHitTestResultCopyNodeHandle(hitTestResultRef.get()));
        stringBuilder.appendLiteral(" originating from ");
        stringBuilder.append(dumpPath(m_page, m_world.get(), nodeHandleRef.get()));
    }

    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());

    injectedBundle.testRunner()->notifyDone();

    return WKBundlePagePolicyActionPassThrough;
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForNewWindowAction(WKBundlePageRef, WKBundleFrameRef, WKBundleNavigationActionRef, WKURLRequestRef, WKStringRef, WKTypeRef*)
{
    return WKBundlePagePolicyActionPassThrough;
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForResponse(WKBundlePageRef page, WKBundleFrameRef, WKURLResponseRef response, WKURLRequestRef, WKTypeRef*)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner() && injectedBundle.testRunner()->isPolicyDelegateEnabled() && WKURLResponseIsAttachment(response)) {
        InjectedBundle::singleton().outputText(makeString("Policy delegate: resource is an attachment, suggested file name \'", toWTFString(adoptWK(WKURLResponseCopySuggestedFilename(response))), "\'\n"));
    }

    return WKBundlePagePolicyActionPassThrough;
}

void InjectedBundlePage::unableToImplementPolicy(WKBundlePageRef, WKBundleFrameRef, WKErrorRef, WKTypeRef*)
{
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

void InjectedBundlePage::didReachApplicationCacheOriginQuota(WKBundlePageRef page, WKSecurityOriginRef origin, int64_t totalBytesNeeded, const void* clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->didReachApplicationCacheOriginQuota(origin, totalBytesNeeded);
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
            line = line.substring(0, line.length() - 1);
        builder.append(line, '\n');
    }
    return builder.toString();
}

static WTF::String addLeadingSpaceStripTrailingSpacesAddNewline(const WTF::String& string)
{
    auto result = stripTrailingSpacesAddNewline(string);
    return (result.isEmpty() || result.startsWith('\n')) ? result : makeString(' ', result);
}

static WTF::String lastFileURLPathComponent(const WTF::String& path)
{
    auto pos = path.find("file://");
    ASSERT(WTF::notFound != pos);

    auto tmpPath = path.substring(pos + 7);
    if (tmpPath.length() < 2) // Keep the lone slash to avoid empty output.
        return tmpPath;

    // Remove the trailing delimiter
    if (tmpPath[tmpPath.length() - 1] == '/')
        tmpPath.remove(tmpPath.length() - 1);

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

    WTF::String messageString = toWTFString(message);
    size_t nullCharPos = messageString.find(UChar(0));
    if (nullCharPos != WTF::notFound)
        messageString.truncate(nullCharPos);

    size_t fileProtocolStart = messageString.find("file://");
    if (fileProtocolStart != WTF::notFound)
        // FIXME: The code below does not handle additional text after url nor multiple urls. This matches DumpRenderTree implementation.
        messageString = messageString.substring(0, fileProtocolStart) + lastFileURLPathComponent(messageString.substring(fileProtocolStart));

    messageString = makeString("CONSOLE MESSAGE:", addLeadingSpaceStripTrailingSpacesAddNewline(messageString));
    if (injectedBundle.dumpJSConsoleLogInStdErr())
        injectedBundle.dumpToStdErr(messageString);
    else
        injectedBundle.outputText(messageString);
}

void InjectedBundlePage::willSetStatusbarText(WKStringRef statusbarText)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    if (!injectedBundle.testRunner()->shouldDumpStatusCallbacks())
        return;

    injectedBundle.outputText(makeString("UI DELEGATE STATUS CALLBACK: setStatusText:", toWTFString(statusbarText), '\n'));
}

void InjectedBundlePage::willRunJavaScriptAlert(WKStringRef message, WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    injectedBundle.outputText(makeString("ALERT:", addLeadingSpaceStripTrailingSpacesAddNewline(toWTFString(message))));
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKStringRef message, WKBundleFrameRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;

    injectedBundle.outputText(makeString("CONFIRM:", addLeadingSpaceStripTrailingSpacesAddNewline(toWTFString(message))));
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef)
{
    InjectedBundle::singleton().outputText(makeString("PROMPT: ", toWTFString(message), ", default text:", addLeadingSpaceStripTrailingSpacesAddNewline(toWTFString(defaultValue))));
}

void InjectedBundlePage::didReachApplicationCacheOriginQuota(WKSecurityOriginRef origin, int64_t totalBytesNeeded)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpApplicationCacheDelegateCallbacks()) {
        // For example, numbers from 30000 - 39999 will output as 30000.
        // Rounding up or down does not really matter for these tests. It's
        // sufficient to just get a range of 10000 to determine if we were
        // above or below a threshold.
        auto truncatedSpaceNeeded = (totalBytesNeeded / 10000) * 10000;
        injectedBundle.outputText(makeString("UI DELEGATE APPLICATION CACHE CALLBACK: exceededApplicationCacheOriginQuotaForSecurityOrigin:", string(origin), " totalSpaceNeeded:~", truncatedSpaceNeeded, '\n'));
    }

    if (injectedBundle.testRunner()->shouldDisallowIncreaseForApplicationCacheQuota())
        return;

    // Reset default application cache quota.
    WKBundlePageResetApplicationCacheOriginQuota(injectedBundle.page()->page(), adoptWK(WKSecurityOriginCopyToString(origin)).get());
}

uint64_t InjectedBundlePage::didExceedDatabaseQuota(WKSecurityOriginRef origin, WKStringRef databaseName, WKStringRef databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpDatabaseCallbacks())
        injectedBundle.outputText(makeString("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:", string(origin), " database:", toWTFString(databaseName), '\n'));

    uint64_t defaultQuota = 5 * 1024 * 1024;
    double testDefaultQuota = injectedBundle.testRunner()->databaseDefaultQuota();
    if (testDefaultQuota >= 0)
        defaultQuota = testDefaultQuota;

    unsigned long long newQuota = defaultQuota;

    double maxQuota = injectedBundle.testRunner()->databaseMaxQuota();
    if (maxQuota >= 0) {
        if (defaultQuota < expectedUsageBytes && expectedUsageBytes <= maxQuota) {
            newQuota = expectedUsageBytes;
            injectedBundle.outputText(makeString("UI DELEGATE DATABASE CALLBACK: increased quota to ", newQuota, '\n'));
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
    if (!injectedBundle.isTestRunning())
        return true;

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks())
        injectedBundle.outputText(makeString("EDITING DELEGATE: shouldBeginEditingInDOMRange:", rangeToStr(m_page, m_world.get(), range), '\n'));
    return injectedBundle.testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldEndEditing(WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks())
        injectedBundle.outputText(makeString("EDITING DELEGATE: shouldEndEditingInDOMRange:", rangeToStr(m_page, m_world.get(), range), '\n'));
    return injectedBundle.testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertNode(WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    static const char* insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("EDITING DELEGATE: shouldInsertNode:");
        stringBuilder.append(dumpPath(m_page, m_world.get(), node));
        stringBuilder.appendLiteral(" replacingDOMRange:");
        stringBuilder.append(rangeToStr(m_page, m_world.get(), rangeToReplace));
        stringBuilder.appendLiteral(" givenAction:");
        stringBuilder.append(insertactionstring[action]);
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }
    return injectedBundle.testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertText(WKStringRef text, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    static const char *insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("EDITING DELEGATE: shouldInsertText:");
        stringBuilder.append(toWTFString(text));
        stringBuilder.appendLiteral(" replacingDOMRange:");
        stringBuilder.append(rangeToStr(m_page, m_world.get(), rangeToReplace));
        stringBuilder.appendLiteral(" givenAction:");
        stringBuilder.append(insertactionstring[action]);
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }
    return injectedBundle.testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldDeleteRange(WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("EDITING DELEGATE: shouldDeleteDOMRange:");
        stringBuilder.append(rangeToStr(m_page, m_world.get(), range));
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }
    return injectedBundle.testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldChangeSelectedRange(WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    static const char *affinitystring[] = {
        "NSSelectionAffinityUpstream",
        "NSSelectionAffinityDownstream"
    };
    static const char *boolstring[] = {
        "FALSE",
        "TRUE"
    };

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("EDITING DELEGATE: shouldChangeSelectedDOMRange:");
        stringBuilder.append(rangeToStr(m_page, m_world.get(), fromRange));
        stringBuilder.appendLiteral(" toDOMRange:");
        stringBuilder.append(rangeToStr(m_page, m_world.get(), toRange));
        stringBuilder.appendLiteral(" affinity:");
        stringBuilder.append(affinitystring[affinity]); 
        stringBuilder.appendLiteral(" stillSelecting:");
        stringBuilder.append(boolstring[stillSelecting]); 
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }
    return injectedBundle.testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldApplyStyle(WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return true;

    if (injectedBundle.testRunner()->shouldDumpEditingCallbacks()) {
        StringBuilder stringBuilder;
        stringBuilder.appendLiteral("EDITING DELEGATE: shouldApplyStyle:");
        stringBuilder.append(styleDecToStr(style));
        stringBuilder.appendLiteral(" toElementsInDOMRange:");
        stringBuilder.append(rangeToStr(m_page, m_world.get(), range));
        stringBuilder.append('\n');
        injectedBundle.outputText(stringBuilder.toString());
    }
    return injectedBundle.testRunner()->shouldAllowEditing();
}

void InjectedBundlePage::didBeginEditing(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;
    if (!injectedBundle.testRunner()->shouldDumpEditingCallbacks())
        return;

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("EDITING DELEGATE: webViewDidBeginEditing:");
    stringBuilder.append(toWTFString(notificationName));
    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didEndEditing(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;
    if (!injectedBundle.testRunner()->shouldDumpEditingCallbacks())
        return;

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("EDITING DELEGATE: webViewDidEndEditing:");
    stringBuilder.append(toWTFString(notificationName));
    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didChange(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;
    if (!injectedBundle.testRunner()->shouldDumpEditingCallbacks())
        return;

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("EDITING DELEGATE: webViewDidChange:");
    stringBuilder.append(toWTFString(notificationName));
    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());
}

void InjectedBundlePage::didChangeSelection(WKStringRef notificationName)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (!injectedBundle.isTestRunning())
        return;
    if (!injectedBundle.testRunner()->shouldDumpEditingCallbacks())
        return;

    StringBuilder stringBuilder;
    stringBuilder.appendLiteral("EDITING DELEGATE: webViewDidChangeSelection:");
    stringBuilder.append(toWTFString(notificationName));
    stringBuilder.append('\n');
    injectedBundle.outputText(stringBuilder.toString());
}

#if ENABLE(FULLSCREEN_API)
bool InjectedBundlePage::supportsFullScreen(WKBundlePageRef pageRef, WKFullScreenKeyboardRequestType requestType)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("supportsFullScreen() == true\n");
    return true;
}

void InjectedBundlePage::enterFullScreenForElement(WKBundlePageRef pageRef, WKBundleNodeHandleRef elementRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("enterFullScreenForElement()\n");

    if (!injectedBundle.testRunner()->hasCustomFullScreenBehavior()) {
        WKBundlePageWillEnterFullScreen(pageRef);
        WKBundlePageDidEnterFullScreen(pageRef);
    }
}

void InjectedBundlePage::exitFullScreenForElement(WKBundlePageRef pageRef, WKBundleNodeHandleRef elementRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("exitFullScreenForElement()\n");

    if (!injectedBundle.testRunner()->hasCustomFullScreenBehavior()) {
        WKBundlePageWillExitFullScreen(pageRef);
        WKBundlePageDidExitFullScreen(pageRef);
    }
}

void InjectedBundlePage::beganEnterFullScreen(WKBundlePageRef, WKRect, WKRect)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("beganEnterFullScreen()\n");
}

void InjectedBundlePage::beganExitFullScreen(WKBundlePageRef, WKRect, WKRect)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("beganExitFullScreen()\n");
}

void InjectedBundlePage::closeFullScreen(WKBundlePageRef pageRef)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (injectedBundle.testRunner()->shouldDumpFullScreenCallbacks())
        injectedBundle.outputText("closeFullScreen()\n");

    if (!injectedBundle.testRunner()->hasCustomFullScreenBehavior()) {
        WKBundlePageWillExitFullScreen(pageRef);
        WKBundlePageDidExitFullScreen(pageRef);
    }
}
#endif

String InjectedBundlePage::dumpHistory()
{
    return makeString(
        "\n============== Back Forward List ==============\n",
        toWTFString(adoptWK(WKBundlePageDumpHistoryForTesting(m_page, toWK("/LayoutTests/").get())).get()),
        "===============================================\n"
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

static bool hasReftestWaitAttribute(WKBundlePageRef page)
{
    auto frame = WKBundlePageGetMainFrame(page);
    return frame && hasReftestWaitAttribute(WKBundleFrameGetJavaScriptContext(frame));
}

static void dumpAfterWaitAttributeIsRemoved(WKBundlePageRef page)
{
    if (hasReftestWaitAttribute(page)) {
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

    if (auto& bundle = InjectedBundle::singleton(); bundle.isTestRunning()) {
        if (auto currentPage = bundle.page(); currentPage && currentPage->page() == page)
            currentPage->dump();
    }
}

void InjectedBundlePage::frameDidChangeLocation(WKBundleFrameRef frame)
{
    auto& injectedBundle = InjectedBundle::singleton();
    if (frame != injectedBundle.topLoadingFrame())
        return;

    injectedBundle.setTopLoadingFrame(nullptr);

    if (injectedBundle.testRunner()->shouldWaitUntilDone())
        return;

    if (injectedBundle.shouldProcessWorkQueue()) {
        injectedBundle.processWorkQueue();
        return;
    }

    auto page = InjectedBundle::singleton().page();
    if (!page) {
        injectedBundle.done();
        return;
    }

    if (auto frame = WKBundlePageGetMainFrame(page->page()))
        sendTestRenderedEvent(WKBundleFrameGetJavaScriptContext(frame));
    dumpAfterWaitAttributeIsRemoved(page->page());
}

} // namespace WTR
