// Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if canImport(LinearMediaKit)

import AVFoundation
import Combine
import LinearMediaKit
import RealityFoundation
import UIKit
import WebKitSwift

private class SwiftOnlyData: NSObject {
    var renderingConfiguration: RenderingConfiguration?
    var thumbnailMaterial: VideoMaterial?
    var videoMaterial: VideoMaterial?
    var peculiarEntity: PeculiarEntity?
    var contentMetadata: ContentMetadataContainer?
    
    // FIXME: It should be possible to store these directly on WKSLinearMediaPlayer since they are
    // bridged to NSDate, but a bug prevents that from compiling (rdar://121877511).
    var startDate: Date?
    var endDate: Date?
}

@_objcImplementation extension WKSLinearMediaPlayer {
    weak var delegate: WKSLinearMediaPlayerDelegate?
    var selectedPlaybackRate = 1.0
    var presentationMode: WKSLinearMediaPresentationMode = .inline
    var error: Error?
    var canTogglePlayback = false
    var requiresLinearPlayback = false
    var interstitialRanges: [WKSLinearMediaTimeRange] = []
    var isInterstitialActive = false
    var duration: TimeInterval = .nan
    var currentTime: TimeInterval = .nan
    var remainingTime: TimeInterval = .nan
    var playbackRate = 0.0
    var playbackRates: [NSNumber] = [0.5, 1.0, 1.25, 1.5, 2.0]
    var isLoading = false
    var isTrimming = false
    var trimView: UIView?
    var thumbnailLayer: CALayer?
    var captionLayer: CALayer?
    var captionContentInsets: UIEdgeInsets = .zero
    var showsPlaybackControls = true
    var canSeek = false
    var seekableTimeRanges: [WKSLinearMediaTimeRange] = []
    var isSeeking = false
    var canScanBackward = false
    var canScanForward = false
    var contentInfoViewControllers: [UIViewController] = []
    var contextualActions: [UIAction] = []
    var contextualActionsInfoView: UIView?
    var contentDimensions: NSValue?
    var contentMode: WKSLinearMediaContentMode = .default
    var videoLayer: CALayer?
    var anticipatedViewingMode: WKSLinearMediaViewingMode = .none
    var contentOverlay: UIView?
    var contentOverlayViewController: UIViewController?
    var volume = 1.0
    var isMuted = false
    var sessionDisplayTitle: String?
    var sessionThumbnail: UIImage?
    var isSessionExtended = false
    var hasAudioContent = true
    var currentAudioTrack: WKSLinearMediaTrack?
    var audioTracks: [WKSLinearMediaTrack] = []
    var currentLegibleTrack: WKSLinearMediaTrack?
    var legibleTracks: [WKSLinearMediaTrack] = []
    var contentType: WKSLinearMediaContentType = .none
    var transportBarIncludesTitleView = true
    var artwork: Data?
    var isPlayableOffline = false
    var allowPip = true
    var allowFullScreenFromInline = true
    var isLiveStream: NSNumber?
    var recommendedViewingRatio: NSNumber?
    var fullscreenSceneBehaviors: WKSLinearMediaFullscreenBehaviors = []
    var startTime: Double = .nan
    var endTime: Double = .nan

    // FIXME: These should be stored properties on WKSLinearMediaPlayer, but a bug prevents that from compiling (rdar://121877511).
    var startDate: Date? {
        get { swiftOnlyData.startDate }
        set { swiftOnlyData.startDate = newValue }
    }
    var endDate: Date? {
        get { swiftOnlyData.endDate }
        set { swiftOnlyData.endDate = newValue }
    }

    @nonobjc private var swiftOnlyData: SwiftOnlyData

    public override init() {
        swiftOnlyData = .init()
        super.init()
    }
}

extension WKSLinearMediaPlayer: @retroactive Playable {
    public var selectedPlaybackRatePublisher: AnyPublisher<Double, Never> {
        publisher(for: \.selectedPlaybackRate).eraseToAnyPublisher()
    }

    public var presentationModePublisher: AnyPublisher<PresentationMode, Never> {
        publisher(for: \.presentationMode).compactMap { $0.presentationMode }.eraseToAnyPublisher()
    }

    public func updateRenderingConfiguration(_ config: RenderingConfiguration) {
        swiftOnlyData.renderingConfiguration = config
    }

    public var errorPublisher: AnyPublisher<Error?, Never> {
        publisher(for: \.error).eraseToAnyPublisher()
    }

