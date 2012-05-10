/*
 * Copyright (C) 2009, 2010, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "LayoutTestController.h"

#include "CString.h"
#include "DatabaseTracker.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentMarker.h"
#include "DumpRenderTree.h"
#include "DumpRenderTreeBlackBerry.h"
#include "DumpRenderTreeSupport.h"
#include "EditingBehaviorTypes.h"
#include "EditorClientBlackBerry.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "JSElement.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "OwnArrayPtr.h"
#include "Page.h"
#include "RenderTreeAsText.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "UnusedParam.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include "WorkerThread.h"

#include <JavaScriptCore/APICast.h>
#include <SharedPointer.h>
#include <WebPage.h>
#include <WebSettings.h>

using WebCore::toElement;
using WebCore::toJS;

LayoutTestController::~LayoutTestController()
{
}

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    UNUSED_PARAM(url);
    notImplemented();
}

void LayoutTestController::clearAllDatabases()
{
#if ENABLE(DATABASE)
    WebCore::DatabaseTracker::tracker().deleteAllDatabases();
#endif
}

void LayoutTestController::clearBackForwardList()
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->clearBackForwardList(true);
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    notImplemented();
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    UNUSED_PARAM(name);
    notImplemented();
    return 0;
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    UNUSED_PARAM(name);
    notImplemented();
    return 0;
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    notImplemented();
}

void LayoutTestController::display()
{
    notImplemented();
}

static String jsStringRefToWebCoreString(JSStringRef str)
{
    size_t strArrSize = JSStringGetMaximumUTF8CStringSize(str);
    OwnArrayPtr<char> strArr = adoptArrayPtr(new char[strArrSize]);
    JSStringGetUTF8CString(str, strArr.get(), strArrSize);
    return String::fromUTF8(strArr.get());
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    if (!mainFrame)
        return;

    String nameStr = jsStringRefToWebCoreString(name);
    String valueStr = jsStringRefToWebCoreString(value);

    mainFrame->editor()->command(nameStr).execute(valueStr);
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    if (!mainFrame)
        return false;

    String nameStr = jsStringRefToWebCoreString(name);

    return mainFrame->editor()->command(nameStr).isEnabled();
}

void LayoutTestController::keepWebHistory()
{
    notImplemented();
}

void LayoutTestController::notifyDone()
{
    if (m_waitToDump && (!topLoadingFrame || BlackBerry::WebKit::DumpRenderTree::currentInstance()->loadFinished()) && !WorkQueue::shared()->count())
        dump();

    m_waitToDump = false;
    waitForPolicy = false;
}

JSStringRef LayoutTestController::pathToLocalResource(JSContextRef, JSStringRef url)
{
    return JSStringRetain(url);
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    size_t urlArrSize = JSStringGetMaximumUTF8CStringSize(url);
    OwnArrayPtr<char> urlArr = adoptArrayPtr(new char[urlArrSize]);
    JSStringGetUTF8CString(url, urlArr.get(), urlArrSize);

    WebCore::KURL base = mainFrame->loader()->documentLoader()->response().url();
    WebCore::KURL absolute(base, urlArr.get());

    JSRetainPtr<JSStringRef> absoluteURL(Adopt, JSStringCreateWithUTF8CString(absolute.string().utf8().data()));
    WorkQueue::shared()->queue(new LoadItem(absoluteURL.get(), target));
}

void LayoutTestController::setAcceptsEditing(bool acceptsEditing)
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->setAcceptsEditing(acceptsEditing);
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long quota)
{
    UNUSED_PARAM(quota);
    notImplemented();
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool enable)
{
    mainFrame->page()->settings()->setAuthorAndUserStylesEnabled(enable);
}

void LayoutTestController::setCacheModel(int)
{
    notImplemented();
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    UNUSED_PARAM(setDelegate);
    UNUSED_PARAM(permissive);
    notImplemented();
}

void LayoutTestController::clearApplicationCacheForOrigin(OpaqueJSString*)
{
    // FIXME: Implement to support deleting all application caches for an origin.
    notImplemented();
}

long long LayoutTestController::localStorageDiskUsageForOrigin(JSStringRef)
{
    // FIXME: Implement to support getting disk usage in bytes for an origin.
    notImplemented();
    return 0;
}

JSValueRef LayoutTestController::originsWithApplicationCache(JSContextRef context)
{
    // FIXME: Implement to get origins that contain application caches.
    notImplemented();
    return JSValueMakeUndefined(context);
}

void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{
    if (!mainFrame)
        return;

    WebCore::DatabaseTracker::tracker().setQuota(mainFrame->document()->securityOrigin(), quota);
}

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(bool forbidden, JSStringRef scheme)
{
    WebCore::SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, jsStringRefToWebCoreString(scheme));
}

void LayoutTestController::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    UNUSED_PARAM(iconDatabaseEnabled);
    notImplemented();
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool profilingEnabled)
{
    UNUSED_PARAM(profilingEnabled);
    notImplemented();
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    UNUSED_PARAM(flag);
    notImplemented();
}

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef path)
{
    UNUSED_PARAM(path);
    notImplemented();
}

void LayoutTestController::setPopupBlockingEnabled(bool flag)
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->settings()->setJavaScriptOpenWindowsAutomatically(!flag);
}

void LayoutTestController::setPrivateBrowsingEnabled(bool flag)
{
    UNUSED_PARAM(flag);
    notImplemented();
}

void LayoutTestController::setXSSAuditorEnabled(bool flag)
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->settings()->setXSSAuditorEnabled(flag);
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->setSelectTrailingWhitespaceEnabled(flag);
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    UNUSED_PARAM(flag);
    notImplemented();
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    if (!mainFrame)
        return;

    mainFrame->page()->setTabKeyCyclesThroughElements(cycles);
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    UNUSED_PARAM(flag);
    notImplemented();
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    UNUSED_PARAM(flag);
    notImplemented();
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    String pathStr = jsStringRefToWebCoreString(path);
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->settings()->setUserStyleSheetLocation(pathStr.utf8().data());
}

void LayoutTestController::waitForPolicyDelegate()
{
    setWaitToDump(true);
    waitForPolicy = true;
}

size_t LayoutTestController::webHistoryItemCount()
{
    SharedArray<BlackBerry::WebKit::WebPage::BackForwardEntry> backForwardList;
    unsigned size;
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->getBackForwardList(backForwardList, size);
    return size;
}

int LayoutTestController::windowCount()
{
    notImplemented();
    return 0;
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef id)
{
    UNUSED_PARAM(id);
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> LayoutTestController::pageProperty(const char* propertyName, int pageNumber) const
{
    UNUSED_PARAM(propertyName);
    UNUSED_PARAM(pageNumber);
    notImplemented();
    return 0;
}

void LayoutTestController::setWaitToDump(bool waitToDump)
{
    // Change from 30s to 35s because some test cases in multipart need 30 seconds,
    // refer to http/tests/multipart/resources/multipart-wait-before-boundary.php please.
    static const double kWaitToDumpWatchdogInterval = 35.0;
    m_waitToDump = waitToDump;
    if (m_waitToDump)
        BlackBerry::WebKit::DumpRenderTree::currentInstance()->setWaitToDumpWatchdog(kWaitToDumpWatchdogInterval);
}

void LayoutTestController::setWindowIsKey(bool windowIsKey)
{
    m_windowIsKey = windowIsKey;
    notImplemented();
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    if (!mainFrame)
        return false;

    int nameLen = JSStringGetMaximumUTF8CStringSize(animationName);
    int idLen = JSStringGetMaximumUTF8CStringSize(elementId);
    OwnArrayPtr<char> name = adoptArrayPtr(new char[nameLen]);
    OwnArrayPtr<char> eId = adoptArrayPtr(new char[idLen]);

    JSStringGetUTF8CString(animationName, name.get(), nameLen);
    JSStringGetUTF8CString(elementId, eId.get(), idLen);

    WebCore::AnimationController* animationController = mainFrame->animation();
    if (!animationController)
        return false;

    WebCore::Node* node = mainFrame->document()->getElementById(eId.get());
    if (!node || !node->renderer())
        return false;

    return animationController->pauseAnimationAtTime(node->renderer(), name.get(), time);
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    if (!mainFrame)
        return false;

    int nameLen = JSStringGetMaximumUTF8CStringSize(propertyName);
    int idLen = JSStringGetMaximumUTF8CStringSize(elementId);
    OwnArrayPtr<char> name = adoptArrayPtr(new char[nameLen]);
    OwnArrayPtr<char> eId = adoptArrayPtr(new char[idLen]);

    JSStringGetUTF8CString(propertyName, name.get(), nameLen);
    JSStringGetUTF8CString(elementId, eId.get(), idLen);

    WebCore::AnimationController* animationController = mainFrame->animation();
    if (!animationController)
        return false;

    WebCore::Node* node = mainFrame->document()->getElementById(eId.get());
    if (!node || !node->renderer())
        return false;

    return animationController->pauseTransitionAtTime(node->renderer(), name.get(), time);
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    if (!mainFrame)
        return false;

    WebCore::AnimationController* animationController = mainFrame->animation();
    if (!animationController)
        return false;

    return animationController->numberOfActiveAnimations(mainFrame->document());
}

unsigned int LayoutTestController::workerThreadCount() const
{
#if ENABLE_WORKERS
    return WebCore::WorkerThread::workerThreadCount();
#else
    return 0;
#endif
}

void LayoutTestController::removeAllVisitedLinks()
{
    notImplemented();
}

void LayoutTestController::disableImageLoading()
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->settings()->setLoadsImagesAutomatically(false);
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef id)
{
    String idStr = jsStringRefToWebCoreString(id);
    WebCore::Element* coreElement = mainFrame->document()->getElementById(AtomicString(idStr));
    if (!coreElement)
        return 0;

    CString counterValueStr = counterValueForElement(coreElement).utf8();
    if (counterValueStr.isNull())
        return 0;

    JSRetainPtr<JSStringRef> counterValue(Adopt, JSStringCreateWithUTF8CString(counterValueStr.data()));
    return counterValue;
}

void LayoutTestController::overridePreference(JSStringRef key, JSStringRef value)
{
    if (!mainFrame)
        return;

    String keyStr = jsStringRefToWebCoreString(key);
    String valueStr = jsStringRefToWebCoreString(value);

    if (keyStr == "WebKitUsesPageCachePreferenceKey")
        BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->settings()->setMaximumPagesInCache(1);
    else if (keyStr == "WebKitUsePreHTML5ParserQuirks")
        mainFrame->page()->settings()->setUsePreHTML5ParserQuirks(true);
    else if (keyStr == "WebKitTabToLinksPreferenceKey")
        DumpRenderTreeSupport::setLinksIncludedInFocusChain(valueStr == "true" || valueStr == "1");
    else if (keyStr == "WebKitHyperlinkAuditingEnabled")
        mainFrame->page()->settings()->setHyperlinkAuditingEnabled(valueStr == "true" || valueStr == "1");
}

void LayoutTestController::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    UNUSED_PARAM(alwaysAcceptCookies);
    notImplemented();
}

void LayoutTestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    DumpRenderTreeSupport::setMockGeolocationPosition(BlackBerry::WebKit::DumpRenderTree::currentInstance()->page(), latitude, longitude, accuracy);
}

void LayoutTestController::setMockGeolocationError(int code, JSStringRef message)
{
    String messageStr = jsStringRefToWebCoreString(message);
    DumpRenderTreeSupport::setMockGeolocationError(BlackBerry::WebKit::DumpRenderTree::currentInstance()->page(), code, messageStr);
}

void LayoutTestController::showWebInspector()
{
    notImplemented();
}

void LayoutTestController::closeWebInspector()
{
    notImplemented();
}

void LayoutTestController::evaluateInWebInspector(long callId, JSStringRef script)
{
    UNUSED_PARAM(callId);
    UNUSED_PARAM(script);
    notImplemented();
}

void LayoutTestController::evaluateScriptInIsolatedWorldAndReturnValue(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    UNUSED_PARAM(worldID);
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(script);
    notImplemented();
}

void LayoutTestController::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    UNUSED_PARAM(worldID);
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(script);
    notImplemented();
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(runAtStart);
    UNUSED_PARAM(allFrames);
    notImplemented();
}

void LayoutTestController::addUserStyleSheet(JSStringRef, bool)
{
    notImplemented();
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int, int, int, int, int, int, int) const
{
    notImplemented();
    return 0;
}

int LayoutTestController::pageNumberForElementById(JSStringRef, float, float)
{
    notImplemented();
    return -1;
}

int LayoutTestController::numberOfPages(float, float)
{
    notImplemented();
    return -1;
}

void LayoutTestController::setScrollbarPolicy(JSStringRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::setWebViewEditable(bool)
{
    notImplemented();
}

void LayoutTestController::authenticateSession(JSStringRef url, JSStringRef username, JSStringRef password)
{
    notImplemented();
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    notImplemented();
    return false;
}

void LayoutTestController::setFrameFlatteningEnabled(bool enable)
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->settings()->setFrameFlatteningEnabled(enable);
}

void LayoutTestController::setSpatialNavigationEnabled(bool enable)
{
    notImplemented();
}

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WebCore::SecurityPolicy::addOriginAccessWhitelistEntry(*WebCore::SecurityOrigin::createFromString(jsStringRefToWebCoreString(sourceOrigin)),
                                                  jsStringRefToWebCoreString(destinationProtocol),
                                                  jsStringRefToWebCoreString(destinationHost),
                                                  allowDestinationSubdomains);
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    WebCore::SecurityPolicy::removeOriginAccessWhitelistEntry(*WebCore::SecurityOrigin::createFromString(jsStringRefToWebCoreString(sourceOrigin)),
                                                     jsStringRefToWebCoreString(destinationProtocol),
                                                     jsStringRefToWebCoreString(destinationHost),
                                                     allowDestinationSubdomains);
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool enabled)
{
    if (!mainFrame)
        return;

    mainFrame->page()->settings()->setAllowFileAccessFromFileURLs(enabled);
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    if (!mainFrame)
        return;

    mainFrame->page()->settings()->setAllowUniversalAccessFromFileURLs(enabled);
}

void LayoutTestController::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{
    notImplemented();
}

void LayoutTestController::apiTestGoToCurrentBackForwardItem()
{
    notImplemented();
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool flag)
{
    BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->setJavaScriptCanAccessClipboard(flag);
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSContextRef context, JSValueRef value)
{
    return DumpRenderTreeSupport::computedStyleIncludingVisitedInfo(context, value);
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSContextRef context, JSValueRef nodeObject) const
{
    WebCore::Element* element = toElement(toJS(toJS(context), nodeObject));
    if (!element)
        return 0;

    JSRetainPtr<JSStringRef> markerText(Adopt, JSStringCreateWithUTF8CString(WebCore::markerTextForListItem(element).utf8().data()));
    return markerText;
}

void LayoutTestController::setPluginsEnabled(bool flag)
{
    notImplemented();
}

void LayoutTestController::setEditingBehavior(const char* editingBehavior)
{
    if (!mainFrame)
        return;
    WebCore::EditingBehaviorType type = WebCore::EditingUnixBehavior;
    if (!strcmp(editingBehavior, "win"))
        type = WebCore::EditingWindowsBehavior;
    else if (!strcmp(editingBehavior, "mac"))
        type = WebCore::EditingMacBehavior;
    else if (!strcmp(editingBehavior, "unix"))
        type = WebCore::EditingUnixBehavior;
    else
        CRASH();
    mainFrame->page()->settings()->setEditingBehaviorType(type);
}

void LayoutTestController::abortModal()
{
    notImplemented();
}

void LayoutTestController::clearAllApplicationCaches()
{
    notImplemented();
}

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long quota)
{
    notImplemented();
}

void LayoutTestController::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    notImplemented();
}

void LayoutTestController::addMockSpeechInputResult(JSStringRef result, double confidence, JSStringRef language)
{
    notImplemented();
}

void LayoutTestController::setGeolocationPermission(bool allow)
{
    setGeolocationPermissionCommon(allow);
    DumpRenderTreeSupport::setMockGeolocationPermission(BlackBerry::WebKit::DumpRenderTree::currentInstance()->page(), allow);
}

void LayoutTestController::setViewModeMediaFeature(const JSStringRef mode)
{
    notImplemented();
}

void LayoutTestController::resumeAnimations() const
{
    if (mainFrame && mainFrame->animation())
        mainFrame->animation()->resumeAnimations();
}

void LayoutTestController::setSerializeHTTPLoads(bool)
{
    // FIXME: Implement if needed for https://bugs.webkit.org/show_bug.cgi?id=50758.
    notImplemented();
}

void LayoutTestController::setMinimumTimerInterval(double)
{
    notImplemented();
}

void LayoutTestController::setTextDirection(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::allowRoundingHacks()
{
    notImplemented();
}

void LayoutTestController::goBack()
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
    notImplemented();
}

void LayoutTestController::setDefersLoading(bool)
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
    notImplemented();
}

JSValueRef LayoutTestController::originsWithLocalStorage(JSContextRef context)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

void LayoutTestController::observeStorageTrackerNotifications(unsigned)
{
    notImplemented();
}

void LayoutTestController::syncLocalStorage()
{
    notImplemented();
}

void LayoutTestController::deleteAllLocalStorage()
{
    notImplemented();
}

void LayoutTestController::setAsynchronousSpellCheckingEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::setAutofilled(JSContextRef context, JSValueRef nodeObject, bool autofilled)
{
    JSC::ExecState* exec = toJS(context);
    WebCore::Element* element = toElement(toJS(exec, nodeObject));
    if (!element)
        return;
    WebCore::HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;

    inputElement->setAutofilled(autofilled);
}

int LayoutTestController::numberOfPendingGeolocationPermissionRequests()
{
    return DumpRenderTreeSupport::numberOfPendingGeolocationPermissionRequests(BlackBerry::WebKit::DumpRenderTree::currentInstance()->page());
}

void LayoutTestController::dumpConfigurationForViewport(int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    if (!mainFrame)
        return;

    DumpRenderTreeSupport::dumpConfigurationForViewport(mainFrame, deviceDPI, deviceWidth, deviceHeight, availableWidth, availableHeight);
}

bool LayoutTestController::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    WebCore::FindOptions options = 0;

    String nameStr = jsStringRefToWebCoreString(target);

    JSRetainPtr<JSStringRef> lengthPropertyName(Adopt, JSStringCreateWithUTF8CString("length"));
    size_t length = 0;
    if (optionsArray) {
        JSValueRef lengthValue = JSObjectGetProperty(context, optionsArray, lengthPropertyName.get(), 0);
        if (!JSValueIsNumber(context, lengthValue))
            return false;
        length = static_cast<size_t>(JSValueToNumber(context, lengthValue, 0));
    }

    for (size_t i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, 0);
        if (!JSValueIsString(context, value))
            continue;

        JSRetainPtr<JSStringRef> optionName(Adopt, JSValueToStringCopy(context, value, 0));

        if (JSStringIsEqualToUTF8CString(optionName.get(), "CaseInsensitive"))
            options |= WebCore::CaseInsensitive;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "AtWordStarts"))
            options |= WebCore::AtWordStarts;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "TreatMedialCapitalAsWordStart"))
            options |= WebCore::TreatMedialCapitalAsWordStart;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "Backwards"))
            options |= WebCore::Backwards;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "WrapAround"))
            options |= WebCore::WrapAround;
        else if (JSStringIsEqualToUTF8CString(optionName.get(), "StartInSelection"))
            options |= WebCore::StartInSelection;
    }

    // Our layout tests assume find will wrap and highlight all matches.
    return BlackBerry::WebKit::DumpRenderTree::currentInstance()->page()->findNextString(nameStr.utf8().data(),
        !(options & WebCore::Backwards), !(options & WebCore::CaseInsensitive), true /* wrap */, true /* highlightAllMatches */);
}

