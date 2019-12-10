/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#include "APIData.h"
#include "InjectedBundleScriptWorld.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkSessionCreationParameters.h"
#include "NotificationPermissionRequestManager.h"
#include "UserData.h"
#include "WebConnectionToUIProcess.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebPreferencesKeys.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebProcessPoolMessages.h"
#include "WebStorageNamespaceProvider.h"
#include "WebUserContentController.h"
#include "WebsiteDataStoreParameters.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <WebCore/ApplicationCache.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/CommonVM.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/GCController.h>
#include <WebCore/GeolocationClient.h>
#include <WebCore/GeolocationController.h>
#include <WebCore/GeolocationPositionData.h>
#include <WebCore/JSDOMConvertBufferSource.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/JSNotification.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PrintContext.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/SWContextManager.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <wtf/ProcessPrivilege.h>

#if ENABLE(NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

namespace WebKit {
using namespace WebCore;
using namespace JSC;

RefPtr<InjectedBundle> InjectedBundle::create(WebProcessCreationParameters& parameters, API::Object* initializationUserData)
{
    auto bundle = adoptRef(*new InjectedBundle(parameters));

    bundle->m_sandboxExtension = SandboxExtension::create(WTFMove(parameters.injectedBundlePathExtensionHandle));
    if (!bundle->initialize(parameters, initializationUserData))
        return nullptr;

    return bundle;
}

InjectedBundle::InjectedBundle(const WebProcessCreationParameters& parameters)
    : m_path(parameters.injectedBundlePath)
    , m_platformBundle(0)
    , m_client(makeUnique<API::InjectedBundle::Client>())
{
}

InjectedBundle::~InjectedBundle()
{
}

void InjectedBundle::setClient(std::unique_ptr<API::InjectedBundle::Client>&& client)
{
    if (!client)
        m_client = makeUnique<API::InjectedBundle::Client>();
    else
        m_client = WTFMove(client);
}

void InjectedBundle::setServiceWorkerProxyCreationCallback(void (*callback)(uint64_t))
{
#if ENABLE(SERVICE_WORKER)
    SWContextManager::singleton().setServiceWorkerCreationCallback(callback);
#endif
}

void InjectedBundle::postMessage(const String& messageName, API::Object* messageBody)
{
    auto& webProcess = WebProcess::singleton();
    webProcess.parentProcessConnection()->send(Messages::WebProcessPool::HandleMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))), 0);
}

void InjectedBundle::postSynchronousMessage(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData)
{
    UserData returnUserData;

    auto& webProcess = WebProcess::singleton();
    if (!webProcess.parentProcessConnection()->sendSync(Messages::WebProcessPool::HandleSynchronousMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))),
        Messages::WebProcessPool::HandleSynchronousMessage::Reply(returnUserData), 0))
        returnData = nullptr;
    else
        returnData = webProcess.transformHandlesToObjects(returnUserData.object());
}

WebConnection* InjectedBundle::webConnectionToUIProcess() const
{
    return WebProcess::singleton().webConnectionToUIProcess();
}

void InjectedBundle::overrideBoolPreferenceForTestRunner(WebPageGroupProxy* pageGroup, const String& preference, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();

    if (preference == "WebKitTabToLinksPreferenceKey") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::tabsToLinksKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(*page).setTabToLinksEnabled(enabled);
    }

    if (preference == "WebKit2AsynchronousPluginInitializationEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(*page).setAsynchronousPluginInitializationEnabled(enabled);
    }

    if (preference == "WebKit2AsynchronousPluginInitializationEnabledForAllPlugins") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::asynchronousPluginInitializationEnabledForAllPluginsKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(*page).setAsynchronousPluginInitializationEnabledForAllPlugins(enabled);
    }

    if (preference == "WebKit2ArtificialPluginInitializationDelayEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::artificialPluginInitializationDelayEnabledKey(), enabled);
        for (auto* page : pages)
            WebPage::fromCorePage(*page).setArtificialPluginInitializationDelayEnabled(enabled);
    }

#if ENABLE(SERVICE_CONTROLS)
    if (preference == "WebKitImageControlsEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::imageControlsEnabledKey(), enabled);
        for (auto* page : pages)
            page->settings().setImageControlsEnabled(enabled);
        return;
    }
