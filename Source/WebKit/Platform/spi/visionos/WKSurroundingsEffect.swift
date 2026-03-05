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

#if os(visionOS)

import Observation
@_weakLinked internal import SwiftUI
import UIKit
internal import WebKit_Internal

@MainActor
@Observable
final class WKSurroundingsEffectManagerWrapper {
    var currentEffect: WKSurroundingsEffectType = .none

    var surroundingsEffect: SurroundingsEffect? {
        switch currentEffect {
        case .none:
            nil
        case .semiDark:
            .semiDark
        case .dark:
            .dark
        case .ultraDark:
            .ultraDark
        @unknown default:
            nil
        }
    }

    nonisolated init() {}
}

private let sharedWrapper = WKSurroundingsEffectManagerWrapper()

extension WKSurroundingsEffectManager {
    @nonobjc
    var wrapper: WKSurroundingsEffectManagerWrapper {
        sharedWrapper
    }
}

@objc
@implementation
extension WKSurroundingsEffectManager {
    static let sharedInstance = WKSurroundingsEffectManager()

    class func shared() -> WKSurroundingsEffectManager {
        sharedInstance
    }

    var currentEffect: WKSurroundingsEffectType {
        get { sharedWrapper.currentEffect }
        set { sharedWrapper.currentEffect = newValue }
    }

    override init() {}
}

@objc
@implementation
extension WKSurroundingsEffectWindow {
    private final var effectHostingController: UIViewController?

    @MainActor
    func setupSurroundingsEffectIfNeeded() {
        guard effectHostingController == nil, let rootViewController else {
            return
        }

        let wrapper = WKSurroundingsEffectManager.shared().wrapper

        let effectView = WKSurroundingsEffectView()
            .environment(wrapper)
        let hostingController = UIHostingController(rootView: effectView)
        hostingController.view.backgroundColor = .clear
        hostingController.view.isUserInteractionEnabled = false
        hostingController.view.frame = rootViewController.view.bounds
        hostingController.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]

        effectHostingController = hostingController
        rootViewController.addChild(hostingController)
        rootViewController.view.insertSubview(hostingController.view, at: 0)
        hostingController.didMove(toParent: rootViewController)
    }
}
#endif
