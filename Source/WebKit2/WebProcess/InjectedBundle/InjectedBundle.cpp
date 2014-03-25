/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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
#include "InjectedBundle.h"

#include "APIArray.h"
#include "Arguments.h"
#include "InjectedBundleScriptWorld.h"
#include "InjectedBundleUserMessageCoders.h"
#include "LayerTreeHost.h"
#include "NotificationPermissionRequestManager.h"
#include "SessionTracker.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebApplicationCacheManager.h"
#include "WebConnectionToUIProcess.h"
#include "WebContextMessageKinds.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebDatabaseManager.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebPage.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSLock.h>
#include <WebCore/ApplicationCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/GCController.h>
#include <WebCore/GeolocationClient.h>
#include <WebCore/GeolocationController.h>
#include <WebCore/GeolocationPosition.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/JSNotification.h>
#include <WebCore/MainFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PrintContext.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceLoadScheduler.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/SessionID.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>

#if ENABLE(CSS_REGIONS) || ENABLE(CSS_COMPOSITING)
#include <WebCore/RuntimeEnabledFeatures.h>
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

using namespace WebCore;
using namespace JSC;

namespace WebKit {

InjectedBundle::InjectedBundle(const WebProcessCreationParameters& parameters)
    : m_path(parameters.injectedBundlePath)
    , m_platformBundle(0)
{
    platformInitialize(parameters);
}

InjectedBundle::~InjectedBundle()
{
}

void InjectedBundle::initializeClient(const WKBundleClientBase* client)
{
    m_client.initialize(client);
}

void InjectedBundle::postMessage(const String& messageName, API::Object* messageBody)
{
    auto encoder = std::make_unique<IPC::MessageEncoder>(WebContextLegacyMessages::messageReceiverName(), WebContextLegacyMessages::postMessageMessageName(), 0);
    encoder->encode(messageName);
    encoder->encode(InjectedBundleUserMessageEncoder(messageBody));

    WebProcess::shared().parentProcessConnection()->sendMessage(std::move(encoder));
}

void InjectedBundle::postSynchronousMessage(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    InjectedBundleUserMessageDecoder messageDecoder(returnData);

    uint64_t syncRequestID;
    std::unique_ptr<IPC::MessageEncoder> encoder = WebProcess::shared().parentProcessConnection()->createSyncMessageEncoder(WebContextLegacyMessages::messageReceiverName(), WebContextLegacyMessages::postSynchronousMessageMessageName(), 0, syncRequestID);
    encoder->encode(messageName);
    encoder->encode(InjectedBundleUserMessageEncoder(messageBody));

    std::unique_ptr<IPC::MessageDecoder> replyDecoder = WebProcess::shared().parentProcessConnection()->sendSyncMessage(syncRequestID, std::move(encoder), std::chrono::milliseconds::max());
    if (!replyDecoder || !replyDecoder->decode(messageDecoder)) {
        returnData = nullptr;
        return;
    }
}

WebConnection* InjectedBundle::webConnectionToUIProcess() const
{
    return WebProcess::shared().webConnectionToUIProcess();
}

void InjectedBundle::setShouldTrackVisitedLinks(bool shouldTrackVisitedLinks)
{
    WebProcess::shared().setShouldTrackVisitedLinks(shouldTrackVisitedLinks);
}

void InjectedBundle::setAlwaysAcceptCookies(bool accept)
{
    WebProcess::shared().supplement<WebCookieManager>()->setHTTPCookieAcceptPolicy(accept ? HTTPCookieAcceptPolicyAlways : HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain);
}

void InjectedBundle::removeAllVisitedLinks()
{
    PageGroup::removeAllVisitedLinks();
}

void InjectedBundle::setCacheModel(uint32_t cacheModel)
{
    WebProcess::shared().setCacheModel(cacheModel);
}

void InjectedBundle::overrideBoolPreferenceForTestRunner(WebPageGroupProxy* pageGroup, const String& preference, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();

    if (preference == "WebKitTabToLinksPreferenceKey") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::tabsToLinksKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(page)->setTabToLinksEnabled(enabled);
    }

    if (preference == "WebKit2AsynchronousPluginInitializationEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(page)->setAsynchronousPluginInitializationEnabled(enabled);
    }

