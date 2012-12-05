/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#include "InjectedBundle.h"
#include "StringFunctions.h"
#include "WebCoreTestSupport.h"
#include <cmath>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit2/WKArray.h>
#include <WebKit2/WKBundle.h>
#include <WebKit2/WKBundleBackForwardList.h>
#include <WebKit2/WKBundleBackForwardListItem.h>
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundleFramePrivate.h>
#include <WebKit2/WKBundleHitTestResult.h>
#include <WebKit2/WKBundleNavigationAction.h>
#include <WebKit2/WKBundleNodeHandlePrivate.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKSecurityOrigin.h>
#include <WebKit2/WKURLRequest.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(QT)
#include "DumpRenderTreeSupportQt.h"
#endif

#if ENABLE(WEB_INTENTS)
#include <WebKit2/WKBundleIntent.h>
#include <WebKit2/WKBundleIntentRequest.h>
#endif
#if ENABLE(WEB_INTENTS_TAG)
#include <WebKit2/WKIntentServiceInfo.h>
#endif

using namespace std;

namespace WTR {

static bool hasPrefix(const WTF::String& searchString, const WTF::String& prefix)
{
    return searchString.length() >= prefix.length() && searchString.substring(0, prefix.length()) == prefix;
}

static JSValueRef propertyValue(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    if (!object)
        return 0;
    JSRetainPtr<JSStringRef> propertyNameString(Adopt, JSStringCreateWithUTF8CString(propertyName));
    return JSObjectGetProperty(context, object, propertyNameString.get(), 0);
}

static double propertyValueDouble(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    JSValueRef value = propertyValue(context, object, propertyName);
    if (!value)
        return 0;
    return JSValueToNumber(context, value, 0);    
}

static int propertyValueInt(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    return static_cast<int>(propertyValueDouble(context, object, propertyName));    
}

static double numericWindowPropertyValue(WKBundleFrameRef frame, const char* propertyName)
{
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    return propertyValueDouble(context, JSContextGetGlobalObject(context), propertyName);
}

static WTF::String dumpPath(JSGlobalContextRef context, JSObjectRef nodeValue)
{
    JSValueRef nodeNameValue = propertyValue(context, nodeValue, "nodeName");
    JSRetainPtr<JSStringRef> jsStringNodeName(Adopt, JSValueToStringCopy(context, nodeNameValue, 0));
    WKRetainPtr<WKStringRef> nodeName = toWK(jsStringNodeName);

    JSValueRef parentNode = propertyValue(context, nodeValue, "parentNode");

    WTF::StringBuilder stringBuilder;
    stringBuilder.append(toWTFString(nodeName));

    if (parentNode && JSValueIsObject(context, parentNode)) {
        stringBuilder.appendLiteral(" > ");
        stringBuilder.append(dumpPath(context, (JSObjectRef)parentNode));
    }

    return stringBuilder.toString();
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
 
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(page);

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSValueRef rangeValue = WKBundleFrameGetJavaScriptWrapperForRangeForWorld(frame, rangeRef, world);
    ASSERT(JSValueIsObject(context, rangeValue));
    JSObjectRef rangeObject = (JSObjectRef)rangeValue;

    JSValueRef startNodeValue = propertyValue(context, rangeObject, "startContainer");
    ASSERT(JSValueIsObject(context, startNodeValue));
    JSObjectRef startNodeObject = (JSObjectRef)startNodeValue;

    JSValueRef endNodeValue = propertyValue(context, rangeObject, "endContainer");
    ASSERT(JSValueIsObject(context, endNodeValue));
    JSObjectRef endNodeObject = (JSObjectRef)endNodeValue;

    int startOffset = propertyValueInt(context, rangeObject, "startOffset");
    int endOffset = propertyValueInt(context, rangeObject, "endOffset");

    WTF::StringBuilder stringBuilder;
    stringBuilder.appendLiteral("range from ");
    stringBuilder.appendNumber(startOffset);
    stringBuilder.appendLiteral(" of ");
    stringBuilder.append(dumpPath(context, startNodeObject));
    stringBuilder.appendLiteral(" to ");
    stringBuilder.appendNumber(endOffset);
    stringBuilder.appendLiteral(" of ");
    stringBuilder.append(dumpPath(context, endNodeObject));
    return stringBuilder.toString();
}

static WKRetainPtr<WKStringRef> NavigationTypeToString(WKFrameNavigationType type)
{
    switch (type) {
    case kWKFrameNavigationTypeLinkClicked:
        return adoptWK(WKStringCreateWithUTF8CString("link clicked"));
    case kWKFrameNavigationTypeFormSubmitted:
        return adoptWK(WKStringCreateWithUTF8CString("form submitted"));
    case kWKFrameNavigationTypeBackForward:
        return adoptWK(WKStringCreateWithUTF8CString("back/forward"));
    case kWKFrameNavigationTypeReload:
        return adoptWK(WKStringCreateWithUTF8CString("reload"));
    case kWKFrameNavigationTypeFormResubmitted:
        return adoptWK(WKStringCreateWithUTF8CString("form resubmitted"));
    case kWKFrameNavigationTypeOther:
        return adoptWK(WKStringCreateWithUTF8CString("other"));
    }
    return adoptWK(WKStringCreateWithUTF8CString("illegal value"));
}

static WTF::String styleDecToStr(WKBundleCSSStyleDeclarationRef style)
{
    // DumpRenderTree calls -[DOMCSSStyleDeclaration description], which just dumps class name and object address.
    // No existing tests actually hit this code path at the time of this writing, because WebCore doesn't call
    // the editing client if the styling operation source is CommandFromDOM or CommandFromDOMWithUserInterface.
    WTF::StringBuilder stringBuilder;
    stringBuilder.appendLiteral("<DOMCSSStyleDeclaration ADDRESS>");
    return stringBuilder.toString();
}

static WTF::String securityOriginToStr(WKSecurityOriginRef origin)
{
    WTF::StringBuilder stringBuilder;
    stringBuilder.append('{');
    stringBuilder.append(toWTFString(adoptWK(WKSecurityOriginCopyProtocol(origin))));
    stringBuilder.appendLiteral(", ");
    stringBuilder.append(toWTFString(adoptWK(WKSecurityOriginCopyHost(origin))));
    stringBuilder.appendLiteral(", ");
    stringBuilder.appendNumber(WKSecurityOriginGetPort(origin));
    stringBuilder.append('}');

    return stringBuilder.toString();
}

static WTF::String frameToStr(WKBundleFrameRef frame)
{
    WKRetainPtr<WKStringRef> name(AdoptWK, WKBundleFrameCopyName(frame));
    WTF::StringBuilder stringBuilder;
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
        return String();

    WKRetainPtr<WKStringRef> schemeString = adoptWK(WKURLCopyScheme(fileUrl));
    if (!isLocalFileScheme(schemeString.get()))
        return toWTFString(adoptWK(WKURLCopyString(fileUrl)));

    String pathString = toWTFString(adoptWK(WKURLCopyPath(fileUrl)));
    WTF::StringBuilder stringBuilder;

    // Remove the leading path from file urls.
    const size_t indexBaseName = pathString.reverseFind(divider);
    if (indexBaseName != notFound) {
        const size_t indexDirName = pathString.reverseFind(divider, indexBaseName - 1);
        if (indexDirName != notFound)
            stringBuilder.append(pathString.substring(indexDirName + 1, indexBaseName - indexDirName - 1));
        stringBuilder.append(divider);
        stringBuilder.append(pathString.substring(indexBaseName + 1)); // Filename.
    } else {
        stringBuilder.append(divider);
        stringBuilder.append(pathString); // Return "/pathString".
    }

    return stringBuilder.toString();
}

static inline WTF::String urlSuitableForTestResult(WKURLRef fileUrl)
{
    if (!fileUrl)
        return String();

    WKRetainPtr<WKStringRef> schemeString = adoptWK(WKURLCopyScheme(fileUrl));
    if (!isLocalFileScheme(schemeString.get()))
        return toWTFString(adoptWK(WKURLCopyString(fileUrl)));

    WTF::String urlString = toWTFString(adoptWK(WKURLCopyString(fileUrl)));
    const size_t indexBaseName = urlString.reverseFind(divider);

    return (indexBaseName == notFound) ? urlString : urlString.substring(indexBaseName + 1);
}

static HashMap<uint64_t, String> assignedUrlsCache;

static inline void dumpResourceURL(uint64_t identifier)
{
    if (assignedUrlsCache.contains(identifier))
        InjectedBundle::shared().stringBuilder()->append(assignedUrlsCache.get(identifier));
    else
        InjectedBundle::shared().stringBuilder()->appendLiteral("<unknown>");
}

InjectedBundlePage::InjectedBundlePage(WKBundlePageRef page)
    : m_page(page)
    , m_world(AdoptWK, WKBundleScriptWorldCreateWorld())
{
    WKBundlePageLoaderClient loaderClient = {
        kWKBundlePageLoaderClientCurrentVersion,
        this,
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
        0, // didNewFirstVisuallyNonEmptyLayoutForFrame
        didDetectXSSForFrame,
        0, // shouldGoToBackForwardListItem
        0, // didCreateGlobalObjectForFrame
        0, // willDisconnectDOMWindowExtensionFromGlobalObject
        0, // didReconnectDOMWindowExtensionToGlobalObject
        0, // willDestroyGlobalObjectForDOMWindowExtension
        didFinishProgress, // didFinishProgress
        0, // shouldForceUniversalAccessFromLocalURL
        didReceiveIntentForFrame, // didReceiveIntentForFrame
        registerIntentServiceForFrame, // registerIntentServiceForFrame
        0, // didLayout
    };
    WKBundlePageSetPageLoaderClient(m_page, &loaderClient);

    WKBundlePageResourceLoadClient resourceLoadClient = {
        kWKBundlePageResourceLoadClientCurrentVersion,
        this,
        didInitiateLoadForResource,
        willSendRequestForFrame,
        didReceiveResponseForResource,
        didReceiveContentLengthForResource,
        didFinishLoadForResource,
        didFailLoadForResource,
        shouldCacheResponse,
        0 // shouldUseCredentialStorage
    };
    WKBundlePageSetResourceLoadClient(m_page, &resourceLoadClient);

    WKBundlePagePolicyClient policyClient = {
        kWKBundlePagePolicyClientCurrentVersion,
        this,
        decidePolicyForNavigationAction,
        decidePolicyForNewWindowAction,
        decidePolicyForResponse,
        unableToImplementPolicy
    };
    WKBundlePageSetPolicyClient(m_page, &policyClient);

    WKBundlePageUIClient uiClient = {
        kWKBundlePageUIClientCurrentVersion,
        this,
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
    };
    WKBundlePageSetUIClient(m_page, &uiClient);

    WKBundlePageEditorClient editorClient = {
        kWKBundlePageEditorClientCurrentVersion,
        this,
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
        didChangeSelection
    };
    WKBundlePageSetEditorClient(m_page, &editorClient);

#if ENABLE(FULLSCREEN_API)
    WKBundlePageFullScreenClient fullScreenClient = {
        kWKBundlePageFullScreenClientCurrentVersion,
        this,
        supportsFullScreen,
        enterFullScreenForElement,
        exitFullScreenForElement,
        beganEnterFullScreen,
        beganExitFullScreen,
        closeFullScreen,
    };
    WKBundlePageSetFullScreenClient(m_page, &fullScreenClient);
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

    m_previousTestBackForwardListItem = adoptWK(WKBundleBackForwardListCopyItemAtIndex(WKBundlePageGetBackForwardList(m_page), 0));

    WKBundleFrameClearOpener(WKBundlePageGetMainFrame(m_page));
    
    WKBundlePageSetTracksRepaints(m_page, false);
}

void InjectedBundlePage::resetAfterTest()
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
#if PLATFORM(QT)
    DumpRenderTreeSupportQt::resetInternalsObject(context);
#else
    WebCoreTestSupport::resetInternalsObject(context);
#endif
    assignedUrlsCache.clear();
}

