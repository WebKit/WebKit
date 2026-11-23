/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CaptionTranscriberManager.h"

#if ENABLE(GPU_PROCESS) && HAVE(AVCAPTIONTRANSCRIBER)

#import "GPUConnectionToWebProcess.h"
#import "Logging.h"
#import "RemoteAudioVideoRendererProxyManager.h"
#import <AVFoundation/AVFoundation.h>
#import <WebCore/AudioVideoRenderer.h>
#import <WebCore/InbandGenericCue.h>
#import <WebCore/MediaSample.h>
#import <cmath>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/darwin/DispatchExtras.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

@interface WKAVCaptionTranscriberDelegate : NSObject <AVCaptionTranscriberDelegate> {
    WeakPtr<WebKit::CaptionTranscriberManager> _manager;
}

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithManager:(WebKit::CaptionTranscriberManager&)manager NS_DESIGNATED_INITIALIZER;
- (void)invalidate;
@end

@implementation WKAVCaptionTranscriberDelegate

- (instancetype)initWithManager:(WebKit::CaptionTranscriberManager&)manager
{
    if (!(self = [super init]))
        return nil;

    _manager = WeakPtr { manager };
    return self;
}

- (void)invalidate
{
    _manager = nullptr;
}

- (void)captionTranscriberDidOutputCaptionGroups:(AVCaptionTranscriber *)transcriber
{
    if (auto manager = _manager.get())
        manager->captionTranscriberDidOutputCaptionGroups(transcriber);
}

- (void)captionTranscriber:(AVCaptionTranscriber *)transcriber didFinishWithError:(NSError *)error
{
    if (auto manager = _manager.get())
        manager->captionTranscriberDidFinish(transcriber, error);
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CaptionTranscriberManager);

CaptionTranscriberManager::CaptionTranscriberManager(GPUProcess& gpuProcess)
{
    gpuProcess.registerForNowPlayingOwnerChanges(*this);

    if (auto& owner = gpuProcess.nowPlayingOwner())
        m_ownerSession = WebCore::QualifiedMediaSessionIdentifier { owner->session, owner->process };
    RELEASE_LOG(Media, "CaptionTranscriberManager: created, now-playing session %" PRIu64 " (0 means none)", m_ownerSession ? m_ownerSession->object().toUInt64() : 0);
}

CaptionTranscriberManager::~CaptionTranscriberManager()
{
    deactivate(NotifyWebProcess::No);
}

void CaptionTranscriberManager::nowPlayingOwnerDidChange(const std::optional<GPUProcess::NowPlayingOwner>& owner)
{
    if (owner)
        m_ownerSession = WebCore::QualifiedMediaSessionIdentifier { owner->session, owner->process };
    else
        m_ownerSession = std::nullopt;
    RELEASE_LOG(Media, "CaptionTranscriberManager::nowPlayingOwnerDidChange: now-playing session %" PRIu64 " (0 means none)", m_ownerSession ? m_ownerSession->object().toUInt64() : 0);

    updateActiveSession();
}

void CaptionTranscriberManager::rendererSessionsDidChange()
{
    RELEASE_LOG(Media, "CaptionTranscriberManager::rendererSessionsDidChange");
    updateActiveSession();
}

static std::optional<RemoteAudioVideoRendererProxyManager::RendererForMediaSession> rendererForSession(const std::optional<WebCore::QualifiedMediaSessionIdentifier>& session)
{
    if (!session)
        return std::nullopt;

    RefPtr connection = GPUProcess::singleton().webProcessConnection(session->processIdentifier());
    if (!connection)
        return std::nullopt;

    auto* proxyManager = connection->remoteAudioVideoRendererProxyManagerIfExists();
    if (!proxyManager)
        return std::nullopt;

    return proxyManager->rendererForMediaSession(session->object());
}

void CaptionTranscriberManager::updateActiveSession()
{
    if (!m_ownerSession) {
        RELEASE_LOG(Media, "CaptionTranscriberManager::updateActiveSession: no now-playing session, not transcribing");
        deactivate();
        return;
    }

    auto match = rendererForSession(m_ownerSession);
    if (!match) {
        RELEASE_LOG(Media, "CaptionTranscriberManager::updateActiveSession: no renderer is registered for now-playing session %" PRIu64 ", not transcribing", m_ownerSession->object().toUInt64());
        deactivate();
        return;
    }

    if (match->language.isEmpty()) {
        RELEASE_LOG(Media, "CaptionTranscriberManager::updateActiveSession: the audio language of session %" PRIu64 " is not known yet, not transcribing", m_ownerSession->object().toUInt64());
        deactivate();
        return;
    }

    if (m_active && m_active->session == *m_ownerSession && m_active->identifier == match->identifier && m_active->language == match->language)
        return;

    deactivate();
    activate(*m_ownerSession, match->identifier, match->language, WTF::move(match->renderer));
}

