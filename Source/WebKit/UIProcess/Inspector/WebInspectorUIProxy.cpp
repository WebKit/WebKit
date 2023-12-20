/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebInspectorUIProxy.h"

#include "APIInspectorClient.h"
#include "APINavigation.h"
#include "APIProcessPoolConfiguration.h"
#include "APIUIClient.h"
#include "InspectorBrowserAgent.h"
#include "MessageSenderInlines.h"
#include "WebAutomationSession.h"
#include "WebFrameProxy.h"
#include "WebInspectorInterruptDispatcherMessages.h"
#include "WebInspectorMessages.h"
#include "WebInspectorUIExtensionControllerProxy.h"
#include "WebInspectorUIMessages.h"
#include "WebInspectorUIProxyMessages.h"
#include "WebPageGroup.h"
#include "WebPageInspectorController.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NotImplemented.h>
#include <pal/text/TextEncoding.h>
#include <wtf/SetForScope.h>

#if PLATFORM(GTK)
#include "WebInspectorUIProxyClient.h"
#endif

namespace WebKit {
using namespace WebCore;

const unsigned WebInspectorUIProxy::minimumWindowWidth = 500;
const unsigned WebInspectorUIProxy::minimumWindowHeight = 400;

const unsigned WebInspectorUIProxy::initialWindowWidth = 1000;
const unsigned WebInspectorUIProxy::initialWindowHeight = 650;

WebInspectorUIProxy::WebInspectorUIProxy(WebPageProxy& inspectedPage)
    : m_inspectedPage(inspectedPage)
    , m_inspectorClient(makeUnique<API::InspectorClient>())
    , m_inspectedPageIdentifier(inspectedPage.identifier())
#if PLATFORM(MAC)
    , m_closeFrontendAfterInactivityTimer(RunLoop::main(), this, &WebInspectorUIProxy::closeFrontendAfterInactivityTimerFired)
#endif
{
    m_inspectedPage->process().addMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPage->webPageID(), *this);
}

WebInspectorUIProxy::~WebInspectorUIProxy()
{
}

void WebInspectorUIProxy::setInspectorClient(std::unique_ptr<API::InspectorClient>&& inspectorClient)
{
    if (!inspectorClient) {
        m_inspectorClient = nullptr;
        return;
    }

    m_inspectorClient = WTFMove(inspectorClient);
}

unsigned WebInspectorUIProxy::inspectionLevel() const
{
    return inspectorLevelForPage(inspectedPage().get());
}

WebPreferences& WebInspectorUIProxy::inspectorPagePreferences() const
{
    ASSERT(m_inspectorPage);
    return m_inspectorPage->pageGroup().preferences();
}

void WebInspectorUIProxy::invalidate()
{
    closeFrontendPageAndWindow();
    platformInvalidate();

    reset();
}

void WebInspectorUIProxy::sendMessageToFrontend(const String& message)
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->send(Messages::WebInspectorUI::SendMessageToFrontend(message));
}

// Public APIs
bool WebInspectorUIProxy::isFront()
{
    if (!m_inspectedPage)
        return false;

    return platformIsFront();
}

void WebInspectorUIProxy::connect()
{
    if (!m_inspectedPage)
        return;

    if (!m_inspectedPage->preferences().developerExtrasEnabled())
        return;

    if (m_showMessageSent)
        return;

    m_showMessageSent = true;
    m_ignoreFirstBringToFront = true;

    createFrontendPage();

    m_inspectedPage->send(Messages::WebInspectorInterruptDispatcher::NotifyNeedDebuggerBreak(), 0);
    m_inspectedPage->send(Messages::WebInspector::Show());
}

void WebInspectorUIProxy::show()
{
    if (!m_inspectedPage)
        return;

    if (isConnected()) {
        bringToFront();
        return;
    }

    connect();

    // Don't ignore the first bringToFront so it opens the Inspector.
    m_ignoreFirstBringToFront = false;
}

