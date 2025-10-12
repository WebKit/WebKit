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

#if ENABLE_SWIFTUI

public import SwiftUI
@_spi(CrossImportOverlay) public import WebKit

extension EdgeInsets {
    #if canImport(UIKit)
    init(_ edgeInsets: UIEdgeInsets) {
        self = EdgeInsets(top: edgeInsets.top, leading: edgeInsets.left, bottom: edgeInsets.bottom, trailing: edgeInsets.right)
    }
    #else
    init(_ edgeInsets: NSEdgeInsets) {
        self = EdgeInsets(top: edgeInsets.top, leading: edgeInsets.left, bottom: edgeInsets.bottom, trailing: edgeInsets.right)
    }
    #endif
}

extension ScrollGeometry {
    init(_ geometry: WKScrollGeometryAdapter) {
        self = ScrollGeometry(
            contentOffset: geometry.contentOffset,
            contentSize: geometry.contentSize,
            contentInsets: EdgeInsets(geometry.contentInsets),
            containerSize: geometry.containerSize
        )
    }
}

extension Transaction {
    var isAnimated: Bool {
        animation != nil && !disablesAnimations
    }
}

extension EventModifiers {
    #if canImport(UIKit)
    init(_ wrapped: UIKeyModifierFlags) {
        self =
            switch wrapped {
            case .alphaShift: .capsLock
            case .command: .command
            case .control: .control
            case .numericPad: .numericPad
            case .alternate: .option
            case .shift: .shift
            default: []
            }
    }
    #else
    init(_ wrapped: NSEvent.ModifierFlags) {
        self =
            switch wrapped {
            case .capsLock: .capsLock
            case .command: .command
            case .control: .control
            case .numericPad: .numericPad
            case .option: .option
            case .shift: .shift
            default: []
            }
    }
    #endif
}

#endif
