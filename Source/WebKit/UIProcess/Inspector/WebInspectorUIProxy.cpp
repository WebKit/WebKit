/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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
#include "APIPageConfiguration.h"
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

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "WebExtensionController.h"
#endif

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
    protectedInspectedPage()->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPage->webPageIDInMainFrameProcess(), *this);
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
    return inspectorLevelForPage(protectedInspectedPage().get());
}

WebPreferences& WebInspectorUIProxy::inspectorPagePreferences() const
{
    ASSERT(m_inspectorPage);
    return protectedInspectorPage()->protectedPageGroup()->preferences();
}

Ref<WebPreferences> WebInspectorUIProxy::protectedInspectorPagePreferences() const
{
    return inspectorPagePreferences();
}

void WebInspectorUIProxy::invalidate()
{
    closeFrontendPageAndWindow();
    platformInvalidate();

    reset();
}

void WebInspectorUIProxy::sendMessageToFrontend(const String& message)
{
    RefPtr inspectorPage = m_inspectorPage.get();
    if (!inspectorPage)
        return;

    inspectorPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::SendMessageToFrontend(message), m_inspectorPage->webPageIDInMainFrameProcess());
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
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    if (!inspectedPage->protectedPreferences()->developerExtrasEnabled())
        return;

    if (m_showMessageSent)
        return;

    m_showMessageSent = true;
    m_ignoreFirstBringToFront = true;

    createFrontendPage();

    Ref legacyMainFrameProcess = inspectedPage->legacyMainFrameProcess();
    legacyMainFrameProcess->send(Messages::WebInspectorInterruptDispatcher::NotifyNeedDebuggerBreak(), 0);
    legacyMainFrameProcess->send(Messages::WebInspector::Show(), m_inspectedPage->webPageIDInMainFrameProcess());
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
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::Close(), m_inspectedPage->webPageIDInMainFrameProcess());

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

    Ref preferences = inspectorPagePreferences();
    preferences->deleteInspectorAttachedHeight();
    preferences->deleteInspectorAttachedWidth();
    preferences->deleteInspectorAttachmentSide();
    preferences->deleteInspectorStartsAttached();
    preferences->deleteInspectorWindowFrame();

    platformResetState();
}

void WebInspectorUIProxy::reset()
{
    if (RefPtr inspectedPage = m_inspectedPage.get()) {
        inspectedPage->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), inspectedPage->webPageIDInMainFrameProcess());
        m_inspectedPage = nullptr;
    }
}

void WebInspectorUIProxy::updateForNewPageProcess(WebPageProxy& inspectedPage)
{
    ASSERT(!m_inspectedPage);

    m_inspectedPage = &inspectedPage;
    m_inspectedPageIdentifier = inspectedPage.identifier();

    protectedInspectedPage()->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPage->webPageIDInMainFrameProcess(), *this);

    if (RefPtr inspectorPage = m_inspectorPage.get())
        inspectorPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::UpdateConnection(), m_inspectorPage->webPageIDInMainFrameProcess());
}

void WebInspectorUIProxy::setFrontendConnection(IPC::Connection::Handle&& connectionIdentifier)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!m_inspectedPage)
        return;

    inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::SetFrontendConnection(WTFMove(connectionIdentifier)), inspectedPage->webPageIDInMainFrameProcess());
}

void WebInspectorUIProxy::showConsole()
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    createFrontendPage();

    inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::ShowConsole(), inspectedPage->webPageIDInMainFrameProcess());
}

void WebInspectorUIProxy::showResources()
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    createFrontendPage();

    inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::ShowResources(), inspectedPage->webPageIDInMainFrameProcess());
}

