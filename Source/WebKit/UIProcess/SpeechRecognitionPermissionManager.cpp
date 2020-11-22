/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SpeechRecognitionPermissionManager.h"

#include "APISecurityOrigin.h"
#include "APIUIClient.h"
#include "MediaPermissionUtilities.h"

namespace WebKit {

static SpeechRecognitionPermissionManager::CheckResult computeMicrophoneAccess()
{
#if HAVE(AVCAPTUREDEVICE)
    auto result = checkAVCaptureAccessForType(MediaPermissionType::Audio);
    if (result != MediaPermissionResult::Unknown)
        return result == MediaPermissionResult::Granted ? SpeechRecognitionPermissionManager::CheckResult::Granted : SpeechRecognitionPermissionManager::CheckResult::Denied;
    
    if (!checkUsageDescriptionStringForType(MediaPermissionType::Audio))
        return SpeechRecognitionPermissionManager::CheckResult::Denied;

    return SpeechRecognitionPermissionManager::CheckResult::Unknown;
#else
    return SpeechRecognitionPermissionManager::CheckResult::Granted;
#endif
}

static SpeechRecognitionPermissionManager::CheckResult computeSpeechRecognitionServiceAccess()
{
#if HAVE(SPEECHRECOGNIZER)
    auto result = checkSpeechRecognitionServiceAccess();
    if (result != MediaPermissionResult::Unknown)
        return result == MediaPermissionResult::Granted ? SpeechRecognitionPermissionManager::CheckResult::Granted : SpeechRecognitionPermissionManager::CheckResult::Denied;

    if (!checkUsageDescriptionStringForSpeechRecognition())
        return SpeechRecognitionPermissionManager::CheckResult::Denied;

    return SpeechRecognitionPermissionManager::CheckResult::Unknown;
#else
    return SpeechRecognitionPermissionManager::CheckResult::Granted;
#endif
}

SpeechRecognitionPermissionManager::SpeechRecognitionPermissionManager(WebPageProxy& page)
    : m_page(page)
{
}

SpeechRecognitionPermissionManager::~SpeechRecognitionPermissionManager()
{
    for (auto& request : m_requests)
        request->complete(SpeechRecognitionPermissionDecision::Deny);
}
    
void SpeechRecognitionPermissionManager::request(const WebCore::ClientOrigin& origin, CompletionHandler<void(SpeechRecognitionPermissionDecision)>&& completiontHandler)
{
    m_requests.append(SpeechRecognitionPermissionRequest::create(origin, WTFMove(completiontHandler)));
    if (m_requests.size() == 1)
        startNextRequest();
}

void SpeechRecognitionPermissionManager::startNextRequest()
{
    if (m_requests.isEmpty())
        return;

    startProcessingRequest();
}

void SpeechRecognitionPermissionManager::startProcessingRequest()
{
#if PLATFORM(COOCA)
    if (!checkSandboxRequirementForType(MediaPermissionType::Audio)) {
        completeCurrentRequest(SpeechRecognitionPermissionDecision::Deny);
        return;
    }
#endif

    // TCC status may have changed between requests.
    m_microphoneCheck = computeMicrophoneAccess();
    m_speechRecognitionServiceCheck = computeSpeechRecognitionServiceAccess();

    if (m_page.preferences().mockCaptureDevicesEnabled()) {
        m_page.syncIfMockDevicesEnabledChanged();
        m_microphoneCheck = CheckResult::Granted;
        m_speechRecognitionServiceCheck = CheckResult::Granted;
    }

    // We currently don't allow third-party access.
    if (m_userPermissionCheck == CheckResult::Unknown) {
        auto clientOrigin = m_requests.first()->origin();
        auto requestingOrigin = clientOrigin.clientOrigin.securityOrigin();
        auto topOrigin = clientOrigin.topOrigin.securityOrigin();
        if (!requestingOrigin->isSameOriginAs(topOrigin))
            m_userPermissionCheck = CheckResult::Denied;
    }

    if (m_microphoneCheck == CheckResult::Denied || m_speechRecognitionServiceCheck == CheckResult::Denied || m_userPermissionCheck == CheckResult::Denied) {
        completeCurrentRequest(SpeechRecognitionPermissionDecision::Deny);
        return;
    }

    continueProcessingRequest();
}

void SpeechRecognitionPermissionManager::continueProcessingRequest()
{
    if (m_speechRecognitionServiceCheck == CheckResult::Unknown) {
        requestSpeechRecognitionServiceAccess();
        return;
    }
    ASSERT(m_speechRecognitionServiceCheck == CheckResult::Granted);

    if (m_microphoneCheck == CheckResult::Unknown) {
        requestMicrophoneAccess();
        return;
    }
    ASSERT(m_microphoneCheck == CheckResult::Granted);

    if (m_userPermissionCheck == CheckResult::Unknown) {
        requestUserPermission();
        return;
    }
    ASSERT(m_userPermissionCheck == CheckResult::Granted);

    completeCurrentRequest(SpeechRecognitionPermissionDecision::Grant);
}

void SpeechRecognitionPermissionManager::completeCurrentRequest(SpeechRecognitionPermissionDecision decision)
{
    ASSERT(!m_requests.isEmpty());
    auto currentRequest = m_requests.takeFirst();
    currentRequest->complete(decision);

    startNextRequest();
}

void SpeechRecognitionPermissionManager::requestSpeechRecognitionServiceAccess()
{
    ASSERT(m_speechRecognitionServiceCheck == CheckResult::Unknown);

#if HAVE(SPEECHRECOGNIZER)
    requestSpeechRecognitionAccess([this, weakThis = makeWeakPtr(this)](bool authorized) mutable {
        if (!weakThis)
            return;

        m_speechRecognitionServiceCheck = authorized ? CheckResult::Granted : CheckResult::Denied;
        if (m_speechRecognitionServiceCheck == CheckResult::Denied) {
            completeCurrentRequest(SpeechRecognitionPermissionDecision::Deny);
            return;
        }

        continueProcessingRequest();
    });
#endif
}

void SpeechRecognitionPermissionManager::requestMicrophoneAccess()
{
    ASSERT(m_microphoneCheck == CheckResult::Unknown);

#if HAVE(AVCAPTUREDEVICE)
    requestAVCaptureAccessForType(MediaPermissionType::Audio, [this, weakThis = makeWeakPtr(this)](bool authorized) {
        if (!weakThis)
            return;

        m_microphoneCheck = authorized ? CheckResult::Granted : CheckResult::Denied;
        if (m_microphoneCheck == CheckResult::Denied) {
            completeCurrentRequest(SpeechRecognitionPermissionDecision::Deny);
            return;
        }

        continueProcessingRequest();
    });
#endif
}

void SpeechRecognitionPermissionManager::requestUserPermission()
{
    ASSERT(!m_requests.isEmpty());

    auto& currentRequest = m_requests.first();
    auto clientOrigin = currentRequest->origin();
    auto topOrigin = clientOrigin.topOrigin.securityOrigin();
    auto decisionHandler = [this, weakThis = makeWeakPtr(*this)](bool granted) {
        if (!weakThis)
            return;

        m_userPermissionCheck = granted ? CheckResult::Granted : CheckResult::Denied;
        if (m_userPermissionCheck == CheckResult::Denied) {
            completeCurrentRequest(SpeechRecognitionPermissionDecision::Deny);
            return;
        }

        continueProcessingRequest();
    };
    m_page.uiClient().decidePolicyForSpeechRecognitionPermissionRequest(m_page, API::SecurityOrigin::create(topOrigin.get()).get(), WTFMove(decisionHandler));
}



void SpeechRecognitionPermissionManager::decideByDefaultAction(const WebCore::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(COCOA)
    OptionSet<MediaPermissionType> type = MediaPermissionType::Audio;
    alertForPermission(m_page, MediaPermissionReason::SpeechRecognition, type, origin, WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

} // namespace WebKit

