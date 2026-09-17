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

#if compiler(>=6.2.3)

#if ENABLE_GPU_PROCESS_MODEL

import WebKit_Internal
import wtf

/// The GPU process receiver for the model element's stream messages.
///
/// Dispatch reaches here directly from the generated `RemoteMeshMessageForwarder`; there is no
/// hand-written C++ in between. The handlers themselves are thin - the work is done by the
/// `Mesh` backing - which is the point: this is the surface a compromised Web Content process
/// talks to, so it is the surface worth having in a memory-safe language.
final class RemoteMesh {
    private let backing: WebKit.Mesh
    private let objectHeap: WebKit.WeakPtrModelObjectHeap
    private let gpu: WebKit.WeakPtrRemoteGPU
    private let streamConnection: IPC.StreamServerConnection
    private let identifier: WebKit.WebModelIdentifier
    private let standardDynamicRange: Bool

    // Optional only because the forwarder needs `self`, which is not available until every other
    // stored property has been initialized. Always occupied afterwards.
    private var messageForwarder: RefRemoteMeshMessageForwarder?

    // @used ensures these are retained even under -O -wmo: rdar://179098545
    @used
    init(
        backing: WebKit.Mesh,
        objectHeap: WebKit.WeakPtrModelObjectHeap,
        gpu: WebKit.WeakPtrRemoteGPU,
        streamConnection: IPC.StreamServerConnection,
        identifier: WebKit.WebModelIdentifier,
        standardDynamicRange: Bool
    ) {
        self.backing = backing
        self.objectHeap = objectHeap
        self.gpu = gpu
        self.streamConnection = streamConnection
        self.identifier = identifier
        self.standardDynamicRange = standardDynamicRange

        let forwarder = WebKit.RemoteMeshMessageForwarder.create(target: self)
        self.messageForwarder = forwarder
        WebKit.startReceivingMeshMessages(streamConnection, forwarder, identifier)
    }

    deinit {
        WebKit.stopReceivingMeshMessages(streamConnection, identifier)
    }

    @used
    func sharedPreferencesForWebProcess() -> WebKit.OptionalSharedPreferencesForWebProcess {
        guard let gpu = gpu.get() else {
            return WebKit.OptionalSharedPreferencesForWebProcess()
        }
        return gpu.sharedPreferencesForWebProcess()
    }

    /// The mesh this receiver drives, for `ModelObjectHeap::convertMeshFromBacking`.
    @used
    func backingMesh() -> WebKit.Mesh {
        backing
    }

    // IPCs from here on.

    func setLabel(connection: IPC.StreamServerConnection, label: WTF.String) {
        backing.setLabel(consuming: label)
    }

    func destruct(connection: IPC.StreamServerConnection) {
        // This drops the last reference to `self`, so nothing may touch it afterwards.
        objectHeap.get()?.removeObject(identifier)
    }

    func update(
        connection: IPC.StreamServerConnection,
        descriptor: WebModel.VectorUpdateMeshDescriptor,
        completionHandler: CompletionHandlers.RemoteMesh.UpdateCompletionHandler
    ) {
        // Safety: the descriptor is ours to move; Swift cannot see that WTF::Vector owns its
        // buffer, and these descriptors are only ever forwarded, never indexed, here.
        unsafe backing.update(consuming: descriptor)
        completionHandler.pointee(true)
    }

    func updateTexture(
        connection: IPC.StreamServerConnection,
        descriptor: WebModel.VectorUpdateTextureDescriptor,
        completionHandler: CompletionHandlers.RemoteMesh.UpdateTextureCompletionHandler
    ) {
        // Safety: as in update(), the descriptor is only forwarded.
        unsafe backing.updateTexture(consuming: descriptor)
        completionHandler.pointee(true)
    }

    func updateMaterial(
        connection: IPC.StreamServerConnection,
        descriptor: WebModel.VectorUpdateMaterialDescriptor,
        completionHandler: CompletionHandlers.RemoteMesh.UpdateMaterialCompletionHandler
    ) {
        // Safety: as in update(), the descriptor is only forwarded.
        unsafe backing.updateMaterial(consuming: descriptor)
        completionHandler.pointee(true)
    }

    func render(
        connection: IPC.StreamServerConnection,
        textureIndex: UInt32,
        completionHandler: CompletionHandlers.RemoteMesh.RenderCompletionHandler
    ) {
        guard let gpu = gpu.get() else {
            completionHandler.pointee(false)
            return
        }
        WebKit.renderMesh(backing, gpu, textureIndex, completionHandler)
    }

    func processRemovals(
        connection: IPC.StreamServerConnection,
        meshRemovals: WebModel.VectorTypedResourceId,
        materialRemovals: WebModel.VectorTypedResourceId,
        textureRemovals: WebModel.VectorTypedResourceId,
        completionHandler: CompletionHandlers.RemoteMesh.ProcessRemovalsCompletionHandler
    ) {
        WebKit.meshProcessRemovals(backing, meshRemovals, materialRemovals, textureRemovals, completionHandler)
    }

    func updateTransform(connection: IPC.StreamServerConnection, transform: WebModel.Float4x4) {
        backing.setEntityTransform(transform)
    }

    func setFOV(connection: IPC.StreamServerConnection, fovY: Float) {
        backing.setFOV(fovY)
    }

    func play(connection: IPC.StreamServerConnection, playing: Bool) {
        backing.play(playing)
    }

    func setEnvironmentMap(connection: IPC.StreamServerConnection, imageAsset: WebModel.UpdateTextureDescriptor) {
        backing.setEnvironmentMap(consuming: imageAsset)
    }

    func updateContentsHeadroom(connection: IPC.StreamServerConnection, headroom: Float) {
        // Mesh implementations ignore this where HAVE(SUPPORT_HDR_DISPLAY) is off.
        backing.updateContentsHeadroom(headroom)
    }

    func updateRenderBuffers(
        connection: IPC.StreamServerConnection,
        width: UInt32,
        height: UInt32,
        completionHandler: CompletionHandlers.RemoteMesh.UpdateRenderBuffersCompletionHandler
    ) {
        // Safety: the reply is a WTF::Vector, which owns its buffer; it is constructed and moved
        // into the completion handler without being indexed.
        guard let gpu = gpu.get() else {
            unsafe completionHandler.pointee(consuming: WTF.VectorMachSendRight())
            return
        }
        let handles = unsafe WebKit.resizeMeshRenderBuffers(backing, gpu, width, height, standardDynamicRange)
        unsafe completionHandler.pointee(consuming: handles)
    }

    func paintCurrentFrameToImageBuffer(
        connection: IPC.StreamServerConnection,
        imageBufferIdentifier: WebCore.RenderingResourceIdentifier,
        bufferIndex: UInt32,
        completionHandler: CompletionHandlers.RemoteMesh.PaintCurrentFrameToImageBufferCompletionHandler
    ) {
        if let gpu = gpu.get() {
            WebKit.paintMeshCurrentFrameToImageBuffer(backing, gpu, imageBufferIdentifier, bufferIndex)
        }
        completionHandler.pointee()
    }
}

#endif // ENABLE_GPU_PROCESS_MODEL

#endif // compiler(>=6.2.3)
