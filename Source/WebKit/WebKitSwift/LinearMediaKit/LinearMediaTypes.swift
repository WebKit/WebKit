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

@objc @implementation extension WKSLinearMediaContentMetadata {
    let title: String?
    let subtitle: String?
    
    init(title: String?, subtitle: String?) {
        self.title = title
        self.subtitle = subtitle
    }
}

@objc @implementation extension WKSLinearMediaTimeRange {
    let lowerBound: TimeInterval
    let upperBound: TimeInterval

    init(lowerBound: TimeInterval, upperBound: TimeInterval) {
        self.lowerBound = lowerBound
        self.upperBound = upperBound
    }
}

@objc @implementation extension WKSLinearMediaTrack {
    let localizedDisplayName: String

    init(localizedDisplayName: String) {
        self.localizedDisplayName = localizedDisplayName
    }
}

@objc @implementation extension WKSLinearMediaSpatialVideoMetadata {
    let width: Int32
    let height: Int32
    let horizontalFOVDegrees: Float
    let baseline: Float
    let disparityAdjustment: Float

    init(width: Int32, height: Int32, horizontalFOVDegrees: Float, baseline: Float, disparityAdjustment: Float) {
        self.width = width
        self.height = height
        self.horizontalFOVDegrees = horizontalFOVDegrees
        self.baseline = baseline
        self.disparityAdjustment = disparityAdjustment
    }
}

@objc @implementation extension WKSPlayableViewControllerHost {
    @nonobjc private let base = PlayableViewController()

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
    public var description: String {
        switch self {
        case .inline:
            return "inline"
        case .enteringFullscreen:
            return "enteringFullscreen"
        case .fullscreen:
            return "fullscreen"
        case .exitingFullscreen:
            return "exitingFullscreen"
        case .enteringExternal:
            return "enteringExternal"
        case .external:
            return "external"
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

    public var description: String {
        switch self {
        case .none:
            return "none"
        case .mono:
            return "mono"
        case .stereo:
            return "stereo"
        case .immersive:
            return "immersive"
        case .spatial:
            return "spatial"
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
        return lowerBound...upperBound
    }

    var range: Range<TimeInterval> {
        return lowerBound..<upperBound
    }
}

@_spi(Internal) extension WKSLinearMediaTrack: Track {
}

extension WKSLinearMediaSpatialVideoMetadata {
    var metadata: SpatialVideoMetadata {
        return SpatialVideoMetadata(width: self.width, height: self.height, horizontalFOVDegrees: self.horizontalFOVDegrees, baseline: self.baseline, disparityAdjustment: self.disparityAdjustment, isRecommendedForImmersive: true)
    }
}

#endif // os(visionOS)
