/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UserMediaController.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "LocalDOMWindow.h"
#include "PermissionsPolicy.h"
#include "RealtimeMediaSourceCenter.h"
#include "UserMediaRequest.h"
#include <algorithm>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(UserMediaController);

ASCIILiteral UserMediaController::supplementName()
{
    return "UserMediaController"_s;
}

UserMediaController::UserMediaController(Ref<UserMediaClient>&& client)
    : m_client(WTFMove(client))
{
}

void provideUserMediaTo(Page* page, Ref<UserMediaClient>&& client)
{
    UserMediaController::provideTo(page, UserMediaController::supplementName(), makeUnique<UserMediaController>(WTFMove(client)));
}

void UserMediaController::logGetUserMediaDenial(Document& document)
{
    if (RefPtr window = document.window())
        window->printErrorMessage("Not allowed to call getUserMedia."_s);
}

void UserMediaController::logGetDisplayMediaDenial(Document& document)
{
    if (RefPtr window = document.window())
        window->printErrorMessage("Not allowed to call getDisplayMedia."_s);
}

void UserMediaController::logEnumerateDevicesDenial(Document& document)
{
    // We redo the check to print to the console log.
    PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Camera, document);
    PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, document);
    if (RefPtr window = document.window())
        window->printErrorMessage("Not allowed to call enumerateDevices."_s);
}

void UserMediaController::setShouldListenToVoiceActivity(Document& document, bool shouldListen)
{
    if (shouldListen) {
        ASSERT(!m_voiceActivityDocuments.contains(document));
        m_voiceActivityDocuments.add(document);
    } else {
        ASSERT(m_voiceActivityDocuments.contains(document));
        m_voiceActivityDocuments.remove(document);
    }
    checkDocumentForVoiceActivity(nullptr);
}

void UserMediaController::checkDocumentForVoiceActivity(const Document* document)
{
    if (document) {
        if (m_shouldListenToVoiceActivity == document->mediaState().containsAny(MediaProducer::IsCapturingAudioMask))
            return;
    }

    bool shouldListenToVoiceActivity = std::ranges::any_of(m_voiceActivityDocuments, [](auto& document) {
        return document.mediaState().containsAny(MediaProducer::IsCapturingAudioMask);
    });
    if (m_shouldListenToVoiceActivity == shouldListenToVoiceActivity)
        return;

    m_shouldListenToVoiceActivity = shouldListenToVoiceActivity;
    if (RefPtr mediaClient = m_client)
        mediaClient->setShouldListenToVoiceActivity(m_shouldListenToVoiceActivity);
}

void UserMediaController::voiceActivityDetected()
{
#if ENABLE(MEDIA_SESSION)
    for (RefPtr document : m_voiceActivityDocuments)
        document->voiceActivityDetected();

#endif
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
