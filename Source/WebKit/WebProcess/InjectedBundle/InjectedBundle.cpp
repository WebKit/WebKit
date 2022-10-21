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
#include <WebCore/DeprecatedGlobalSettings.h>
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
#include <WebCore/SWContextManager.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/UserScript.h>
#include <WebCore/UserStyleSheet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/SystemTracing.h>

#if ENABLE(NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

namespace WebKit {
using namespace WebCore;
using namespace JSC;

RefPtr<InjectedBundle> InjectedBundle::create(WebProcessCreationParameters& parameters, API::Object* initializationUserData)
{
    TraceScope scope(TracePointCode::CreateInjectedBundleStart, TracePointCode::CreateInjectedBundleEnd);
    
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
    auto& webProcess = WebProcess::singleton();
    auto sendResult = webProcess.parentProcessConnection()->sendSync(Messages::WebProcessPool::HandleSynchronousMessage(messageName, UserData(webProcess.transformObjectsToHandles(messageBody))), 0);
    if (sendResult) {
        auto [returnUserData] = sendResult.takeReply();
        returnData = webProcess.transformHandlesToObjects(returnUserData.object());
    } else
        returnData = nullptr;
}

WebConnection* InjectedBundle::webConnectionToUIProcess() const
{
    return WebProcess::singleton().webConnectionToUIProcess();
}

void InjectedBundle::addOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::addOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::AddOriginAccessAllowListEntry { sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains }, 0);
}

void InjectedBundle::removeOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains)
{
    SecurityPolicy::removeOriginAccessAllowlistEntry(SecurityOrigin::createFromString(sourceOrigin).get(), destinationProtocol, destinationHost, allowDestinationSubdomains);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveOriginAccessAllowListEntry { sourceOrigin, destinationProtocol, destinationHost, allowDestinationSubdomains }, 0);
}

void InjectedBundle::resetOriginAccessAllowLists()
{
    SecurityPolicy::resetOriginAccessAllowlists();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ResetOriginAccessAllowLists { }, 0);
}

void InjectedBundle::setAsynchronousSpellCheckingEnabled(bool enabled)
{
    Page::forEachPage([enabled](Page& page) {
        page.settings().setAsynchronousSpellCheckingEnabled(enabled);
    });
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
    if (!globalObject->inherits<JSDOMWindow>())
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

void InjectedBundle::didReceiveMessage(const String& messageName, API::Object* messageBody)
{
    m_client->didReceiveMessage(*this, messageName, messageBody);
}

void InjectedBundle::didReceiveMessageToPage(WebPage* page, const String& messageName, API::Object* messageBody)
{
    m_client->didReceiveMessageToPage(*this, *page, messageName, messageBody);
}

void InjectedBundle::setUserStyleSheetLocation(const String& location)
{
    Page::forEachPage([location](Page& page) {
        page.settings().setUserStyleSheetLocation(URL { location });
    });
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

std::optional<UUID> InjectedBundle::webNotificationID(JSContextRef jsContext, JSValueRef jsNotification)
{
#if ENABLE(NOTIFICATIONS)
    WebCore::Notification* notification = JSNotification::toWrapped(toJS(jsContext)->vm(), toJS(toJS(jsContext), jsNotification));
    if (!notification)
        return std::nullopt;
    return notification->identifier();
#else
    UNUSED_PARAM(jsContext);
    UNUSED_PARAM(jsNotification);
    return std::nullopt;
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

InjectedBundle::DocumentIDToURLMap InjectedBundle::liveDocumentURLs(bool excludeDocumentsInPageGroupPages)
{
    DocumentIDToURLMap result;

    for (const auto* document : Document::allDocuments())
        result.add(document->identifier().object(), document->url().string());

    if (excludeDocumentsInPageGroupPages) {
        Page::forEachPage([&](Page& page) {
            for (AbstractFrame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
                auto* localFrame = dynamicDowncast<LocalFrame>(frame);
                if (!localFrame)
                    continue;
                auto* document = localFrame->document();
                if (!document)
                    continue;
                result.remove(document->identifier().object());
            }
        });
    }

    return result;
}

void InjectedBundle::setTabKeyCyclesThroughElements(WebPage* page, bool enabled)
{
    page->corePage()->setTabKeyCyclesThroughElements(enabled);
}

void InjectedBundle::setAccessibilityIsolatedTreeEnabled(bool enabled)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    DeprecatedGlobalSettings::setIsAccessibilityIsolatedTreeEnabled(enabled);
#endif
}

} // namespace WebKit