void LayoutTestController::deleteLocalStorageForOrigin(JSStringRef URL)
{
    // FIXME: Implement.
}

void LayoutTestController::setValueForUser(JSContextRef context, JSValueRef nodeObject, JSStringRef value)
{
    JSC::ExecState* exec = toJS(context);
    WebCore::Element* element = toElement(toJS(exec, nodeObject));
    if (!element)
        return;
    WebCore::HTMLInputElement* inputElement = element->toInputElement();
    if (!inputElement)
        return;

    inputElement->setValueForUser(jsStringRefToWebCoreString(value));
}

long long LayoutTestController::applicationCacheDiskUsageForOrigin(JSStringRef origin)
{
    // FIXME: Implement to support getting disk usage by all application caches for an origin.
    return 0;
}

void LayoutTestController::addChromeInputField()
{
}

void LayoutTestController::removeChromeInputField()
{
}

void LayoutTestController::focusWebView()
{
}

void LayoutTestController::setBackingScaleFactor(double)
{
}

void LayoutTestController::setMockSpeechInputDumpRect(bool)
{
}

void LayoutTestController::simulateDesktopNotificationClick(JSStringRef title)
{
}

void LayoutTestController::resetPageVisibility()
{
    notImplemented();
}

void LayoutTestController::setPageVisibility(const char*)
{
    notImplemented();
}
