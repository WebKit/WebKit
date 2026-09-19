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

#if ENABLE_CONNECTED_VOLUMETRIC_SCENE && WTF_PLATFORM_VISION

import Foundation
@_weakLinked import SwiftUI
@_weakLinked @_spi(RealityKit) import RealityKit
import WebKit_Internal

// The gesture must be targeted to the entity; attached to the view instead it never fires.
private struct WKPortalVolumetricGestureOverlay: View {
    let proxyEntity: Entity
    let onDragBegan: (CGPoint) -> Void
    let onDragChanged: (CGPoint) -> Void
    let onDragEnded: () -> Void

    // DragGesture reports no began phase, so the first change is it.
    @State
    private var isDragging = false

    var body: some View {
        RealityView { content in
            content.add(proxyEntity)
        }
        .gesture(
            DragGesture(minimumDistance: 0)
                .targetedToAnyEntity()
                .onChanged { value in
                    if !isDragging {
                        isDragging = true
                        onDragBegan(value.location)
                    } else {
                        onDragChanged(value.location)
                    }
                }
                .onEnded { _ in
                    isDragging = false
                    onDragEnded()
                }
        )
    }
}

@objc
@implementation
extension WKPortalVolumetricGestureController {
    var onDragBegan: ((CGPoint) -> Void)?
    var onDragChanged: ((CGPoint) -> Void)?
    var onDragEnded: (() -> Void)?

    @nonobjc
    private let proxyEntity = Entity()

    func makeHostingController() -> UIViewController {
        let overlay = WKPortalVolumetricGestureOverlay(
            proxyEntity: proxyEntity,
            onDragBegan: { [weak self] location in self?.onDragBegan?(location) },
            onDragChanged: { [weak self] location in self?.onDragChanged?(location) },
            onDragEnded: { [weak self] in self?.onDragEnded?() }
        )

        let hostingController = UIHostingController(rootView: overlay)
        hostingController.view.backgroundColor = .clear
        return hostingController
    }

    func updateProxyExtents(withWidth width: Float, height: Float, depth: Float) {
        proxyEntity.components[InputTargetComponent.self] = InputTargetComponent(allowedInputTypes: .indirect)
        proxyEntity.components[CollisionComponent.self] = CollisionComponent(
            shapes: [.generateBox(width: width, height: height, depth: depth)]
        )
    }
}

#endif // ENABLE_CONNECTED_VOLUMETRIC_SCENE && WTF_PLATFORM_VISION
