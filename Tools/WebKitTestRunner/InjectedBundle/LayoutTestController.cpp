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

#include "config.h"
#include "LayoutTestController.h"

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSLayoutTestController.h"
#include "PlatformWebView.h"
#include "StringFunctions.h"
#include "TestController.h"
#include <WebKit2/WKBundleBackForwardList.h>
#include <WebKit2/WKBundleFrame.h>
#include <WebKit2/WKBundleFramePrivate.h>
#include <WebKit2/WKBundleInspector.h>
#include <WebKit2/WKBundleNodeHandlePrivate.h>
#include <WebKit2/WKBundlePagePrivate.h>
#include <WebKit2/WKBundlePrivate.h>
#include <WebKit2/WKBundleScriptWorld.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WebKit2.h>
#include <wtf/HashMap.h>

namespace WTR {

// This is lower than DumpRenderTree's timeout, to make it easier to work through the failures
// Eventually it should be changed to match.
const double LayoutTestController::waitToDumpWatchdogTimerInterval = 6;

static JSValueRef propertyValue(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    if (!object)
        return 0;
    JSRetainPtr<JSStringRef> propertyNameString(Adopt, JSStringCreateWithUTF8CString(propertyName));
    JSValueRef exception;
    return JSObjectGetProperty(context, object, propertyNameString.get(), &exception);
}

static JSObjectRef propertyObject(JSContextRef context, JSObjectRef object, const char* propertyName)
{
    JSValueRef value = propertyValue(context, object, propertyName);
    if (!value || !JSValueIsObject(context, value))
        return 0;
    return const_cast<JSObjectRef>(value);
}

static JSObjectRef getElementById(WKBundleFrameRef frame, JSStringRef elementId)
{
    JSContextRef context = WKBundleFrameGetJavaScriptContext(frame);
    JSObjectRef document = propertyObject(context, JSContextGetGlobalObject(context), "document");
    if (!document)
        return 0;
    JSValueRef getElementById = propertyObject(context, document, "getElementById");
    if (!getElementById || !JSValueIsObject(context, getElementById))
        return 0;
    JSValueRef elementIdValue = JSValueMakeString(context, elementId);
    JSValueRef exception;
    JSValueRef element = JSObjectCallAsFunction(context, const_cast<JSObjectRef>(getElementById), document, 1, &elementIdValue, &exception);
    if (!element || !JSValueIsObject(context, element))
        return 0;
    return const_cast<JSObjectRef>(element);
}

PassRefPtr<LayoutTestController> LayoutTestController::create()
{
    return adoptRef(new LayoutTestController);
}

LayoutTestController::LayoutTestController()
    : m_whatToDump(RenderTree)
    , m_shouldDumpAllFrameScrollPositions(false)
    , m_shouldDumpBackForwardListsForAllWindows(false)
    , m_shouldAllowEditing(true)
    , m_shouldCloseExtraWindows(false)
    , m_dumpEditingCallbacks(false)
    , m_dumpStatusCallbacks(false)
    , m_dumpTitleChanges(false)
    , m_dumpPixels(true)
    , m_dumpFullScreenCallbacks(false)
    , m_waitToDump(false)
    , m_testRepaint(false)
    , m_testRepaintSweepHorizontally(false)
    , m_willSendRequestReturnsNull(false)
    , m_policyDelegateEnabled(false)
    , m_policyDelegatePermissive(false)
    , m_globalFlag(false)
{
    platformInitialize();
}

LayoutTestController::~LayoutTestController()
{
}

JSClassRef LayoutTestController::wrapperClass()
{
    return JSLayoutTestController::layoutTestControllerClass();
}

void LayoutTestController::display()
{
    WKBundlePageRef page = InjectedBundle::shared().page()->page();
    WKBundlePageForceRepaint(page);
    WKBundlePageSetTracksRepaints(page, true);
    WKBundlePageResetTrackedRepaints(page);
}

void LayoutTestController::dumpAsText(bool dumpPixels)
{
    m_whatToDump = MainFrameText;
    m_dumpPixels = dumpPixels;
}

// FIXME: Needs a full implementation see https://bugs.webkit.org/show_bug.cgi?id=42546
void LayoutTestController::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    m_policyDelegateEnabled = enabled;
    m_policyDelegatePermissive = permissive;
}