#endif

    if (preference == "WebKitWebAnimationsEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setWebAnimationsEnabled(enabled);

    if (preference == "WebKitWebAnimationsCSSIntegrationEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setWebAnimationsCSSIntegrationEnabled(enabled);

    if (preference == "WebKitCacheAPIEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setCacheAPIEnabled(enabled);

#if ENABLE(STREAMS_API)
    if (preference == "WebKitReadableByteStreamAPIEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setReadableByteStreamAPIEnabled(enabled);
    if (preference == "WebKitWritableStreamAPIEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setWritableStreamAPIEnabled(enabled);
#endif

    if (preference == "WebKitInteractiveFormValidationEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setInteractiveFormValidationEnabled(enabled);

#if ENABLE(WEBGL2)
    if (preference == "WebKitWebGL2Enabled")
        RuntimeEnabledFeatures::sharedFeatures().setWebGL2Enabled(enabled);
#endif

#if ENABLE(WEBGPU)
    if (preference == "WebKitWebGPUEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setWebGPUEnabled(enabled);
#endif

    if (preference == "WebKitModernMediaControlsEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setModernMediaControlsEnabled(enabled);

#if ENABLE(ENCRYPTED_MEDIA)
    if (preference == "WebKitEncryptedMediaAPIEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::encryptedMediaAPIEnabledKey(), enabled);
        RuntimeEnabledFeatures::sharedFeatures().setEncryptedMediaAPIEnabled(enabled);
    }
#endif

#if ENABLE(MEDIA_STREAM)
    if (preference == "WebKitMediaDevicesEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setMediaDevicesEnabled(enabled);
    if (preference == "WebKitScreenCaptureEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setScreenCaptureEnabled(enabled);
#endif

#if ENABLE(WEB_RTC)
    if (preference == "WebKitWebRTCMDNSICECandidatesEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setWebRTCMDNSICECandidatesEnabled(enabled);
#endif

#if ENABLE(VIDEO_TRACK)
    if (preference == "WebKitGenericCueAPIEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::genericCueAPIEnabledKey(), enabled);
        for (auto* page : pages)
            page->settings().setGenericCueAPIEnabled(enabled);
        return;
    }
#endif

#if ENABLE(GPU_PROCESS)
    if (preference == "WebKitUseGPUProcessForMedia" || preference == "WebKitCaptureAudioInGPUProcessEnabledKey") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::useGPUProcessForMediaKey(), enabled);
        for (auto* page : pages)
            page->settings().setUseGPUProcessForMedia(enabled);
        return;
    }
#endif

    if (preference == "WebKitIsSecureContextAttributeEnabled") {
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::isSecureContextAttributeEnabledKey(), enabled);
        RuntimeEnabledFeatures::sharedFeatures().setIsSecureContextAttributeEnabled(enabled);
    }

    if (preference == "WebKitWebAPIStatisticsEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setWebAPIStatisticsEnabled(enabled);

    if (preference == "CSSCustomPropertiesAndValuesEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setCSSCustomPropertiesAndValuesEnabled(enabled);

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (preference == "LayoutFormattingContextEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setLayoutFormattingContextEnabled(enabled);
    if (preference == "LayoutFormattingContextIntegrationEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setLayoutFormattingContextIntegrationEnabled(enabled);
#endif

#if ENABLE(CSS_PAINTING_API)
    if (preference == "CSSPaintingAPIEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setCSSPaintingAPIEnabled(enabled);
#endif

#if ENABLE(CSS_TYPED_OM)
    if (preference == "CSSTypedOMEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setCSSTypedOMEnabled(enabled);
#endif

#if ENABLE(OFFSCREEN_CANVAS)
    if (preference == "OffscreenCanvasEnabled")
        RuntimeEnabledFeatures::sharedFeatures().setOffscreenCanvasEnabled(enabled);
#endif

    // Map the names used in LayoutTests with the names used in WebCore::Settings and WebPreferencesStore.
#define FOR_EACH_OVERRIDE_BOOL_PREFERENCE(macro) \
    macro(WebKitJavaEnabled, JavaEnabled, javaEnabled) \
    macro(WebKitJavaScriptEnabled, ScriptEnabled, javaScriptEnabled) \
    macro(WebKitPluginsEnabled, PluginsEnabled, pluginsEnabled) \
    macro(WebKitWebAudioEnabled, WebAudioEnabled, webAudioEnabled) \
    macro(WebKitWebGLEnabled, WebGLEnabled, webGLEnabled) \
    macro(WebKitXSSAuditorEnabled, XSSAuditorEnabled, xssAuditorEnabled) \
    macro(WebKitShouldRespectImageOrientation, ShouldRespectImageOrientation, shouldRespectImageOrientation) \
    macro(WebKitDisplayImagesKey, LoadsImagesAutomatically, loadsImagesAutomatically) \
    macro(WebKitLargeImageAsyncDecodingEnabled, LargeImageAsyncDecodingEnabled, largeImageAsyncDecodingEnabled) \
    macro(WebKitAnimatedImageAsyncDecodingEnabled, AnimatedImageAsyncDecodingEnabled, animatedImageAsyncDecodingEnabled) \
    \

#define OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES(TestRunnerName, SettingsName, WebPreferencesName) \
    if (preference == #TestRunnerName) { \
        WebPreferencesStore::overrideBoolValueForKey(WebPreferencesKey::WebPreferencesName##Key(), enabled); \
        for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter) \
            (*iter)->settings().set##SettingsName(enabled); \
        return; \
    }

    FOR_EACH_OVERRIDE_BOOL_PREFERENCE(OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES)

    OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES(WebKitHiddenPageDOMTimerThrottlingEnabled, HiddenPageDOMTimerThrottlingEnabled, hiddenPageDOMTimerThrottlingEnabled)

#undef OVERRIDE_PREFERENCE_AND_SET_IN_EXISTING_PAGES
#undef FOR_EACH_OVERRIDE_BOOL_PREFERENCE
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

void InjectedBundle::setNeedsStorageAccessFromFileURLsQuirk(WebPageGroupProxy* pageGroup, bool needsQuirk)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (auto page : pages)
        page->settings().setNeedsStorageAccessFromFileURLsQuirk(needsQuirk);
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
        (*iter)->settings().setFrameFlattening(enabled ? FrameFlattening::FullyEnabled : FrameFlattening::Disabled);
}

void InjectedBundle::setAsyncFrameScrollingEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setAsyncFrameScrollingEnabled(enabled);
}

void InjectedBundle::setJavaScriptCanAccessClipboard(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setJavaScriptCanAccessClipboard(enabled);
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
    SecurityPolicy::addOriginAccessWhitelistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::AddOriginAccessWhitelistEntry { sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains }, 0);

}

void InjectedBundle::removeOriginAccessWhitelistEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessWhitelistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveOriginAccessWhitelistEntry { sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains }, 0);
}

void InjectedBundle::resetOriginAccessWhitelists()
{
    SecurityPolicy::resetOriginAccessWhitelists();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ResetOriginAccessWhitelists { }, 0);
}

void InjectedBundle::setAsynchronousSpellCheckingEnabled(WebPageGroupProxy* pageGroup, bool enabled)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setAsynchronousSpellCheckingEnabled(enabled);
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

    return PrintContext::numberOfPages(*coreFrame, FloatSize(pageWidthInPixels, pageHeightInPixels));
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
    return UserGestureIndicator::processingUserGesture();
}

void InjectedBundle::addUserScript(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, String&& source, String&& url, API::Array* whitelist, API::Array* blacklist, WebCore::UserScriptInjectionTime injectionTime, WebCore::UserContentInjectedFrames injectedFrames)
{
    UserScript userScript { WTFMove(source), URL(URL(), url), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectionTime, injectedFrames };

    pageGroup->userContentController().addUserScript(*scriptWorld, WTFMove(userScript));
}

void InjectedBundle::addUserStyleSheet(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& source, const String& url, API::Array* whitelist, API::Array* blacklist, WebCore::UserContentInjectedFrames injectedFrames)
{
    UserStyleSheet userStyleSheet{ source, URL(URL(), url), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), injectedFrames, UserStyleUserLevel };

    pageGroup->userContentController().addUserStyleSheet(*scriptWorld, WTFMove(userStyleSheet));
}

void InjectedBundle::removeUserScript(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& url)
{
    pageGroup->userContentController().removeUserScriptWithURL(*scriptWorld, URL(URL(), url));
}

void InjectedBundle::removeUserStyleSheet(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld, const String& url)
{
    pageGroup->userContentController().removeUserStyleSheetWithURL(*scriptWorld, URL(URL(), url));
}

void InjectedBundle::removeUserScripts(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld)
{
    pageGroup->userContentController().removeUserScripts(*scriptWorld);
}

void InjectedBundle::removeUserStyleSheets(WebPageGroupProxy* pageGroup, InjectedBundleScriptWorld* scriptWorld)
{
    pageGroup->userContentController().removeUserStyleSheets(*scriptWorld);
}

void InjectedBundle::removeAllUserContent(WebPageGroupProxy* pageGroup)
{
    pageGroup->userContentController().removeAllUserContent();
}

void InjectedBundle::garbageCollectJavaScriptObjects()
{
    GCController::singleton().garbageCollectNow();
}

void InjectedBundle::garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(bool waitUntilDone)
{
    GCController::singleton().garbageCollectOnAlternateThreadForDebugging(waitUntilDone);
}

size_t InjectedBundle::javaScriptObjectsCount()
{
    JSLockHolder lock(commonVM());
    return commonVM().heap.objectCount();
}

void InjectedBundle::reportException(JSContextRef context, JSValueRef exception)
{
    if (!context || !exception)
        return;

    JSC::JSGlobalObject* globalObject = toJS(context);
    JSLockHolder lock(globalObject);

    // Make sure the context has a DOMWindow global object, otherwise this context didn't originate from a Page.
    if (!toJSDOMWindow(globalObject->vm(), globalObject))
        return;

    WebCore::reportException(globalObject, toJS(globalObject, exception));
}

void InjectedBundle::didCreatePage(WebPage* page)
{
    m_client->didCreatePage(*this, *page);
}

void InjectedBundle::willDestroyPage(WebPage* page)
{
    m_client->willDestroyPage(*this, *page);
}

void InjectedBundle::didInitializePageGroup(WebPageGroupProxy* pageGroup)
{
    m_client->didInitializePageGroup(*this, *pageGroup);
}

void InjectedBundle::didReceiveMessage(const String& messageName, API::Object* messageBody)
{
    m_client->didReceiveMessage(*this, messageName, messageBody);
}

void InjectedBundle::didReceiveMessageToPage(WebPage* page, const String& messageName, API::Object* messageBody)
{
    m_client->didReceiveMessageToPage(*this, *page, messageName, messageBody);
}

void InjectedBundle::setUserStyleSheetLocation(WebPageGroupProxy* pageGroup, const String& location)
{
    const HashSet<Page*>& pages = PageGroup::pageGroup(pageGroup->identifier())->pages();
    for (HashSet<Page*>::iterator iter = pages.begin(); iter != pages.end(); ++iter)
        (*iter)->settings().setUserStyleSheetLocation(URL(URL(), location));
}

void InjectedBundle::setWebNotificationPermission(WebPage* page, const String& originString, bool allowed)
{
#if ENABLE(NOTIFICATIONS)
    page->notificationPermissionRequestManager()->setPermissionLevelForTesting(originString, allowed);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(originString);
    UNUSED_PARAM(allowed);
#endif
}

void InjectedBundle::removeAllWebNotificationPermissions(WebPage* page)
{
#if ENABLE(NOTIFICATIONS)
    page->notificationPermissionRequestManager()->removeAllPermissionsForTesting();
#else
    UNUSED_PARAM(page);
#endif
}

uint64_t InjectedBundle::webNotificationID(JSContextRef jsContext, JSValueRef jsNotification)
{
#if ENABLE(NOTIFICATIONS)
    WebCore::Notification* notification = JSNotification::toWrapped(toJS(jsContext)->vm(), toJS(toJS(jsContext), jsNotification));
    if (!notification)
        return 0;
    return WebProcess::singleton().supplement<WebNotificationManager>()->notificationIDForTesting(notification);
#else
    UNUSED_PARAM(jsContext);
    UNUSED_PARAM(jsNotification);
    return 0;
#endif
}

// FIXME Get rid of this function and move it into WKBundle.cpp.
Ref<API::Data> InjectedBundle::createWebDataFromUint8Array(JSContextRef context, JSValueRef data)
{
    JSC::JSGlobalObject* globalObject = toJS(context);
    JSLockHolder lock(globalObject);
    RefPtr<Uint8Array> arrayData = WebCore::toUnsharedUint8Array(globalObject->vm(), toJS(globalObject, data));
    return API::Data::create(static_cast<unsigned char*>(arrayData->baseAddress()), arrayData->byteLength());
}

InjectedBundle::DocumentIDToURLMap InjectedBundle::liveDocumentURLs(WebPageGroupProxy* pageGroup, bool excludeDocumentsInPageGroupPages)
{
    DocumentIDToURLMap result;

    for (const auto* document : Document::allDocuments())
        result.add(document->identifier().toUInt64(), document->url().string());

    if (excludeDocumentsInPageGroupPages) {
        for (const auto* page : PageGroup::pageGroup(pageGroup->identifier())->pages()) {
            for (const auto* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
                if (!frame->document())
                    continue;
                result.remove(frame->document()->identifier().toUInt64());
            }
        }
    }

    return result;
}

void InjectedBundle::setTabKeyCyclesThroughElements(WebPage* page, bool enabled)
{
    page->corePage()->setTabKeyCyclesThroughElements(enabled);
}

void InjectedBundle::setWebAnimationsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::sharedFeatures().setWebAnimationsEnabled(enabled);
}

void InjectedBundle::setWebAnimationsCSSIntegrationEnabled(bool enabled)
{
    RuntimeEnabledFeatures::sharedFeatures().setWebAnimationsCSSIntegrationEnabled(enabled);
}

} // namespace WebKit
