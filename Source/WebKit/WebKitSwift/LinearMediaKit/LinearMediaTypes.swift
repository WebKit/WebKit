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

#if os(visionOS)

#if canImport(AVKit, _version: 1270)
#if USE_APPLE_INTERNAL_SDK
@_spi(LinearMediaKit) @_spi(LinearMediaKit_WebKitOnly) import AVKit
#else
import AVKit_SPI
#endif
#else
@_spi(WebKitOnly) import LinearMediaKit
#endif

// MARK: Objective-C Implementations

@objc
@implementation
extension WKSLinearMediaContentMetadata {
    let title: String?
    let subtitle: String?

    init(title: String?, subtitle: String?) {
        self.title = title
        self.subtitle = subtitle
    }
}

@objc
@implementation
extension WKSLinearMediaTimeRange {
    let lowerBound: TimeInterval
    let upperBound: TimeInterval

    init(lowerBound: TimeInterval, upperBound: TimeInterval) {
        self.lowerBound = lowerBound
        self.upperBound = upperBound
    }
}

@objc
@implementation
extension WKSLinearMediaTrack {
    let localizedDisplayName: String

    init(localizedDisplayName: String) {
        self.localizedDisplayName = localizedDisplayName
    }
}

@objc
@implementation
extension WKSLinearMediaSpatialVideoMetadata {
    let width: Int32
    let height: Int32
    let horizontalFieldOfView: Int32
    let stereoCameraBaseline: UInt32
    let horizontalDisparityAdjustment: Int32

    init(width: Int32, height: Int32, horizontalFieldOfView: Int32, stereoCameraBaseline: UInt32, horizontalDisparityAdjustment: Int32) {
        self.width = width
        self.height = height
        self.horizontalFieldOfView = horizontalFieldOfView
        self.stereoCameraBaseline = stereoCameraBaseline
        self.horizontalDisparityAdjustment = horizontalDisparityAdjustment
    }
}

@objc
@implementation
extension WKSPlayableViewControllerHost {
    @nonobjc
    private let base = PlayableViewController()

    var viewController: UIViewController {
        base
    }

    var automaticallyDockOnFullScreenPresentation: Bool {
        get { base.automaticallyDockOnFullScreenPresentation }
        set { base.automaticallyDockOnFullScreenPresentation = newValue }
    }

    var dismissFullScreenOnExitingDocking: Bool {
        get { base.dismissFullScreenOnExitingDocking }
        set { base.dismissFullScreenOnExitingDocking = newValue }
    }

    var environmentPickerButtonViewController: UIViewController? {
        base.environmentPickerButtonViewController
    }

    @objc
    var prefersAutoDimming: Bool {
        get {
            #if USE_APPLE_INTERNAL_SDK
            base.prefersAutoDimming
            #else
            false
            #endif
        }
        set {
            base.prefersAutoDimming = newValue
        }
    }

    @nonobjc
    final var playable: (any Playable)? {
        get {
            #if USE_APPLE_INTERNAL_SDK
            base.playable
            #else
            nil
            #endif
        }
        set {
            base.playable = newValue
        }
    }
}

// MARK: LinearMediaKit Extensions

extension WKSLinearMediaContentMetadata {
    var contentMetadata: ContentMetadataContainer {
        var container = ContentMetadataContainer()
        container.displayTitle = title
        container.displaySubtitle = subtitle
        return container
    }
}

extension WKSLinearMediaContentMode {
    init(_ contentMode: ContentMode?) {
        switch contentMode {
        case .scaleAspectFit?:
            self = .scaleAspectFit
        case .scaleAspectFill?:
            self = .scaleAspectFill
        case .scaleToFill?:
            self = .scaleToFill
        case .none:
            self = .none
        @unknown default:
            fatalError()
        }
    }

    var contentMode: ContentMode? {
        switch self {
        case .none:
            nil
        case .scaleAspectFit:
            .scaleAspectFit
        case .scaleAspectFill:
            .scaleAspectFill
        case .scaleToFill:
            .scaleToFill
        @unknown default:
            fatalError()
        }
    }

    static var `default`: WKSLinearMediaContentMode {
        // `ContentMode.default` is invalid; workaround by using the value of it directly.
        .init(.scaleAspectFit)
    }
}

extension WKSLinearMediaContentType {
    var contentType: ContentType? {
        switch self {
        case .none:
            nil
        case .immersive:
            .immersive
        case .spatial:
            .spatial
        case .planar:
            .planar
        case .audioOnly:
            .audioOnly
        @unknown default:
            fatalError()
        }
    }
}

extension WKSLinearMediaPresentationState: CustomStringConvertible {
    // FIXME: Objective-C interface type WKSLinearMediaPresentationState should not itself conform to a Swift protocol.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        switch self {
        case .inline:
            "inline"
        case .enteringFullscreen:
            "enteringFullscreen"
        case .fullscreen:
            "fullscreen"
        case .exitingFullscreen:
            "exitingFullscreen"
        case .enteringExternal:
            "enteringExternal"
        case .external:
            "external"
        @unknown default:
            fatalError()
        }
    }
}

extension WKSLinearMediaViewingMode: CustomStringConvertible {
    init(_ viewingMode: ViewingMode?) {
        switch viewingMode {
        case .mono?:
            self = .mono
        case .stereo?:
            self = .stereo
        case .immersive?:
            self = .immersive
        case .spatial?:
            self = .spatial
        case .none:
            self = .none
        @unknown default:
            fatalError()
        }
    }

    var viewingMode: ViewingMode? {
        switch self {
        case .none:
            nil
        case .mono:
            .mono
        case .stereo:
            .stereo
        case .immersive:
            .immersive
        case .spatial:
            .spatial
        @unknown default:
            fatalError()
        }
    }

    // FIXME: Objective-C interface type WKSLinearMediaViewingMode should not itself conform to a Swift protocol.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public var description: String {
        switch self {
        case .none:
            "none"
        case .mono:
            "mono"
        case .stereo:
            "stereo"
        case .immersive:
            "immersive"
        case .spatial:
            "spatial"
        @unknown default:
            fatalError()
        }
    }
}

extension WKSLinearMediaFullscreenBehaviors {
    init(_ fullscreenBehaviors: FullscreenBehaviors) {
        self = .init(rawValue: fullscreenBehaviors.rawValue)
    }

    var fullscreenBehaviors: FullscreenBehaviors {
        .init(rawValue: self.rawValue)
    }
}

extension WKSLinearMediaTimeRange {
    var closedRange: ClosedRange<TimeInterval> {
        lowerBound...upperBound
    }

    var range: Range<TimeInterval> {
        lowerBound..<upperBound
    }
}

@_spi(Internal)
extension WKSLinearMediaTrack: Track {
}

extension WKSLinearMediaSpatialVideoMetadata {
    var metadata: SpatialVideoMetadata {
        .init(
            width: self.width,
            height: self.height,
            horizontalFOVDegrees: Float(self.horizontalFieldOfView) / 1000.0,
            baseline: Float(self.stereoCameraBaseline),
            disparityAdjustment: Float(self.horizontalDisparityAdjustment) / 10000.0,
            isRecommendedForImmersive: true
        )
    }
}

#endif // os(visionOS)