void WebInspectorUIProxy::hide()
{
    if (!m_inspectedPage)
        return;

    m_isVisible = false;

    platformHide();
}

void WebInspectorUIProxy::close()
{
    if (!m_inspectedPage)
        return;

    m_inspectedPage->send(Messages::WebInspector::Close());

    closeFrontendPageAndWindow();
}

void WebInspectorUIProxy::closeForCrash()
{
    close();

    platformDidCloseForCrash();
}

void WebInspectorUIProxy::reopen()
{
    if (!m_inspectedPage)
        return;

    close();
    show();
}

void WebInspectorUIProxy::resetState()
{
    ASSERT(m_inspectedPage);
    ASSERT(m_inspectorPage);
    if (!m_inspectedPage || !m_inspectorPage)
        return;

    inspectorPagePreferences().deleteInspectorAttachedHeight();
    inspectorPagePreferences().deleteInspectorAttachedWidth();
    inspectorPagePreferences().deleteInspectorAttachmentSide();
    inspectorPagePreferences().deleteInspectorStartsAttached();
    inspectorPagePreferences().deleteInspectorWindowFrame();

    platformResetState();
}

void WebInspectorUIProxy::reset()
{
    if (m_inspectedPage) {
        m_inspectedPage->process().removeMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPage->webPageID());
        m_inspectedPage = nullptr;
    }
}

void WebInspectorUIProxy::updateForNewPageProcess(WebPageProxy& inspectedPage)
{
    ASSERT(!m_inspectedPage);

    m_inspectedPage = &inspectedPage;
    m_inspectedPageIdentifier = m_inspectedPage->identifier();

    m_inspectedPage->process().addMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPage->webPageID(), *this);

    if (m_inspectorPage)
        m_inspectorPage->send(Messages::WebInspectorUI::UpdateConnection());
}

void WebInspectorUIProxy::setFrontendConnection(IPC::Connection::Handle&& connectionIdentifier)
{
    if (!m_inspectedPage)
        return;

    m_inspectedPage->send(Messages::WebInspector::SetFrontendConnection(WTFMove(connectionIdentifier)));
}

void WebInspectorUIProxy::showConsole()
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

    m_inspectedPage->send(Messages::WebInspector::ShowConsole());
}

void WebInspectorUIProxy::showResources()
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

    m_inspectedPage->send(Messages::WebInspector::ShowResources());
}

void WebInspectorUIProxy::showMainResourceForFrame(WebFrameProxy* frame)
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

    m_inspectedPage->send(Messages::WebInspector::ShowMainResourceForFrame(frame->frameID()));
}

void WebInspectorUIProxy::attachBottom()
{
    attach(AttachmentSide::Bottom);
}

void WebInspectorUIProxy::attachRight()
{
    attach(AttachmentSide::Right);
}

void WebInspectorUIProxy::attachLeft()
{
    attach(AttachmentSide::Left);
}

void WebInspectorUIProxy::attach(AttachmentSide side)
{
    ASSERT(m_inspectorPage);
    if (!m_inspectedPage || !m_inspectorPage || (!m_isAttached && !platformCanAttach(m_canAttach)))
        return;

    m_isAttached = true;
    m_attachmentSide = side;

    inspectorPagePreferences().setInspectorAttachmentSide(static_cast<uint32_t>(side));

    if (m_isVisible)
        inspectorPagePreferences().setInspectorStartsAttached(true);

    m_inspectedPage->send(Messages::WebInspector::SetAttached(true));

    switch (m_attachmentSide) {
    case AttachmentSide::Bottom:
        m_inspectorPage->send(Messages::WebInspectorUI::AttachedBottom());
        break;

    case AttachmentSide::Right:
        m_inspectorPage->send(Messages::WebInspectorUI::AttachedRight());
        break;

    case AttachmentSide::Left:
        m_inspectorPage->send(Messages::WebInspectorUI::AttachedLeft());
        break;
    }

    platformAttach();
}