// Loader Client Callbacks

// String output must be identical to -[WebFrame _drt_descriptionSuitableForTestResult].
static void dumpFrameDescriptionSuitableForTestResult(WKBundleFrameRef frame)
{
    WKRetainPtr<WKStringRef> name(AdoptWK, WKBundleFrameCopyName(frame));
    if (WKBundleFrameIsMainFrame(frame)) {
        if (WKStringIsEmpty(name.get())) {
            InjectedBundle::shared().stringBuilder()->appendLiteral("main frame");
            return;
        }

        InjectedBundle::shared().stringBuilder()->appendLiteral("main frame \"");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(name));
        InjectedBundle::shared().stringBuilder()->append('"');
        return;
    }

    if (WKStringIsEmpty(name.get())) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("frame (anonymous)");
        return;
    }

    InjectedBundle::shared().stringBuilder()->appendLiteral("frame \"");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(name));
    InjectedBundle::shared().stringBuilder()->append('"');
}

static inline void dumpRequestDescriptionSuitableForTestResult(WKURLRequestRef request)
{
    WKRetainPtr<WKURLRef> url = adoptWK(WKURLRequestCopyURL(request));
    WKRetainPtr<WKURLRef> firstParty = adoptWK(WKURLRequestCopyFirstPartyForCookies(request));
    WKRetainPtr<WKStringRef> httpMethod = adoptWK(WKURLRequestCopyHTTPMethod(request));

    InjectedBundle::shared().stringBuilder()->appendLiteral("<NSURLRequest URL ");
    InjectedBundle::shared().stringBuilder()->append(pathSuitableForTestResult(url.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(", main document URL ");
    InjectedBundle::shared().stringBuilder()->append(urlSuitableForTestResult(firstParty.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(", http method ");

    if (WKStringIsEmpty(httpMethod.get()))
        InjectedBundle::shared().stringBuilder()->appendLiteral("(none)");
    else
        InjectedBundle::shared().stringBuilder()->append(toWTFString(httpMethod));

    InjectedBundle::shared().stringBuilder()->append('>');
}

static inline void dumpResponseDescriptionSuitableForTestResult(WKURLResponseRef response)
{
    WKRetainPtr<WKURLRef> url = adoptWK(WKURLResponseCopyURL(response));
    if (!url) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("(null)");
        return;
    }
    InjectedBundle::shared().stringBuilder()->appendLiteral("<NSURLResponse ");
    InjectedBundle::shared().stringBuilder()->append(pathSuitableForTestResult(url.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(", http status code ");
    InjectedBundle::shared().stringBuilder()->appendNumber(WKURLResponseHTTPStatusCode(response));
    InjectedBundle::shared().stringBuilder()->append('>');
}

static inline void dumpErrorDescriptionSuitableForTestResult(WKErrorRef error)
{
    WKRetainPtr<WKStringRef> errorDomain = adoptWK(WKErrorCopyDomain(error));
    int errorCode = WKErrorGetErrorCode(error);

    // We need to do some error mapping here to match the test expectations (Mac error names are expected).
    if (WKStringIsEqualToUTF8CString(errorDomain.get(), "WebKitNetworkError")) {
        errorDomain = adoptWK(WKStringCreateWithUTF8CString("NSURLErrorDomain"));
        errorCode = -999;
    }

    if (WKStringIsEqualToUTF8CString(errorDomain.get(), "WebKitPolicyError"))
        errorDomain = adoptWK(WKStringCreateWithUTF8CString("WebKitErrorDomain"));

    InjectedBundle::shared().stringBuilder()->appendLiteral("<NSError domain ");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(errorDomain));
    InjectedBundle::shared().stringBuilder()->appendLiteral(", code ");
    InjectedBundle::shared().stringBuilder()->appendNumber(errorCode);

    WKRetainPtr<WKURLRef> url = adoptWK(WKErrorCopyFailingURL(error));
    if (url.get()) {
        WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyString(url.get()));
        InjectedBundle::shared().stringBuilder()->appendLiteral(", failing URL \"");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(urlString));
        InjectedBundle::shared().stringBuilder()->append('"');
    }

    InjectedBundle::shared().stringBuilder()->append('>');
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

void InjectedBundlePage::didReceiveIntentForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKBundleIntentRequestRef intentRequest, WKTypeRef* userData, const void* clientInfo)
{
#if ENABLE(WEB_INTENTS)
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->m_currentIntentRequest = intentRequest;
    WKRetainPtr<WKBundleIntentRef> intent(AdoptWK, WKBundleIntentRequestCopyIntent(intentRequest));

    InjectedBundle::shared().stringBuilder()->appendLiteral("Received Web Intent: action=");
    WKRetainPtr<WKStringRef> wkAction(AdoptWK, WKBundleIntentCopyAction(intent.get()));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(wkAction.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" type=");
    WKRetainPtr<WKStringRef> wkType(AdoptWK, WKBundleIntentCopyType(intent.get()));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(wkType.get()));
    InjectedBundle::shared().stringBuilder()->append('\n');

    const size_t numMessagePorts = WKBundleIntentMessagePortCount(intent.get());
    if (numMessagePorts) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("Have ");
        InjectedBundle::shared().stringBuilder()->appendNumber(numMessagePorts);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" ports\n");
    }

    WKRetainPtr<WKURLRef> wkServiceUrl(AdoptWK, WKBundleIntentCopyService(intent.get()));
    if (wkServiceUrl) {
        WKRetainPtr<WKStringRef> wkService(AdoptWK, WKURLCopyString(wkServiceUrl.get()));
        if (wkService && !WKStringIsEmpty(wkService.get())) {
            InjectedBundle::shared().stringBuilder()->appendLiteral("Explicit intent service: ");
            InjectedBundle::shared().stringBuilder()->append(toWTFString(wkService.get()));
            InjectedBundle::shared().stringBuilder()->append('\n');
        }
    }

    WKRetainPtr<WKDictionaryRef> wkExtras(AdoptWK, WKBundleIntentCopyExtras(intent.get()));
    WKRetainPtr<WKArrayRef> wkExtraKeys(AdoptWK, WKDictionaryCopyKeys(wkExtras.get()));
    const size_t numExtraKeys = WKArrayGetSize(wkExtraKeys.get());
    for (size_t i = 0; i < numExtraKeys; ++i) {
        WKStringRef wkKey = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkExtraKeys.get(), i));
        WKStringRef wkValue = static_cast<WKStringRef>(WKDictionaryGetItemForKey(wkExtras.get(), wkKey));
        InjectedBundle::shared().stringBuilder()->appendLiteral("Extras[");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(wkKey));
        InjectedBundle::shared().stringBuilder()->appendLiteral("] = ");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(wkValue));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }

    WKRetainPtr<WKArrayRef> wkSuggestions(AdoptWK, WKBundleIntentCopySuggestions(intent.get()));
    const size_t numSuggestions = WKArrayGetSize(wkSuggestions.get());
    for (size_t i = 0; i < numSuggestions; ++i) {
        WKStringRef wkSuggestion = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkSuggestions.get(), i));
        InjectedBundle::shared().stringBuilder()->appendLiteral("Have suggestion ");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(wkSuggestion));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
