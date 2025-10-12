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
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SpeechRecognitionPermissionManager);

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

Ref<SpeechRecognitionPermissionManager> SpeechRecognitionPermissionManager::create(WebPageProxy& page)
{
    return adoptRef(*new SpeechRecognitionPermissionManager(page));
}

SpeechRecognitionPermissionManager::SpeechRecognitionPermissionManager(WebPageProxy& page)
    : m_page(page)
{
}

SpeechRecognitionPermissionManager::~SpeechRecognitionPermissionManager()
{
    for (auto& [request, frameInfo] : m_requests)
        Ref { request }->complete(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "Permission manager has exited"_s });
}

RefPtr<WebPageProxy> SpeechRecognitionPermissionManager::protectedPage() const
{
    return m_page.get();
}

void SpeechRecognitionPermissionManager::request(WebCore::SpeechRecognitionRequest& request, FrameInfoData&& frameInfo, SpeechRecognitionPermissionRequestCallback&& completiontHandler)
{
    m_requests.append({ SpeechRecognitionPermissionRequest::create(request, WTFMove(completiontHandler)), WTFMove(frameInfo) });
    if (m_requests.size() == 1)
        startNextRequest();
}

WebPageProxy* SpeechRecognitionPermissionManager::page()
{
    return m_page.get();
}

void SpeechRecognitionPermissionManager::startNextRequest()
{
    while (!m_requests.isEmpty() && !m_requests.first().first->request())
        m_requests.removeFirst();

    if (m_requests.isEmpty())
        return;

    startProcessingRequest();
}

void SpeechRecognitionPermissionManager::startProcessingRequest()
{
    Ref page = *this->page();
    page->syncIfMockDevicesEnabledChanged();
    if (page->protectedPreferences()->mockCaptureDevicesEnabled()) {
        m_microphoneCheck = CheckResult::Granted;
        m_speechRecognitionServiceCheck = CheckResult::Granted;
    } else {
        // TCC status may have changed between requests.
        m_microphoneCheck = computeMicrophoneAccess();
        if (m_microphoneCheck == CheckResult::Denied) {
            completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "Microphone permission check has failed"_s });
            return;
        }

        m_speechRecognitionServiceCheck = computeSpeechRecognitionServiceAccess();
        if (m_speechRecognitionServiceCheck == CheckResult::Denied) {
            completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::ServiceNotAllowed, "Speech recognition service permission check has failed"_s });
            return;
        }

#if HAVE(SPEECHRECOGNIZER)
        if (!checkSpeechRecognitionServiceAvailability(m_requests.first().first->request()->lang())) {
            completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::ServiceNotAllowed, "Speech recognition service is not available"_s });
            return;
        }
#endif
    }

    if (m_userPermissionCheck == CheckResult::Denied) {
        completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "User permission check has failed"_s });
        return;
    }

    continueProcessingRequest();
}

void SpeechRecognitionPermissionManager::continueProcessingRequest()
{
    ASSERT(!m_requests.isEmpty());
    auto recognitionRequest = m_requests.first().first->request();
    auto frameInfo = m_requests.first().second;
    if (!recognitionRequest) {
        completeCurrentRequest();
        return;
    }
        
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
        requestUserPermission(*recognitionRequest, WTFMove(frameInfo));
        return;
    }
    ASSERT(m_userPermissionCheck == CheckResult::Granted);

    if (!protectedPage()->isViewVisible()) {
        completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "Page is not visible to user"_s });
        return;
    }

    completeCurrentRequest();
}

void SpeechRecognitionPermissionManager::completeCurrentRequest(std::optional<WebCore::SpeechRecognitionError>&& error)
{
    ASSERT(!m_requests.isEmpty());
    Ref currentRequest = m_requests.takeFirst().first;
    currentRequest->complete(WTFMove(error));

    startNextRequest();
}

void SpeechRecognitionPermissionManager::requestSpeechRecognitionServiceAccess()
{
    ASSERT(m_speechRecognitionServiceCheck == CheckResult::Unknown);

#if HAVE(SPEECHRECOGNIZER)
    requestSpeechRecognitionAccess([weakThis = WeakPtr { *this }](bool authorized) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_speechRecognitionServiceCheck = authorized ? CheckResult::Granted : CheckResult::Denied;
        if (protectedThis->m_speechRecognitionServiceCheck == CheckResult::Denied) {
            protectedThis->completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::ServiceNotAllowed, "Speech recognition service permission check has failed"_s });
            return;
        }

        protectedThis->continueProcessingRequest();
    });
#endif
}

void SpeechRecognitionPermissionManager::requestMicrophoneAccess()
{
    ASSERT(m_microphoneCheck == CheckResult::Unknown);

#if HAVE(AVCAPTUREDEVICE)
    requestAVCaptureAccessForType(MediaPermissionType::Audio, [weakThis = WeakPtr { *this }](bool authorized) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_microphoneCheck = authorized ? CheckResult::Granted : CheckResult::Denied;
        if (protectedThis->m_microphoneCheck == CheckResult::Denied) {
            protectedThis->completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "Microphone permission check has failed"_s });
            return;
        }

        protectedThis->continueProcessingRequest();
    });
#endif
}

void SpeechRecognitionPermissionManager::requestUserPermission(WebCore::SpeechRecognitionRequest& recognitionRequest, FrameInfoData&& frameInfo)
{
    auto clientOrigin = recognitionRequest.clientOrigin();
    auto requestingOrigin = clientOrigin.clientOrigin.securityOrigin();
    auto topOrigin = clientOrigin.topOrigin.securityOrigin();
    auto decisionHandler = [weakThis = WeakPtr { *this }](bool granted) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_userPermissionCheck = granted ? CheckResult::Granted : CheckResult::Denied;
        if (protectedThis->m_userPermissionCheck == CheckResult::Denied) {
            protectedThis->completeCurrentRequest(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "User permission check has failed"_s });
            return;
        }

        protectedThis->continueProcessingRequest();
    };
    protectedPage()->requestUserMediaPermissionForSpeechRecognition(recognitionRequest.mainFrameIdentifier(), WTFMove(frameInfo), requestingOrigin, topOrigin, WTFMove(decisionHandler));
}

void SpeechRecognitionPermissionManager::decideByDefaultAction(const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(COCOA)
    if (RefPtr page = m_page.get())
        alertForPermission(*page, MediaPermissionReason::SpeechRecognition, origin, WTFMove(completionHandler));
    else
        completionHandler(false);
#else
    completionHandler(false);
#endif
}

} // namespace WebKit

