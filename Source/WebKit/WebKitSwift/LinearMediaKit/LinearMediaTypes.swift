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

import WebKitSwift

// MARK: Objective-C Implementations

@_objcImplementation extension WKSLinearMediaTimeRange {
    let lowerBound: TimeInterval
    let upperBound: TimeInterval

    init(lowerBound: TimeInterval, upperBound: TimeInterval) {
        self.lowerBound = lowerBound
        self.upperBound = upperBound
    }
}

@_objcImplementation extension WKSLinearMediaTrack {
    let localizedDisplayName: String

    init(localizedDisplayName: String) {
        self.localizedDisplayName = localizedDisplayName
    }
}

#if canImport(LinearMediaKit)

import LinearMediaKit

// MARK: LinearMediaKit Extensions

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
        .init(.default)
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

extension WKSLinearMediaPresentationMode {
    var presentationMode: PresentationMode? {
        switch self {
        case .none:
            nil
        case .inline:
            .inline
        case .fullscreen:
            .fullscreen
        case .fullscreenFromInline:
            .fullscreenFromInline
        case .pip:
            .pip
        @unknown default:
            fatalError()
        }
    }
}

extension WKSLinearMediaViewingMode {
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

extension WKSLinearMediaTrack: @retroactive Track {
}

#endif // canImport(LinearMediaKit)

#endif // os(visionOS)