void CaptionTranscriberManager::activate(WebCore::QualifiedMediaSessionIdentifier session, RemoteAudioVideoRendererIdentifier identifier, const String& language, Ref<WebCore::AudioVideoRenderer>&& renderer)
{
    ASSERT(!m_active);

    if (!m_mockEnabled && !createTranscriber(renderer.get(), language))
        return;

    m_active = ActiveRenderer { session, identifier, language, ThreadSafeWeakPtr { renderer.get() } };
    m_audioSampleCount = 0;
    m_mockNextCueSecond = std::nullopt;
    RELEASE_LOG(Media, "CaptionTranscriberManager::activate: now transcribing session %" PRIu64 ", language %{public}@, mock: %d", session.object().toUInt64(), language.createNSString().get(), m_mockEnabled);

    sendAvailability(true);

    renderer->setCaptionAudioObserver({
        .sampleEnqueued = [weakThis = WeakPtr { *this }](const WebCore::MediaSample& sample) {
            if (weakThis)
                weakThis->appendSample(sample);
        },
        .flushed = [weakThis = WeakPtr { *this }] {
            if (weakThis)
                weakThis->resetTranscriber();
        },
    });
}

bool CaptionTranscriberManager::createTranscriber(WebCore::AudioVideoRenderer& renderer, const String& language)
{
    CMTimebaseRef timebase = renderer.captionTimebase();
    if (!timebase) {
        RELEASE_LOG_ERROR(Media, "CaptionTranscriberManager::createTranscriber: the renderer has no timebase");
        return false;
    }

    RetainPtr languageIdentifier = [NSLocale canonicalLanguageIdentifierFromString:language.createNSString().get()];
    RetainPtr locale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:languageIdentifier.get()]);
    RetainPtr configuration = adoptNS([PAL::allocAVCaptionTranscriberConfigurationInstance() initWithSourceLocale:locale.get() targetLocale:locale.get()]);
    [configuration setTimebase:timebase];

    NSError *error = nil;
    m_transcriber = adoptNS([PAL::allocAVCaptionTranscriberInstance() initWithConfiguration:configuration.get() error:&error]);
    if (!m_transcriber) {
        RELEASE_LOG_ERROR(Media, "CaptionTranscriberManager::createTranscriber: failed to create a transcriber for language %{public}@: %{public}@", languageIdentifier.get(), error.localizedDescription);
        return false;
    }

    m_transcriberDelegate = adoptNS([[WKAVCaptionTranscriberDelegate alloc] initWithManager:*this]);
    [m_transcriber setDelegate:m_transcriberDelegate.get() queue:mainDispatchQueueSingleton()];
    RELEASE_LOG(Media, "CaptionTranscriberManager::createTranscriber: created a transcriber for language %{public}@", languageIdentifier.get());
    return true;
}

void CaptionTranscriberManager::deactivate(NotifyWebProcess notifyWebProcess)
{
    if (!m_active)
        return;

    RELEASE_LOG(Media, "CaptionTranscriberManager::deactivate: session %" PRIu64, m_active->session.object().toUInt64());

    if (notifyWebProcess == NotifyWebProcess::Yes)
        sendAvailability(false);

    if (auto renderer = m_active->renderer.get())
        renderer->setCaptionAudioObserver({ });

    [m_transcriberDelegate invalidate];
    [m_transcriber setDelegate:nil queue:nil];
    [m_transcriber cancel];
    m_transcriberDelegate = nil;
    m_transcriber = nil;
    m_active = std::nullopt;
}

void CaptionTranscriberManager::appendSample(const WebCore::MediaSample& sample)
{
    ++m_audioSampleCount;
    if (m_audioSampleCount == 1 || !(m_audioSampleCount % 100))
        RELEASE_LOG(Media, "CaptionTranscriberManager::appendSample: received %" PRIu64 " audio samples so far, latest presentation time %f", m_audioSampleCount, sample.presentationTime().toDouble());

    if (m_mockEnabled) {
        appendMockSample(sample);
        return;
    }

    if (!m_transcriber)
        return;

    NSError *error = nil;
    RetainPtr cmSampleBuffer = sample.platformSample().cmSampleBuffer();
    if ([m_transcriber isReadyForMoreInput]
        && ![m_transcriber appendSampleBuffer:cmSampleBuffer.get() error:&error]
        && [m_transcriber status] == AVCaptionGenerationStatusRunning) {
        RELEASE_LOG_ERROR(Media, "CaptionTranscriberManager::appendSample: audio sample transcription failed: %{public}@", error.localizedDescription);
    }
}

void CaptionTranscriberManager::resetTranscriber()
{
    [m_transcriber reset];
}

void CaptionTranscriberManager::appendMockSample(const WebCore::MediaSample& sample)
{
    auto second = std::max<int64_t>(0, static_cast<int64_t>(std::floor(sample.presentationTime().toDouble())));
    if (!m_mockNextCueSecond)
        m_mockNextCueSecond = second;

    Vector<WebCore::GenericCueData> cues;
    for (; *m_mockNextCueSecond <= second; ++*m_mockNextCueSecond) {
        WebCore::GenericCueData cue;
        cue.m_uniqueId = WebCore::InbandGenericCueIdentifier::generate();
        cue.m_startTime = MediaTime { *m_mockNextCueSecond, 1 };
        cue.m_endTime = MediaTime { *m_mockNextCueSecond + 1, 1 };
        cue.m_content = makeString("Mock caption "_s, *m_mockNextCueSecond);
        cue.m_status = WebCore::GenericCueData::Status::Complete;
        cues.append(WTF::move(cue));
    }
    deliverCues(WTF::move(cues));
}

