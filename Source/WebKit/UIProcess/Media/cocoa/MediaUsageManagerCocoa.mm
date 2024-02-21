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

#import "config.h"
#import "MediaUsageManagerCocoa.h"

#if ENABLE(MEDIA_USAGE)

#import <WebCore/NotImplemented.h>
#import <pal/cocoa/UsageTrackingSoftLink.h>

NS_ASSUME_NONNULL_BEGIN

@interface USVideoUsage : NSObject
- (instancetype)initWithBundleIdentifier:(NSString *)bundleIdentifier URL:(NSURL *)url mediaURL:(NSURL *)mediaURL videoMetadata:(NSDictionary<NSString *, id> *)videoMetadata NS_DESIGNATED_INITIALIZER;
- (void)stop;
- (void)restart;
- (void)updateVideoMetadata:(NSDictionary<NSString *, id> *)videoMetadata;
@end

NS_ASSUME_NONNULL_END

namespace WebKit {
using namespace WebCore;

static bool usageTrackingAvailable()
{
    static bool available;

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] () {
        available = PAL::isUsageTrackingFrameworkAvailable()
            && PAL::getUSVideoUsageClass()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyCanShowControlsManager()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyCanShowNowPlayingControls()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsSuspended()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsInActiveDocument()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsFullscreen()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsMuted()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsMediaDocumentInMainFrame()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsAudio()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyAudioElementWithUserGesture()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyUserHasPlayedAudioBefore()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsElementRectMostlyInMainFrame()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyNoAudio()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyPlaybackPermitted()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyPageMediaPlaybackSuspended()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsMediaDocumentAndNotOwnerElement()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyPageExplicitlyAllowsElementToAutoplayInline()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyRequiresFullscreenForVideoPlaybackAndFullscreenNotPermitted()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoRateChange()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsAudioAndRequiresUserGestureForAudioRateChange()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoDueToLowPowerMode()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyNoUserGestureRequired()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyRequiresPlaybackAndIsNotPlaying()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyHasEverNotifiedAboutPlaying()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyOutsideOfFullscreen()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsVideo()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyRenderer()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyNoVideo()
            && PAL::canLoad_UsageTracking_USVideoMetadataKeyIsLargeEnoughForMainContent();
    });

    return available;
}

std::unique_ptr<MediaUsageManager> MediaUsageManager::create()
{
    return makeUnique<MediaUsageManagerCocoa>();
}

#if PLATFORM(COCOA) && !HAVE(CGS_FIX_FOR_RADAR_97530095)
bool MediaUsageManager::isPlayingVideoInViewport() const
{
    notImplemented();
    return false;
}
#endif

MediaUsageManagerCocoa::~MediaUsageManagerCocoa()
{
    reset();
}

void MediaUsageManagerCocoa::reset()
{
    for (auto& session : m_mediaSessions.values()) {
        if (session->usageTracker && session->mediaUsageInfo && session->mediaUsageInfo->isPlaying)
            [session->usageTracker stop];
    }
    m_mediaSessions.clear();
}