#endif
}

void InjectedBundlePage::registerIntentServiceForFrame(WKBundlePageRef page, WKBundleFrameRef frame, WKIntentServiceInfoRef serviceInfo, WKTypeRef* userData, const void* clientInfo)
{
#if ENABLE(WEB_INTENTS_TAG)
    InjectedBundle::shared().stringBuilder()->appendLiteral("Registered Web Intent Service: action=");
    WKRetainPtr<WKStringRef> wkAction(AdoptWK, WKIntentServiceInfoCopyAction(serviceInfo));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(wkAction.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" type=");
    WKRetainPtr<WKStringRef> wkType(AdoptWK, WKIntentServiceInfoCopyType(serviceInfo));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(wkType.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" title=");
    WKRetainPtr<WKStringRef> wkTitle(AdoptWK, WKIntentServiceInfoCopyTitle(serviceInfo));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(wkTitle.get()));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" url=");
    WKRetainPtr<WKURLRef> wkUrl(AdoptWK, WKIntentServiceInfoCopyHref(serviceInfo));
    if (wkUrl)
        InjectedBundle::shared().stringBuilder()->append(toWTFString(adoptWK(WKURLCopyString(wkUrl.get()))));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" disposition=");
    WKRetainPtr<WKStringRef> wkDisposition(AdoptWK, WKIntentServiceInfoCopyDisposition(serviceInfo));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(wkDisposition.get()));
    InjectedBundle::shared().stringBuilder()->append('\n');
#endif
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
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willPerformClientRedirectForFrame(frame, url, delay, date);
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
    if (!InjectedBundle::shared().isTestRunning())
        return;

    platformDidStartProvisionalLoadForFrame(frame);

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didStartProvisionalLoadForFrame\n");
    }

    if (!InjectedBundle::shared().topLoadingFrame())
        InjectedBundle::shared().setTopLoadingFrame(frame);

    if (InjectedBundle::shared().testRunner()->shouldStopProvisionalFrameLoads()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - stopping load in didStartProvisionalLoadForFrame callback\n");
        WKBundleFrameStopLoading(frame);
    }
}

