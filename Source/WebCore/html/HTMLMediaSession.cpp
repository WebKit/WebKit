/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "HTMLMediaSession.h"

#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaSessionManager.h"
#include "Page.h"
#include "ScriptController.h"

#if PLATFORM(IOS)
#include "AudioSession.h"
#include "RuntimeApplicationChecksIOS.h"
#endif

namespace WebCore {

#if !LOG_DISABLED
static const char* restrictionName(HTMLMediaSession::BehaviorRestrictions restriction)
{
#define CASE(restriction) case HTMLMediaSession::restriction: return #restriction
    switch (restriction) {
    CASE(NoRestrictions);
    CASE(RequireUserGestureForLoad);
    CASE(RequireUserGestureForRateChange);
    CASE(RequireUserGestureForFullscreen);
    CASE(RequirePageConsentToLoadMedia);
    CASE(RequirePageConsentToResumeMedia);
#if ENABLE(IOS_AIRPLAY)
    CASE(RequireUserGestureToShowPlaybackTargetPicker);
#endif
    }

    ASSERT_NOT_REACHED();
    return "";
}
#endif

static void initializeAudioSession()
{
#if PLATFORM(IOS)
    static bool wasAudioSessionInitialized;
    if (wasAudioSessionInitialized)
        return;

    wasAudioSessionInitialized = true;
    if (!WebCore::applicationIsMobileSafari())
        return;

    AudioSession::sharedSession().setCategory(AudioSession::MediaPlayback);
#endif
}

std::unique_ptr<HTMLMediaSession> HTMLMediaSession::create(MediaSessionClient& client)
{
    return std::make_unique<HTMLMediaSession>(client);
}

HTMLMediaSession::HTMLMediaSession(MediaSessionClient& client)
    : MediaSession(client)
    , m_restrictions(NoRestrictions)
{
    initializeAudioSession();
}

void HTMLMediaSession::addBehaviorRestriction(BehaviorRestrictions restriction)
{
    LOG(Media, "HTMLMediaSession::addBehaviorRestriction - adding %s", restrictionName(restriction));
    m_restrictions |= restriction;
}

void HTMLMediaSession::removeBehaviorRestriction(BehaviorRestrictions restriction)
{
    LOG(Media, "HTMLMediaSession::removeBehaviorRestriction - removing %s", restrictionName(restriction));
    m_restrictions &= ~restriction;
}

bool HTMLMediaSession::playbackPermitted(const HTMLMediaElement&) const
{
    if (m_restrictions & RequireUserGestureForRateChange && !ScriptController::processingUserGesture()) {
        LOG(Media, "HTMLMediaSession::playbackPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool HTMLMediaSession::dataLoadingPermitted(const HTMLMediaElement&) const
{
    if (m_restrictions & RequireUserGestureForLoad && !ScriptController::processingUserGesture()) {
        LOG(Media, "HTMLMediaSession::dataLoadingPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool HTMLMediaSession::fullscreenPermitted(const HTMLMediaElement&) const
{
    if (m_restrictions & RequireUserGestureForFullscreen && !ScriptController::processingUserGesture()) {
        LOG(Media, "HTMLMediaSession::fullscreenPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool HTMLMediaSession::pageAllowsDataLoading(const HTMLMediaElement& element) const
{
    Page* page = element.document().page();
    if (m_restrictions & RequirePageConsentToLoadMedia && page && !page->canStartMedia()) {
        LOG(Media, "HTMLMediaSession::pageAllowsDataLoading - returning FALSE");
        return false;
    }

    return true;
}

bool HTMLMediaSession::pageAllowsPlaybackAfterResuming(const HTMLMediaElement& element) const
{
    Page* page = element.document().page();
    if (m_restrictions & RequirePageConsentToResumeMedia && page && !page->canStartMedia()) {
        LOG(Media, "HTMLMediaSession::pageAllowsPlaybackAfterResuming - returning FALSE");
        return false;
    }

    return true;
}

#if ENABLE(IOS_AIRPLAY)
bool HTMLMediaSession::showingPlaybackTargetPickerPermitted(const HTMLMediaElement&) const
{
    if (m_restrictions & RequireUserGestureToShowPlaybackTargetPicker && !ScriptController::processingUserGesture()) {
        LOG(Media, "HTMLMediaSession::showingPlaybackTargetPickerPermitted - returning FALSE");
        return false;
    }

    return true;
}
#endif

MediaPlayer::Preload HTMLMediaSession::effectivePreloadForElement(const HTMLMediaElement& element) const
{
    MediaSessionManager::SessionRestrictions restrictions = MediaSessionManager::sharedManager().restrictions(mediaType());
    MediaPlayer::Preload preload = element.preloadValue();

    if ((restrictions & MediaSessionManager::MetadataPreloadingNotPermitted) == MediaSessionManager::MetadataPreloadingNotPermitted)
        return MediaPlayer::None;

    if ((restrictions & MediaSessionManager::AutoPreloadingNotPermitted) == MediaSessionManager::AutoPreloadingNotPermitted) {
        if (preload > MediaPlayer::MetaData)
            return MediaPlayer::MetaData;
    }

    return preload;
}

bool HTMLMediaSession::requiresFullscreenForVideoPlayback(const HTMLMediaElement& element) const
{
    if (!MediaSessionManager::sharedManager().sessionRestrictsInlineVideoPlayback(*this))
        return false;

    Settings* settings = element.document().settings();
    if (!settings || !settings->mediaPlaybackAllowsInline())
        return true;

    if (element.fastHasAttribute(HTMLNames::webkit_playsinlineAttr))
        return false;

#if PLATFORM(IOS)
    if (applicationIsDumpRenderTree())
        return false;
#endif

    return true;
}

}

#endif // ENABLE(VIDEO)