void WebInspectorUIProxy::showMainResourceForFrame(WebCore::FrameIdentifier frameID)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    createFrontendPage();

    inspectedPage->sendToProcessContainingFrame(frameID, Messages::WebInspector::ShowMainResourceForFrame(frameID));
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

    Ref preferences = inspectorPagePreferences();
    preferences->setInspectorAttachmentSide(static_cast<uint32_t>(side));

    if (m_isVisible)
        preferences->setInspectorStartsAttached(true);

    protectedInspectedPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::SetAttached(true), m_inspectedPage->webPageIDInMainFrameProcess());

    switch (m_attachmentSide) {
    case AttachmentSide::Bottom:
        protectedInspectorPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::AttachedBottom(), m_inspectorPage->webPageIDInMainFrameProcess());
        break;

    case AttachmentSide::Right:
        protectedInspectorPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::AttachedRight(), m_inspectorPage->webPageIDInMainFrameProcess());
        break;

    case AttachmentSide::Left:
        protectedInspectorPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::AttachedLeft(), m_inspectorPage->webPageIDInMainFrameProcess());
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
        protectedInspectorPagePreferences()->setInspectorStartsAttached(false);

    protectedInspectedPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::SetAttached(false), m_inspectedPage->webPageIDInMainFrameProcess());
    protectedInspectorPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::Detached(), m_inspectorPage->webPageIDInMainFrameProcess());

    platformDetach();
}

void WebInspectorUIProxy::setAttachedWindowHeight(unsigned height)
{
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    protectedInspectorPagePreferences()->setInspectorAttachedHeight(height);
    platformSetAttachedWindowHeight(height);
}

void WebInspectorUIProxy::setAttachedWindowWidth(unsigned width)
{
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    protectedInspectorPagePreferences()->setInspectorAttachedWidth(width);
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

    RefPtr inspectedPage = m_inspectedPage.get();
    if (m_isProfilingPage)
        inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::StopPageProfiling(), inspectedPage->webPageIDInMainFrameProcess());
    else
        inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::StartPageProfiling(), inspectedPage->webPageIDInMainFrameProcess());
}

void WebInspectorUIProxy::toggleElementSelection()
{
    if (!m_inspectedPage)
        return;

    RefPtr inspectedPage = m_inspectedPage.get();
    if (m_elementSelectionActive) {
        m_ignoreElementSelectionChange = true;
        inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::StopElementSelection(), inspectedPage->webPageIDInMainFrameProcess());
    } else {
        connect();
        inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::StartElementSelection(), inspectedPage->webPageIDInMainFrameProcess());
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

    RefPtr inspectorPage = platformCreateFrontendPage();
    m_inspectorPage = inspectorPage.get();
    ASSERT(inspectorPage);
    if (!inspectorPage)
        return;

    trackInspectorPage(inspectorPage.get(), protectedInspectedPage().get());

    // Make sure the inspected page has a running WebProcess so we can inspect it.
    protectedInspectedPage()->launchInitialProcessIfNecessary();

    inspectorPage->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPageIdentifier, *this);

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = WebInspectorUIExtensionControllerProxy::create(*inspectorPage);
#endif
}

