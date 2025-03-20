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

import Combine
import RealityKit
import Spatial
import Foundation
@_spi(RealityKit) import RealityKit
import WebKitSwift
import os
import simd

/// A driver that maps all gesture updates to the specific transform we want for the specified StageMode behavior
@MainActor
@objc(WKStageModeInteractionDriver)
final public class WKStageModeInteractionDriver: NSObject {
    private let kDragToRotationMultiplier: Float = 3.0
    private let kPitchSettleAnimationDuration: Double = 0.2
    private let kMaxDecelerationDuration: Double = 2.0
    private let kDecelerationDampeningFactor: Float = 0.9
    private let kOrbitVelocityTerminationThreshold: Float = 0.0001
    
    private var stageModeOperation: WKStageModeOperation = .none
    
    /// The parent container on which pitch changes will be applied
    let interactionContainer: Entity
    
    /// The nested child container on which yaw changes will be applied
    /// We need to separate rotation-related transforms into two entities so that we can later apply post-gesture animations along the yaw and pitch separately
    let turntableInteractionContainer: Entity
    
    /// A proxy entity used to trigger an animation update for the turntable deceleration
    /// Because the turntable animation depends on the velocity of the pinch, we have to apply a separate animation for each animation tick.
    let turntableAnimationProxyEntity = Entity()
    
    let modelEntity: WKSRKEntity
    
    weak var delegate: WKStageModeInteractionAware?
    
    private var driverInitialized: Bool = false
    private var initialManipulationPose: Transform = .identity
    private var previousManipulationPose: Transform = .identity
    private var initialTargetPose: Transform = .identity
    private var initialTurntablePose: Transform = .identity
    
    private var currentOrbitVelocity: simd_float2 = .zero
    
    // Animation Controllers
    private var pitchSettleAnimationController: AnimationPlaybackController? = nil
    private var yawDecelerationAnimationController: AnimationPlaybackController? = nil
    
    private var pitchAnimationIsPlaying: Bool {
        pitchSettleAnimationController?.isPlaying ?? false
    }
    
    private var yawAnimationIsPlaying: Bool {
        yawDecelerationAnimationController?.isPlaying ?? false
    }
    
    // MARK: ObjC Exposed API
    @objc(interactionContainerRef)
    var interactionContainerRef: REEntityRef {
        self.interactionContainer.__coreEntity.__as(REEntityRef.self)
    }
    
    @objc(stageModeInteractionInProgress)
    var stageModeInteractionInProgress: Bool {
        self.driverInitialized && self.stageModeOperation != .none && (pitchAnimationIsPlaying || yawAnimationIsPlaying)
    }
    
    @objc(initWithModel:container:delegate:)
    init(with model: WKSRKEntity, container: REEntityRef, delegate: WKStageModeInteractionAware?) {
        self.modelEntity = model
        self.interactionContainer = Entity()
        self.turntableInteractionContainer = Entity()
        self.interactionContainer.name = "WebKit:InteractionContainerEntity"
        self.turntableInteractionContainer.name = "WebKit:TurntableContainerEntity"
        self.delegate = delegate
        
        let containerEntity = Entity.__fromCore(__EntityRef.__fromCore(container))
        self.interactionContainer.setParent(containerEntity, preservingWorldTransform: true)
        self.turntableInteractionContainer.setPosition(self.interactionContainer.position(relativeTo: nil), relativeTo: nil)
        self.turntableInteractionContainer.setParent(self.interactionContainer, preservingWorldTransform: true)
        self.turntableAnimationProxyEntity.setParent(self.interactionContainer.parent)
    }
    
    @objc(setContainerTransformInPortal)
    func setContainerTransformInPortal() {
        // Configure entity hierarchy after we have correctly positioned the model
        self.interactionContainer.setPosition(modelEntity.interactionPivotPoint, relativeTo: nil)
        modelEntity.setParent(self.turntableInteractionContainer.__coreEntity.__as(REEntityRef.self), preservingWorldTransform: true)
    }
    
    @objc(removeInteractionContainerFromSceneOrParent)
    func removeInteractionContainerFromSceneOrParent() {
        self.interactionContainer.removeFromParent()
        self.turntableInteractionContainer.removeFromParent()
    }
    