void InjectedBundlePage::didReceiveServerRedirectForProvisionalLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    dumpFrameDescriptionSuitableForTestResult(frame);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - didReceiveServerRedirectForProvisionalLoadForFrame\n");
}

void InjectedBundlePage::didFailProvisionalLoadWithErrorForFrame(WKBundleFrameRef frame, WKErrorRef)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didFailProvisionalLoadWithError\n");
    }

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didCommitLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    dumpFrameDescriptionSuitableForTestResult(frame);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - didCommitLoadForFrame\n");
}

void InjectedBundlePage::didFinishProgress()
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpProgressFinishedCallback())
        return;

    InjectedBundle::shared().stringBuilder()->appendLiteral("postProgressFinishedNotification\n");
}

enum FrameNamePolicy { ShouldNotIncludeFrameName, ShouldIncludeFrameName };

static void dumpFrameScrollPosition(WKBundleFrameRef frame, FrameNamePolicy shouldIncludeFrameName = ShouldNotIncludeFrameName)
{
    double x = numericWindowPropertyValue(frame, "pageXOffset");
    double y = numericWindowPropertyValue(frame, "pageYOffset");
    if (fabs(x) > 0.00000001 || fabs(y) > 0.00000001) {
        if (shouldIncludeFrameName) {
            WKRetainPtr<WKStringRef> name(AdoptWK, WKBundleFrameCopyName(frame));
            InjectedBundle::shared().stringBuilder()->appendLiteral("frame '");
            InjectedBundle::shared().stringBuilder()->append(toWTFString(name));
            InjectedBundle::shared().stringBuilder()->appendLiteral("' ");
        }
        InjectedBundle::shared().stringBuilder()->appendLiteral("scrolled to ");
        InjectedBundle::shared().stringBuilder()->append(WTF::String::number(x));
        InjectedBundle::shared().stringBuilder()->append(',');
        InjectedBundle::shared().stringBuilder()->append(WTF::String::number(y));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
}

static void dumpDescendantFrameScrollPositions(WKBundleFrameRef frame)
{
    WKRetainPtr<WKArrayRef> childFrames(AdoptWK, WKBundleFrameCopyChildFrames(frame));
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        dumpFrameScrollPosition(subframe, ShouldIncludeFrameName);
        dumpDescendantFrameScrollPositions(subframe);
    }
}

void InjectedBundlePage::dumpAllFrameScrollPositions()
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    dumpFrameScrollPosition(frame);
    dumpDescendantFrameScrollPositions(frame);
}

static JSRetainPtr<JSStringRef> toJS(const char* string)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(string));
}

static bool hasDocumentElement(WKBundleFrameRef frame)
{
    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);

    JSValueRef documentValue = JSObjectGetProperty(context, globalObject, toJS("document").get(), 0);
    if (!documentValue)
        return false;

    ASSERT(JSValueIsObject(context, documentValue));
    JSObjectRef document = JSValueToObject(context, documentValue, 0);

    JSValueRef documentElementValue = JSObjectGetProperty(context, document, toJS("documentElement").get(), 0);
    if (!documentElementValue)
        return false;

    return JSValueToBoolean(context, documentElementValue);
}

static void dumpFrameText(WKBundleFrameRef frame)
{
    // If the frame doesn't have a document element, its inner text will be an empty string, so
    // we'll end up just appending a single newline below. But DumpRenderTree doesn't append
    // anything in this case, so we shouldn't either.
    if (!hasDocumentElement(frame))
        return;

    WKRetainPtr<WKStringRef> text(AdoptWK, WKBundleFrameCopyInnerText(frame));
    InjectedBundle::shared().stringBuilder()->append(toWTFString(text));
    InjectedBundle::shared().stringBuilder()->append('\n');
}

static void dumpDescendantFramesText(WKBundleFrameRef frame)
{
    WKRetainPtr<WKArrayRef> childFrames(AdoptWK, WKBundleFrameCopyChildFrames(frame));
    size_t size = WKArrayGetSize(childFrames.get());
    for (size_t i = 0; i < size; ++i) {
        WKBundleFrameRef subframe = static_cast<WKBundleFrameRef>(WKArrayGetItemAtIndex(childFrames.get(), i));
        WKRetainPtr<WKStringRef> subframeName(AdoptWK, WKBundleFrameCopyName(subframe));
        InjectedBundle::shared().stringBuilder()->appendLiteral("\n--------\nFrame: '");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(subframeName));
        InjectedBundle::shared().stringBuilder()->appendLiteral("'\n--------\n");
        dumpFrameText(subframe);
        dumpDescendantFramesText(subframe);
    }
}

void InjectedBundlePage::dumpAllFramesText()
{
    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    dumpFrameText(frame);
    dumpDescendantFramesText(frame);
}

void InjectedBundlePage::dump()
{
    ASSERT(InjectedBundle::shared().isTestRunning());

    InjectedBundle::shared().testRunner()->invalidateWaitToDumpWatchdogTimer();

    // Force a paint before dumping. This matches DumpRenderTree on Windows. (DumpRenderTree on Mac
    // does this at a slightly different time.) See <http://webkit.org/b/55469> for details.
    WKBundlePageForceRepaint(m_page);

    WKBundleFrameRef frame = WKBundlePageGetMainFrame(m_page);
    WTF::String url = toWTFString(adoptWK(WKURLCopyString(adoptWK(WKBundleFrameCopyURL(frame)).get())));
    if (url.find("dumpAsText/") != WTF::notFound)
        InjectedBundle::shared().testRunner()->dumpAsText(false);

    switch (InjectedBundle::shared().testRunner()->whatToDump()) {
    case TestRunner::RenderTree: {
        WKRetainPtr<WKStringRef> text(AdoptWK, WKBundlePageCopyRenderTreeExternalRepresentation(m_page));
        InjectedBundle::shared().stringBuilder()->append(toWTFString(text));
        break;
    }
    case TestRunner::MainFrameText:
        dumpFrameText(WKBundlePageGetMainFrame(m_page));
        break;
    case TestRunner::AllFramesText:
        dumpAllFramesText();
        break;
    }

    if (InjectedBundle::shared().testRunner()->shouldDumpAllFrameScrollPositions())
        dumpAllFrameScrollPositions();
    else if (InjectedBundle::shared().testRunner()->shouldDumpMainFrameScrollPosition())
        dumpFrameScrollPosition(WKBundlePageGetMainFrame(m_page));

    if (InjectedBundle::shared().testRunner()->shouldDumpBackForwardListsForAllWindows())
        InjectedBundle::shared().dumpBackForwardListsForAllPages();

    if (InjectedBundle::shared().shouldDumpPixels() && InjectedBundle::shared().testRunner()->shouldDumpPixels()) {
        WKSnapshotOptions options = kWKSnapshotOptionsShareable | kWKSnapshotOptionsInViewCoordinates;
        if (InjectedBundle::shared().testRunner()->shouldDumpSelectionRect())
            options |= kWKSnapshotOptionsPaintSelectionRectangle;

        InjectedBundle::shared().setPixelResult(adoptWK(WKBundlePageCreateSnapshotWithOptions(m_page, WKBundleFrameGetVisibleContentBounds(WKBundlePageGetMainFrame(m_page)), options)).get());
        if (WKBundlePageIsTrackingRepaints(m_page))
            InjectedBundle::shared().setRepaintRects(adoptWK(WKBundlePageCopyTrackedRepaintRects(m_page)).get());
    }

    InjectedBundle::shared().done();
}