void LayoutTestController::waitForPolicyDelegate()
{
    setCustomPolicyDelegate(true);
    waitUntilDone();
}

void LayoutTestController::waitUntilDone()
{
    m_waitToDump = true;
    initializeWaitToDumpWatchdogTimerIfNeeded();
}

void LayoutTestController::waitToDumpWatchdogTimerFired()
{
    invalidateWaitToDumpWatchdogTimer();
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    InjectedBundle::shared().os() << message << "\n";
    InjectedBundle::shared().done();
}

void LayoutTestController::notifyDone()
{
    if (!InjectedBundle::shared().isTestRunning())
        return;

    if (m_waitToDump && !InjectedBundle::shared().topLoadingFrame())
        InjectedBundle::shared().page()->dump();

    m_waitToDump = false;
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    // FIXME: Is it OK this works only for the main frame?
    // FIXME: If this is needed only for the main frame, then why is the function on WKBundleFrame instead of WKBundlePage?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFrameGetNumberOfActiveAnimations(mainFrame);
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    // FIXME: Is it OK this works only for the main frame?
    // FIXME: If this is needed only for the main frame, then why is the function on WKBundleFrame instead of WKBundlePage?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFramePauseAnimationOnElementWithId(mainFrame, toWK(animationName).get(), toWK(elementId).get(), time);
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    // FIXME: Is it OK this works only for the main frame?
    // FIXME: If this is needed only for the main frame, then why is the function on WKBundleFrame instead of WKBundlePage?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFramePauseTransitionOnElementWithId(mainFrame, toWK(propertyName).get(), toWK(elementId).get(), time);
}

void LayoutTestController::suspendAnimations()
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    WKBundleFrameSuspendAnimations(mainFrame);
}

void LayoutTestController::resumeAnimations()
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    WKBundleFrameResumeAnimations(mainFrame);
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    WKRetainPtr<WKStringRef> text(AdoptWK, WKBundleFrameCopyLayerTreeAsText(mainFrame));
    return toJS(text);
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    WKRetainPtr<WKStringRef> sourceWK = toWK(source);
    WKRetainPtr<WKBundleScriptWorldRef> scriptWorld(AdoptWK, WKBundleScriptWorldCreateWorld());

    WKBundleAddUserScript(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), scriptWorld.get(), sourceWK.get(), 0, 0, 0,
        (runAtStart ? kWKInjectAtDocumentStart : kWKInjectAtDocumentEnd),
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void LayoutTestController::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    WKRetainPtr<WKStringRef> sourceWK = toWK(source);
    WKRetainPtr<WKBundleScriptWorldRef> scriptWorld(AdoptWK, WKBundleScriptWorldCreateWorld());

    WKBundleAddUserStyleSheet(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), scriptWorld.get(), sourceWK.get(), 0, 0, 0,
        (allFrames ? kWKInjectInAllFrames : kWKInjectInTopFrameOnly));
}

void LayoutTestController::keepWebHistory()
{
    WKBundleSetShouldTrackVisitedLinks(InjectedBundle::shared().bundle(), true);
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSValueRef element)
{
    // FIXME: Is it OK this works only for the main frame?
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    if (!JSValueIsObject(context, element))
        return JSValueMakeUndefined(context);
    JSValueRef value = WKBundleFrameGetComputedStyleIncludingVisitedInfo(mainFrame, const_cast<JSObjectRef>(element));
    if (!value)
        return JSValueMakeUndefined(context);
    return value;
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef elementId)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSObjectRef element = getElementById(mainFrame, elementId);
    if (!element)
        return 0;
    WKRetainPtr<WKStringRef> value(AdoptWK, WKBundleFrameCopyCounterValue(mainFrame, const_cast<JSObjectRef>(element)));
    return toJS(value);
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSValueRef element)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    if (!element || !JSValueIsObject(context, element))
        return 0;
    WKRetainPtr<WKStringRef> text(AdoptWK, WKBundleFrameCopyMarkerText(mainFrame, const_cast<JSObjectRef>(element)));
    if (WKStringIsEmpty(text.get()))
        return 0;
    return toJS(text);
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef argument)
{
    WKBundlePageExecuteEditingCommand(InjectedBundle::shared().page()->page(), toWK(name).get(), toWK(argument).get());
}

