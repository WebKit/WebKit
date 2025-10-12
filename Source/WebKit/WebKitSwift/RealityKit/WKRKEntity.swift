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

#if ENABLE_MODEL_PROCESS

import Combine
import CoreGraphics
import Foundation
import os
import simd
@_spi(Private) @_spi(RealityKit) @_spi(RealityKit_Webkit) import RealityKit

private extension Logger {
    static let realityKitEntity = Logger(subsystem: "com.apple.WebKit", category: "RealityKitEntity")
}

@objc
@implementation
extension WKRKEntity {
    @nonobjc private let entity: Entity

    weak var delegate: WKRKEntityDelegate?

    @nonobjc private var animationPlaybackController: AnimationPlaybackController? = nil
    @nonobjc private var animationFinishedSubscription: Cancellable?
    @nonobjc private var _duration: TimeInterval? = nil
    @nonobjc private var _playbackRate: Float = 1.0

    private static var _defaultEnvironmentResource: EnvironmentResource?

    class func isLoadFromDataAvailable() -> Bool {
#if canImport(RealityKit, _version: 377)
        true
#else
        false
#endif
    }

    @objc(loadFromData:withAttributionTaskID:entityMemoryLimit:completionHandler:)
    class func load(from data: Data, withAttributionTaskID attributionTaskId: String?, entityMemoryLimit: Int) async -> WKRKEntity? {
#if canImport(RealityKit, _version: "403.0.3")
        do {
            var loadOptions = Entity.__LoadOptions()
            if let attributionTaskId {
                loadOptions.memoryAttributionID = attributionTaskId
            }
            if entityMemoryLimit > 0 {
                loadOptions.enforceMemoryConstraints = true
                loadOptions.memoryLimit = entityMemoryLimit * 1024 * 1024
            }
#if canImport(RealityKit, _version: "403.0.9")
            loadOptions.featuresToSkip = [.audio]
#endif

            let loadedEntity = try await Entity(from: data, options: loadOptions)
            return WKRKEntity(loadedEntity)
        } catch {
            Logger.realityKitEntity.error("Failed to load entity from data")
            return nil
        }
#else
        return nil
#endif // canImport(RealityKit, _version: "403.0.3")
    }

    @nonobjc convenience init(_ rkEntity: Entity) {
        self.init(coreEntity: rkEntity.coreEntity)
    }

    init(coreEntity: REEntityRef) {
        entity = Entity.fromCore(coreEntity)
    }

    var name: String {
        get { entity.name }
        set { entity.name = newValue }
    }
    
    var interactionPivotPoint: simd_float3 {
        entity.visualBounds(relativeTo: nil).center
    }

    var boundingBoxExtents: simd_float3 {
        boundingBox?.extents ?? SIMD3<Float>(0, 0, 0)
    }

    var boundingBoxCenter: simd_float3 {
        boundingBox?.center ?? SIMD3<Float>(0, 0, 0)
    }
    
    var boundingRadius: Float {
        boundingBox?.boundingRadius ?? 0
    }

    @nonobjc private final var boundingBox: BoundingBox? {
        entity.visualBounds(relativeTo: entity)
    }

    var transform: WKEntityTransform {
        get {
            let transform = Transform(matrix: entity.transformMatrix(relativeTo: nil))
            return WKEntityTransform(scale: transform.scale, rotation: transform.rotation, translation: transform.translation)
        }
        set {
            var adjustedTransform = Transform(scale: newValue.scale, rotation: newValue.rotation, translation: newValue.translation)
            if let container = entity.parent {
                adjustedTransform = container.convert(transform: adjustedTransform, from: nil)
            }
            
            entity.transform = adjustedTransform
        }
    }