void InjectedBundlePage::didFinishLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didFinishLoadForFrame\n");
    }

    frameDidChangeLocation(frame, /*shouldDump*/ true);
}

void InjectedBundlePage::didFailLoadWithErrorForFrame(WKBundleFrameRef frame, WKErrorRef)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didFailLoadWithError\n");
    }

    frameDidChangeLocation(frame);
}

void InjectedBundlePage::didReceiveTitleForFrame(WKStringRef title, WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didReceiveTitle: ");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(title));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }

    if (!InjectedBundle::shared().testRunner()->shouldDumpTitleChanges())
        return;

    InjectedBundle::shared().stringBuilder()->appendLiteral("TITLE CHANGED: '");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(title));
    InjectedBundle::shared().stringBuilder()->appendLiteral("'\n");
}

void InjectedBundlePage::didClearWindowForFrame(WKBundleFrameRef frame, WKBundleScriptWorldRef world)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
    JSObjectRef window = JSContextGetGlobalObject(context);

    if (WKBundleScriptWorldNormalWorld() != world) {
        JSObjectSetProperty(context, window, toJS("__worldID").get(), JSValueMakeNumber(context, TestRunner::worldIDForWorld(world)), kJSPropertyAttributeReadOnly, 0);
        return;
    }

    JSValueRef exception = 0;
    InjectedBundle::shared().testRunner()->makeWindowObject(context, window, &exception);
    InjectedBundle::shared().gcController()->makeWindowObject(context, window, &exception);
    InjectedBundle::shared().eventSendingController()->makeWindowObject(context, window, &exception);
    InjectedBundle::shared().textInputController()->makeWindowObject(context, window, &exception);
    InjectedBundle::shared().accessibilityController()->makeWindowObject(context, window, &exception);

#if PLATFORM(QT)
    DumpRenderTreeSupportQt::injectInternalsObject(context);
#else
    WebCoreTestSupport::injectInternalsObject(context);
#endif
}

void InjectedBundlePage::didCancelClientRedirectForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    dumpFrameDescriptionSuitableForTestResult(frame);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - didCancelClientRedirectForFrame\n");
}

void InjectedBundlePage::willPerformClientRedirectForFrame(WKBundleFrameRef frame, WKURLRef url, double delay, double date)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        return;

    dumpFrameDescriptionSuitableForTestResult(frame);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - willPerformClientRedirectToURL: ");
    InjectedBundle::shared().stringBuilder()->append(pathSuitableForTestResult(url));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" \n");
}

void InjectedBundlePage::didSameDocumentNavigationForFrame(WKBundleFrameRef frame, WKSameDocumentNavigationType type)
{
}

void InjectedBundlePage::didFinishDocumentLoadForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didFinishDocumentLoadForFrame\n");
    }

    unsigned pendingFrameUnloadEvents = WKBundleFrameGetPendingUnloadCount(frame);
    if (pendingFrameUnloadEvents) {
        InjectedBundle::shared().stringBuilder()->append(frameToStr(frame));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - has ");
        InjectedBundle::shared().stringBuilder()->appendNumber(pendingFrameUnloadEvents);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" onunload handler(s)\n");
    }
}

void InjectedBundlePage::didHandleOnloadEventsForFrame(WKBundleFrameRef frame)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks()) {
        dumpFrameDescriptionSuitableForTestResult(frame);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didHandleOnloadEventsForFrame\n");
    }
}

void InjectedBundlePage::didDisplayInsecureContentForFrame(WKBundleFrameRef frame)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("didDisplayInsecureContent\n");
}

void InjectedBundlePage::didRunInsecureContentForFrame(WKBundleFrameRef frame)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("didRunInsecureContent\n");
}

void InjectedBundlePage::didDetectXSSForFrame(WKBundleFrameRef frame)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFrameLoadCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("didDetectXSS\n");
}

void InjectedBundlePage::didInitiateLoadForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier, WKURLRequestRef request, bool)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpResourceLoadCallbacks())
        return;

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLRequestCopyURL(request));
    assignedUrlsCache.add(identifier, pathSuitableForTestResult(url.get()));
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

WKURLRequestRef InjectedBundlePage::willSendRequestForFrame(WKBundlePageRef, WKBundleFrameRef frame, uint64_t identifier, WKURLRequestRef request, WKURLResponseRef response)
{
    if (InjectedBundle::shared().isTestRunning()
        && InjectedBundle::shared().testRunner()->shouldDumpResourceLoadCallbacks()) {
        dumpResourceURL(identifier);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - willSendRequest ");
        dumpRequestDescriptionSuitableForTestResult(request);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" redirectResponse ");
        dumpResponseDescriptionSuitableForTestResult(response);
        InjectedBundle::shared().stringBuilder()->append('\n');
    }

    if (InjectedBundle::shared().isTestRunning() && InjectedBundle::shared().testRunner()->willSendRequestReturnsNull())
        return 0;

    WKRetainPtr<WKURLRef> redirectURL = adoptWK(WKURLResponseCopyURL(response));
    if (InjectedBundle::shared().isTestRunning() && InjectedBundle::shared().testRunner()->willSendRequestReturnsNullOnRedirect() && redirectURL) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("Returning null for this redirect\n");
        return 0;
    }

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLRequestCopyURL(request));
    WKRetainPtr<WKStringRef> host = adoptWK(WKURLCopyHostName(url.get()));
    WKRetainPtr<WKStringRef> scheme = adoptWK(WKURLCopyScheme(url.get()));
    WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyString(url.get()));
    if (host && !WKStringIsEmpty(host.get())
        && isHTTPOrHTTPSScheme(scheme.get())
        && !WKStringIsEqualToUTF8CString(host.get(), "255.255.255.255") // Used in some tests that expect to get back an error.
        && !isLocalHost(host.get())) {
        bool mainFrameIsExternal = false;
        if (InjectedBundle::shared().isTestRunning()) {
            WKBundleFrameRef mainFrame = InjectedBundle::shared().topLoadingFrame();
            WKRetainPtr<WKURLRef> mainFrameURL = adoptWK(WKBundleFrameCopyURL(mainFrame));
            if (!mainFrameURL || WKStringIsEqualToUTF8CString(adoptWK(WKURLCopyString(mainFrameURL.get())).get(), "about:blank"))
                mainFrameURL = adoptWK(WKBundleFrameCopyProvisionalURL(mainFrame));

            WKRetainPtr<WKStringRef> mainFrameHost = WKURLCopyHostName(mainFrameURL.get());
            WKRetainPtr<WKStringRef> mainFrameScheme = WKURLCopyScheme(mainFrameURL.get());
            mainFrameIsExternal = isHTTPOrHTTPSScheme(mainFrameScheme.get()) && !isLocalHost(mainFrameHost.get());
        }
        if (!mainFrameIsExternal) {
            InjectedBundle::shared().stringBuilder()->appendLiteral("Blocked access to external URL ");
            InjectedBundle::shared().stringBuilder()->append(toWTFString(urlString));
            InjectedBundle::shared().stringBuilder()->append('\n');
            return 0;
        }
    }

    WKRetain(request);
    return request;
}

