/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PlatformXRSystem.h"

#if ENABLE(WEBXR)

#include "GPUProcessProxy.h"
#include "MessageSenderInlines.h"
#include "PlatformXRSystemMessages.h"
#include "PlatformXRSystemProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/GPUTextureFormat.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/XRCanvasConfiguration.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformXRSystem);

PlatformXRSystem::PlatformXRSystem(WebPageProxy& page)
    : m_page(page)
{
    protect(page.legacyMainFrameProcess())->addMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), page.webPageIDInMainFrameProcess(), *this);
}

PlatformXRSystem::~PlatformXRSystem()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    protect(page->legacyMainFrameProcess())->removeMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), page->webPageIDInMainFrameProcess());
}

std::optional<SharedPreferencesForWebProcess> PlatformXRSystem::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

void PlatformXRSystem::invalidate(InvalidationReason reason)
{
    ASSERT(RunLoop::isMain());
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (m_immersiveSessionState == ImmersiveSessionState::Idle)
        return;

    if (xrCoordinator())
        xrCoordinator()->endSessionIfExists(*page);

    invalidateImmersiveSessionState(reason == InvalidationReason::Client ? ImmersiveSessionState::SessionEndingFromSystem : ImmersiveSessionState::Idle);
}

void PlatformXRSystem::ensureImmersiveSessionActivity()
{
    ASSERT(RunLoop::isMain());

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (m_immersiveSessionActivity && m_immersiveSessionActivity->isValid())
        return;

    m_immersiveSessionActivity = protect(protect(page->legacyMainFrameProcess())->throttler())->foregroundActivity("XR immersive session"_s);
}

void PlatformXRSystem::enumerateImmersiveXRDevices(CompletionHandler<void(Vector<XRDeviceInfo>&&)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler({ });
        return;
    }

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler({ });
        return;
    }

    xrCoordinator->getPrimaryDeviceInfo(*page, [completionHandler = WTF::move(completionHandler)](std::optional<XRDeviceInfo> deviceInfo) mutable {
        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), deviceInfo = WTF::move(deviceInfo)]() mutable {
            if (!deviceInfo) {
                completionHandler({ });
                return;
            }

            completionHandler({ deviceInfo.value() });
        });
    });
}

static bool checkFeaturesConsent(const std::optional<PlatformXR::Device::FeatureList>& requestedFeatures, const std::optional<PlatformXR::Device::FeatureList>& grantedFeatures)
{
    if (!grantedFeatures || !requestedFeatures)
        return false;

    bool result = true;
    for (auto requestedFeature : *requestedFeatures) {
        if (!grantedFeatures->contains(requestedFeature)) {
            result = false;
            break;
        }
    }
    return result;
}

void PlatformXRSystem::requestPermissionOnSessionFeatures(IPC::Connection& connection, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(granted);
        return;
    }

    if (!PlatformXR::isImmersive(mode)) {
        completionHandler(granted);
        return;
    }

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler(granted);
        return;
    }

    MESSAGE_CHECK_COMPLETION(m_immersiveSessionState == ImmersiveSessionState::Idle || m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromWebContent, connection, completionHandler({ }));
    setImmersiveSessionState(ImmersiveSessionState::RequestingPermissions, [](bool) mutable { });
    m_immersiveSessionGrantedFeatures = std::nullopt;

    xrCoordinator->requestPermissionOnSessionFeatures(*page, securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, [weakThis = WeakPtr { *this }, mode, securityOriginData, consentRequired, completionHandler = WTF::move(completionHandler)](std::optional<PlatformXR::Device::FeatureList>&& grantedFeatures) mutable {
        ASSERT(RunLoop::isMain());
        auto protectedThis = weakThis.get();
        if (protectedThis && PlatformXR::isImmersive(mode)) {
            if (checkFeaturesConsent(consentRequired, grantedFeatures)) {
                protectedThis->m_immersiveSessionMode = mode;
                protectedThis->m_immersiveSessionGrantedFeatures = grantedFeatures;
                protectedThis->m_immersiveSessionSecurityOriginData = securityOriginData;
                protectedThis->setImmersiveSessionState(ImmersiveSessionState::PermissionsGranted, [grantedFeatures = WTF::move(grantedFeatures), completionHandler = WTF::move(completionHandler)](bool) mutable {
                    completionHandler(WTF::move(grantedFeatures));
                });
            } else {
                protectedThis->invalidateImmersiveSessionState();
                completionHandler(WTF::move(grantedFeatures));
            }
        } else
            completionHandler(WTF::move(grantedFeatures));
    });
}

