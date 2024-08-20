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

import Foundation
import RealityKit
import WebKitSwift
import os
import simd

private extension Logger {
    static let realityKitEntity = Logger(subsystem: "com.apple.WebKit", category: "RealityKitEntity")
}

@objc(WKSRKEntity)
public final class WKSRKEntity: NSObject {
    let entity: Entity

    @objc(initWithCoreEntity:) init(with coreEntity: REEntityRef) {
        entity = Entity.__fromCore(__EntityRef.__fromCore(coreEntity))
    }

    @objc(boundingBoxExtents) public var boundingBoxExtents: simd_float3 {
        guard let boundingBox = self.boundingBox else { return SIMD3<Float>(0, 0, 0) }
        return boundingBox.extents
    }

    @objc(boundingBoxCenter) public var boundingBoxCenter: simd_float3 {
        guard let boundingBox = self.boundingBox else { return SIMD3<Float>(0, 0, 0) }
        return boundingBox.center
    }

    private var boundingBox: BoundingBox? {
        entity.visualBounds(relativeTo: entity)
    }

    @objc(transform) public var transform: WKEntityTransform {
        get {
            guard let transformComponent = entity.components[Transform.self] else {
                Logger.realityKitEntity.error("No transform component available from entity")
                return WKEntityTransform(scale: simd_float3.one, rotation: simd_quatf(ix: 0, iy: 0, iz: 0, r: 1), translation: simd_float3.zero)
            }

            return WKEntityTransform(scale: transformComponent.scale, rotation: transformComponent.rotation, translation: transformComponent.translation)
        }

        set {
            entity.components[Transform.self] = Transform(scale: newValue.scale, rotation: newValue.rotation, translation: newValue.translation)
        }
    }

    @objc(opacity) public var opacity: Float {
        get {
            guard let opacityComponent = entity.components[OpacityComponent.self] else {
                return 1.0
            }

            return opacityComponent.opacity
        }

        set {
            let clampedValue = max(0, newValue)
            if clampedValue >= 1.0 {
                entity.components[OpacityComponent.self] = nil
                return
            }

            entity.components[OpacityComponent.self] = OpacityComponent(opacity: clampedValue)
        }
    }

    @objc(startAnimating) public func startAnimating() {
        guard let animation = entity.availableAnimations.first else {
            Logger.realityKitEntity.info("No animation found in entity to play")
            return
        }
        entity.playAnimation(animation.repeat(duration: .infinity))
    }
}

#endif // os(visionOS)
