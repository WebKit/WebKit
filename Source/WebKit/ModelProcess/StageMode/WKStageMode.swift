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

internal import Combine
internal import RealityKit
internal import Spatial
internal import WebKit_Internal
internal import os
internal import simd
@_spi(Private) @_spi(RealityKit) @_spi(CoreREAdditions) internal import RealityFoundation

/// A driver that maps all gesture updates to the specific transform we want for the specified StageMode behavior
@MainActor
@objc @implementation extension WKStageModeInteractionDriver {
    @nonobjc private let kDragToRotationMultiplier: Float = 5.0
    
    @nonobjc private var stageModeOperation: WKStageModeOperation = .none
    
    /// The parent container on which pitch changes will be applied
    @nonobjc let interactionContainer: Entity
    
    /// The nested child container on which yaw changes will be applied
    /// We need to separate rotation-related transforms into two entities so that we can later apply post-gesture animations along the yaw and pitch separately
    @nonobjc let turntableInteractionContainer: Entity
    
    @nonobjc let modelEntity: WKSRKEntity
    
    @nonobjc var delegate: WKStageModeInteractionAware?
    
    // Transform state machine
    @nonobjc private var driverInitialized: Bool = false
    @nonobjc private var initialManipulationPose: Transform = .identity
    @nonobjc private var previousManipulationPose: Transform = .identity
    @nonobjc private var initialTargetPose: Transform = .identity
    
    // MARK: ObjC Exposed API
    @objc(initWithModel:container:delegate:)
    init(with model: WKSRKEntity, container: REEntityRef, delegate: WKStageModeInteractionAware?) {
        self.modelEntity = model
        self.interactionContainer = Entity()
        self.turntableInteractionContainer = Entity()
        self.interactionContainer.name = "WebKit:InteractionContainerEntity"
        self.turntableInteractionContainer.name = "WebKit:TurntableContainerEntity"
        
        let containerEntity = Entity.__fromCore(__EntityRef.__fromCore(container))
        self.interactionContainer.setParent(containerEntity, preservingWorldTransform: true)
        self.turntableInteractionContainer.setPosition(self.interactionContainer.position(relativeTo: nil), relativeTo: nil)
        self.turntableInteractionContainer.setParent(self.interactionContainer, preservingWorldTransform: true)
        
        REEntitySubtreeAddNetworkComponentRecursive(self.interactionContainer.__coreEntity.__as(REEntityRef.self))
        RENetworkMarkEntityMetadataDirty(self.interactionContainer.__coreEntity.__as(REEntityRef.self))
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

        let initialCenter = modelEntity.interactionPivotPoint
        let initialTransform = modelEntity.transform
        let transformMatrix = Transform(scale: initialTransform.scale, rotation: initialTransform.rotation, translation: initialTransform.translation)
        self.interactionContainer.setPosition(initialCenter, relativeTo: nil)
        self.modelEntity.interactionContainerDidRecenter(transformMatrix.matrix)
        
        let tf = Transform(matrix: transform)
        initialManipulationPose = tf
        previousManipulationPose = tf
        initialTargetPose = interactionContainer.transform
        self.delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(interactionDidUpdate:)
    func interactionDidUpdate(_ transform: simd_float4x4) {
        let tf = Transform(matrix: transform)
        switch stageModeOperation {
        case .orbit:
            do {
                let xyDelta = (tf.translation._inMeters - previousManipulationPose.translation._inMeters).xy * kDragToRotationMultiplier
                
                // Apply pitch along global x axis
                var quat = Rotation3D(angle: .init(radians: xyDelta.y), axis: .init(vector: self.interactionContainer.convert(direction: .init(1, 0, 0), from: nil).double3))
                
                // Apply yaw along local y axis
                quat = quat.rotated(by: Rotation3D(angle: .init(radians: xyDelta.x), axis: .y))
                
                self.interactionContainer.orientation *= quat.quaternion.quatf
                break
            }
        default:
            break
        }

        previousManipulationPose = tf
        self.delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(interactionDidEnd)
    func interactionDidEnd() {
        driverInitialized = false
        initialManipulationPose = .identity
        previousManipulationPose = .identity
        self.delegate?.stageModeInteractionDidUpdateModel()
    }
    
    @objc(operationDidUpdate:)
    func operationDidUpdate(_ operation: WKStageModeOperation) {
        self.stageModeOperation = operation
    }
    
    @objc(stageModeInteractionInProgress)
    func stageModeInteractionInProgress() -> Bool {
        self.driverInitialized && self.stageModeOperation != .none
    }
}

// MARK: - SIMD Extentions

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