void PlatformXRSystem::initializeTrackingAndRendering(IPC::Connection& connection, std::optional<WebCore::WebGPU::TextureFormat> colorFormat, std::optional<WebCore::WebGPU::TextureFormat> depthStencilFormat)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionMode, connection);
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::PermissionsGranted, connection);
    MESSAGE_CHECK(m_immersiveSessionSecurityOriginData, connection);
    MESSAGE_CHECK(m_immersiveSessionGrantedFeatures && !m_immersiveSessionGrantedFeatures->isEmpty(), connection);

    RefPtr page = m_page.get();
    if (!page)
        return;

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator)
        return;

    setImmersiveSessionState(ImmersiveSessionState::SessionRunning, [](bool) mutable { });

    ensureImmersiveSessionActivity();

    WeakPtr weakThis { *this };
    std::optional<WebCore::XRCanvasConfiguration> init;
    if (colorFormat) {
        init = WebCore::XRCanvasConfiguration();
        init->colorFormat = colorFormat;
        init->depthStencilFormat = depthStencilFormat;
    }
    xrCoordinator->startSession(*page, weakThis, *m_immersiveSessionSecurityOriginData, *m_immersiveSessionMode, *m_immersiveSessionGrantedFeatures, WTF::move(init));
}

void PlatformXRSystem::shutDownTrackingAndRendering(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning, connection);

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->endSessionIfExists(*page);
    setImmersiveSessionState(ImmersiveSessionState::SessionEndingFromWebContent, [](bool) mutable { });
}

void PlatformXRSystem::requestFrame(IPC::Connection& connection, std::optional<PlatformXR::RequestData>&& requestData, CompletionHandler<void(PlatformXR::FrameData&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK_COMPLETION(m_immersiveSessionState == ImmersiveSessionState::SessionRunning || m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromSystem, connection, completionHandler({ }));
    if (m_immersiveSessionState != ImmersiveSessionState::SessionRunning) {
        completionHandler({ });
        return;
    }

    RefPtr page = m_page.get();
    if (!page) {
        completionHandler({ });
        return;
    }

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->scheduleAnimationFrame(*page, WTF::move(requestData), WTF::move(completionHandler));
    else
        completionHandler({ });
}

#if USE(OPENXR)
void PlatformXRSystem::submitFrame(IPC::Connection& connection, Vector<XRDeviceLayer>&& layers)
#else
void PlatformXRSystem::submitFrame(IPC::Connection& connection)
#endif
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning || m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromSystem, connection);
    if (m_immersiveSessionState != ImmersiveSessionState::SessionRunning)
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator()) {
#if USE(OPENXR)
        xrCoordinator->submitFrame(*page, WTF::move(layers));
#else
        xrCoordinator->submitFrame(*page);
#endif
    }
}

#if ENABLE(WEBXR_HIT_TEST)
void PlatformXRSystem::requestHitTestSource(const PlatformXR::HitTestOptions& hitTestOptions, CompletionHandler<void(Expected<PlatformXR::HitTestSource, WebCore::ExceptionData>)>&& passedCompletionHandler)
{
    auto completionHandler = [passedCompletionHandler = WTF::move(passedCompletionHandler)](WebCore::ExceptionOr<PlatformXR::HitTestSource> exceptionOrValue) mutable {
        if (exceptionOrValue.hasException()) {
            auto exception = exceptionOrValue.releaseException();
            passedCompletionHandler(makeUnexpected(WebCore::ExceptionData { exception.code(), exception.releaseMessage() }));
        } else
            passedCompletionHandler(exceptionOrValue.releaseReturnValue());
    };
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
        return;
    }
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
        return;
    }
    xrCoordinator->requestHitTestSource(*page, hitTestOptions, WTF::move(completionHandler));
}

void PlatformXRSystem::deleteHitTestSource(PlatformXR::HitTestSource source)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->deleteHitTestSource(*page, source);
}

