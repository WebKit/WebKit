// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if HAVE_AVEXPERIENCECONTROLLER

import AVKit_Private
import os
@_spi(AVExperienceController) @_spi(AVNowPlayingExperienceController) import AVKit

extension Logger {
    static let experienceController = Logger(subsystem: "com.apple.WebKit", category: "Fullscreen")
}

@MainActor
@objc
@implementation
extension WKSExperienceController {
    weak var delegate: (any WKSExperienceControllerDelegate)?

    @nonobjc
    private let platformExperienceController: WKSPlatformExperienceController

    @objc(initWithContentSource:)
    init(contentSource: AVPlayerViewControllerContentSource) {
        platformExperienceController = WKSPlatformExperienceController(contentSource: contentSource)
        super.init()
        platformExperienceController.experienceController = self
        Logger.experienceController.log("\(#function)")
    }

    @objc(enterFullscreenWithCompletionHandler:)
    func enterFullscreen(completionHandler: @MainActor @Sendable @escaping (Bool) -> Void) {
        Task { @MainActor in
            completionHandler(await platformExperienceController.transitionToFullscreen())
        }
    }

    @objc(exitFullscreenWithCompletionHandler:)
    func exitFullscreen(completionHandler: @MainActor @Sendable @escaping (Bool) -> Void) {
        Task { @MainActor in
            completionHandler(await platformExperienceController.transitionFromFullscreen())
        }
    }
}

@MainActor
class WKSPlatformExperienceController {
    weak var experienceController: WKSExperienceController?
    let avExperienceController: AVExperienceController

    init(contentSource: AVPlayerViewControllerContentSource) {
        avExperienceController = AVExperienceController(contentSource: contentSource)
        avExperienceController.delegate = self
    }
}

extension WKSPlatformExperienceController {
    func transitionToFullscreen() async -> Bool {
        guard avExperienceController.experience == .background else {
            return false
        }

        Logger.experienceController.log("\(#function)")
        return await avExperienceController.transition(to: .nowPlaying) == .completed
    }

    func transitionFromFullscreen() async -> Bool {
        guard avExperienceController.experience == .nowPlaying else {
            return false
        }

        Logger.experienceController.log("\(#function)")
        return await avExperienceController.transition(to: .background) == .completed
    }
}

extension WKSPlatformExperienceController: @MainActor AVExperienceController.Delegate {
    func experienceController(
        _ controller: AVExperienceController,
        didChangeTransitionContext context: AVExperienceController.TransitionContext
    ) {
        guard let experienceController, case .finished(let result) = context.status, result == .completed else {
            return
        }

        Logger.experienceController.log("\(#function)")
        experienceController.delegate?.experienceControllerDidExitFullscreen(experienceController)
    }

    func experienceController(
        _ controller: AVExperienceController,
        prepareForTransitionUsing context: AVExperienceController.TransitionContext
    ) async {
        guard context.toExperience == .nowPlaying else {
            return
        }

        Logger.experienceController.log("\(#function)")

        let nowPlayingController = controller.nowPlayingExperienceController

        let skipConfiguration = AVNowPlayingExperienceController.Configuration.SkipIntervalConfiguration(
            enabled: true,
            interval: 10.0
        )

        let configuration = AVNowPlayingExperienceController.Configuration(
            playbackSpeeds: AVPlaybackSpeed.systemDefaultSpeeds,
            skipBehavior: .interval(backward: skipConfiguration, forward: skipConfiguration),
            updatesNowPlayingInfoCenter: false
        )

        nowPlayingController.configuration = configuration
        nowPlayingController.delegate = self
    }

    func experienceController(
        _ controller: AVExperienceController,
        didChangeAvailableExperiences availableExperiences: AVExperienceController.Experiences
    ) {
    }
}

extension WKSPlatformExperienceController: @MainActor AVNowPlayingExperienceController.Delegate {
    func nowPlayingExperienceControllerDidSkipToNextItem(_ controller: AVNowPlayingExperienceController) {
    }

    func nowPlayingExperienceControllerDidSkipToPreviousItem(_ controller: AVNowPlayingExperienceController) {
    }

    func nowPlayingExperienceControllerDidToggleCaptionStylePreviewID(_ controller: AVNowPlayingExperienceController, previewID: String?) {
        // FIXME: Remove this Task once AVKit resolves rdar://173021517
        Task.immediate {
            if let experienceController {
                experienceController.delegate?.experienceController(experienceController, didToggleCaptionStylePreviewID: previewID)
            }
        }
    }

    func nowPlayingExperienceControllerDidBeginScrubbing(_ controller: AVNowPlayingExperienceController) {
        // FIXME: Remove this Task once AVKit resolves rdar://173021517
        Task.immediate {
            if let experienceController {
                experienceController.delegate?.experienceControllerDidBeginScrubbing(experienceController)
            }
        }
    }

    func nowPlayingExperienceControllerDidEndScrubbing(_ controller: AVNowPlayingExperienceController) {
        // FIXME: Remove this Task once AVKit resolves rdar://173021517
        Task.immediate {
            if let experienceController {
                experienceController.delegate?.experienceControllerDidEndScrubbing(experienceController)
            }
        }
    }

    func nowPlayingExperienceControllerUserWillPause(_ controller: AVNowPlayingExperienceController) {
    }

    func nowPlayingExperienceControllerUserWillPlay(_ controller: AVNowPlayingExperienceController) {
    }
}

#endif // HAVE_AVEXPERIENCECONTROLLER
