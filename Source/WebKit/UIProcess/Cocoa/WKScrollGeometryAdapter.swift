// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI && compiler(>=6.0)

public import Foundation
internal import WebKit_Internal

// SPI for the cross-import overlay.
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_spi(CrossImportOverlay)
public struct WKScrollGeometryAdapter {
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public let containerSize: CGSize

    #if canImport(UIKit)
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public let contentInsets: UIEdgeInsets
    #else
    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public let contentInsets: NSEdgeInsets
    #endif

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public let contentOffset: CGPoint

    // SPI for the cross-import overlay.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public let contentSize: CGSize

    #if compiler(>=6.2.3)
    // Workaround for rdar://164465358
    @_expose(!Cxx)
    #endif
    init(_ geometry: WKScrollGeometry) {
        self.containerSize = geometry.containerSize
        self.contentInsets = geometry.contentInsets
        self.contentOffset = geometry.contentOffset
        self.contentSize = geometry.contentSize
    }
}

#endif