    var opacity: Float {
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

    var duration: TimeInterval {
        _duration ?? 0
    }

    var loop: Bool = false

    var playbackRate: Float {
        get {
            animationPlaybackController?.speed ?? _playbackRate
        }
        set {
            // FIXME (280081): Support negative playback rate
            _playbackRate = max(newValue, 0);
            guard let animationPlaybackController else {
                return
            }
            animationPlaybackController.speed = _playbackRate
            animationPlaybackStateDidUpdate()
        }
    }

    var paused: Bool {
        get {
            animationPlaybackController?.isPaused ?? true
        }
        set {
            guard let animationPlaybackController, animationPlaybackController.isPaused != newValue else {
                return
            }

            if newValue {
                animationPlaybackController.pause()
            } else {
                animationPlaybackController.resume()
            }
            animationPlaybackStateDidUpdate()
        }
    }

    var currentTime: TimeInterval {
        get {
            animationPlaybackController?.time ?? 0
        }

        set {
            guard let animationPlaybackController, let duration = _duration else {
                return
            }

            let clampedTime = min(max(newValue, 0), duration)
            animationPlaybackController.time = clampedTime
            animationPlaybackStateDidUpdate()
        }
    }

    func setUpAnimationWithAutoPlay(_ autoplay: Bool) {
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

    private static func resizedImage(_ imageSource: CGImageSource) -> CGImage? {
        guard let properties = CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nil) as? [CFString: Any] else {
            Logger.realityKitEntity.error("Resizing IBL image: image source properties are not valid")
            return nil
        }

        guard let imageWidth = properties[kCGImagePropertyPixelWidth] as? CGFloat,
              let imageHeight = properties[kCGImagePropertyPixelHeight] as? CGFloat else {
            Logger.realityKitEntity.error("Resizing IBL image: width and height properties are not valid")
            return nil
        }

        // Use a max of 2k resolution for images to create IBL with.
        let maxSize: CGFloat = 2048
        var scaleFactor: CGFloat = max(imageWidth / maxSize, imageHeight / maxSize)
        var subsampleFactor: CGFloat = 8.0
        switch scaleFactor {
        case ...1:
            return CGImageSourceCreateImageAtIndex(imageSource, 0, nil)
        case ...2:
            subsampleFactor = 2.0
        case ...4:
            subsampleFactor = 4.0
        case ...8:
            subsampleFactor = 8.0
        default:
            Logger.realityKitEntity.error("Resizing IBL image: IBL source image is too large")
            return nil
        }

        let options: [CFString: Any] = [
            kCGImageSourceSubsampleFactor: subsampleFactor
        ]

        guard let image = CGImageSourceCreateImageAtIndex(imageSource, 0, options as CFDictionary) else {
            Logger.realityKitEntity.error("Resizing IBL image: cannot create CGImage")
            return nil
        }

        // Not all image formats support kCGImageSourceSubsampleFactor. If that doesn't work, do an actual resize.
        scaleFactor = max(CGFloat(image.width) / maxSize, CGFloat(image.height) / maxSize)
        if scaleFactor <= 1 {
            return image
        }

        let targetWidth = Int(CGFloat(image.width) / scaleFactor)
        let targetHeight = Int(CGFloat(image.height) / scaleFactor)
        Logger.realityKitEntity.info("Resizing IBL image: doing an actual resize for image with size: (\(image.width), \(image.height)) to target size: (\(targetWidth), \(targetHeight)), bitsPerComponent: \(image.bitsPerComponent), colorSpace: \(String(describing: image.colorSpace)), bitmapInfo: \(String(describing: image.bitmapInfo))")

        var imageBitmapInfoRawValue = image.bitmapInfo.rawValue
        // CGBitmapContext will not render to any non-premultiplied alpha format.
        switch image.bitmapInfo.intersection(.alphaInfoMask).rawValue {
        case CGImageAlphaInfo.first.rawValue:
            imageBitmapInfoRawValue = (imageBitmapInfoRawValue & ~CGBitmapInfo.alphaInfoMask.rawValue) | CGImageAlphaInfo.noneSkipFirst.rawValue
        case CGImageAlphaInfo.last.rawValue:
            imageBitmapInfoRawValue = (imageBitmapInfoRawValue & ~CGBitmapInfo.alphaInfoMask.rawValue) | CGImageAlphaInfo.noneSkipLast.rawValue
        default:
            break
        }
        
        guard let context = CGContext.init(data: nil, width: targetWidth, height: targetHeight, bitsPerComponent: image.bitsPerComponent, bytesPerRow: 0, space: image.colorSpace ?? CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: imageBitmapInfoRawValue) else {
            Logger.realityKitEntity.error("Resizing IBL image: Unable to create CGContext for image resizing")
            return nil
        }
        context.draw(image, in: CGRect(origin: .zero, size: .init(width: targetWidth, height: targetHeight)))
        return context.makeImage()
    }

    @nonobjc private final func applyIBL(_ environment: EnvironmentResource) {
        entity.components[VirtualEnvironmentProbeComponent.self] = .init(source: .single(.init(environment: environment)))
        entity.components[ImageBasedLightComponent.self] = .init(source: .none)
        entity.components[ImageBasedLightReceiverComponent.self] = .init(imageBasedLight: entity)
    }

    @objc(applyIBLData:attributionHandler:withCompletion:)
    func applyIBLData(_ data: Data, attributionHandler: @MainActor @Sendable @escaping (REAssetRef) -> Void) async -> Bool {
        guard let imageSource = CGImageSourceCreateWithData(data as CFData, nil) else {
            Logger.realityKitEntity.error("Cannot get CGImageSource from IBL image data.")
            return false
        }

        guard let cgImage = Self.resizedImage(imageSource) else {
            Logger.realityKitEntity.error("Cannot get CGImage from CGImageSource.")
            return false
        }

        do {
            let textureResource = try await TextureResource(cubeFromEquirectangular: cgImage, options: TextureResource.CreateOptions(semantic: .hdrColor))
            let environment = try await EnvironmentResource(cube: textureResource, options: .init())

            if let coreEnvironmentResourceAsset = environment.coreIBLAsset?.__as(REAssetRef.self) {
                attributionHandler(coreEnvironmentResourceAsset)
            }

            applyIBL(environment)
            Logger.realityKitEntity.info("Successfully applied IBL to entity")
            return true
        } catch {
            Logger.realityKitEntity.error("Cannot load environment resource from CGImage.")
            return false
        }
    }

    func applyDefaultIBL() {
#if canImport(RealityFoundation, _version: 380)
        if let environment = WKRKEntity._defaultEnvironmentResource {
            applyIBL(environment)
            return
        }

        let defaultEnvironmentResource = EnvironmentResource.defaultObject()
        WKRKEntity._defaultEnvironmentResource = defaultEnvironmentResource
        applyIBL(defaultEnvironmentResource)
#else
        entity.components[ImageBasedLightComponent.self] = .init(source: .none)
        entity.components[ImageBasedLightReceiverComponent.self] = nil
#endif
    }

    private func animationPlaybackStateDidUpdate() {
        delegate?.entityAnimationPlaybackStateDidUpdate?(self)
    }

    @objc(setParentCoreEntity:preservingWorldTransform:)
    func setParentCore(_ coreEntity: REEntityRef, preservingWorldTransform: Bool) {
        let parentEntity = Entity.fromCore(coreEntity)
        entity.setParent(parentEntity, preservingWorldTransform: preservingWorldTransform)
    }
    
    @objc(interactionContainerDidRecenterFromTransform:)
    func interactionContainerDidRecenter(fromTransform transform: simd_float4x4) {
        entity.setTransformMatrix(transform, relativeTo: nil)
    }
    
    @objc(recenterEntityAtTransform:)
    func recenter(at transform: WKEntityTransform) {
        // Apply the scale and translation of the entity separately from the rotation
        self.transform = WKEntityTransform(scale: transform.scale, rotation: .init(ix: 0, iy: 0, iz: 0, r: 1), translation: transform.translation)

        // The pivot for the orientation may be different from the center of the model's bounding box
        // As a result, we offset the translation after the rotation has been applied to recenter it
        let pivotPoint = interactionPivotPoint
        self.transform = transform
        let offset = pivotPoint - interactionPivotPoint
        self.transform = WKEntityTransform(scale: transform.scale, rotation: transform.rotation, translation: transform.translation + offset)
    }
}

#endif // ENABLE_MODEL_PROCESS