void WebInspectorUIProxy::openLocalInspectorFrontend(bool canAttach, bool underTest)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    if (!inspectedPage->protectedPreferences()->developerExtrasEnabled())
        return;

    if (inspectedPage->inspectorController().hasLocalFrontend()) {
        show();
        return;
    }

    m_underTest = underTest;
    createFrontendPage();

    RefPtr inspectorPage = m_inspectorPage.get();
    ASSERT(inspectorPage);
    if (!inspectorPage)
        return;

    inspectorPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::EstablishConnection(m_inspectedPageIdentifier, infoForLocalDebuggable(), m_underTest, inspectionLevel()), m_inspectorPage->webPageIDInMainFrameProcess());

    ASSERT(!m_isActiveFrontend);
    m_isActiveFrontend = true;
    inspectedPage->inspectorController().connectFrontend(*this);

    if (!m_underTest) {
        m_canAttach = platformCanAttach(canAttach);
        m_isAttached = shouldOpenAttached();
        m_attachmentSide = static_cast<AttachmentSide>(protectedInspectorPagePreferences()->inspectorAttachmentSide());

        inspectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspector::SetAttached(m_isAttached), inspectedPage->webPageIDInMainFrameProcess());

        Ref inspectorPageProcess = inspectorPage->legacyMainFrameProcess();
        if (m_isAttached) {
            switch (m_attachmentSide) {
            case AttachmentSide::Bottom:
                inspectorPageProcess->send(Messages::WebInspectorUI::AttachedBottom(), inspectorPage->webPageIDInMainFrameProcess());
                break;

            case AttachmentSide::Right:
                inspectorPageProcess->send(Messages::WebInspectorUI::AttachedRight(), inspectorPage->webPageIDInMainFrameProcess());
                break;

            case AttachmentSide::Left:
                inspectorPageProcess->send(Messages::WebInspectorUI::AttachedLeft(), inspectorPage->webPageIDInMainFrameProcess());
                break;
            }
        } else
            inspectorPageProcess->send(Messages::WebInspectorUI::Detached(), inspectorPage->webPageIDInMainFrameProcess());

        inspectorPageProcess->send(Messages::WebInspectorUI::SetDockingUnavailable(!m_canAttach), inspectorPage->webPageIDInMainFrameProcess());
    }

    // Notify clients when a local inspector attaches so that it may install delegates prior to the _WKInspector loading its frontend.

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (RefPtr webExtensionController = inspectedPage->webExtensionController())
        webExtensionController->inspectorWillOpen(*this, *inspectedPage);
#endif

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
    protectedInspectorPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::SetIsVisible(m_isVisible), m_inspectorPage->webPageIDInMainFrameProcess());

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
    
    // Notify clients when a local inspector closes so it can clear _WKInspectorDelegate and perform other cleanup.
    if (RefPtr inspectedPage = m_inspectedPage.get()) {
#if ENABLE(INSPECTOR_EXTENSIONS)
        if (RefPtr webExtensionController = inspectedPage->webExtensionController())
            webExtensionController->inspectorWillClose(*this, *inspectedPage);
#endif

        inspectedPage->uiClient().willCloseLocalInspector(*inspectedPage, *this);
    }

    m_isVisible = false;
    m_isProfilingPage = false;
    m_showMessageSent = false;
    m_ignoreFirstBringToFront = false;

    RefPtr inspectorPage = m_inspectorPage.get();
    untrackInspectorPage(inspectorPage.get());

    Ref inspectorPageProcess = inspectorPage->legacyMainFrameProcess();
    inspectorPageProcess->send(Messages::WebInspectorUI::SetIsVisible(m_isVisible), inspectorPage->webPageIDInMainFrameProcess());
    inspectorPageProcess->removeMessageReceiver(Messages::WebInspectorUIProxy::messageReceiverName(), m_inspectedPageIdentifier);

    if (RefPtr inspectedPage = m_inspectedPage.get(); inspectedPage && m_isActiveFrontend)
        inspectedPage->inspectorController().disconnectFrontend(*this);

    m_isActiveFrontend = false;

    if (m_isAttached)
        platformDetach();

#if ENABLE(INSPECTOR_EXTENSIONS)
    // This extension controller may be kept alive by the IPC dispatcher beyond the point
    // when m_inspectorPage is cleared below. Notify the controller so it can clean up before then.
    protectedExtensionController()->inspectorFrontendWillClose();
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
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    inspectedPage->inspectorController().dispatchMessageFromFrontend(message);
}

void WebInspectorUIProxy::frontendLoaded()
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    if (RefPtr automationSession = inspectedPage->protectedConfiguration()->processPool().automationSession())
        automationSession->inspectorFrontendLoaded(*inspectedPage);

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (RefPtr extensionController = m_extensionController)
        extensionController->inspectorFrontendLoaded();
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

    if (RefPtr inspectorPage = m_inspectorPage.get(); inspectorPage && !m_underTest)
        inspectorPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::SetDockingUnavailable(!m_canAttach), inspectorPage->webPageIDInMainFrameProcess());

    platformAttachAvailabilityChanged(m_canAttach);
}