bool LayoutTestController::findString(JSStringRef target, JSValueRef optionsArrayAsValue)
{
    WKFindOptions options = 0;

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSRetainPtr<JSStringRef> lengthPropertyName(Adopt, JSStringCreateWithUTF8CString("length"));
    JSObjectRef optionsArray = JSValueToObject(context, optionsArrayAsValue, 0);
    JSValueRef lengthValue = JSObjectGetProperty(context, optionsArray, lengthPropertyName.get(), 0);
    if (!JSValueIsNumber(context, lengthValue))
        return false;

    size_t length = static_cast<size_t>(JSValueToNumber(context, lengthValue, 0));
    for (size_t i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, 0);
        if (!JSValueIsString(context, value))
            continue;

        JSRetainPtr<JSStringRef> optionName(Adopt, JSValueToStringCopy(context, value, 0));

        if (JSStringIsEqualToUTF8CString(optionName.get(), "CaseInsensitive"))
            options |= kWKFindOptionsCaseInsensitive;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "AtWordStarts"))
            options |= kWKFindOptionsAtWordStarts;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "TreatMedialCapitalAsWordStart"))
            options |= kWKFindOptionsTreatMedialCapitalAsWordStart;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "Backwards"))
            options |= kWKFindOptionsBackwards;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "WrapAround"))
            options |= kWKFindOptionsWrapAround;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "StartInSelection")) {
            // FIXME: No kWKFindOptionsStartInSelection.
        }
    }

    return WKBundlePageFindString(InjectedBundle::shared().page()->page(), toWK(target).get(), options);
}

void LayoutTestController::clearAllDatabases()
{
    WKBundleClearAllDatabases(InjectedBundle::shared().bundle());
}

void LayoutTestController::setDatabaseQuota(uint64_t quota)
{
    return WKBundleSetDatabaseQuota(InjectedBundle::shared().bundle(), quota);
}

void LayoutTestController::clearAllApplicationCaches()
{
    WKBundleClearApplicationCache(InjectedBundle::shared().bundle());
}

void LayoutTestController::setAppCacheMaximumSize(uint64_t size)
{
    WKBundleSetAppCacheMaximumSize(InjectedBundle::shared().bundle(), size);
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    return WKBundlePageIsEditingCommandEnabled(InjectedBundle::shared().page()->page(), toWK(name).get());
}

void LayoutTestController::setCanOpenWindows(bool)
{
    // It's not clear if or why any tests require opening windows be forbidden.
    // For now, just ignore this setting, and if we find later it's needed we can add it.
}