    if (preference == "WebKit2AsynchronousPluginInitializationEnabledForAllPlugins") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledForAllPluginsKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(page)->setAsynchronousPluginInitializationEnabledForAllPlugins(enabled);
    }

    if (preference == "WebKit2ArtificialPluginInitializationDelayEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::artificialPluginInitializationDelayEnabledKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(page)->setArtificialPluginInitializationDelayEnabled(enabled);
    }

#if ENABLE(IMAGE_CONTROLS)
    if (preference == "WebKitImageControlsEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::imageControlsEnabledKey(), enabled);
        for (auto* page : pages)
            page->settings().setImageControlsEnabled(enabled);
        return;
    }
#endif

#if ENABLE(CSS_REGIONS)
    if (preference == "WebKitCSSRegionsEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setCSSRegionsEnabled(enabled);
#endif

#if ENABLE(CSS_COMPOSITING)
    if (preference == "WebKitCSSCompositingEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setCSSCompositingEnabled(enabled);
#endif

    // Map the names used in LayoutTests with the names used in WebCore::Settings and WebPreferencesStore.
#define FOR_EACH_OVERRIDE_BOOL_PREFERENCE(macro) \
    macro(WebKitAcceleratedCompositingEnabled, AcceleratedCompositingEnabled, acceleratedCompositingEnabled) \
    macro(WebKitCanvasUsesAcceleratedDrawing, CanvasUsesAcceleratedDrawing, canvasUsesAcceleratedDrawing) \
    macro(WebKitCSSGridLayoutEnabled, CSSGridLayoutEnabled, cssGridLayoutEnabled) \
    macro(WebKitFrameFlatteningEnabled, FrameFlatteningEnabled, frameFlatteningEnabled) \
    macro(WebKitJavaEnabled, JavaEnabled, javaEnabled) \
    macro(WebKitJavaScriptEnabled, ScriptEnabled, javaScriptEnabled) \
    macro(WebKitLoadSiteIconsKey, LoadsSiteIconsIgnoringImageLoadingSetting, loadsSiteIconsIgnoringImageLoadingPreference) \
    macro(WebKitOfflineWebApplicationCacheEnabled, OfflineWebApplicationCacheEnabled, offlineWebApplicationCacheEnabled) \
    macro(WebKitPageCacheSupportsPluginsPreferenceKey, PageCacheSupportsPlugins, pageCacheSupportsPlugins) \
    macro(WebKitPluginsEnabled, PluginsEnabled, pluginsEnabled) \
    macro(WebKitUsesPageCachePreferenceKey, UsesPageCache, usesPageCache) \
    macro(WebKitWebAudioEnabled, WebAudioEnabled, webAudioEnabled) \
    macro(WebKitWebGLEnabled, WebGLEnabled, webGLEnabled) \
    macro(WebKitXSSAuditorEnabled, XSSAuditorEnabled, xssAuditorEnabled) \
    macro(WebKitShouldRespectImageOrientation, ShouldRespectImageOrientation, shouldRespectImageOrientation) \
    macro(WebKitEnableCaretBrowsing, CaretBrowsingEnabled, caretBrowsingEnabled) \
    macro(WebKitDisplayImagesKey, LoadsImagesAutomatically, loadsImagesAutomatically) \
    macro(WebKitMediaStreamEnabled, MediaStreamEnabled, mediaStreamEnabled)

    if (preference == "WebKitAcceleratedCompositingEnabled")
        enabled = enabled && LayerTreeHost::supportsAcceleratedCompositing();

#define OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES(TestRunnerName, SettingsName, WebPreferencesName) \
    if (preference == #TestRunnerName) { \
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::WebPreferencesName##Key(), enabled); \
        for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter) \
            (*iter)->settings().set##SettingsName(enabled); \
        return; \
    }

    FOR_EACH_OVERRIDE_BOOL_PREFERENCE(OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES)

#if ENABLE(HIDDEN_PAGE_DOM_TIMER_THROTTLING)
    OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES(WebKitHiddenPageDOMTimerThrottlingEnabled, HiddenPageDOMTimerThrottlingEnabled, hiddenPageDOMTimerThrottlingEnabled)
#endif

#undef OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES
#undef FOR_EACH_OVERRIDE_BOOL_PREFERENCE
}

void InjectedBundle::overrideXSSAuditorEnabledForTestRunner(WebPageGroupProxy* pageGroup, bool enabled)
{
    // Override the preference for all future pages.
    WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::xssAuditorEnabledKey(), enabled);

    // Change the setting for existing ones.
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setXSSAuditorEnabled(enabled);
}

