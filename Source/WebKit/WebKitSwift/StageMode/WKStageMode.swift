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
@objc
@implementation
extension WKStageModeInteractionDriver {
    private static let kDragToRotationMultiplier: Float = 3.0

    private var stageModeOperation: WKStageModeOperation = .none

    /// The parent container on which pitch changes will be applied.
    @nonobjc
    private let interactionContainer: Entity

    /// The nested child container on which yaw changes will be applied.
    @nonobjc
    private let turntableInteractionContainer: Entity

    private let modelEntity: WKRKEntity

    private weak var delegate: (any WKStageModeInteractionAware)?

    private var driverInitialized: Bool = false

    @nonobjc
    private var initialManipulationPose: Transform = .identity

    private let simulator = WKStageModeOrbitSimulator()

    @nonobjc
    private var sceneUpdateSubscription: (any Cancellable)?

    // MARK: ObjC Exposed API
    var interactionContainerRef: REEntityRef {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=313180
        unsafe interactionContainer.coreEntity
    }

    var stageModeInteractionInProgress: Bool {
        driverInitialized && stageModeOperation != .none
    }

    init(model: WKRKEntity, container: REEntityRef, delegate: (any WKStageModeInteractionAware)?) {
        self.modelEntity = model
        self.interactionContainer = Entity()
        self.turntableInteractionContainer = Entity()
        self.interactionContainer.name = "WebKit:InteractionContainerEntity"
        self.turntableInteractionContainer.name = "WebKit:TurntableContainerEntity"
        self.delegate = delegate

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=313180
        let containerEntity = unsafe Entity.fromCore(container)
        self.interactionContainer.setParent(containerEntity, preservingWorldTransform: true)
        self.turntableInteractionContainer.setPosition(self.interactionContainer.position(relativeTo: nil), relativeTo: nil)
        self.turntableInteractionContainer.setParent(self.interactionContainer, preservingWorldTransform: true)
    }

    func setContainerTransformInPortal() {
        // Configure entity hierarchy after we have correctly positioned the model
        interactionContainer.setPosition(modelEntity.interactionPivotPoint, relativeTo: nil)
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=313180
        unsafe modelEntity.setParentCore(turntableInteractionContainer.coreEntity, preservingWorldTransform: true)
    }

    func removeInteractionContainerFromSceneOrParent() {
        interactionContainer.removeFromParent()
        turntableInteractionContainer.removeFromParent()
    }

    func interactionDidBegin(_ transform: simd_float4x4) {
        sceneUpdateSubscription?.cancel()
        sceneUpdateSubscription = nil

        driverInitialized = true
        initialManipulationPose = Transform(matrix: transform)
        simulator.gestureDidBegin()

        delegate?.stageModeInteractionDidUpdateModel()
    }

    @objc(interactionDidUpdate:)
    func interactionDidUpdate(_ transform: simd_float4x4) {
        let poseTransform = Transform(matrix: transform)
        switch stageModeOperation {
        case .orbit:
            let xyDelta =
                (poseTransform.translation.inMeters - initialManipulationPose.translation.inMeters).xy
                * Self.kDragToRotationMultiplier
            simulator.gestureDidUpdate(withDeltaX: xyDelta.x, deltaY: xyDelta.y)
            applySimulatorState()
        default:
            break
        }

        delegate?.stageModeInteractionDidUpdateModel()
    }

    @objc(interactionDidEnd)
    func interactionDidEnd() {
        initialManipulationPose = .identity
        simulator.gestureDidEnd()

        sceneUpdateSubscription = interactionContainer.scene?
            .subscribe(to: SceneEvents.Update.self) { [weak self] event in
                guard let self else { return }
                if self.simulator.step(withElapsedTime: Float(event.deltaTime)) {
                    self.applySimulatorState()
                    self.delegate?.stageModeInteractionDidUpdateModel()
                } else {
                    self.sceneUpdateSubscription?.cancel()
                    self.sceneUpdateSubscription = nil
                    self.driverInitialized = false
                }
            }
    }

    func operationDidUpdate(_ operation: WKStageModeOperation) {
        stageModeOperation = operation

        if operation != .none {
            let initialCenter = modelEntity.interactionPivotPoint
            let initialTransform = modelEntity.transform
            let transformMatrix = Transform(
                scale: initialTransform.scale,
                rotation: initialTransform.rotation,
                translation: initialTransform.translation
            )
            interactionContainer.setPosition(initialCenter, relativeTo: nil)
            modelEntity.interactionContainerDidRecenter(fromTransform: transformMatrix.matrix)
        }
    }

    private func applySimulatorState() {
        let pitchRotation = Rotation3D(angle: .init(radians: simulator.currentPitch), axis: .x)
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=313180
        interactionContainer.orientation = unsafe pitchRotation.quaternion.quatf

        let yawRotation = Rotation3D(angle: .init(radians: simulator.currentYaw), axis: .y)
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=313180
        turntableInteractionContainer.orientation = unsafe yawRotation.quaternion.quatf
    }
}

extension simd_float3 {
    // Based on visionOS's Points Per Meter (PPM) heuristics
    var inMeters: simd_float3 {
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