void WebInspectorUIProxy::setForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    platformSetForcedAppearance(appearance);
}

void WebInspectorUIProxy::effectiveAppearanceDidChange(InspectorFrontendClient::Appearance appearance)
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    if (!m_extensionController)
        return;

    ASSERT(appearance == WebCore::InspectorFrontendClient::Appearance::Dark || appearance == WebCore::InspectorFrontendClient::Appearance::Light);
    auto extensionAppearance = appearance == WebCore::InspectorFrontendClient::Appearance::Dark ? Inspector::ExtensionAppearance::Dark : Inspector::ExtensionAppearance::Light;

    protectedExtensionController()->effectiveAppearanceDidChange(extensionAppearance);
#endif
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
    RefPtr inspectorPage = m_inspectorPage.get();
    if (!inspectorPage)
        return;

    inspectorPage->protectedPreferences()->setDeveloperExtrasEnabled(enabled);
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
        if (RefPtr inspectedPage = m_inspectedPage.get())
            inspectedPage->protectedWebsiteDataStore()->setPrivateClickMeasurementDebugMode(overrideValue && overrideValue.value());
        return;

    case InspectorClient::DeveloperPreference::ITPDebugModeEnabled:
        if (RefPtr inspectedPage = m_inspectedPage.get())
            inspectedPage->protectedWebsiteDataStore()->setResourceLoadStatisticsDebugMode(overrideValue && overrideValue.value());
        return;

    case InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled:
#if ENABLE(MEDIA_STREAM)
        if (RefPtr inspectedPage = m_inspectedPage.get())
            inspectedPage->setMockCaptureDevicesEnabledOverride(overrideValue);
#endif // ENABLE(MEDIA_STREAM)
        return;

    case InspectorClient::DeveloperPreference::NeedsSiteSpecificQuirks:
        if (RefPtr inspectedPage = m_inspectedPage.get())
            inspectedPage->protectedPreferences()->setNeedsSiteSpecificQuirksInspectorOverride(overrideValue);
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
    protectedInspectorPage()->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::SetDiagnosticLoggingAvailable(available), m_inspectorPage->webPageIDInMainFrameProcess());
#else
    UNUSED_PARAM(available);
#endif
}

void WebInspectorUIProxy::save(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    if (!protectedInspectedPage()->protectedPreferences()->developerExtrasEnabled())
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
    if (!protectedInspectedPage()->protectedPreferences()->developerExtrasEnabled())
        return;

    ASSERT(!path.isEmpty());
    if (path.isEmpty())
        return;

    platformLoad(path, WTFMove(completionHandler));
}

void WebInspectorUIProxy::pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color> &)>&& completionHandler)
{
    if (!protectedInspectedPage()->protectedPreferences()->developerExtrasEnabled()) {
        completionHandler({ });
        return;
    }

    platformPickColorFromScreen(WTFMove(completionHandler));
}

bool WebInspectorUIProxy::shouldOpenAttached()
{
    return protectedInspectorPagePreferences()->inspectorStartsAttached() && canAttach();
}

void WebInspectorUIProxy::evaluateInFrontendForTesting(const String& expression)
{
    RefPtr inspectorPage = m_inspectorPage.get();
    if (!inspectorPage)
        return;

    inspectorPage->protectedLegacyMainFrameProcess()->send(Messages::WebInspectorUI::EvaluateInFrontendForTesting(expression), inspectorPage->webPageIDInMainFrameProcess());
}

#if ENABLE(INSPECTOR_EXTENSIONS)
RefPtr<WebInspectorUIExtensionControllerProxy> WebInspectorUIProxy::protectedExtensionController() const
{
    return extensionController();
}
#endif

// Unsupported configurations can use the stubs provided here.

#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN) && !ENABLE(WPE_PLATFORM)

RefPtr<WebPageProxy> WebInspectorUIProxy::platformCreateFrontendPage()
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