void InjectedBundle::setAllowUniversalAccessFromFileURLs(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setAllowUniversalAccessFromFileURLs(enabled);
}

void InjectedBundle::setAllowFileAccessFromFileURLs(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setAllowFileAccessFromFileURLs(enabled);
}

void InjectedBundle::setMinimumLogicalFontSize(WebPageGroupProxy* pageGroup, int size)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setMinimumLogicalFontSize(size);
}

void InjectedBundle::setFrameFlatteningEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setFrameFlatteningEnabled(enabled);
}

void InjectedBundle::setPluginsEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setPluginsEnabled(enabled);
}

void InjectedBundle::setJavaScriptCanAccessClipboard(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setJavaScriptCanAccessClipboard(enabled);
}

void InjectedBundle::setPrivateBrowsingEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    // FIXME (NetworkProcess): This test-only function doesn't work with NetworkProcess, <https://bugs.webkit.org/show_bug.cgi?id=115274>.
#if PLATFORM(COCOA) || USE(CFNETWORK) || USE(SOUP)
    if (enabled)
        WebFrameNetworkingContext::ensurePrivateBrowsingSession(SessionID::legacyPrivateSessionID());
    else
        SessionTracker::destroySession(SessionID::legacyPrivateSessionID());
#endif
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setPrivateBrowsingEnabled(enabled);
}

void InjectedBundle::setPopupBlockingEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator iter = pages.begin(); iter != end; ++iter)
        (*iter)->settings().setJavaScriptCanOpenWindowsAutomatically(!enabled);
}

void InjectedBundle::setAuthorAndUserStylesEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setAuthorAndUserStylesEnabled(enabled);
}

void InjectedBundle::setSpatialNavigationEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setSpatialNavigationEnabled(enabled);
}

void InjectedBundle::addOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessWhitelistEntry(*SecurityOrigin::createFromString(sourceOrigin), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void InjectedBundle::removeOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessWhitelistEntry(*SecurityOrigin::createFromString(sourceOrigin), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void InjectedBundle::resetOriginAccessWhitelists()
{
    SecurityPolicy::resetOriginAccessWhitelists();
}

void InjectedBundle::setAsynchronousSpellCheckingEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setAsynchronousSpellCheckingEnabled(enabled);
}

void InjectedBundle::clearAllDatabases()
{
#if ENABLE(SQL_DATABASE)
    WebProcess::shared().supplement<WebDatabaseManager>()->deleteAllDatabases();
#endif
}

void InjectedBundle::setDatabaseQuota(uint64_t quota)
{
#if ENABLE(SQL_DATABASE)
    // Historically, we've used the following (somewhat non-sensical) string
    // for the databaseIdentifier of local files.
    WebProcess::shared().supplement<WebDatabaseManager>()->setQuotaForOrigin("file__0", quota);
#else
    UNUSED_PARAM(quota);
#endif
}

void InjectedBundle::clearApplicationCache()
{
    WebProcess::shared().supplement<WebApplicationCacheManager>()->deleteAllEntries();
}

void InjectedBundle::clearApplicationCacheForOrigin(const String& originString)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(originString);
    ApplicationCache::deleteCacheForOrigin(origin.get());
}

void InjectedBundle::setAppCacheMaximumSize(uint64_t size)
{
    WebProcess::shared().supplement<WebApplicationCacheManager>()->setAppCacheMaximumSize(size);
}

uint64_t InjectedBundle::appCacheUsageForOrigin(const String& originString)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(originString);
    return ApplicationCache::diskUsageForOrigin(origin.get());
}

void InjectedBundle::setApplicationCacheOriginQuota(const String& originString, uint64_t bytes)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(originString);
    cacheStorage().storeUpdatedQuotaForOrigin(origin.get(), bytes);
}

void InjectedBundle::resetApplicationCacheOriginQuota(const String& originString)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(originString);
    cacheStorage().storeUpdatedQuotaForOrigin(origin.get(), cacheStorage().defaultOriginQuota());
}

PassRefPtr<API::Array> InjectedBundle::originsWithApplicationCache()
{
    HashSet<RefPtr<SecurityOrigin>> origins;
    cacheStorage().getOriginsWithCache(origins);

    Vector<RefPtr<API::Object>> originIdentifiers;
    originIdentifiers.reserveInitialCapacity(origins.size());

    for (const auto& origin : origins)
        originIdentifiers.uncheckedAppend(API::String::create(origin->databaseIdentifier()));

    return API::Array::create(std::move(originIdentifiers));
}

