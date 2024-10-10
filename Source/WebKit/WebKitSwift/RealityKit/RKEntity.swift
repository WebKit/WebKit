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

import Combine
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
    @objc(delegate) weak var delegate: WKSRKEntityDelegate?
    private var animationPlaybackController: AnimationPlaybackController? = nil
    private var animationFinishedSubscription: Cancellable?
    private var _duration: TimeInterval? = nil
    private var _playbackRate: Float = 1.0

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

    @objc(duration) public var duration: TimeInterval {
        guard let _duration else { return 0 }
        return _duration
    }

    @objc(loop) public var loop: Bool = false

    @objc(playbackRate) public var playbackRate: Float {
        get {
            guard let animationPlaybackController else { return _playbackRate }
            return animationPlaybackController.speed
        }

        set {
            // FIXME (280081): Support negative playback rate
            _playbackRate = max(newValue, 0);
            guard let animationPlaybackController else { return }
            animationPlaybackController.speed = _playbackRate
            animationPlaybackStateDidUpdate()
        }
    }

    @objc(paused) public var paused: Bool {
        get {
            guard let animationPlaybackController else { return true }
            return animationPlaybackController.isPaused
        }

        set {
            guard let animationPlaybackController, animationPlaybackController.isPaused != newValue else { return }
            if newValue {
                animationPlaybackController.pause()
            } else {
                animationPlaybackController.resume()
            }
            animationPlaybackStateDidUpdate()
        }
    }

    @objc(currentTime) public var currentTime: TimeInterval {
        get {
            guard let animationPlaybackController else { return 0 }
            return animationPlaybackController.time
        }

        set {
            guard let animationPlaybackController, let duration = _duration else { return }
            let clampedTime = min(max(newValue, 0), duration)
            animationPlaybackController.time = clampedTime
            animationPlaybackStateDidUpdate()
        }
    }

    @objc(setUpAnimationWithAutoPlay:) public func setUpAnimation(with autoplay: Bool) {
        assert(animationPlaybackController == nil)

        guard let animation = entity.availableAnimations.first else {
            Logger.realityKitEntity.info("No animation found in entity to play")
            return
        }

        animationPlaybackController = entity.playAnimation(animation, startsPaused: !autoplay)
        guard let animationPlaybackController else {
            Logger.realityKitEntity.error("Cannot play entity animation")
            return
        }
        _duration = animationPlaybackController.duration
        animationPlaybackController.speed = _playbackRate
        animationPlaybackStateDidUpdate()

        guard let scene = entity.scene else {
            Logger.realityKitEntity.error("No scene to subscribe for animation events")
            return
        }
        animationFinishedSubscription = scene.subscribe(to: AnimationEvents.PlaybackCompleted.self, on: entity) { [weak self] event in
            guard let self,
                  let playbackController = self.animationPlaybackController,
                  event.playbackController == playbackController else {
                Logger.realityKitEntity.error("Cannot schedule the next animation")
                return
            }

            let startsPaused = !self.loop
            let animationController = self.entity.playAnimation(animation, startsPaused: startsPaused)
            animationController.speed = self._playbackRate

            self.animationPlaybackController = animationController
            self.animationPlaybackStateDidUpdate()
        }
    }

    @objc(applyIBLData:) public func applyIBL(data: Data) {
#if canImport(RealityKit, _version: 366)
        guard let imageSource = CGImageSourceCreateWithData(data as CFData, nil) else {
            Logger.realityKitEntity.error("Cannot get CGImageSource from IBL image data")
            return
        }
        guard let cgImage = CGImageSourceCreateImageAtIndex(imageSource, 0, nil) else {
            Logger.realityKitEntity.error("Cannot get CGImage from CGImageSource")
            return
        }

        Task {
            do {
                let textureResource = try await TextureResource(cubeFromEquirectangular: cgImage, options: TextureResource.CreateOptions(semantic: .hdrColor))
                let environment = try await EnvironmentResource(cube: textureResource, options: .init())

                await MainActor.run {
                    entity.components[ImageBasedLightComponent.self] = .init(source: .single(environment))
                    entity.components[ImageBasedLightReceiverComponent.self] = .init(imageBasedLight: entity)
                }
            } catch {
                Logger.realityKitEntity.error("Cannot load environment resource from CGImage")
            }
        }
#endif
    }

    @objc(removeIBL) public func removeIBL() {
        entity.components[ImageBasedLightComponent.self] = nil
        entity.components[ImageBasedLightReceiverComponent.self] = nil
    }

    private func animationPlaybackStateDidUpdate() {
        delegate?.entityAnimationPlaybackStateDidUpdate?(self)
     }
}

#endif // os(visionOS)
