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

#if ENABLE_MODEL_PROCESS

import Combine
import Spatial
import Foundation
@_spi(RealityKit) import RealityKit
import os
import simd

/// A driver that maps all gesture updates to the specific transform we want for the specified StageMode behavior
@objc @implementation extension WKStageModeInteractionDriver {
    private static let kDragToRotationMultiplier: Float = 3.0
    private static let kPitchSettleAnimationDuration: Double = 0.4
    private static let kMaxDecelerationDuration: Double = 2.0
    private static let kDecelerationDampeningFactor: Float = 0.95
    private static let kOrbitVelocityTerminationThreshold: Float = 0.0001

    private var stageModeOperation: WKStageModeOperation = .none
    
    /// The parent container on which pitch changes will be applied
    @nonobjc private let interactionContainer: Entity

    /// The nested child container on which yaw changes will be applied
    /// We need to separate rotation-related transforms into two entities so that we can later apply post-gesture animations along the yaw and pitch separately
    @nonobjc private let turntableInteractionContainer: Entity

    /// A proxy entity used to trigger an animation update for the turntable deceleration
    /// Because the turntable animation depends on the velocity of the pinch, we have to apply a separate animation for each animation tick.
    @nonobjc private let turntableAnimationProxyEntity = Entity()

    private let modelEntity: WKRKEntity

    private weak var delegate: WKStageModeInteractionAware?

    private var driverInitialized: Bool = false
    private var allowAnimationObservation: Bool = false
    @nonobjc private var initialManipulationPose: Transform = .identity
    @nonobjc private var previousManipulationPose: Transform = .identity
    @nonobjc private var initialTargetPose: Transform = .identity
    @nonobjc private var initialTurntablePose: Transform = .identity

    private var currentOrbitVelocity: simd_float2 = .zero
    
    // Animation Controllers
    @nonobjc private var pitchSettleAnimationController: AnimationPlaybackController? = nil
    @nonobjc private var yawDecelerationAnimationController: AnimationPlaybackController? = nil
    @nonobjc private var pitchAnimationCompletionSubscription: Cancellable? = nil
    @nonobjc private var yawAnimationCompletionSubscription: Cancellable? = nil

    private var pitchAnimationIsPlaying: Bool {
        pitchSettleAnimationController?.isPlaying ?? false
    }
    
    private var yawAnimationIsPlaying: Bool {
        yawDecelerationAnimationController?.isPlaying ?? false
    }
    
    // MARK: ObjC Exposed API
    var interactionContainerRef: REEntityRef {
        interactionContainer.coreEntity
    }

    var stageModeInteractionInProgress: Bool {
        driverInitialized && stageModeOperation != .none
    }
    
    init(model: WKRKEntity, container: REEntityRef, delegate: WKStageModeInteractionAware?) {
        self.modelEntity = model
        self.interactionContainer = Entity()
        self.turntableInteractionContainer = Entity()
        self.interactionContainer.name = "WebKit:InteractionContainerEntity"
        self.turntableInteractionContainer.name = "WebKit:TurntableContainerEntity"
        self.delegate = delegate
        
        let containerEntity = Entity.fromCore(container)
        self.interactionContainer.setParent(containerEntity, preservingWorldTransform: true)
        self.turntableInteractionContainer.setPosition(self.interactionContainer.position(relativeTo: nil), relativeTo: nil)
        self.turntableInteractionContainer.setParent(self.interactionContainer, preservingWorldTransform: true)
        self.turntableAnimationProxyEntity.setParent(self.interactionContainer.parent)
    }

    func setContainerTransformInPortal() {
        // Configure entity hierarchy after we have correctly positioned the model
        interactionContainer.setPosition(modelEntity.interactionPivotPoint, relativeTo: nil)
        modelEntity.setParentCore(turntableInteractionContainer.coreEntity, preservingWorldTransform: true)
    }
    
    func removeInteractionContainerFromSceneOrParent() {
        interactionContainer.removeFromParent()
        turntableInteractionContainer.removeFromParent()
    }
    