int InjectedBundle::numberOfPages(WebFrame* frame, double pageWidthInPixels, double pageHeightInPixels)
{
    Frame* coreFrame = frame ? frame->coreFrame() : 0;
    if (!coreFrame)
        return -1;
    if (!pageWidthInPixels)
        pageWidthInPixels = coreFrame->view()->width();
    if (!pageHeightInPixels)
        pageHeightInPixels = coreFrame->view()->height();

    return PrintContext::numberOfPages(coreFrame, FloatSize(pageWidthInPixels, pageHeightInPixels));
}

int InjectedBundle::pageNumberForElementById(WebFrame* frame, const String& id, double pageWidthInPixels, double pageHeightInPixels)
{
    Frame* coreFrame = frame ? frame->coreFrame() : 0;
    if (!coreFrame)
        return -1;

    Element* element = coreFrame->document()->getElementById(id);
    if (!element)
        return -1;

    if (!pageWidthInPixels)
        pageWidthInPixels = coreFrame->view()->width();
    if (!pageHeightInPixels)
        pageHeightInPixels = coreFrame->view()->height();

    return PrintContext::pageNumberForElement(element, FloatSize(pageWidthInPixels, pageHeightInPixels));
}

String InjectedBundle::pageSizeAndMarginsInPixels(WebFrame* frame, int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    Frame* coreFrame = frame ? frame->coreFrame() : 0;
    if (!coreFrame)
        return String();

    return PrintContext::pageSizeAndMarginsInPixels(coreFrame, pageIndex, width, height, marginTop, marginRight, marginBottom, marginLeft);
}

bool InjectedBundle::isPageBoxVisible(WebFrame* frame, int pageIndex)
{
    Frame* coreFrame = frame ? frame->coreFrame() : 0;
    if (!coreFrame)
        return false;

    return PrintContext::isPageBoxVisible(coreFrame, pageIndex);
}

bool InjectedBundle::isProcessingUserGesture()
{
    return ScriptController::processingUserGesture();
}

void InjectedBundle::addUserScript(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& source, const String& url, API::Array* whitelist, API::Array* blacklist, WebCore::UserScriptInjectionTime injectionTime, WebCore::UserContentInjectedFrames injectedFrames)
{
    // url is not from URL::string(), i.e. it has not already been parsed by URL, so we have to use the relative URL constructor for URL instead of the ParsedURLStringTag version.
    PageGroup::pageGroup(pageGroup->identifier())->addUserScriptToWorld(scriptWorld->coreWorld(), source, URL(URL(), url), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectionTime, injectedFrames);
}

void InjectedBundle::addUserStyleSheet(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& source, const String& url, API::Array* whitelist, API::Array* blacklist, WebCore::UserContentInjectedFrames injectedFrames)
{
    // url is not from URL::string(), i.e. it has not already been parsed by URL, so we have to use the relative URL constructor for URL instead of the ParsedURLStringTag version.
    PageGroup::pageGroup(pageGroup->identifier())->addUserStyleSheetToWorld(scriptWorld->coreWorld(), source, URL(URL(), url), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectedFrames);
}

void InjectedBundle::removeUserScript(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& url)
{
    // url is not from URL::string(), i.e. it has not already been parsed by URL, so we have to use the relative URL constructor for URL instead of the ParsedURLStringTag version.
    PageGroup::pageGroup(pageGroup->identifier())->removeUserScriptFromWorld(scriptWorld->coreWorld(), URL(URL(), url));
}

void InjectedBundle::removeUserStyleSheet(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& url)
{
    // url is not from URL::string(), i.e. it has not already been parsed by URL, so we have to use the relative URL constructor for URL instead of the ParsedURLStringTag version.
    PageGroup::pageGroup(pageGroup->identifier())->removeUserStyleSheetFromWorld(scriptWorld->coreWorld(), URL(URL(), url));
}

void InjectedBundle::removeUserScripts(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld)
{
    PageGroup::pageGroup(pageGroup->identifier())->removeUserScriptsFromWorld(scriptWorld->coreWorld());
}

void InjectedBundle::removeUserStyleSheets(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld)
{
    PageGroup::pageGroup(pageGroup->identifier())->removeUserStyleSheetsFromWorld(scriptWorld->coreWorld());
}