void InjectedBundlePage::didReceiveResponseForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier, WKURLResponseRef response)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpResourceLoadCallbacks()) {
        dumpResourceURL(identifier);
        InjectedBundle::shared().stringBuilder()->appendLiteral(" - didReceiveResponse ");
        dumpResponseDescriptionSuitableForTestResult(response);
        InjectedBundle::shared().stringBuilder()->append('\n');
    }


    if (!InjectedBundle::shared().testRunner()->shouldDumpResourceResponseMIMETypes())
        return;

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLResponseCopyURL(response));
    WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyLastPathComponent(url.get()));
    WKRetainPtr<WKStringRef> mimeTypeString = adoptWK(WKURLResponseCopyMIMEType(response));

    InjectedBundle::shared().stringBuilder()->append(toWTFString(urlString));
    InjectedBundle::shared().stringBuilder()->appendLiteral(" has MIME type ");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(mimeTypeString));
    InjectedBundle::shared().stringBuilder()->append('\n');
}

void InjectedBundlePage::didReceiveContentLengthForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t, uint64_t)
{
}

void InjectedBundlePage::didFinishLoadForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpResourceLoadCallbacks())
        return;

    dumpResourceURL(identifier);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - didFinishLoading\n");
}

void InjectedBundlePage::didFailLoadForResource(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier, WKErrorRef error)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpResourceLoadCallbacks())
        return;

    dumpResourceURL(identifier);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - didFailLoadingWithError: ");

    dumpErrorDescriptionSuitableForTestResult(error);
    InjectedBundle::shared().stringBuilder()->append('\n');
}

bool InjectedBundlePage::shouldCacheResponse(WKBundlePageRef, WKBundleFrameRef, uint64_t identifier)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (!InjectedBundle::shared().testRunner()->shouldDumpWillCacheResponse())
        return true;

    InjectedBundle::shared().stringBuilder()->appendNumber(identifier);
    InjectedBundle::shared().stringBuilder()->appendLiteral(" - willCacheResponse: called\n");

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
    if (!InjectedBundle::shared().isTestRunning())
        return WKBundlePagePolicyActionUse;

    if (!InjectedBundle::shared().testRunner()->isPolicyDelegateEnabled())
        return WKBundlePagePolicyActionUse;

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLRequestCopyURL(request));
    WKRetainPtr<WKStringRef> urlScheme = adoptWK(WKURLCopyScheme(url.get()));

    InjectedBundle::shared().stringBuilder()->appendLiteral("Policy delegate: attempt to load ");
    if (isLocalFileScheme(urlScheme.get())) {
        WKRetainPtr<WKStringRef> filename = adoptWK(WKURLCopyLastPathComponent(url.get()));
        InjectedBundle::shared().stringBuilder()->append(toWTFString(filename));
    } else {
        WKRetainPtr<WKStringRef> urlString = adoptWK(WKURLCopyString(url.get()));
        InjectedBundle::shared().stringBuilder()->append(toWTFString(urlString));
    }
    InjectedBundle::shared().stringBuilder()->appendLiteral(" with navigation type \'");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(NavigationTypeToString(WKBundleNavigationActionGetNavigationType(navigationAction))));
    InjectedBundle::shared().stringBuilder()->appendLiteral("\'");
    WKBundleHitTestResultRef hitTestResultRef = WKBundleNavigationActionCopyHitTestResult(navigationAction);
    if (hitTestResultRef) {
        InjectedBundle::shared().stringBuilder()->appendLiteral(" originating from ");
        InjectedBundle::shared().stringBuilder()->append(dumpPath(m_page, m_world.get(), WKBundleHitTestResultCopyNodeHandle(hitTestResultRef)));
    }

    InjectedBundle::shared().stringBuilder()->append('\n');
    InjectedBundle::shared().testRunner()->notifyDone();

    if (InjectedBundle::shared().testRunner()->isPolicyDelegatePermissive())
        return WKBundlePagePolicyActionUse;
    return WKBundlePagePolicyActionPassThrough;
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForNewWindowAction(WKBundlePageRef, WKBundleFrameRef, WKBundleNavigationActionRef, WKURLRequestRef, WKStringRef, WKTypeRef*)
{
    return WKBundlePagePolicyActionUse;
}

WKBundlePagePolicyAction InjectedBundlePage::decidePolicyForResponse(WKBundlePageRef page, WKBundleFrameRef, WKURLResponseRef response, WKURLRequestRef, WKTypeRef*)
{
    if (WKURLResponseIsAttachment(response)) {
        WKRetainPtr<WKStringRef> filename = adoptWK(WKURLResponseCopySuggestedFilename(response));
        InjectedBundle::shared().stringBuilder()->appendLiteral("Policy delegate: resource is an attachment, suggested file name \'");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(filename));
        InjectedBundle::shared().stringBuilder()->appendLiteral("\'\n");
    }

    WKRetainPtr<WKStringRef> mimeType = adoptWK(WKURLResponseCopyMIMEType(response));
    return WKBundlePageCanShowMIMEType(page, mimeType.get()) ? WKBundlePagePolicyActionUse : WKBundlePagePolicyActionPassThrough;
}

void InjectedBundlePage::unableToImplementPolicy(WKBundlePageRef, WKBundleFrameRef, WKErrorRef, WKTypeRef*)
{
}

// UI Client Callbacks

void InjectedBundlePage::willAddMessageToConsole(WKBundlePageRef page, WKStringRef message, uint32_t lineNumber, const void *clientInfo)
{
    static_cast<InjectedBundlePage*>(const_cast<void*>(clientInfo))->willAddMessageToConsole(message, lineNumber);
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

static WTF::String lastFileURLPathComponent(const WTF::String& path)
{
    size_t pos = path.find("file://");
    ASSERT(WTF::notFound != pos);

    WTF::String tmpPath = path.substring(pos + 7);
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

void InjectedBundlePage::willAddMessageToConsole(WKStringRef message, uint32_t lineNumber)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    WTF::String messageString = toWTFString(message);
    size_t fileProtocolStart = messageString.find("file://");
    if (fileProtocolStart != WTF::notFound)
        // FIXME: The code below does not handle additional text after url nor multiple urls. This matches DumpRenderTree implementation.
        messageString = messageString.substring(0, fileProtocolStart) + lastFileURLPathComponent(messageString.substring(fileProtocolStart));

    InjectedBundle::shared().stringBuilder()->appendLiteral("CONSOLE MESSAGE: ");
    if (lineNumber) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("line ");
        InjectedBundle::shared().stringBuilder()->appendNumber(lineNumber);
        InjectedBundle::shared().stringBuilder()->appendLiteral(": ");
    }
    InjectedBundle::shared().stringBuilder()->append(messageString);
    InjectedBundle::shared().stringBuilder()->append('\n');

}