    func interactionDidBegin(_ transform: simd_float4x4) {
        pitchSettleAnimationController?.pause()
        pitchSettleAnimationController = nil

        yawDecelerationAnimationController?.pause()
        yawDecelerationAnimationController = nil
        
        driverInitialized = true
        allowAnimationObservation = false
        
        currentOrbitVelocity = .zero

        let initialPoseTransform = Transform(matrix: transform)
        initialManipulationPose = initialPoseTransform
        previousManipulationPose = initialPoseTransform
        initialTargetPose = interactionContainer.transform
        initialTargetPose.rotation = .init(ix: 0, iy: 0, iz: 0, r: 1)
        initialTurntablePose = turntableInteractionContainer.transform
        turntableAnimationProxyEntity.setPosition(.zero, relativeTo: nil)
        
        delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(interactionDidUpdate:)
    func interactionDidUpdate(_ transform: simd_float4x4) {
        let poseTransform = Transform(matrix: transform)
        switch stageModeOperation {
        case .orbit:
            do {
                let xyDelta = (poseTransform.translation._inMeters - initialManipulationPose.translation._inMeters).xy * Self.kDragToRotationMultiplier

                // Apply pitch along global x axis
                let containerPitchRotation = Rotation3D(angle: .init(radians: xyDelta.y), axis: .x)
                interactionContainer.orientation = initialTargetPose.rotation * containerPitchRotation.quaternion.quatf

                // Apply yaw along local y axis
                let turntableYawRotation = Rotation3D(angle: .init(radians: xyDelta.x), axis: .y)
                turntableInteractionContainer.orientation = initialTurntablePose.rotation * turntableYawRotation.quaternion.quatf

                currentOrbitVelocity = (poseTransform.translation._inMeters - previousManipulationPose.translation._inMeters).xy * Self.kDragToRotationMultiplier
                break
            }
        default:
            break
        }

        previousManipulationPose = poseTransform
        delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(interactionDidEnd)
    func interactionDidEnd() {
        allowAnimationObservation = true
        
        initialManipulationPose = .identity
        previousManipulationPose = .identity
        
        // Settle the pitch of the interaction container
        pitchSettleAnimationController = interactionContainer.move(to: initialTargetPose, relativeTo: interactionContainer.parent, duration: Self.kPitchSettleAnimationDuration, timingFunction: .cubicBezier(controlPoint1: .init(0.08, 0.6), controlPoint2: .init(0.4, 1.0)))
        subscribeToPitchChanges()
        pitchAnimationCompletionSubscription = interactionContainer.scene?.subscribe(to: AnimationEvents.PlaybackCompleted.self, on: interactionContainer) { [weak self] _ in
            guard let self else {
                return
            }
            
            self.driverInitialized = self.yawAnimationIsPlaying || self.pitchAnimationIsPlaying
        }
        
        // The proxy does not actually perform the turntable animation; we instead use it to programmatically apply a deceleration curve to the yaw
        // based on the user's current orbit velocity
        yawDecelerationAnimationController = turntableAnimationProxyEntity.move(to: Transform(scale: .one, rotation: .init(ix: 0, iy: 0, iz: 0, r: 1), translation: .init(repeating: 1)), relativeTo: nil, duration: Self.kMaxDecelerationDuration, timingFunction: .linear)
        subscribeToYawChanges()
        yawAnimationCompletionSubscription = turntableAnimationProxyEntity.scene?.subscribe(to: AnimationEvents.PlaybackTerminated.self, on: turntableAnimationProxyEntity) { [weak self] _ in
            guard let self else {
                return
            }
            
            self.driverInitialized = self.yawAnimationIsPlaying || self.pitchAnimationIsPlaying
        }
    }

    func operationDidUpdate(_ operation: WKStageModeOperation) {
        stageModeOperation = operation
        
        if operation != .none {
            let initialCenter = modelEntity.interactionPivotPoint
            let initialTransform = modelEntity.transform
            let transformMatrix = Transform(scale: initialTransform.scale, rotation: initialTransform.rotation, translation: initialTransform.translation)
            interactionContainer.setPosition(initialCenter, relativeTo: nil)
            modelEntity.interactionContainerDidRecenter(fromTransform: transformMatrix.matrix)
        }
    }
    
    private func subscribeToPitchChanges() {
        withObservationTracking {
            #if canImport(RealityFoundation, _version: "403.0.4")
            _ = interactionContainer.observable.components[Transform.self]
            #endif
        } onChange: { [self] in
            Task { @MainActor in
                guard allowAnimationObservation else {
                    return
                }
                
                delegate?.stageModeInteractionDidUpdateModel()
                
                // Because the onChange only gets called once, we need to re-subscribe to the function while we are animating
                if pitchAnimationIsPlaying {
                    subscribeToPitchChanges()
                }
            }
        }
    }
    
    private func subscribeToYawChanges() {
        withObservationTracking {
            #if canImport(RealityFoundation, _version: "403.0.4")
            // By default, we do not care about the proxy, but we use the update to set the deceleration of the turntable container
            _ = turntableAnimationProxyEntity.observable.components[Transform.self]
            #endif
        } onChange: { [self] in
            Task { @MainActor in
                guard allowAnimationObservation else {
                    return
                }
                
                let deltaX = currentOrbitVelocity.x
                let turntableYawQuat = Rotation3D(angle: .init(radians: deltaX), axis: .y)
                turntableInteractionContainer.orientation *= turntableYawQuat.quaternion.quatf
                currentOrbitVelocity *= Self.kDecelerationDampeningFactor

                delegate?.stageModeInteractionDidUpdateModel()
                
                // Because the onChange only gets called once, we need to re-subscribe to the function while we are animating
                // It is possible that the models stops moving even if the animation continues, so we should check for early stop
                if (abs(currentOrbitVelocity.x) <= Self.kOrbitVelocityTerminationThreshold) {
                    yawDecelerationAnimationController?.stop()
                }
                
                if yawAnimationIsPlaying {
                    subscribeToYawChanges()
                }
            }
        }
    }
}

extension simd_float3 {
    // Based on visionOS's Points Per Meter (PPM) heuristics
    var _inMeters: simd_float3 {
        self / 1360.0
    }

    var xy: simd_float2 {
        .init(x, y)
    }

    var double3: simd_double3 {
        .init(Double(x), Double(y), Double(z))
    }
}

extension simd_quatd {
    var quatf: simd_quatf {
        .init(ix: Float(imag.x), iy: Float(imag.y), iz: Float(imag.z), r: Float(real))
    }
}

#endif // ENABLE_MODEL_PROCESS