void CaptionTranscriberManager::sendAvailability(bool available)
{
    if (!m_active)
        return;

    RefPtr connection = GPUProcess::singleton().webProcessConnection(m_active->session.processIdentifier());
    if (!connection)
        return;

    if (auto* proxyManager = connection->remoteAudioVideoRendererProxyManagerIfExists())
        proxyManager->sendCaptionTranscriptionAvailability(m_active->identifier, available);
}

void CaptionTranscriberManager::deliverCues(Vector<WebCore::GenericCueData>&& cues)
{
    if (cues.isEmpty())
        return;

    if (!m_active) {
        RELEASE_LOG(Media, "CaptionTranscriberManager::deliverCues: dropping %zu cues, nothing is active", cues.size());
        return;
    }

    RefPtr connection = GPUProcess::singleton().webProcessConnection(m_active->session.processIdentifier());
    if (!connection) {
        RELEASE_LOG(Media, "CaptionTranscriberManager::deliverCues: dropping %zu cues, the web process is gone", cues.size());
        return;
    }

    auto* proxyManager = connection->remoteAudioVideoRendererProxyManagerIfExists();
    if (!proxyManager) {
        RELEASE_LOG(Media, "CaptionTranscriberManager::deliverCues: dropping %zu cues, the web process has no renderers", cues.size());
        return;
    }

    RELEASE_LOG(Media, "CaptionTranscriberManager::deliverCues: sending %zu cues to the web process", cues.size());
    proxyManager->sendTranscribedCues(m_active->identifier, WTF::move(cues));
}

void CaptionTranscriberManager::setMockEnabledForTesting(bool enabled)
{
    if (m_mockEnabled == enabled)
        return;

    RELEASE_LOG(Media, "CaptionTranscriberManager::setMockEnabledForTesting: %d", enabled);

    deactivate();
    m_mockEnabled = enabled;
    updateActiveSession();
}

void CaptionTranscriberManager::captionTranscriberDidOutputCaptionGroups(AVCaptionTranscriber *transcriber)
{
    ASSERT(isMainRunLoop());
    if (transcriber != m_transcriber.get())
        return;

    AVCaptionTranscriberOutputFlags flags = 0;
    while (RetainPtr captionGroup = [transcriber nextCaptionGroupWithFlags:&flags]) {
        RELEASE_LOG_DEBUG(Media, "CaptionTranscriberManager: caption group generated, sequence was restarted: %d", !!(flags & AVCaptionTranscriberOutputFlagSequenceWasRestarted));

        auto groupStart = PAL::toMediaTime([captionGroup timeRange].start);
        auto groupEnd = groupStart + PAL::toMediaTime([captionGroup timeRange].duration);
        RELEASE_LOG_DEBUG(Media, "CaptionTranscriberManager: group range: [%f, %f), captions: %lu", groupStart.toDouble(), groupEnd.toDouble(), static_cast<unsigned long>([[captionGroup captions] count]));

        Vector<WebCore::GenericCueData> cues;
        for (AVCaption *caption in [captionGroup captions]) {
            CMTimeRange captionTimeRange = [caption timeRange];
            auto captionStart = PAL::toMediaTime(captionTimeRange.start);
            auto captionEnd = captionStart + PAL::toMediaTime(captionTimeRange.duration);
            RELEASE_LOG_DEBUG(Media, "CaptionTranscriberManager: caption range: [%f, %f), starts before group: %d", captionStart.toDouble(), captionEnd.toDouble(), captionStart < groupStart);
            if (captionStart < groupStart)
                continue;

            String text { [caption text] };
            if (text.isEmpty())
                continue;

            WebCore::GenericCueData cue;
            cue.m_uniqueId = WebCore::InbandGenericCueIdentifier::generate();
            cue.m_startTime = captionStart;
            cue.m_endTime = captionEnd;
            cue.m_content = text;
            cue.m_status = WebCore::GenericCueData::Status::Complete;
            cues.append(WTF::move(cue));
        }

        deliverCues(WTF::move(cues));
    }
}

void CaptionTranscriberManager::captionTranscriberDidFinish(AVCaptionTranscriber *transcriber, NSError *error)
{
    ASSERT(isMainRunLoop());
    if (transcriber != m_transcriber.get())
        return;

    if (!error) {
        RELEASE_LOG(Media, "CaptionTranscriberManager: caption generation finished");
        return;
    }
    RELEASE_LOG_ERROR(Media, "CaptionTranscriberManager: caption generation failed: %{public}@", error.localizedDescription);
}

}

#endif