void WebInspectorUIProxy::detach()
{
    if (!m_inspectedPage || !m_inspectorPage || (!m_isAttached && m_isVisible))
        return;

    m_isAttached = false;

    if (m_isVisible)
        inspectorPagePreferences().setInspectorStartsAttached(false);

    m_inspectedPage->send(Messages::WebInspector::SetAttached(false));
    m_inspectorPage->send(Messages::WebInspectorUI::Detached());

    platformDetach();
}

void WebInspectorUIProxy::setAttachedWindowHeight(unsigned height)
{
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    inspectorPagePreferences().setInspectorAttachedHeight(height);
    platformSetAttachedWindowHeight(height);
}

void WebInspectorUIProxy::setAttachedWindowWidth(unsigned width)
{
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    inspectorPagePreferences().setInspectorAttachedWidth(width);
    platformSetAttachedWindowWidth(width);
}

void WebInspectorUIProxy::setSheetRect(const FloatRect& rect)
{
    platformSetSheetRect(rect);
}

void WebInspectorUIProxy::startWindowDrag()
{
    platformStartWindowDrag();
}

void WebInspectorUIProxy::togglePageProfiling()
{
    if (!m_inspectedPage)
        return;

    if (m_isProfilingPage)
        m_inspectedPage->send(Messages::WebInspector::StopPageProfiling());
    else
        m_inspectedPage->send(Messages::WebInspector::StartPageProfiling());
}

void WebInspectorUIProxy::toggleElementSelection()
{
    if (!m_inspectedPage)
        return;

    if (m_elementSelectionActive) {
        m_ignoreElementSelectionChange = true;
        m_inspectedPage->send(Messages::WebInspector::StopElementSelection());
    } else {
        connect();
        m_inspectedPage->send(Messages::WebInspector::StartElementSelection());
    }
}

bool WebInspectorUIProxy::isMainOrTestInspectorPage(const URL& url)
{
    // Use URL so we can compare the paths and protocols.
    URL mainPageURL { WebInspectorUIProxy::inspectorPageURL() };
    if (url.protocol() == mainPageURL.protocol() && PAL::decodeURLEscapeSequences(url.path()) == PAL::decodeURLEscapeSequences(mainPageURL.path()))
        return true;

    // We might not have a Test URL in Production builds.
    String testPageURLString = WebInspectorUIProxy::inspectorTestPageURL();
    if (testPageURLString.isNull())
        return false;

    URL testPageURL { testPageURLString };
    return url.protocol() == testPageURL.protocol() && PAL::decodeURLEscapeSequences(url.path()) == PAL::decodeURLEscapeSequences(testPageURL.path());
}

void WebInspectorUIProxy::createFrontendPage()
{
    if (m_inspectorPage)
        return;

    m_inspectorPage = platformCreateFrontendPage();
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    trackInspectorPage(inspectorPage().get(), inspectedPage().get());

    // Make sure the inspected page has a running WebProcess so we can inspect it.
    m_inspectedPage->launchInitialProcessIfNecessary();

    m_inspectorPage->process().addMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPageIdentifier, *this);

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = WebInspectorUIExtensionControllerProxy::create(*m_inspectorPage);
#endif
}