void InjectedBundlePage::willSetStatusbarText(WKStringRef statusbarText)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (!InjectedBundle::shared().testRunner()->shouldDumpStatusCallbacks())
        return;

    InjectedBundle::shared().stringBuilder()->appendLiteral("UI DELEGATE STATUS CALLBACK: setStatusText:");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(statusbarText));
    InjectedBundle::shared().stringBuilder()->append('\n');
}

void InjectedBundlePage::willRunJavaScriptAlert(WKStringRef message, WKBundleFrameRef)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    InjectedBundle::shared().stringBuilder()->appendLiteral("ALERT: ");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(message));
    InjectedBundle::shared().stringBuilder()->append('\n');
}

void InjectedBundlePage::willRunJavaScriptConfirm(WKStringRef message, WKBundleFrameRef)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    InjectedBundle::shared().stringBuilder()->appendLiteral("CONFIRM: ");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(message));
    InjectedBundle::shared().stringBuilder()->append('\n');
}

void InjectedBundlePage::willRunJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKBundleFrameRef)
{
    InjectedBundle::shared().stringBuilder()->appendLiteral("PROMPT: ");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(message));
    InjectedBundle::shared().stringBuilder()->appendLiteral(", default text: ");
    InjectedBundle::shared().stringBuilder()->append(toWTFString(defaultValue));
    InjectedBundle::shared().stringBuilder()->append('\n');
}

void InjectedBundlePage::didReachApplicationCacheOriginQuota(WKSecurityOriginRef origin, int64_t totalBytesNeeded)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpApplicationCacheDelegateCallbacks()) {
        // For example, numbers from 30000 - 39999 will output as 30000.
        // Rounding up or down does not really matter for these tests. It's
        // sufficient to just get a range of 10000 to determine if we were
        // above or below a threshold.
        int64_t truncatedSpaceNeeded = (totalBytesNeeded / 10000) * 10000;

        InjectedBundle::shared().stringBuilder()->appendLiteral("UI DELEGATE APPLICATION CACHE CALLBACK: exceededApplicationCacheOriginQuotaForSecurityOrigin:");
        InjectedBundle::shared().stringBuilder()->append(securityOriginToStr(origin));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" totalSpaceNeeded:~");
        InjectedBundle::shared().stringBuilder()->appendNumber(truncatedSpaceNeeded);
        InjectedBundle::shared().stringBuilder()->append('\n');
    }

    if (InjectedBundle::shared().testRunner()->shouldDisallowIncreaseForApplicationCacheQuota())
        return;

    // Reset default application cache quota.
    WKBundleResetApplicationCacheOriginQuota(InjectedBundle::shared().bundle(), adoptWK(WKSecurityOriginCopyToString(origin)).get());
}

uint64_t InjectedBundlePage::didExceedDatabaseQuota(WKSecurityOriginRef origin, WKStringRef databaseName, WKStringRef databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpDatabaseCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:");
        InjectedBundle::shared().stringBuilder()->append(securityOriginToStr(origin));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" database:");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(databaseName));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }

    static const uint64_t defaultQuota = 5 * 1024 * 1024;
    return defaultQuota;
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
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldBeginEditingInDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), range));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldEndEditing(WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldEndEditingInDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), range));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertNode(WKBundleNodeHandleRef node, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    static const char* insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldInsertNode:");
        InjectedBundle::shared().stringBuilder()->append(dumpPath(m_page, m_world.get(), node));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" replacingDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), rangeToReplace));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" givenAction:");
        InjectedBundle::shared().stringBuilder()->append(insertactionstring[action]);
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldInsertText(WKStringRef text, WKBundleRangeHandleRef rangeToReplace, WKInsertActionType action)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    static const char *insertactionstring[] = {
        "WebViewInsertActionTyped",
        "WebViewInsertActionPasted",
        "WebViewInsertActionDropped",
    };

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldInsertText:");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(text));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" replacingDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), rangeToReplace));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" givenAction:");
        InjectedBundle::shared().stringBuilder()->append(insertactionstring[action]);
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldDeleteRange(WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldDeleteDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), range));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldChangeSelectedRange(WKBundleRangeHandleRef fromRange, WKBundleRangeHandleRef toRange, WKAffinityType affinity, bool stillSelecting)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    static const char *affinitystring[] = {
        "NSSelectionAffinityUpstream",
        "NSSelectionAffinityDownstream"
    };
    static const char *boolstring[] = {
        "FALSE",
        "TRUE"
    };

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldChangeSelectedDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), fromRange));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" toDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), toRange));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" affinity:");
        InjectedBundle::shared().stringBuilder()->append(affinitystring[affinity]); 
        InjectedBundle::shared().stringBuilder()->appendLiteral(" stillSelecting:");
        InjectedBundle::shared().stringBuilder()->append(boolstring[stillSelecting]); 
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

bool InjectedBundlePage::shouldApplyStyle(WKBundleCSSStyleDeclarationRef style, WKBundleRangeHandleRef range)
{
    if (!InjectedBundle::shared().isTestRunning())
        return true;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: shouldApplyStyle:");
        InjectedBundle::shared().stringBuilder()->append(styleDecToStr(style));
        InjectedBundle::shared().stringBuilder()->appendLiteral(" toElementsInDOMRange:");
        InjectedBundle::shared().stringBuilder()->append(rangeToStr(m_page, m_world.get(), range));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
    return InjectedBundle::shared().testRunner()->shouldAllowEditing();
}

void InjectedBundlePage::didBeginEditing(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: webViewDidBeginEditing:");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(notificationName));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
}

void InjectedBundlePage::didEndEditing(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: webViewDidEndEditing:");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(notificationName));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
}

void InjectedBundlePage::didChange(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: webViewDidChange:");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(notificationName));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
}

void InjectedBundlePage::didChangeSelection(WKStringRef notificationName)
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (InjectedBundle::shared().testRunner()->shouldDumpEditingCallbacks()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("EDITING DELEGATE: webViewDidChangeSelection:");
        InjectedBundle::shared().stringBuilder()->append(toWTFString(notificationName));
        InjectedBundle::shared().stringBuilder()->append('\n');
    }
}

#if ENABLE(FULLSCREEN_API)
bool InjectedBundlePage::supportsFullScreen(WKBundlePageRef pageRef, WKFullScreenKeyboardRequestType requestType)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFullScreenCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("supportsFullScreen() == true\n");
    return true;
}

void InjectedBundlePage::enterFullScreenForElement(WKBundlePageRef pageRef, WKBundleNodeHandleRef elementRef)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFullScreenCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("enterFullScreenForElement()\n");

    if (!InjectedBundle::shared().testRunner()->hasCustomFullScreenBehavior()) {
        WKBundlePageWillEnterFullScreen(pageRef);
        WKBundlePageDidEnterFullScreen(pageRef);
    }
}