void InjectedBundle::removeAllUserContent(WebPageGroupProxy* pageGroup)
{
    PageGroup::pageGroup(pageGroup->identifier())->removeAllUserContent();
}

void InjectedBundle::garbageCollectJavaScriptObjects()
{
    gcController().garbageCollectNow();
}

void InjectedBundle::garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(bool waitUntilDone)
{
    gcController().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

size_t InjectedBundle::javaScriptObjectsCount()
{
    JSLockHolder lock(JSDOMWindow::commonVM());
    return JSDOMWindow::commonVM().heap.objectCount();
}

void InjectedBundle::reportException(JSContextRef context, JSValueRef exception)
{
    if (!context || !exception)
        return;

    JSC::ExecState* execState = toJS(context);
    JSLockHolder lock(execState);

    // Make sure the context has a DOMWindow global object, otherwise this context didn't originate from a Page.
    if (!toJSDOMWindow(execState->lexicalGlobalObject()))
        return;

    WebCore::reportException(execState, toJS(execState, exception));
}

void InjectedBundle::didCreatePage(WebPage* page)
{
    m_client.didCreatePage(this, page);
}

void InjectedBundle::willDestroyPage(WebPage* page)
{
    m_client.willDestroyPage(this, page);
}

void InjectedBundle::didInitializePageGroup(WebPageGroupProxy* pageGroup)
{
    m_client.didInitializePageGroup(this, pageGroup);
}

void InjectedBundle::didReceiveMessage(const String& messageName, API::Object* messageBody)
{
    m_client.didReceiveMessage(this, messageName, messageBody);
}

void InjectedBundle::didReceiveMessageToPage(WebPage* page, const String& messageName, API::Object* messageBody)
{
    m_client.didReceiveMessageToPage(this, page, messageName, messageBody);
}

void InjectedBundle::setUserStyleSheetLocation(WebPageGroupProxy* pageGroup, const String& location)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setUserStyleSheetLocation(URL(URL(), location));
}

void InjectedBundle::setWebNotificationPermission(WebPage* page, const String& originString, bool allowed)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    page->notificationPermissionRequestManager()->setPermissionLevelForTesting(originString, allowed);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(originString);
    UNUSED_PARAM(allowed);
#endif
}

void InjectedBundle::removeAllWebNotificationPermissions(WebPage* page)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    page->notificationPermissionRequestManager()->removeAllPermissionsForTesting();
#else
    UNUSED_PARAM(page);
#endif
}

uint64_t InjectedBundle::webNotificationID(JSContextRef jsContext, JSValueRef jsNotification)
{
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::Notification* notification = toNotification(toJS(toJS(jsContext), jsNotification));
    if (!notification)
        return 0;
    return WebProcess::shared().supplement<WebNotificationManager>()->notificationIDForTesting(notification);
#else
    UNUSED_PARAM(jsContext);
    UNUSED_PARAM(jsNotification);
    return 0;
#endif
}

// FIXME Get rid of this function and move it into WKBundle.cpp.
PassRefPtr<API::Data> InjectedBundle::createWebDataFromUint8Array(JSContextRef context, JSValueRef data)
{
    JSC::ExecState* execState = toJS(context);
    RefPtr<Uint8Array> arrayData = WebCore::toUint8Array(toJS(execState, data));
    return API::Data::create(static_cast<unsigned char*>(arrayData->baseAddress()), arrayData->byteLength());
}

void InjectedBundle::setTabKeyCyclesThroughElements(WebPage* page, bool enabled)
{
    page->corePage()->setTabKeyCyclesThroughElements(enabled);
}

void InjectedBundle::setSerialLoadingEnabled(bool enabled)
{
    resourceLoadScheduler()->setSerialLoadingEnabled(enabled);
}

void InjectedBundle::setCSSRegionsEnabled(bool enabled)
{
#if ENABLE(CSS_REGIONS)
    RuntimeEnabledFeatures::sharedFeatures().setCSSRegionsEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void InjectedBundle::setCSSCompositingEnabled(bool enabled)
{
#if ENABLE(CSS_COMPOSITING)
    RuntimeEnabledFeatures::sharedFeatures().setCSSCompositingEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void InjectedBundle::dispatchPendingLoadRequests()
{
    resourceLoadScheduler()->servePendingRequests();
}

} // namespace WebKit