void WebInspectorUIProxy::openLocalInspectorFrontend(bool canAttach, bool underTest)
{
    auto inspectedPage = this->inspectedPage();
    if (!inspectedPage)
        return;

    if (!inspectedPage->preferences().developerExtrasEnabled())
        return;

    if (inspectedPage->inspectorController().hasLocalFrontend()) {
        show();
        return;
    }

    m_underTest = underTest;
    createFrontendPage();

    auto inspectorPage = this->inspectorPage();
    ASSERT(inspectorPage);
    if (!inspectorPage)
        return;

    inspectorPage->send(Messages::WebInspectorUI::EstablishConnection(m_inspectedPageIdentifier, infoForLocalDebuggable(), m_underTest, inspectionLevel()));

    ASSERT(!m_isActiveFrontend);
    m_isActiveFrontend = true;
    inspectedPage->inspectorController().connectFrontend(*this);

    if (!m_underTest) {
        m_canAttach = platformCanAttach(canAttach);
        m_isAttached = shouldOpenAttached();
        m_attachmentSide = static_cast<AttachmentSide>(inspectorPagePreferences().inspectorAttachmentSide());

        inspectedPage->send(Messages::WebInspector::SetAttached(m_isAttached));

        if (m_isAttached) {
            switch (m_attachmentSide) {
            case AttachmentSide::Bottom:
                inspectorPage->send(Messages::WebInspectorUI::AttachedBottom());
                break;

            case AttachmentSide::Right:
                inspectorPage->send(Messages::WebInspectorUI::AttachedRight());
                break;

            case AttachmentSide::Left:
                inspectorPage->send(Messages::WebInspectorUI::AttachedLeft());
                break;
            }
        } else
            inspectorPage->send(Messages::WebInspectorUI::Detached());

        inspectorPage->send(Messages::WebInspectorUI::SetDockingUnavailable(!m_canAttach));
    }

    // Notify WebKit client when a local inspector attaches so that it may install delegates prior to the _WKInspector loading its frontend.
    inspectedPage->uiClient().didAttachLocalInspector(*inspectedPage, *this);

    // Bail out if the client closed the inspector from the delegate method.

    if (!inspectorPage)
        return;

    inspectorPage->loadRequest(URL { m_underTest ? WebInspectorUIProxy::inspectorTestPageURL() : WebInspectorUIProxy::inspectorPageURL() });
}

void WebInspectorUIProxy::open()
{
    if (m_underTest)
        return;

    if (!m_inspectorPage)
        return;

    SetForScope isOpening(m_isOpening, true);

    m_isVisible = true;
    m_inspectorPage->send(Messages::WebInspectorUI::SetIsVisible(m_isVisible));

    if (m_isAttached && platformCanAttach(m_canAttach))
        platformAttach();
    else {
        m_isAttached = false;
        platformCreateFrontendWindow();
    }

    platformBringToFront();
}

void WebInspectorUIProxy::didClose()
{
    closeFrontendPageAndWindow();
}

void WebInspectorUIProxy::closeFrontendPageAndWindow()
{
    if (!m_inspectorPage)
        return;

    // Guard against calls to close() made by the client while already closing.
    if (m_closing)
        return;
    
    SetForScope reentrancyProtector(m_closing, true);
    
    // Notify WebKit client when a local inspector closes so it can clear _WKInspectorDelegate and perform other cleanup.
    if (auto inspectedPage = this->inspectedPage())
        inspectedPage->uiClient().willCloseLocalInspector(*inspectedPage, *this);

    m_isVisible = false;
    m_isProfilingPage = false;
    m_showMessageSent = false;
    m_ignoreFirstBringToFront = false;

    auto inspectorPage = this->inspectorPage();
    untrackInspectorPage(inspectorPage.get());

    inspectorPage->send(Messages::WebInspectorUI::SetIsVisible(m_isVisible));
    inspectorPage->process().removeMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPageIdentifier);

    if (auto inspectedPage = this->inspectedPage(); inspectedPage && m_isActiveFrontend)
        inspectedPage->inspectorController().disconnectFrontend(*this);

    m_isActiveFrontend = false;

    if (m_isAttached)
        platformDetach();

#if ENABLE(INSPECTOR_EXTENSIONS)
    // This extension controller may be kept alive by the IPC dispatcher beyond the point
    // when m_inspectorPage is cleared below. Notify the controller so it can clean up before then.
    m_extensionController->inspectorFrontendWillClose();
    m_extensionController = nullptr;
#endif
    
    // Null out m_inspectorPage after platformDetach(), so the views can be cleaned up correctly.
    m_inspectorPage = nullptr;

    m_isAttached = false;
    m_canAttach = false;
    m_underTest = false;

    platformCloseFrontendPageAndWindow();
}