void InjectedBundlePage::exitFullScreenForElement(WKBundlePageRef pageRef, WKBundleNodeHandleRef elementRef)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFullScreenCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("exitFullScreenForElement()\n");

    if (!InjectedBundle::shared().testRunner()->hasCustomFullScreenBehavior()) {
        WKBundlePageWillExitFullScreen(pageRef);
        WKBundlePageDidExitFullScreen(pageRef);
    }
}

void InjectedBundlePage::beganEnterFullScreen(WKBundlePageRef, WKRect, WKRect)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFullScreenCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("beganEnterFullScreen()\n");
}

void InjectedBundlePage::beganExitFullScreen(WKBundlePageRef, WKRect, WKRect)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFullScreenCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("beganExitFullScreen()\n");
}

void InjectedBundlePage::closeFullScreen(WKBundlePageRef pageRef)
{
    if (InjectedBundle::shared().testRunner()->shouldDumpFullScreenCallbacks())
        InjectedBundle::shared().stringBuilder()->appendLiteral("closeFullScreen()\n");

    if (!InjectedBundle::shared().testRunner()->hasCustomFullScreenBehavior()) {
        WKBundlePageWillExitFullScreen(pageRef);
        WKBundlePageDidExitFullScreen(pageRef);
    }
}
#endif

static bool compareByTargetName(WKBundleBackForwardListItemRef item1, WKBundleBackForwardListItemRef item2)
{
    return toSTD(adoptWK(WKBundleBackForwardListItemCopyTarget(item1))) < toSTD(adoptWK(WKBundleBackForwardListItemCopyTarget(item2)));
}

static void dumpBackForwardListItem(WKBundleBackForwardListItemRef item, unsigned indent, bool isCurrentItem)
{
    unsigned column = 0;
    if (isCurrentItem) {
        InjectedBundle::shared().stringBuilder()->appendLiteral("curr->");
        column = 6;
    }
    for (unsigned i = column; i < indent; i++)
        InjectedBundle::shared().stringBuilder()->append(' ');

    WTF::String url = toWTFString(adoptWK(WKURLCopyString(adoptWK(WKBundleBackForwardListItemCopyURL(item)).get())));
    if (hasPrefix(url, "file:")) {
        WTF::String directoryName = "/LayoutTests/";
        size_t start = url.find(directoryName);
        if (start == WTF::notFound)
            start = 0;
        else
            start += directoryName.length();
        InjectedBundle::shared().stringBuilder()->appendLiteral("(file test):");
        InjectedBundle::shared().stringBuilder()->append(url.substring(start));
    } else
        InjectedBundle::shared().stringBuilder()->append(url);

    WTF::String target = toWTFString(adoptWK(WKBundleBackForwardListItemCopyTarget(item)));
    if (target.length()) {
        InjectedBundle::shared().stringBuilder()->appendLiteral(" (in frame \"");
        InjectedBundle::shared().stringBuilder()->append(target);
        InjectedBundle::shared().stringBuilder()->appendLiteral("\")");
    }

    // FIXME: Need WKBackForwardListItemIsTargetItem.
    if (WKBundleBackForwardListItemIsTargetItem(item))
        InjectedBundle::shared().stringBuilder()->appendLiteral("  **nav target**");

    InjectedBundle::shared().stringBuilder()->append('\n');

    if (WKRetainPtr<WKArrayRef> kids = adoptWK(WKBundleBackForwardListItemCopyChildren(item))) {
        // Sort to eliminate arbitrary result ordering which defeats reproducible testing.
        size_t size = WKArrayGetSize(kids.get());
        Vector<WKBundleBackForwardListItemRef> sortedKids(size);
        for (size_t i = 0; i < size; ++i)
            sortedKids[i] = static_cast<WKBundleBackForwardListItemRef>(WKArrayGetItemAtIndex(kids.get(), i));
        stable_sort(sortedKids.begin(), sortedKids.end(), compareByTargetName);
        for (size_t i = 0; i < size; ++i)
            dumpBackForwardListItem(sortedKids[i], indent + 4, false);
    }
}

void InjectedBundlePage::dumpBackForwardList()
{
    InjectedBundle::shared().stringBuilder()->appendLiteral("\n============== Back Forward List ==============\n");

    WKBundleBackForwardListRef list = WKBundlePageGetBackForwardList(m_page);

    // Print out all items in the list after m_previousTestBackForwardListItem.
    // Gather items from the end of the list, then print them out from oldest to newest.
    Vector<WKRetainPtr<WKBundleBackForwardListItemRef> > itemsToPrint;
    for (unsigned i = WKBundleBackForwardListGetForwardListCount(list); i; --i) {
        WKRetainPtr<WKBundleBackForwardListItemRef> item = adoptWK(WKBundleBackForwardListCopyItemAtIndex(list, i));
        // Something is wrong if the item from the last test is in the forward part of the list.
        ASSERT(!WKBundleBackForwardListItemIsSame(item.get(), m_previousTestBackForwardListItem.get()));
        itemsToPrint.append(item);
    }

    ASSERT(!WKBundleBackForwardListItemIsSame(adoptWK(WKBundleBackForwardListCopyItemAtIndex(list, 0)).get(), m_previousTestBackForwardListItem.get()));

    itemsToPrint.append(adoptWK(WKBundleBackForwardListCopyItemAtIndex(list, 0)));

    int currentItemIndex = itemsToPrint.size() - 1;

    int backListCount = WKBundleBackForwardListGetBackListCount(list);
    for (int i = -1; i >= -backListCount; --i) {
        WKRetainPtr<WKBundleBackForwardListItemRef> item = adoptWK(WKBundleBackForwardListCopyItemAtIndex(list, i));
        if (WKBundleBackForwardListItemIsSame(item.get(), m_previousTestBackForwardListItem.get()))
            break;
        itemsToPrint.append(item);
    }

    for (int i = itemsToPrint.size() - 1; i >= 0; i--)
        dumpBackForwardListItem(itemsToPrint[i].get(), 8, i == currentItemIndex);

    InjectedBundle::shared().stringBuilder()->appendLiteral("===============================================\n");
}

#if !PLATFORM(MAC)
void InjectedBundlePage::platformDidStartProvisionalLoadForFrame(WKBundleFrameRef)
{
}
#endif

void InjectedBundlePage::frameDidChangeLocation(WKBundleFrameRef frame, bool shouldDump)
{
    if (frame != InjectedBundle::shared().topLoadingFrame())
        return;

    InjectedBundle::shared().setTopLoadingFrame(0);

    if (InjectedBundle::shared().testRunner()->waitToDump())
        return;

    if (InjectedBundle::shared().shouldProcessWorkQueue()) {
        InjectedBundle::shared().processWorkQueue();
        return;
    }

    if (shouldDump)
        InjectedBundle::shared().page()->dump();
    else
        InjectedBundle::shared().done();
}

} // namespace WTR