void LayoutTestController::setXSSAuditorEnabled(bool enabled)
{
    WKBundleOverrideXSSAuditorEnabledForTestRunner(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), true);
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    WKBundleSetAllowUniversalAccessFromFileURLs(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool enabled)
{
    WKBundleSetAllowFileAccessFromFileURLs(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setFrameFlatteningEnabled(bool enabled)
{
    WKBundleSetFrameFlatteningEnabled(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setGeolocationPermission(bool enabled)
{
    WKBundleSetGeolocationPermission(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool enabled)
{
     WKBundleSetJavaScriptCanAccessClipboard(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setPrivateBrowsingEnabled(bool enabled)
{
     WKBundleSetPrivateBrowsingEnabled(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setPopupBlockingEnabled(bool enabled)
{
     WKBundleSetPopupBlockingEnabled(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool enabled)
{
     WKBundleSetAuthorAndUserStylesEnabled(InjectedBundle::shared().bundle(), InjectedBundle::shared().pageGroup(), enabled);
}

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WKBundleAddOriginAccessWhitelistEntry(InjectedBundle::shared().bundle(), toWK(sourceOrigin).get(), toWK(destinationProtocol).get(), toWK(destinationHost).get(), allowDestinationSubdomains);
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WKBundleRemoveOriginAccessWhitelistEntry(InjectedBundle::shared().bundle(), toWK(sourceOrigin).get(), toWK(destinationProtocol).get(), toWK(destinationHost).get(), allowDestinationSubdomains);
}

int LayoutTestController::numberOfPages(double pageWidthInPixels, double pageHeightInPixels)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleNumberOfPages(InjectedBundle::shared().bundle(), mainFrame, pageWidthInPixels, pageHeightInPixels);
}

int LayoutTestController::pageNumberForElementById(JSStringRef id, double pageWidthInPixels, double pageHeightInPixels)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundlePageNumberForElementById(InjectedBundle::shared().bundle(), mainFrame, toWK(id).get(), pageWidthInPixels, pageHeightInPixels);
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return toJS(WKBundlePageSizeAndMarginsInPixels(InjectedBundle::shared().bundle(), mainFrame, pageIndex, width, height, marginTop, marginRight, marginBottom, marginLeft));
}

bool LayoutTestController::isPageBoxVisible(int pageIndex)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleIsPageBoxVisible(InjectedBundle::shared().bundle(), mainFrame, pageIndex);
}

unsigned LayoutTestController::windowCount()
{
    return InjectedBundle::shared().pageCount();
}

void LayoutTestController::clearBackForwardList()
{
    WKBundleBackForwardListClear(WKBundlePageGetBackForwardList(InjectedBundle::shared().page()->page()));
}

// Object Creation

void LayoutTestController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "layoutTestController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

void LayoutTestController::showWebInspector()
{
    WKBundleInspectorShow(WKBundlePageGetInspector(InjectedBundle::shared().page()->page()));
}

void LayoutTestController::closeWebInspector()
{
    WKBundleInspectorClose(WKBundlePageGetInspector(InjectedBundle::shared().page()->page()));
}

void LayoutTestController::evaluateInWebInspector(long callID, JSStringRef script)
{
    WKRetainPtr<WKStringRef> scriptWK = toWK(script);
    WKBundleInspectorEvaluateScriptForTest(WKBundlePageGetInspector(InjectedBundle::shared().page()->page()), callID, scriptWK.get());
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool enabled)
{
    WKBundleInspectorSetJavaScriptProfilingEnabled(WKBundlePageGetInspector(InjectedBundle::shared().page()->page()), enabled);
}

typedef WTF::HashMap<unsigned, WKRetainPtr<WKBundleScriptWorldRef> > WorldMap;
static WorldMap& worldMap()
{
    static WorldMap& map = *new WorldMap;
    return map;
}

unsigned LayoutTestController::worldIDForWorld(WKBundleScriptWorldRef world)
{
    WorldMap::const_iterator end = worldMap().end();
    for (WorldMap::const_iterator it = worldMap().begin(); it != end; ++it) {
        if (it->second == world)
            return it->first;
    }

    return 0;
}

void LayoutTestController::evaluateScriptInIsolatedWorld(JSContextRef context, unsigned worldID, JSStringRef script)
{
    // A worldID of 0 always corresponds to a new world. Any other worldID corresponds to a world
    // that is created once and cached forever.
    WKRetainPtr<WKBundleScriptWorldRef> world;
    if (!worldID)
        world.adopt(WKBundleScriptWorldCreateWorld());
    else {
        WKRetainPtr<WKBundleScriptWorldRef>& worldSlot = worldMap().add(worldID, 0).first->second;
        if (!worldSlot)
            worldSlot.adopt(WKBundleScriptWorldCreateWorld());
        world = worldSlot;
    }

    WKBundleFrameRef frame = WKBundleFrameForJavaScriptContext(context);
    if (!frame)
        frame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());

    JSGlobalContextRef jsContext = WKBundleFrameGetJavaScriptContextForWorld(frame, world.get());
    JSEvaluateScript(jsContext, script, 0, 0, 0, 0); 
}

void LayoutTestController::setPOSIXLocale(JSStringRef locale)
{
    char localeBuf[32];
    JSStringGetUTF8CString(locale, localeBuf, sizeof(localeBuf));
    setlocale(LC_ALL, localeBuf);
}

void LayoutTestController::setTextDirection(JSStringRef direction)
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    return WKBundleFrameSetTextDirection(mainFrame, toWK(direction).get());
}
    
void LayoutTestController::setShouldStayOnPageAfterHandlingBeforeUnload(bool shouldStayOnPage)
{
    InjectedBundle::shared().postNewBeforeUnloadReturnValue(!shouldStayOnPage);
}

void LayoutTestController::dumpConfigurationForViewport(int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    InjectedBundle::shared().os() << toSTD(adoptWK(WKBundlePageViewportConfigurationAsText(InjectedBundle::shared().page()->page(), deviceDPI, deviceWidth, deviceHeight, availableWidth, availableHeight)));
}

typedef WTF::HashMap<unsigned, JSValueRef> CallbackMap;
static CallbackMap& callbackMap()
{
    static CallbackMap& map = *new CallbackMap;
    return map;
}

enum {
    AddChromeInputFieldCallbackID,
    RemoveChromeInputFieldCallbackID,
    FocusWebViewCallbackID,
    SetBackingScaleFactorCallbackID
};

static void cacheLayoutTestControllerCallback(unsigned index, JSValueRef callback)
{
    if (!callback)
        return;

    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSValueProtect(context, callback);
    callbackMap().add(index, callback);
}

static void callLayoutTestControllerCallback(unsigned index)
{
    if (!callbackMap().contains(index))
        return;
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::shared().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    JSObjectRef callback = JSValueToObject(context, callbackMap().take(index), 0);
    JSObjectCallAsFunction(context, callback, JSContextGetGlobalObject(context), 0, 0, 0);
    JSValueUnprotect(context, callback);
}

void LayoutTestController::addChromeInputField(JSValueRef callback)
{
    cacheLayoutTestControllerCallback(AddChromeInputFieldCallbackID, callback);
    InjectedBundle::shared().postAddChromeInputField();
}

void LayoutTestController::removeChromeInputField(JSValueRef callback)
{
    cacheLayoutTestControllerCallback(RemoveChromeInputFieldCallbackID, callback);
    InjectedBundle::shared().postRemoveChromeInputField();
}

void LayoutTestController::focusWebView(JSValueRef callback)
{
    cacheLayoutTestControllerCallback(FocusWebViewCallbackID, callback);
    InjectedBundle::shared().postFocusWebView();
}

void LayoutTestController::setBackingScaleFactor(double backingScaleFactor, JSValueRef callback)
{
    cacheLayoutTestControllerCallback(SetBackingScaleFactorCallbackID, callback);
    InjectedBundle::shared().postSetBackingScaleFactor(backingScaleFactor);
}

void LayoutTestController::setWindowIsKey(bool isKey)
{
    InjectedBundle::shared().postSetWindowIsKey(isKey);
}

void LayoutTestController::callAddChromeInputFieldCallback()
{
    callLayoutTestControllerCallback(AddChromeInputFieldCallbackID);
}

void LayoutTestController::callRemoveChromeInputFieldCallback()
{
    callLayoutTestControllerCallback(RemoveChromeInputFieldCallbackID);
}

void LayoutTestController::callFocusWebViewCallback()
{
    callLayoutTestControllerCallback(FocusWebViewCallbackID);
}

void LayoutTestController::callSetBackingScaleFactorCallback()
{
    callLayoutTestControllerCallback(SetBackingScaleFactorCallbackID);
}

} // namespace WTR