void MediaUsageManagerCocoa::addMediaSession(WebCore::MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
{
    auto addResult = m_mediaSessions.ensure(identifier, [&] {
        return makeUnique<MediaUsageManagerCocoa::SessionMediaUsage>(identifier, bundleIdentifier, pageURL);
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void MediaUsageManagerCocoa::removeMediaSession(WebCore::MediaSessionIdentifier identifier)
{
    ASSERT(m_mediaSessions.contains(identifier));
    m_mediaSessions.remove(identifier);
}

void MediaUsageManagerCocoa::updateMediaUsage(WebCore::MediaSessionIdentifier identifier, const WebCore::MediaUsageInfo& mediaUsageInfo)
{
    ASSERT(m_mediaSessions.contains(identifier));
    auto session = m_mediaSessions.get(identifier);
    if (!session)
        return;

    if (!usageTrackingAvailable())
        return;

    @try {

        if (session->mediaUsageInfo) {
            if (session->mediaUsageInfo == mediaUsageInfo)
                return;

            if (session->usageTracker && session->mediaUsageInfo->mediaURL != mediaUsageInfo.mediaURL) {
                [session->usageTracker stop];
                session->usageTracker = nullptr;
            }
        }

        NSDictionary<NSString *, id> *metadata = @{
            USVideoMetadataKeyCanShowControlsManager: @(mediaUsageInfo.canShowControlsManager),
            USVideoMetadataKeyCanShowNowPlayingControls: @(mediaUsageInfo.canShowNowPlayingControls),
            USVideoMetadataKeyIsSuspended: @(mediaUsageInfo.isSuspended),
            USVideoMetadataKeyIsInActiveDocument: @(mediaUsageInfo.isInActiveDocument),
            USVideoMetadataKeyIsFullscreen: @(mediaUsageInfo.isFullscreen),
            USVideoMetadataKeyIsMuted: @(mediaUsageInfo.isMuted),
            USVideoMetadataKeyIsMediaDocumentInMainFrame: @(mediaUsageInfo.isMediaDocumentInMainFrame),
            USVideoMetadataKeyIsVideo: @(mediaUsageInfo.isVideo),
            USVideoMetadataKeyIsAudio: @(mediaUsageInfo.isAudio),
            USVideoMetadataKeyNoVideo: @(!mediaUsageInfo.hasVideo),
            USVideoMetadataKeyNoAudio: @(!mediaUsageInfo.hasAudio),
            USVideoMetadataKeyRenderer: @(mediaUsageInfo.hasRenderer),
            USVideoMetadataKeyAudioElementWithUserGesture: @(mediaUsageInfo.audioElementWithUserGesture),
            USVideoMetadataKeyUserHasPlayedAudioBefore: @(mediaUsageInfo.userHasPlayedAudioBefore),
            USVideoMetadataKeyIsElementRectMostlyInMainFrame: @(mediaUsageInfo.isElementRectMostlyInMainFrame),
            USVideoMetadataKeyPlaybackPermitted: @(mediaUsageInfo.playbackPermitted),
            USVideoMetadataKeyPageMediaPlaybackSuspended: @(mediaUsageInfo.pageMediaPlaybackSuspended),
            USVideoMetadataKeyIsMediaDocumentAndNotOwnerElement: @(mediaUsageInfo.isMediaDocumentAndNotOwnerElement),
            USVideoMetadataKeyPageExplicitlyAllowsElementToAutoplayInline: @(mediaUsageInfo.pageExplicitlyAllowsElementToAutoplayInline),
            USVideoMetadataKeyRequiresFullscreenForVideoPlaybackAndFullscreenNotPermitted: @(mediaUsageInfo.requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted),
            USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoRateChange: @(mediaUsageInfo.isVideoAndRequiresUserGestureForVideoRateChange),
            USVideoMetadataKeyIsAudioAndRequiresUserGestureForAudioRateChange: @(mediaUsageInfo.isAudioAndRequiresUserGestureForAudioRateChange),
            USVideoMetadataKeyIsVideoAndRequiresUserGestureForVideoDueToLowPowerMode: @(mediaUsageInfo.isVideoAndRequiresUserGestureForVideoDueToLowPowerMode),
            USVideoMetadataKeyNoUserGestureRequired: @(mediaUsageInfo.noUserGestureRequired),
            USVideoMetadataKeyRequiresPlaybackAndIsNotPlaying: @(mediaUsageInfo.requiresPlaybackAndIsNotPlaying),
            USVideoMetadataKeyHasEverNotifiedAboutPlaying: @(mediaUsageInfo.hasEverNotifiedAboutPlaying),
            USVideoMetadataKeyOutsideOfFullscreen: @(mediaUsageInfo.outsideOfFullscreen),
            USVideoMetadataKeyIsLargeEnoughForMainContent: @(mediaUsageInfo.isLargeEnoughForMainContent),
        };

        if (!session->usageTracker) {
            if (!mediaUsageInfo.isPlaying)
                return;

            session->usageTracker = adoptNS([PAL::allocUSVideoUsageInstance() initWithBundleIdentifier:session->bundleIdentifier URL:(NSURL *)session->pageURL
                mediaURL:(NSURL *)mediaUsageInfo.mediaURL videoMetadata:metadata]);
            ASSERT(session->usageTracker);
            if (!session->usageTracker)
                return;
        } else
            [session->usageTracker updateVideoMetadata:metadata];

        if (session->mediaUsageInfo && session->mediaUsageInfo->isPlaying != mediaUsageInfo.isPlaying) {
            if (mediaUsageInfo.isPlaying)
                [session->usageTracker restart];
            else
                [session->usageTracker stop];
        }

        session->mediaUsageInfo = mediaUsageInfo;

    } @catch(NSException *exception) {
        WTFLogAlways("MediaUsageManagerCocoa::updateMediaUsage caught exception: %@", [[exception reason] UTF8String]);
    }
}

#if !HAVE(CGS_FIX_FOR_RADAR_97530095)
bool MediaUsageManagerCocoa::isPlayingVideoInViewport() const
{
    for (auto& session : m_mediaSessions.values()) {
        if (session->mediaUsageInfo && session->mediaUsageInfo->isPlaying && session->mediaUsageInfo->isVideo && session->mediaUsageInfo->isInViewport)
            return true;
    }
    return false;
}
#endif

} // namespace WebKit

#endif // ENABLE(MEDIA_USAGE)