void PlatformXRSystem::requestTransientInputHitTestSource(const PlatformXR::TransientInputHitTestOptions& hitTestOptions, CompletionHandler<void(Expected<PlatformXR::TransientInputHitTestSource, WebCore::ExceptionData>)>&& passedCompletionHandler)
{
    auto completionHandler = [passedCompletionHandler = WTF::move(passedCompletionHandler)](WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource> exceptionOrValue) mutable {
        if (exceptionOrValue.hasException()) {
            auto exception = exceptionOrValue.releaseException();
            passedCompletionHandler(makeUnexpected(WebCore::ExceptionData { exception.code(), exception.releaseMessage() }));
        } else
            passedCompletionHandler(exceptionOrValue.releaseReturnValue());
    };
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
        return;
    }
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError });
        return;
    }
    xrCoordinator->requestTransientInputHitTestSource(*page, hitTestOptions, WTF::move(completionHandler));
}

void PlatformXRSystem::deleteTransientInputHitTestSource(PlatformXR::TransientInputHitTestSource source)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->deleteTransientInputHitTestSource(*page, source);
}
#endif

void PlatformXRSystem::didCompleteShutdownTriggeredBySystem(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromSystem, connection);
    setImmersiveSessionState(ImmersiveSessionState::Idle, [](bool) mutable { });
}

void PlatformXRSystem::sessionDidEnd(XRDeviceIdentifier deviceIdentifier)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = protectedThis->m_page.get();
        if (!page)
            return;

        protect(page->legacyMainFrameProcess())->send(Messages::PlatformXRSystemProxy::SessionDidEnd(deviceIdentifier), page->webPageIDInMainFrameProcess());
        protectedThis->m_immersiveSessionActivity = nullptr;
        // If this is called when the session is running, the ending of the session is triggered by the system side
        // and we should set the state to SessionEndingFromSystem. We expect the web process to send a
        // didCompleteShutdownTriggeredBySystem message later when it has ended the XRSession, which will
        // reset the session state to Idle.
        protectedThis->invalidateImmersiveSessionState(protectedThis->m_immersiveSessionState == ImmersiveSessionState::SessionRunning ? ImmersiveSessionState::SessionEndingFromSystem : ImmersiveSessionState::Idle);
    });
}

void PlatformXRSystem::sessionDidUpdateVisibilityState(XRDeviceIdentifier deviceIdentifier, PlatformXR::VisibilityState visibilityState)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier, visibilityState]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = protectedThis->m_page.get();
        if (!page)
            return;

        protect(page->legacyMainFrameProcess())->send(Messages::PlatformXRSystemProxy::SessionDidUpdateVisibilityState(deviceIdentifier, visibilityState), page->webPageIDInMainFrameProcess());
    });
}

void PlatformXRSystem::setImmersiveSessionState(ImmersiveSessionState state, CompletionHandler<void(bool)>&& completion)
{
    m_immersiveSessionState = state;
#if PLATFORM(COCOA)
    RefPtr page = m_page.get();
    if (!page || page->isClosed()) {
        completion(false);
        return;
    }

    switch (state) {
    case ImmersiveSessionState::Idle:
    case ImmersiveSessionState::RequestingPermissions:
        break;
    case ImmersiveSessionState::PermissionsGranted:
        return GPUProcessProxy::getOrCreate()->webXRPromptAccepted(page->ensureRunningProcess().processIdentity(), WTF::move(completion));
    case ImmersiveSessionState::SessionRunning:
    case ImmersiveSessionState::SessionEndingFromWebContent:
    case ImmersiveSessionState::SessionEndingFromSystem:
        break;
    }
#endif
    completion(true);
}

void PlatformXRSystem::invalidateImmersiveSessionState(ImmersiveSessionState nextSessionState)
{
    ASSERT(RunLoop::isMain());

    m_immersiveSessionMode = std::nullopt;
    m_immersiveSessionSecurityOriginData = std::nullopt;
    m_immersiveSessionGrantedFeatures = std::nullopt;
    setImmersiveSessionState(nextSessionState, [](bool) mutable { });
}

bool PlatformXRSystem::webXREnabled() const
{
    RefPtr page = m_page.get();
    return page && protect(page->preferences())->webXREnabled();
}

#if !USE(APPLE_INTERNAL_SDK) && !USE(OPENXR)

PlatformXRCoordinator* PlatformXRSystem::xrCoordinator()
{
    return nullptr;
}

#endif // !USE(APPLE_INTERNAL_SDK) && !USE(OPENXR)

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK

#endif // ENABLE(WEBXR)