    public var canTogglePlaybackPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.canTogglePlayback).eraseToAnyPublisher()
    }

    public var requiresLinearPlaybackPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.requiresLinearPlayback).eraseToAnyPublisher()
    }

    public var interstitialRangesPublisher: AnyPublisher<[Range<TimeInterval>], Never> {
        publisher(for: \.interstitialRanges).map { $0.map { $0.range } }.eraseToAnyPublisher()
    }

    public var isInterstitialActivePublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isInterstitialActive).eraseToAnyPublisher()
    }

    public var durationPublisher: AnyPublisher<TimeInterval, Never> {
        publisher(for: \.duration).eraseToAnyPublisher()
    }

    public var currentTimePublisher: AnyPublisher<TimeInterval, Never> {
        publisher(for: \.currentTime).eraseToAnyPublisher()
    }

    public var remainingTimePublisher: AnyPublisher<TimeInterval, Never> {
        publisher(for: \.remainingTime).eraseToAnyPublisher()
    }

    public var playbackRatePublisher: AnyPublisher<Double, Never> {
        publisher(for: \.playbackRate).eraseToAnyPublisher()
    }

    public var playbackRatesPublisher: AnyPublisher<[Double], Never> {
        publisher(for: \.playbackRates).map { $0.map { $0.doubleValue } }.eraseToAnyPublisher()
    }

    public var isPlayingPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.playbackRate).map { $0 != 0.0 }.eraseToAnyPublisher()
    }

    public var isLoadingPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isLoading).eraseToAnyPublisher()
    }

    public var isTrimmingPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isTrimming).eraseToAnyPublisher()
    }

    private static let preferredTimescale: CMTimeScale = 600

    public var forwardPlaybackEndTimePublisher: AnyPublisher<CMTime?, Never> {
        publisher(for: \.endTime)
            .dropFirst()
            .map { $0.isNaN ? .invalid : CMTime(seconds: $0, preferredTimescale: Self.preferredTimescale) }
            .eraseToAnyPublisher()
    }

    public var reversePlaybackEndTimePublisher: AnyPublisher<CMTime?, Never> {
        publisher(for: \.startTime)
            .dropFirst()
            .map { $0.isNaN ? .invalid : CMTime(seconds: $0, preferredTimescale: Self.preferredTimescale) }
            .eraseToAnyPublisher()
    }

    public var trimViewPublisher: AnyPublisher<UIView?, Never> {
        publisher(for: \.trimView).eraseToAnyPublisher()
    }

    public var thumbnailLayerPublisher: AnyPublisher<CALayer?, Never> {
        publisher(for: \.thumbnailLayer).eraseToAnyPublisher()
    }

    public var thumbnailMaterialPublisher: AnyPublisher<VideoMaterial?, Never> {
        publisher(for: \.swiftOnlyData.thumbnailMaterial).eraseToAnyPublisher()
    }

    public var captionLayerPublisher: AnyPublisher<CALayer?, Never> {
        publisher(for: \.captionLayer).eraseToAnyPublisher()
    }

    public var captionContentInsetsPublisher: AnyPublisher<UIEdgeInsets, Never> {
        publisher(for: \.captionContentInsets).eraseToAnyPublisher()
    }

    public var showsPlaybackControlsPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.showsPlaybackControls).eraseToAnyPublisher()
    }

    public var canSeekPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.canSeek).eraseToAnyPublisher()
    }

    public var seekableTimeRangesPublisher: AnyPublisher<[ClosedRange<TimeInterval>], Never> {
        publisher(for: \.seekableTimeRanges).map { $0.map { $0.closedRange } }.eraseToAnyPublisher()
    }

    public var isSeekingPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isSeeking).eraseToAnyPublisher()
    }

    public var canScanBackwardPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.canScanBackward).eraseToAnyPublisher()
    }

    public var canScanForwardPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.canScanForward).eraseToAnyPublisher()
    }

    public var contentInfoViewControllersPublisher: AnyPublisher<[UIViewController], Never> {
        publisher(for: \.contentInfoViewControllers).eraseToAnyPublisher()
    }

    public var contextualActionsPublisher: AnyPublisher<[UIAction], Never> {
        publisher(for: \.contextualActions).eraseToAnyPublisher()
    }

    public var contextualActionsInfoViewPublisher: AnyPublisher<UIView?, Never> {
        publisher(for: \.contextualActionsInfoView).eraseToAnyPublisher()
    }

    public var contentDimensionsPublisher: AnyPublisher<CGSize, Never> {
        publisher(for: \.contentDimensions).compactMap { $0?.cgSizeValue }.eraseToAnyPublisher()
    }

    public var contentModePublisher: AnyPublisher<ContentMode, Never> {
        publisher(for: \.contentMode).compactMap { $0.contentMode }.eraseToAnyPublisher()
    }

    public var videoLayerPublisher: AnyPublisher<CALayer?, Never> {
        publisher(for: \.videoLayer).eraseToAnyPublisher()
    }

    public var videoMaterialPublisher: AnyPublisher<VideoMaterial?, Never> {
        publisher(for: \.swiftOnlyData.videoMaterial).eraseToAnyPublisher()
    }

    public var peculiarEntityPublisher: AnyPublisher<PeculiarEntity?, Never> {
        publisher(for: \.swiftOnlyData.peculiarEntity).eraseToAnyPublisher()
    }

    public var anticipatedViewingModePublisher: AnyPublisher<ViewingMode?, Never> {
        publisher(for: \.anticipatedViewingMode).compactMap { $0.viewingMode }.eraseToAnyPublisher()
    }

    public var contentOverlayPublisher: AnyPublisher<UIView?, Never> {
        publisher(for: \.contentOverlay).eraseToAnyPublisher()
    }

    public var contentOverlayViewControllerPublisher: AnyPublisher<UIViewController?, Never> {
        publisher(for: \.contentOverlayViewController).eraseToAnyPublisher()
    }

    public var volumePublisher: AnyPublisher<Double, Never> {
        publisher(for: \.volume).eraseToAnyPublisher()
    }

    public var isMutedPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isMuted).eraseToAnyPublisher()
    }

    public var sessionDisplayTitlePublisher: AnyPublisher<String?, Never> {
        publisher(for: \.sessionDisplayTitle).eraseToAnyPublisher()
    }

    public var sessionThumbnailPublisher: AnyPublisher<UIImage?, Never> {
        publisher(for: \.sessionThumbnail).eraseToAnyPublisher()
    }

    public var isSessionExtendedPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isSessionExtended).eraseToAnyPublisher()
    }

    public var hasAudioContentPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.hasAudioContent).eraseToAnyPublisher()
    }

    public var currentAudioTrackPublisher: AnyPublisher<Track?, Never> {
        publisher(for: \.currentAudioTrack).map { $0 }.eraseToAnyPublisher()
    }

    public var audioTracksPublisher: AnyPublisher<[Track]?, Never> {
        publisher(for: \.audioTracks).map { $0 }.eraseToAnyPublisher()
    }

    public var currentLegibleTrackPublisher: AnyPublisher<Track?, Never> {
        publisher(for: \.currentLegibleTrack).map { $0 }.eraseToAnyPublisher()
    }

    public var legibleTracksPublisher: AnyPublisher<[Track]?, Never> {
        publisher(for: \.legibleTracks).map { $0 }.eraseToAnyPublisher()
    }

    public var contentTypePublisher: AnyPublisher<ContentType?, Never> {
        publisher(for: \.contentType).map { $0.contentType }.eraseToAnyPublisher()
    }

    public var contentMetadataPublisher: AnyPublisher<ContentMetadataContainer, Never> {
        publisher(for: \.swiftOnlyData.contentMetadata).compactMap { $0 }.eraseToAnyPublisher()
    }

    public var transportBarIncludesTitleViewPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.transportBarIncludesTitleView).eraseToAnyPublisher()
    }

    public var artworkPublisher: AnyPublisher<Data?, Never> {
        publisher(for: \.artwork).eraseToAnyPublisher()
    }

    public var isPlayableOfflinePublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isPlayableOffline).eraseToAnyPublisher()
    }

    public var allowPipPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.allowPip).eraseToAnyPublisher()
    }

    public var allowFullScreenFromInlinePublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.allowFullScreenFromInline).eraseToAnyPublisher()
    }

    public var isLiveStreamPublisher: AnyPublisher<Bool, Never> {
        publisher(for: \.isLiveStream).compactMap { $0?.boolValue }.eraseToAnyPublisher()
    }

    public var startDatePublisher: AnyPublisher<Date, Never> {
        publisher(for: \.swiftOnlyData.startDate).compactMap { $0 }.eraseToAnyPublisher()
    }

    public var endDatePublisher: AnyPublisher<Date, Never> {
        publisher(for: \.swiftOnlyData.endDate).compactMap { $0 }.eraseToAnyPublisher()
    }

    public var recommendedViewingRatioPublisher: AnyPublisher<Double?, Never> {
        publisher(for: \.recommendedViewingRatio).compactMap { $0?.doubleValue }.eraseToAnyPublisher()
    }

    public var fullscreenSceneBehaviorsPublisher: AnyPublisher<[FullscreenBehaviors], Never> {
        publisher(for: \.fullscreenSceneBehaviors).compactMap { [$0.fullscreenBehaviors] }.eraseToAnyPublisher()
    }

    public func play() {
        delegate?.linearMediaPlayerPlay?(self)
    }

    public func pause() {
        delegate?.linearMediaPlayerPause?(self)
    }

    public func togglePlayback() {
        delegate?.linearMediaPlayerTogglePlayback?(self)
    }

    public func setPlaybackRate(_ rate: Double) {
        delegate?.linearMediaPlayer?(self, setPlaybackRate: rate)
    }

    public func seek(to time: TimeInterval) {
        delegate?.linearMediaPlayer?(self, seekToTime: time)
    }

    public func seek(delta: TimeInterval) {
        delegate?.linearMediaPlayer?(self, seekByDelta: delta)
    }

    public func seek(to destination: TimeInterval, from source: TimeInterval, metadata: SeekMetadata) -> TimeInterval {
        delegate?.linearMediaPlayer?(self, seekToDestination: destination, fromSource: source) ?? TimeInterval.zero
    }

    public func completeTrimming(commitChanges: Bool) {
        delegate?.linearMediaPlayer?(self, completeTrimming: commitChanges)
    }

    public func updateStartTime(_ time: TimeInterval) {
        delegate?.linearMediaPlayer?(self, updateStartTime: time)
    }

    public func updateEndTime(_ time: TimeInterval) {
        delegate?.linearMediaPlayer?(self, updateEndTime: time)
    }

    public func beginEditingVolume() {
        delegate?.linearMediaPlayerBeginEditingVolume?(self)
    }

    public func endEditingVolume() {
        delegate?.linearMediaPlayerEndEditingVolume?(self)
    }

    public func setAudioTrack(_ newTrack: Track?) {
        delegate?.linearMediaPlayer?(self, setAudioTrack: newTrack as? WKSLinearMediaTrack)
    }

    public func setLegibleTrack(_ newTrack: Track?) {
        delegate?.linearMediaPlayer?(self, setLegibleTrack: newTrack as? WKSLinearMediaTrack)
    }

    public func skipActiveInterstitial() {
        delegate?.linearMediaPlayerSkipActiveInterstitial?(self)
    }

    public func setCaptionContentInsets(_ insets: UIEdgeInsets) {
        delegate?.linearMediaPlayer?(self, setCaptionContentInsets: insets)
    }

    public func updateVideoBounds(_ bounds: CGRect) {
        delegate?.linearMediaPlayer?(self, updateVideoBounds: bounds)
    }

    public func updateViewingMode(_ mode: ViewingMode?) {
        delegate?.linearMediaPlayer?(self, update: .init(mode))
    }

    public func togglePip() {
        delegate?.linearMediaPlayerTogglePip?(self)
    }

    public func toggleInlineMode() {
        delegate?.linearMediaPlayerToggleInlineMode?(self)
    }

    public func willEnterFullscreen() {
        delegate?.linearMediaPlayerWillEnterFullscreen?(self)
    }

    public func didCompleteEnterFullscreen(result: Result<Void, Error>) {
        switch result {
        case .success():
            delegate?.linearMediaPlayer?(self, didEnterFullscreenWithError: nil)
        case .failure(let error):
            delegate?.linearMediaPlayer?(self, didEnterFullscreenWithError: error)
        }
    }

    public func willExitFullscreen() {
        delegate?.linearMediaPlayerWillExitFullscreen?(self)
    }

    public func didCompleteExitFullscreen(result: Result<Void, Error>) {
        switch result {
        case .success():
            delegate?.linearMediaPlayer?(self, didExitFullscreenWithError: nil)
        case .failure(let error):
            delegate?.linearMediaPlayer?(self, didExitFullscreenWithError: error)
        }
    }

    public func makeDefaultEntity() -> Entity? {
        nil
    }

    public func setTimeResolverInterval(_ interval: TimeInterval) {
        delegate?.linearMediaPlayer?(self, setTimeResolverInterval: interval)
    }

    public func setTimeResolverResolution(_ resolution: TimeInterval) {
        delegate?.linearMediaPlayer?(self, setTimeResolverResolution: resolution)
    }

    public func setThumbnailSize(_ size: CGSize) {
        delegate?.linearMediaPlayer?(self, setThumbnailSize: size)
    }

    public func seekThumbnail(to time: TimeInterval) {
        delegate?.linearMediaPlayer?(self, seekThumbnailToTime: time)
    }

    public func beginScrubbing() {
        delegate?.linearMediaPlayerBeginScrubbing?(self)
    }

    public func endScrubbing() {
        delegate?.linearMediaPlayerEndScrubbing?(self)
    }

    public func beginScanningForward() {
        delegate?.linearMediaPlayerBeginScanningForward?(self)
    }

    public func endScanningForward() {
        delegate?.linearMediaPlayerEndScanningForward?(self)
    }

    public func beginScanningBackward() {
        delegate?.linearMediaPlayerBeginScanningBackward?(self)
    }

    public func endScanningBackward() {
        delegate?.linearMediaPlayerEndScanningBackward?(self)
    }

    public func setVolume(_ volume: Double) {
        delegate?.linearMediaPlayer?(self, setVolume: volume)
    }

    public func setIsMuted(_ value: Bool) {
        delegate?.linearMediaPlayer?(self, setMuted: value)
    }
}

#endif // canImport(LinearMediaKit)