void WebInspectorUIProxy::sendMessageToBackend(const String& message)
{
    auto inspectedPage = this->inspectedPage();
    if (!inspectedPage)
        return;

    inspectedPage->inspectorController().dispatchMessageFromFrontend(message);
}

void WebInspectorUIProxy::frontendLoaded()
{
    auto inspectedPage = this->inspectedPage();
    if (!inspectedPage)
        return;

    if (auto* automationSession = inspectedPage->process().processPool().automationSession())
        automationSession->inspectorFrontendLoaded(*inspectedPage);
    
#if ENABLE(INSPECTOR_EXTENSIONS)
    if (m_extensionController)
        m_extensionController->inspectorFrontendLoaded();
#endif

    if (m_inspectorClient)
        m_inspectorClient->frontendLoaded(*this);
}

void WebInspectorUIProxy::bringToFront()
{
    // WebCore::InspectorFrontendClientLocal tells us to do this on load. We want to
    // ignore it once if we only wanted to connect. This allows the Inspector to later
    // request to be brought to the front when a breakpoint is hit or some other action.
    if (m_ignoreFirstBringToFront) {
        m_ignoreFirstBringToFront = false;
        return;
    }

    if (m_isVisible)
        platformBringToFront();
    else
        open();
}

void WebInspectorUIProxy::attachAvailabilityChanged(bool available)
{
    bool previousCanAttach = m_canAttach;

    m_canAttach = m_isAttached || platformCanAttach(available);

    if (previousCanAttach == m_canAttach)
        return;

    if (auto inspectorPage = this->inspectorPage(); inspectorPage && !m_underTest)
        inspectorPage->send(Messages::WebInspectorUI::SetDockingUnavailable(!m_canAttach));

    platformAttachAvailabilityChanged(m_canAttach);
}

void WebInspectorUIProxy::setForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    platformSetForcedAppearance(appearance);
}

void WebInspectorUIProxy::openURLExternally(const String& url)
{
    if (m_inspectorClient)
        m_inspectorClient->openURLExternally(*this, url);
}

void WebInspectorUIProxy::revealFileExternally(const String& path)
{
    platformRevealFileExternally(path);
}

void WebInspectorUIProxy::inspectedURLChanged(const String& urlString)
{
    platformInspectedURLChanged(urlString);
}

void WebInspectorUIProxy::showCertificate(const CertificateInfo& certificateInfo)
{
    platformShowCertificate(certificateInfo);
}

void WebInspectorUIProxy::setInspectorPageDeveloperExtrasEnabled(bool enabled)
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->preferences().setDeveloperExtrasEnabled(enabled);
}

void WebInspectorUIProxy::elementSelectionChanged(bool active)
{
    m_elementSelectionActive = active;

    if (m_ignoreElementSelectionChange) {
        m_ignoreElementSelectionChange = false;
        if (!m_isVisible)
            close();
        return;
    }

    if (active)
        platformBringInspectedPageToFront();
    else if (isConnected())
        bringToFront();
}

void WebInspectorUIProxy::timelineRecordingChanged(bool active)
{
    m_isProfilingPage = active;
}