    @objc(interactionDidBegin:)
    func interactionDidBegin(_ transform: simd_float4x4) {
        driverInitialized = true
        
        self.currentOrbitVelocity = .zero
        if pitchAnimationIsPlaying {
            pitchSettleAnimationController?.pause()
            self.pitchSettleAnimationController = nil
        }

        if yawAnimationIsPlaying {
            yawDecelerationAnimationController?.pause()
            self.yawDecelerationAnimationController = nil
        }

        let initialCenter = modelEntity.interactionPivotPoint
        let initialTransform = modelEntity.transform
        let transformMatrix = Transform(scale: initialTransform.scale, rotation: initialTransform.rotation, translation: initialTransform.translation)
        self.interactionContainer.setPosition(initialCenter, relativeTo: nil)
        self.modelEntity.interactionContainerDidRecenter(transformMatrix.matrix)

        let initialPoseTransform = Transform(matrix: transform)
        initialManipulationPose = initialPoseTransform
        previousManipulationPose = initialPoseTransform
        initialTargetPose = interactionContainer.transform
        initialTargetPose.rotation = .init(ix: 0, iy: 0, iz: 0, r: 1)
        initialTurntablePose = turntableInteractionContainer.transform
        turntableAnimationProxyEntity.setPosition(.zero, relativeTo: nil)
        
        self.delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(interactionDidUpdate:)
    func interactionDidUpdate(_ transform: simd_float4x4) {
        let poseTransform = Transform(matrix: transform)
        switch stageModeOperation {
        case .orbit:
            do {
                let xyDelta = (poseTransform.translation._inMeters - initialManipulationPose.translation._inMeters).xy * kDragToRotationMultiplier

                // Apply pitch along global x axis
                let containerPitchRotation = Rotation3D(angle: .init(radians: xyDelta.y), axis: .x)
                self.interactionContainer.orientation = initialTargetPose.rotation * containerPitchRotation.quaternion.quatf

                // Apply yaw along local y axis
                let turntableYawRotation = Rotation3D(angle: .init(radians: xyDelta.x), axis: .y)
                self.turntableInteractionContainer.orientation = initialTurntablePose.rotation * turntableYawRotation.quaternion.quatf

                self.currentOrbitVelocity = (poseTransform.translation._inMeters - previousManipulationPose.translation._inMeters).xy * kDragToRotationMultiplier
                break
            }
        default:
            break
        }

        previousManipulationPose = poseTransform
        self.delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(interactionDidEnd)
    func interactionDidEnd() {
        driverInitialized = false
        initialManipulationPose = .identity
        previousManipulationPose = .identity
        
        // Settle the pitch of the interaction container
        pitchSettleAnimationController = self.interactionContainer.move(to: initialTargetPose, relativeTo: self.interactionContainer.parent, duration: kPitchSettleAnimationDuration, timingFunction: .easeOut)
        subscribeToPitchChanges()
        
        // The proxy does not actually perform the turntable animation; we instead use it to programmatically apply a deceleration curve to the yaw
        // based on the user's current orbit velocity
        yawDecelerationAnimationController = self.turntableAnimationProxyEntity.move(to: Transform(scale: .one, rotation: .init(ix: 0, iy: 0, iz: 0, r: 1), translation: .init(repeating: 1)), relativeTo: nil, duration: kMaxDecelerationDuration, timingFunction: .linear)
        subscribeToYawChanges()
    }
    
    @objc(operationDidUpdate:)
    func operationDidUpdate(_ operation: WKStageModeOperation) {
        self.stageModeOperation = operation
    }
    
    private func subscribeToPitchChanges() {
        if pitchAnimationIsPlaying {
            withObservationTracking {
#if canImport(RealityFoundation, _version: 380)
                _ = self.interactionContainer.proto_observableComponents[Transform.self]
#endif
            } onChange: {
                Task { @MainActor in
                    self.delegate?.stageModeInteractionDidUpdateModel()
                    
                    // Because the onChange only gets called once, we need to re-subscribe to the function while we are animating
                    self.subscribeToPitchChanges()
                }
            }
        } else {
            self.driverInitialized = self.yawAnimationIsPlaying
        }
    }
    
    private func subscribeToYawChanges() {
        if yawAnimationIsPlaying {
            withObservationTracking {
#if canImport(RealityFoundation, _version: 380)
                // By default, we do not care about the proxy, but we use the update to set the deceleration of the turntable container
                _ = turntableAnimationProxyEntity.proto_observableComponents[Transform.self]
#endif
            } onChange: {
                Task { @MainActor in
                    let deltaX = self.currentOrbitVelocity.x
                    let turntableYawQuat = Rotation3D(angle: .init(radians: deltaX), axis: .y)
                    self.turntableInteractionContainer.orientation *= turntableYawQuat.quaternion.quatf
                    self.currentOrbitVelocity *= self.kDecelerationDampeningFactor

                    self.delegate?.stageModeInteractionDidUpdateModel()
                    
                    // Because the onChange only gets called once, we need to re-subscribe to the function while we are animating
                    // It is possible that the models stops moving even if the animation continues, so we should check for early stop
                    if (abs(self.currentOrbitVelocity.x) > self.kOrbitVelocityTerminationThreshold) {
                        self.subscribeToYawChanges()
                    } else {
                        self.yawDecelerationAnimationController?.stop()
                        self.driverInitialized = self.pitchAnimationIsPlaying
                    }
                }
            }
        } else {
            self.driverInitialized = self.pitchAnimationIsPlaying
        }
    }
}

extension simd_float3 {
    // Based on visionOS's Points Per Meter (PPM) heuristics
    var _inMeters: simd_float3 {
        self / 1360.0
    }

    var xy: simd_float2 {
        return .init(x, y)
    }

    var double3: simd_double3 {
        return .init(Double(x), Double(y), Double(z))
    }
}

extension simd_quatd {
    var quatf: simd_quatf {
        return .init(ix: Float(self.imag.x), iy: Float(self.imag.y), iz: Float(self.imag.z), r: Float(self.real))
    }
}

#endif