void WebInspectorUIProxy::setDeveloperPreferenceOverride(WebCore::InspectorClient::DeveloperPreference developerPreference, std::optional<bool> overrideValue)
{
    switch (developerPreference) {
    case InspectorClient::DeveloperPreference::PrivateClickMeasurementDebugModeEnabled:
        if (auto inspectedPage = this->inspectedPage())
            inspectedPage->websiteDataStore().setPrivateClickMeasurementDebugMode(overrideValue && overrideValue.value());
        return;

    case InspectorClient::DeveloperPreference::ITPDebugModeEnabled:
        if (auto inspectedPage = this->inspectedPage())
            inspectedPage->websiteDataStore().setResourceLoadStatisticsDebugMode(overrideValue && overrideValue.value());
        return;

    case InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled:
#if ENABLE(MEDIA_STREAM)
        if (auto inspectedPage = this->inspectedPage())
            inspectedPage->setMockCaptureDevicesEnabledOverride(overrideValue);
#endif // ENABLE(MEDIA_STREAM)
        return;
    }

    ASSERT_NOT_REACHED();
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebInspectorUIProxy::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    if (auto inspectedPage = this->inspectedPage())
        inspectedPage->websiteDataStore().setEmulatedConditions(WTFMove(bytesPerSecondLimit));
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebInspectorUIProxy::setDiagnosticLoggingAvailable(bool available)
{
#if ENABLE(INSPECTOR_TELEMETRY)
    m_inspectorPage->send(Messages::WebInspectorUI::SetDiagnosticLoggingAvailable(available));
#else
    UNUSED_PARAM(available);
#endif
}

void WebInspectorUIProxy::save(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    if (!m_inspectedPage->preferences().developerExtrasEnabled())
        return;

    ASSERT(!saveDatas.isEmpty());
    if (saveDatas.isEmpty())
        return;

    ASSERT(!saveDatas[0].url.isEmpty());
    if (saveDatas[0].url.isEmpty())
        return;

    platformSave(WTFMove(saveDatas), forceSaveAs);
}

void WebInspectorUIProxy::load(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (!m_inspectedPage->preferences().developerExtrasEnabled())
        return;

    ASSERT(!path.isEmpty());
    if (path.isEmpty())
        return;

    platformLoad(path, WTFMove(completionHandler));
}

void WebInspectorUIProxy::pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color> &)>&& completionHandler)
{
    if (!m_inspectedPage->preferences().developerExtrasEnabled()) {
        completionHandler({ });
        return;
    }

    platformPickColorFromScreen(WTFMove(completionHandler));
}

bool WebInspectorUIProxy::shouldOpenAttached()
{
    return inspectorPagePreferences().inspectorStartsAttached() && canAttach();
}

void WebInspectorUIProxy::evaluateInFrontendForTesting(const String& expression)
{
    auto inspectorPage = this->inspectorPage();
    if (!inspectorPage)
        return;

    inspectorPage->send(Messages::WebInspectorUI::EvaluateInFrontendForTesting(expression));
}

// Unsupported configurations can use the stubs provided here.

#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)

WebPageProxy* WebInspectorUIProxy::platformCreateFrontendPage()
{
    notImplemented();
    return nullptr;
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    notImplemented();
}

void WebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    notImplemented();
}

void WebInspectorUIProxy::platformDidCloseForCrash()
{
    notImplemented();
}

void WebInspectorUIProxy::platformInvalidate()
{
    notImplemented();
}

void WebInspectorUIProxy::platformResetState()
{
    notImplemented();
}

void WebInspectorUIProxy::platformBringToFront()
{
    notImplemented();
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    notImplemented();
}

void WebInspectorUIProxy::platformHide()
{
    notImplemented();
}

bool WebInspectorUIProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance)
{
    notImplemented();
}

void WebInspectorUIProxy::platformRevealFileExternally(const String&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformInspectedURLChanged(const String&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformShowCertificate(const CertificateInfo&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&&, bool /* forceSaveAs */)
{
    notImplemented();
}

void WebInspectorUIProxy::platformLoad(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    notImplemented();
    completionHandler(nullString());
}

void WebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebInspectorUIProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorUIProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void WebInspectorUIProxy::platformSetSheetRect(const FloatRect&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    notImplemented();
}

String WebInspectorUIProxy::inspectorPageURL()
{
    notImplemented();
    return String();
}

String WebInspectorUIProxy::inspectorTestPageURL()
{
    notImplemented();
    return String();
}

DebuggableInfoData WebInspectorUIProxy::infoForLocalDebuggable()
{
    notImplemented();
    return DebuggableInfoData::empty();
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned)
{
    notImplemented();
}

void WebInspectorUIProxy::platformAttachAvailabilityChanged(bool)
{
    notImplemented();
}

#endif // !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)

} // namespace WebKit
