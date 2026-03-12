// Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

private import CxxStdlib
public import Metal
internal import WebGPU_Internal.Buffer
internal import WebGPU_Internal.CommandEncoder
internal import WebGPU_Internal.CxxBridging
internal import WebGPU_Internal.QuerySet
internal import WebGPU_Internal.TextureOrTextureView
public import WebGPU_Private.WebGPU

typealias String = Swift.String

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public func clearBuffer(
    commandEncoder: WebGPU.CommandEncoder,
    buffer: WebGPU.Buffer,
    offset: UInt64,
    size: inout UInt64
) {
    commandEncoder.clearBuffer(buffer: buffer, offset: offset, size: &size)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
public func resolveQuerySet(
    commandEncoder: WebGPU.CommandEncoder,
    querySet: WebGPU.QuerySet,
    firstQuery: UInt32,
    queryCount: UInt32,
    destination: WebGPU.Buffer,
    destinationOffset: UInt64
) {
    commandEncoder.resolveQuerySet(
        querySet,
        firstQuery: firstQuery,
        queryCount: queryCount,
        destination: destination,
        destinationOffset: destinationOffset
    )
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_copyBufferToTexture_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    source: WGPUImageCopyBuffer,
    destination: WGPUImageCopyTexture,
    copySize: WGPUExtent3D
) {
    commandEncoder.copyBufferToTexture(source: source, destination: destination, copySize: copySize)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_copyTextureToBuffer_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    source: WGPUImageCopyTexture,
    destination: WGPUImageCopyBuffer,
    copySize: WGPUExtent3D
) {
    commandEncoder.copyTextureToBuffer(source: source, destination: destination, copySize: copySize)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_copyTextureToTexture_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    source: WGPUImageCopyTexture,
    destination: WGPUImageCopyTexture,
    copySize: WGPUExtent3D
) {
    commandEncoder.copyTextureToTexture(source: source, destination: destination, copySize: copySize)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_copyBufferToBuffer_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    source: WebGPU.Buffer,
    sourceOffset: UInt64,
    destination: WebGPU.Buffer,
    destinationOffset: UInt64,
    size: UInt64
) {
    commandEncoder.copyBufferToBuffer(
        source: source,
        sourceOffset: sourceOffset,
        destination: destination,
        destinationOffset: destinationOffset,
        size: size
    )
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_beginRenderPass_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    descriptor: WGPURenderPassDescriptor,
) -> CxxBridging.RefRenderPassEncoder {
    commandEncoder.beginRenderPass(descriptor: descriptor)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_beginComputePass_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    descriptor: WGPUComputePassDescriptor,
) -> CxxBridging.RefComputePassEncoder {
    commandEncoder.beginComputePass(descriptor: descriptor)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_runClearEncoder_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    attachmentsToClear: NSMutableDictionary,
    depthStencilAttachmentToClear: inout (any MTLTexture)?,
    depthAttachmentToClear: Bool,
    stencilAttachmentToClear: Bool,
    depthClearValue: Double,
    stencilClearValue: UInt32,
    existingEncoder: (any MTLRenderCommandEncoder)?
) {
    guard let dInput = attachmentsToClear as? [NSNumber: TextureAndClearColor] else {
        preconditionFailure("Dictionary not convertible")
    }

    return commandEncoder.runClearEncoder(
        attachmentsToClear: dInput,
        depthStencilAttachmentToClear: &depthStencilAttachmentToClear,
        depthAttachmentToClear: depthAttachmentToClear,
        stencilAttachmentToClear: stencilAttachmentToClear,
        depthClearValue: depthClearValue,
        stencilClearValue: stencilClearValue,
        existingEncoder: existingEncoder
    )
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_clearTextureIfNeeded_thunk(commandEncoder: WebGPU.CommandEncoder, destination: WGPUImageCopyTexture, slice: UInt)
{
    commandEncoder.clearTextureIfNeeded(destination: destination, slice: slice)
}

// FIXME: Eventually all these "thunks" should be removed.
// swift-format-ignore: AlwaysUseLowerCamelCase
// swift-format-ignore: AllPublicDeclarationsHaveDocumentation
@_expose(Cxx)
public func CommandEncoder_finish_thunk(
    commandEncoder: WebGPU.CommandEncoder,
    descriptor: WGPUCommandBufferDescriptor
) -> CxxBridging.RefCommandBuffer {
    commandEncoder.finish(descriptor: descriptor)
}

extension WebGPU.TextureOrTextureView {
    init(_ attachment: WGPURenderPassColorAttachment?) {
        if let view = attachment?.view {
            self.init(WebGPU.fromAPI(view))
            return
        }

        if let texture = attachment?.texture {
            self.init(WebGPU.fromAPI(texture))
            return
        }

        fatalError()
    }

    init(_ attachment: WGPURenderPassDepthStencilAttachment?) {
        if let view = attachment?.view {
            self.init(WebGPU.fromAPI(view))
            return
        }

        if let texture = attachment?.texture {
            self.init(WebGPU.fromAPI(texture))
            return
        }

        fatalError()
    }
}

extension WebGPU.CommandEncoder {
    private func validateFinishError() -> String? {
        if !isValid() {
            return "GPUCommandEncoder.finish: encoder is not valid"
        }

        if getEncoderState() != WebGPU.CommandsMixin.EncoderState.Open {
            return "GPUCommandEncoder.finish: encoder state is '\(encoderStateName() ?? "")', expected 'Open'"
        }

        if m_debugGroupStackSize != 0 {
            return "GPUCommandEncoder.finish: encoder stack size '\(m_debugGroupStackSize)'"
        }

        // FIXME: "Every usage scope contained in this must satisfy the usage scope validation."

        return nil
    }

    func finish(descriptor: WGPUCommandBufferDescriptor) -> CxxBridging.RefCommandBuffer {
        if !isValid() || (m_existingCommandEncoder != nil && m_existingCommandEncoder !== m_blitCommandEncoder) {
            setEncoderState(WebGPU.CommandsMixin.EncoderState.Ended)
            discardCommandBuffer()
            m_device.ptr()
                .generateAValidationError(m_lastErrorString != nil ? m_lastErrorString! as String : String("Invalid CommandEncoder"))
            return WebGPU.CommandBuffer.createInvalid(m_device.ptr())
        }

        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-finish
        let validationFailedError = validateFinishError()

        _ = getEncoderState()

        setEncoderState(WebGPU.CommandsMixin.EncoderState.Ended)
        if validationFailedError != nil {
            discardCommandBuffer()
            m_device.ptr()
                .generateAValidationError(m_lastErrorString != nil ? m_lastErrorString! as String : validationFailedError)
            return WebGPU.CommandBuffer.createInvalid(m_device.ptr())
        }

        finalizeBlitCommandEncoder()

        let commandBuffer = m_commandBuffer
        m_commandBuffer = nil
        m_existingCommandEncoder = nil

        commandBuffer?.label = CxxBridging.convertWTFStringToNSString(descriptor.label)

        #if arch(x86_64) && (os(macOS) || targetEnvironment(macCatalyst))
        if (m_managedBuffers?.count ?? 0) != 0 || (m_managedTextures?.count ?? 0) != 0 {
            let blitCommandEncoder = commandBuffer?.makeBlitCommandEncoder()

            // swift-format-ignore: AlwaysUseLowerCamelCase
            if let m_managedBuffers {
                for buffer in m_managedBuffers {
                    // FIXME: `NSSet` should not be used, and then this cast will not be needed.
                    // This is safe because `m_managedBuffers` is a `NSMutableSet<id<MTLBuffer>>`.
                    // swift-format-ignore: NeverForceUnwrap
                    blitCommandEncoder?.synchronize(resource: buffer as! any MTLBuffer)
                }
            }

            // swift-format-ignore: AlwaysUseLowerCamelCase
            if let m_managedTextures {
                for texture in m_managedTextures {
                    // FIXME: `NSSet` should not be used, and then this cast will not be needed.
                    // This is safe because `m_managedTextures` is a `NSMutableSet<id<MTLTexture>>`.
                    // swift-format-ignore: NeverForceUnwrap
                    blitCommandEncoder?.synchronize(resource: texture as! any MTLTexture)
                }
            }

            blitCommandEncoder?.endEncoding()
        }
        #endif // arch(x86_64) && (os(macOS) || targetEnvironment(macCatalyst))

        let result = createCommandBuffer(commandBuffer, m_device.ptr(), m_sharedEvent, m_sharedEventSignalValue)
        m_sharedEvent = nil
        m_cachedCommandBuffer = CxxBridging.commandBufferThreadSafeWeakPtr(result.ptr())
        result.ptr().setBufferMapCount(m_bufferMapCount)
        if m_makeSubmitInvalid {
            result.ptr().makeInvalid(m_lastErrorString as? String ?? "Invalid CommandEncoder")
        }

        return result
    }

    fileprivate func clearTextureIfNeeded(destination: WGPUImageCopyTexture, slice: UInt) {
        WebGPU.CommandEncoder.clearTextureIfNeeded(destination, slice, m_device.ptr(), m_blitCommandEncoder)
    }

    private func clearTextureIfNeeded(
        destination: WGPUImageCopyTexture,
        slice: UInt,
        device: WebGPU.Device,
        blitCommandEncoder: (any MTLBlitCommandEncoder)?
    ) {
        let texture = WebGPU.fromAPI(destination.texture)
        let mipLevel = UInt(destination.mipLevel)
        clearTextureIfNeeded(texture, mipLevel, slice, device, blitCommandEncoder)
    }

    private func clearTextureIfNeeded(
        _ texture: WebGPU.Texture,
        _ mipLevel: UInt,
        _ slice: UInt,
        _ device: WebGPU.Device,
        _ blitCommandEncoder: (any MTLBlitCommandEncoder)?
    ) {
        if blitCommandEncoder == nil || texture.previouslyCleared(UInt32(mipLevel), UInt32(slice)) {
            return
        }

        texture.setPreviouslyCleared(UInt32(mipLevel), UInt32(slice), true)
        let logicalExtent = texture.logicalMiplevelSpecificTextureExtent(UInt32(mipLevel))
        if logicalExtent.width == 0 {
            return
        }
        if texture.dimension() != WGPUTextureDimension_1D && logicalExtent.height == 0 {
            return
        }
        if texture.dimension() == WGPUTextureDimension_3D && logicalExtent.depthOrArrayLayers == 0 {
            return
        }

        guard let mtlTexture = texture.texture() else {
            return
        }

        var textureFormat = texture.format()
        if mtlTexture.pixelFormat == .depth32Float_stencil8 || mtlTexture.pixelFormat == .x32_stencil8 {
            textureFormat = WGPUTextureFormat_Depth32Float
        }

        let physicalExtent = WebGPU.Texture.physicalTextureExtent(texture.dimension(), textureFormat, logicalExtent)
        var sourceBytesPerRow = WebGPU.Texture.bytesPerRow(textureFormat, physicalExtent.width, texture.sampleCount())
        let depth = UInt(texture.dimension() == WGPUTextureDimension_3D ? physicalExtent.depthOrArrayLayers : 1)
        var didOverflow = false
        var checkedBytesPerImage = sourceBytesPerRow
        (checkedBytesPerImage, didOverflow) = sourceBytesPerRow.multipliedReportingOverflow(by: UInt(physicalExtent.height))
        if didOverflow {
            return
        }
        let bytesPerImage = checkedBytesPerImage
        var bufferLength = bytesPerImage
        (bufferLength, didOverflow) = bufferLength.multipliedReportingOverflow(by: depth)
        if didOverflow {
            return
        }
        if bufferLength == 0 {
            return
        }

        guard let temporaryBuffer = device.safeCreateBuffer(bufferLength) else {
            return
        }

        var sourceSize: MTLSize
        var sourceBytesPerImage: UInt = 0
        var mutableSlice = slice
        switch texture.dimension() {
        case WGPUTextureDimension_1D:
            sourceSize = MTLSize(width: Int(logicalExtent.width), height: 1, depth: 1)
        case WGPUTextureDimension_2D:
            sourceSize = MTLSize(width: Int(logicalExtent.width), height: Int(logicalExtent.height), depth: 1)
        case WGPUTextureDimension_3D:
            sourceSize = MTLSize(
                width: Int(logicalExtent.width),
                height: Int(logicalExtent.height),
                depth: Int(logicalExtent.depthOrArrayLayers)
            )
            sourceBytesPerImage = bytesPerImage
            mutableSlice = 0
        case WGPUTextureDimension_Force32:
            fatalError()
        default:
            fatalError()
        }

        var options: MTLBlitOption = []
        if mtlTexture.pixelFormat == .depth32Float_stencil8 {
            options = .depthFromDepthStencil
        }

        if mutableSlice >= mtlTexture.arrayLength {
            return
        }

        blitCommandEncoder?
            .copy(
                from: temporaryBuffer,
                sourceOffset: 0,
                sourceBytesPerRow: Int(sourceBytesPerRow),
                sourceBytesPerImage: Int(sourceBytesPerImage),
                sourceSize: sourceSize,
                to: mtlTexture,
                destinationSlice: Int(mutableSlice),
                destinationLevel: Int(mipLevel),
                destinationOrigin: MTLOrigin(x: 0, y: 0, z: 0),
                options: options,
            )

        if !options.isEmpty {
            // FIXME:  maybe it needs to be another value that is not sizeof.
            sourceBytesPerRow /= UInt(MemoryLayout<Float>.stride)
            sourceBytesPerImage /= UInt(MemoryLayout<Float>.stride)

            blitCommandEncoder?
                .copy(
                    from: temporaryBuffer,
                    sourceOffset: 0,
                    sourceBytesPerRow: Int(sourceBytesPerRow),
                    sourceBytesPerImage: Int(sourceBytesPerImage),
                    sourceSize: sourceSize,
                    to: mtlTexture,
                    destinationSlice: Int(mutableSlice),
                    destinationLevel: Int(mipLevel),
                    destinationOrigin: MTLOrigin(x: 0, y: 0, z: 0),
                    options: .stencilFromDepthStencil
                )
        }
    }

    func runClearEncoder(
        attachmentsToClear: [NSNumber: TextureAndClearColor],
        depthStencilAttachmentToClear: inout (any MTLTexture)?,
        depthAttachmentToClear: Bool,
        stencilAttachmentToClear: Bool,
        depthClearValue: Double,
        stencilClearValue: UInt32,
        existingEncoder: (any MTLRenderCommandEncoder)?
    ) {
        func createSimplePso(
            attachmentsToClear: [NSNumber: TextureAndClearColor],
            depthStencilAttachmentToClear: (any MTLTexture)?,
            depthAttachmentToClear: Bool,
            stencilAttachmentToClear: Bool,
            device: WebGPU.Device
        ) -> ((any MTLRenderPipelineState)?, (any MTLDepthStencilState)?) {
            let mtlRenderPipelineDescriptor = MTLRenderPipelineDescriptor()

            var sampleCount: UInt = 0
            var depthStencilDescriptor: MTLDepthStencilDescriptor? = nil
            for (key, textureAndClearColor) in attachmentsToClear {
                let t = textureAndClearColor.texture
                sampleCount = UInt(t.sampleCount)

                let mtlColorAttachment = mtlRenderPipelineDescriptor.colorAttachments[key.intValue]
                mtlColorAttachment?.pixelFormat = t.pixelFormat
                mtlColorAttachment?.isBlendingEnabled = false
            }

            if let depthStencilAttachmentToClear {
                depthStencilDescriptor = MTLDepthStencilDescriptor()
                sampleCount = UInt(depthStencilAttachmentToClear.sampleCount)
                mtlRenderPipelineDescriptor.depthAttachmentPixelFormat =
                    (!depthAttachmentToClear || WebGPU.Device.isStencilOnlyFormat(depthStencilAttachmentToClear.pixelFormat))
                    ? .invalid : depthStencilAttachmentToClear.pixelFormat
                depthStencilDescriptor?.isDepthWriteEnabled = false

                if stencilAttachmentToClear
                    && (depthStencilAttachmentToClear.pixelFormat == .depth32Float_stencil8
                        || depthStencilAttachmentToClear.pixelFormat == .stencil8
                        || depthStencilAttachmentToClear.pixelFormat == .x32_stencil8)
                {
                    mtlRenderPipelineDescriptor.stencilAttachmentPixelFormat = depthStencilAttachmentToClear.pixelFormat
                }
            }

            mtlRenderPipelineDescriptor.vertexFunction = WebGPU.Device.nopVertexFunction(device.device())
            mtlRenderPipelineDescriptor.fragmentFunction = nil

            precondition(sampleCount != 0, "sampleCount must be non-zero")
            mtlRenderPipelineDescriptor.rasterSampleCount = Int(sampleCount)
            mtlRenderPipelineDescriptor.inputPrimitiveTopology = .point

            guard let deviceMetal = device.device() else {
                fatalError()
            }

            var pso: (any MTLRenderPipelineState)? = nil
            var depthStencil: (any MTLDepthStencilState)? = nil
            do {
                pso = try deviceMetal.makeRenderPipelineState(descriptor: mtlRenderPipelineDescriptor)
                depthStencil = depthStencilDescriptor.flatMap { deviceMetal.makeDepthStencilState(descriptor: $0) }
            } catch {
                fatalError(error.localizedDescription)
            }

            return (pso, depthStencil)
        }

        if attachmentsToClear.isEmpty && !depthAttachmentToClear && !stencilAttachmentToClear {
            return endEncoding(existingEncoder)
        }
        if !stencilAttachmentToClear && !depthAttachmentToClear {
            depthStencilAttachmentToClear = nil
        }

        let device = m_device.ptr().device()
        guard device != nil else {
            return endEncoding(existingEncoder)
        }
        var clearRenderCommandEncoder = existingEncoder
        if clearRenderCommandEncoder == nil {
            let clearDescriptor = MTLRenderPassDescriptor()
            if depthAttachmentToClear {
                clearDescriptor.depthAttachment.loadAction = .clear
                clearDescriptor.depthAttachment.storeAction = .store
                clearDescriptor.depthAttachment.clearDepth = depthClearValue
                clearDescriptor.depthAttachment.texture = depthStencilAttachmentToClear
            }

            if stencilAttachmentToClear {
                clearDescriptor.stencilAttachment.loadAction = .clear
                clearDescriptor.stencilAttachment.storeAction = .store
                clearDescriptor.stencilAttachment.clearStencil = stencilClearValue
                clearDescriptor.stencilAttachment.texture = depthStencilAttachmentToClear
            }

            if attachmentsToClear.count == 0 {
                guard let depthStencilAttachmentToClear else {
                    fatalError()
                }
                clearDescriptor.defaultRasterSampleCount = depthStencilAttachmentToClear.sampleCount
                clearDescriptor.renderTargetWidth = depthStencilAttachmentToClear.width
                clearDescriptor.renderTargetHeight = depthStencilAttachmentToClear.height
            }
            for (key, textureAndClearColor) in attachmentsToClear {
                let t = textureAndClearColor.texture
                let mtlAttachment = clearDescriptor.colorAttachments[key.intValue]
                mtlAttachment?.loadAction = .clear
                mtlAttachment?.storeAction = .store
                mtlAttachment?.clearColor = textureAndClearColor.clearColor
                mtlAttachment?.texture = t
                mtlAttachment?.level = 0
                mtlAttachment?.slice = 0
                mtlAttachment?.depthPlane = Int(textureAndClearColor.depthPlane)
            }
            clearRenderCommandEncoder = m_commandBuffer?.makeRenderCommandEncoder(descriptor: clearDescriptor)
            setExistingEncoder(clearRenderCommandEncoder)
        }

        let (pso, depthStencil) = createSimplePso(
            attachmentsToClear: attachmentsToClear,
            depthStencilAttachmentToClear: depthStencilAttachmentToClear,
            depthAttachmentToClear: depthAttachmentToClear,
            stencilAttachmentToClear: stencilAttachmentToClear,
            device: m_device.ptr()
        )

        guard let pso else {
            fatalError("pso should not be nil")
        }

        clearRenderCommandEncoder?.setRenderPipelineState(pso)
        if let depthStencil {
            clearRenderCommandEncoder?.setDepthStencilState(depthStencil)
        }
        clearRenderCommandEncoder?.setCullMode(.none)
        clearRenderCommandEncoder?.drawPrimitives(type: .point, vertexStart: 0, vertexCount: 1, instanceCount: 1, baseInstance: 0)
        m_device.ptr().getQueue().ptr().endEncoding(clearRenderCommandEncoder, m_commandBuffer)
        setExistingEncoder(nil)
    }

    private func timestampWriteIndex(writeIndex: UInt32) -> Int {
        writeIndex == WGPU_QUERY_SET_INDEX_UNDEFINED ? 0 : Int(UInt(writeIndex))
    }

    private func timestampWriteIndex(
        writeIndex: UInt32,
        defaultValue: Int,
        offset: UInt32
    ) -> Int {
        writeIndex == WGPU_QUERY_SET_INDEX_UNDEFINED ? defaultValue : Int(UInt(writeIndex + offset))
    }

    private func errorValidatingCopyBufferToBuffer(
        source: WebGPU.Buffer,
        sourceOffset: UInt64,
        destination: WebGPU.Buffer,
        destinationOffset: UInt64,
        size: UInt64
    ) -> String? {
        func errorString(_ format: String) -> String {
            "GPUCommandEncoder.copyBufferToBuffer: \(format)"
        }
        if !source.isDestroyed() && !CxxBridging.isValidToUseWithBufferCommandEncoder(source, self) {
            return errorString("source buffer is not valid")
        }

        if !destination.isDestroyed() && !CxxBridging.isValidToUseWithBufferCommandEncoder(destination, self) {
            return errorString("destination buffer is not valid")
        }

        if source.usage() & WGPUBufferUsage_CopySrc.rawValue == 0 {
            return errorString("source usage does not have COPY_SRC")
        }

        if destination.usage() & WGPUBufferUsage_CopyDst.rawValue == 0 {
            return errorString("destination usage does not have COPY_DST")
        }

        if destination.state() == WebGPU.Buffer.State.MappingPending || source.state() == WebGPU.Buffer.State.MappingPending {
            return errorString("destination state is not unmapped or source state is not unmapped")
        }

        if size % 4 != 0 {
            return errorString("size is not a multiple of 4")
        }

        if sourceOffset % 4 != 0 {
            return errorString("source offset is not a multiple of 4")
        }

        if destinationOffset % 4 != 0 {
            return errorString("destination offset is not a multiple of 4")
        }
        var sourceEnd = sourceOffset
        var didOverflow = false
        (sourceEnd, didOverflow) = sourceEnd.addingReportingOverflow(size)
        if didOverflow {
            return errorString("source size + offset overflows")
        }
        var destinationEnd = destinationOffset
        (destinationEnd, didOverflow) = destinationEnd.addingReportingOverflow(size)
        if didOverflow {
            return errorString("destination size + offset overflows")
        }

        if source.initialSize() < sourceEnd {
            return errorString("source size + offset overflows")
        }

        if destination.initialSize() < destinationEnd {
            return errorString("destination size + offset overflows")
        }
        // FIXME: rdar://138415945
        if CxxBridging.areBuffersEqual(source, destination) {
            return errorString("source equals destination not valid")
        }

        return nil
    }

    private func areCopyCompatible(format1: WGPUTextureFormat, format2: WGPUTextureFormat) -> Bool {
        // https://gpuweb.github.io/gpuweb/#copy-compatible
        format1 == format2 ? true : WebGPU.Texture.removeSRGBSuffix(format1) == WebGPU.Texture.removeSRGBSuffix(format2)
    }

    private func errorValidatingCopyTextureToTexture(
        source: WGPUImageCopyTexture,
        destination: WGPUImageCopyTexture,
        copySize: WGPUExtent3D
    ) -> String? {
        func refersToAllAspects(format: WGPUTextureFormat, aspect: WGPUTextureAspect) -> Bool {
            switch aspect {
            case WGPUTextureAspect_All:
                return true
            case WGPUTextureAspect_StencilOnly:
                return WebGPU.Texture.containsStencilAspect(format) && !WebGPU.Texture.containsDepthAspect(format)
            case WGPUTextureAspect_DepthOnly:
                return WebGPU.Texture.containsDepthAspect(format) && !WebGPU.Texture.containsStencilAspect(format)
            case WGPUTextureAspect_Force32:
                assertionFailure()
                return false
            default:
                assertionFailure()
                return false
            }
        }
        func errorString(_ error: String) -> String {
            "GPUCommandEncoder.copyTextureToTexture: \(error)"
        }
        let sourceTexture = WebGPU.fromAPI(source.texture)
        if !CxxBridging.isValidToUseWithTextureCommandEncoder(sourceTexture, self) {
            return errorString("source texture is not valid to use with this GPUCommandEncoder")
        }

        let destinationTexture = WebGPU.fromAPI(destination.texture)
        if !CxxBridging.isValidToUseWithTextureCommandEncoder(destinationTexture, self) {
            return errorString("destination texture is not valid to use with this GPUCommandEncoder")
        }

        if let error = WebGPU.Texture.errorValidatingImageCopyTexture(source, copySize) {
            return errorString(error)
        }

        if sourceTexture.usage() & WGPUTextureUsage_CopySrc.rawValue == 0 {
            return errorString("source texture usage does not contain CopySrc")
        }

        if let error = WebGPU.Texture.errorValidatingImageCopyTexture(destination, copySize) {
            return errorString(error)
        }

        if destinationTexture.usage() & WGPUTextureUsage_CopyDst.rawValue == 0 {
            return errorString("destination texture usage does not contain CopyDst")
        }

        if sourceTexture.sampleCount() != destinationTexture.sampleCount() {
            return errorString("destination texture sample count does not equal source texture sample count")
        }

        if !areCopyCompatible(format1: sourceTexture.format(), format2: destinationTexture.format()) {
            return errorString("destination texture and source texture are not copy compatible")
        }

        let srcIsDepthOrStencil = WebGPU.Texture.isDepthOrStencilFormat(sourceTexture.format())
        let dstIsDepthOrStencil = WebGPU.Texture.isDepthOrStencilFormat(destinationTexture.format())

        if srcIsDepthOrStencil {
            if !refersToAllAspects(format: sourceTexture.format(), aspect: source.aspect)
                || !refersToAllAspects(format: destinationTexture.format(), aspect: destination.aspect)
            {
                return errorString("source or destination do not refer to a single copy aspect")
            }
        } else {
            if source.aspect != WGPUTextureAspect_All {
                return errorString("source aspect is not All")
            }
            if !dstIsDepthOrStencil {
                if destination.aspect != WGPUTextureAspect_All {
                    return errorString("destination aspect is not All")
                }
            }
        }

        if let error = WebGPU.Texture.errorValidatingTextureCopyRange(source, copySize) {
            return errorString(error)
        }

        if let error = WebGPU.Texture.errorValidatingTextureCopyRange(destination, copySize) {
            return errorString(error)
        }

        // https://gpuweb.github.io/gpuweb/#abstract-opdef-set-of-subresources-for-texture-copy
        if source.texture === destination.texture {
            // Mip levels are never ranges.
            if source.mipLevel == destination.mipLevel {
                switch WebGPU.fromAPI(source.texture).dimension() {
                case WGPUTextureDimension_1D:
                    return errorString("can't copy 1D texture to itself")
                case WGPUTextureDimension_2D:
                    let sourceRange = source.origin.z..<(source.origin.z + copySize.depthOrArrayLayers)
                    let destinationRange = destination.origin.z..<(destination.origin.z + copySize.depthOrArrayLayers)
                    if sourceRange.overlaps(destinationRange) {
                        return errorString("can't copy 2D texture to itself with overlapping array range")
                    }
                case WGPUTextureDimension_3D:
                    return errorString("can't copy 3D texture to itself")
                case WGPUTextureDimension_Force32:
                    assertionFailure()
                    return errorString("unknown texture format")
                default:
                    assertionFailure()
                    return errorString("Default. Should not be reached")
                }
            }
        }

        return nil
    }

    private func errorValidatingCopyTextureToBuffer(
        source: WGPUImageCopyTexture,
        destination: WGPUImageCopyBuffer,
        copySize: WGPUExtent3D
    ) -> String? {
        func errorString(_ error: String) -> String {
            "GPUCommandEncoder.copyTextureToBuffer: \(error)"
        }
        let sourceTexture = WebGPU.fromAPI(source.texture)

        if !CxxBridging.isValidToUseWithTextureCommandEncoder(sourceTexture, self) {
            return errorString("source texture is not valid to use with this GPUCommandEncoder")
        }

        if let error = WebGPU.Texture.errorValidatingImageCopyTexture(source, copySize) {
            return errorString(error)
        }

        if sourceTexture.usage() & WGPUTextureUsage_CopySrc.rawValue == 0 {
            return errorString("sourceTexture usage does not contain CopySrc")
        }

        if sourceTexture.sampleCount() != 1 {
            return errorString("sourceTexture sample count != 1")
        }

        var aspectSpecificFormat = sourceTexture.format()

        if WebGPU.Texture.isDepthOrStencilFormat(sourceTexture.format()) {
            if !WebGPU.Texture.refersToSingleAspect(sourceTexture.format(), source.aspect) {
                return errorString("copying to depth stencil texture with more than one aspect")
            }

            if !WebGPU.Texture.isValidDepthStencilCopySource(sourceTexture.format(), source.aspect) {
                return errorString("copying to depth stencil texture, validDepthStencilCopySource fails")
            }

            aspectSpecificFormat = WebGPU.Texture.aspectSpecificFormat(sourceTexture.format(), source.aspect)
        }

        if let error = errorValidatingImageCopyBuffer(imageCopyBuffer: destination) {
            return errorString(error)
        }

        if WebGPU.fromAPI(destination.buffer).usage() & WGPUBufferUsage_CopyDst.rawValue == 0 {
            return errorString("destination buffer usage does not contain CopyDst")
        }

        if let error = WebGPU.Texture.errorValidatingTextureCopyRange(source, copySize) {
            return errorString(error)
        }

        if !WebGPU.Texture.isDepthOrStencilFormat(sourceTexture.format()) {
            let texelBlockSize = WebGPU.Texture.texelBlockSize(sourceTexture.format())
            if destination.layout.offset % texelBlockSize.value() != 0 {
                return errorString("destination.layout.offset is not a multiple of texelBlockSize")
            }
        }

        if WebGPU.Texture.isDepthOrStencilFormat(sourceTexture.format()) {
            if destination.layout.offset % 4 != 0 {
                return errorString("destination.layout.offset is not a multiple of 4")
            }
        }

        if let error = WebGPU.Texture.errorValidatingLinearTextureData(
            destination.layout,
            WebGPU.fromAPI(destination.buffer).initialSize(),
            aspectSpecificFormat,
            copySize
        ) {
            return errorString(error)
        }
        return nil
    }

    private func errorValidatingImageCopyBuffer(imageCopyBuffer: WGPUImageCopyBuffer) -> String? {
        // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopybuffer
        let buffer = WebGPU.fromAPI(imageCopyBuffer.buffer)
        if !CxxBridging.isValidToUseWithBufferCommandEncoder(buffer, self) {
            return "buffer is not valid"
        }

        if imageCopyBuffer.layout.bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED && (imageCopyBuffer.layout.bytesPerRow % 256 != 0) {
            return "imageCopyBuffer.layout.bytesPerRow is not a multiple of 256"
        }

        return nil
    }

    private func errorValidatingCopyBufferToTexture(
        source: WGPUImageCopyBuffer,
        destination: WGPUImageCopyTexture,
        copySize: WGPUExtent3D
    ) -> String? {
        func errorString(_ error: String) -> String {
            "GPUCommandEncoder.copyBufferToTexture: \(error)"
        }
        let destinationTexture = WebGPU.fromAPI(destination.texture)
        let sourceBuffer = WebGPU.fromAPI(source.buffer)

        if let error = errorValidatingImageCopyBuffer(imageCopyBuffer: source) {
            return errorString(error)
        }

        if sourceBuffer.usage() & WGPUBufferUsage_CopySrc.rawValue == 0 {
            return errorString("source usage does not contain CopySrc")
        }

        if !CxxBridging.isValidToUseWithTextureCommandEncoder(destinationTexture, self) {
            return errorString("destination texture is not valid to use with this GPUCommandEncoder")
        }

        if let error = WebGPU.Texture.errorValidatingImageCopyTexture(destination, copySize) {
            return errorString(error)
        }

        if destinationTexture.usage() & WGPUTextureUsage_CopyDst.rawValue == 0 {
            return errorString("destination usage does not contain CopyDst")
        }

        if destinationTexture.sampleCount() != 1 {
            return errorString("destination sample count is not one")
        }

        var aspectSpecificFormat = destinationTexture.format()

        if WebGPU.Texture.isDepthOrStencilFormat(destinationTexture.format()) {
            if !WebGPU.Texture.refersToSingleAspect(destinationTexture.format(), destination.aspect) {
                return errorString("destination aspect refers to more than one asepct")
            }

            if !WebGPU.Texture.isValidDepthStencilCopyDestination(destinationTexture.format(), destination.aspect) {
                return errorString("destination is not valid depthStencilCopyDestination")
            }

            aspectSpecificFormat = WebGPU.Texture.aspectSpecificFormat(destinationTexture.format(), destination.aspect)
        }

        if let error = WebGPU.Texture.errorValidatingTextureCopyRange(destination, copySize) {
            return errorString(error)
        }

        if !WebGPU.Texture.isDepthOrStencilFormat(destinationTexture.format()) {
            let texelBlockSize = WebGPU.Texture.texelBlockSize(destinationTexture.format())
            if source.layout.offset % texelBlockSize.value() != 0 {
                return errorString("source.layout.offset is not a multiple of texelBlockSize")
            }
        }

        if WebGPU.Texture.isDepthOrStencilFormat(destinationTexture.format()) {
            if source.layout.offset % 4 != 0 {
                return errorString("source.layout.offset is not a multiple of four for depth stencil format")
            }
        }

        if let error = WebGPU.Texture.errorValidatingLinearTextureData(
            source.layout,
            WebGPU.fromAPI(source.buffer).initialSize(),
            aspectSpecificFormat,
            copySize
        ) {
            return errorString(error)
        }
        return nil
    }

    private func errorValidatingRenderPassDescriptor(descriptor: WGPURenderPassDescriptor) -> String? {
        if let wgpuOcclusionQuery = descriptor.occlusionQuerySet {
            let occlusionQuery = WebGPU.fromAPI(wgpuOcclusionQuery)
            if !CxxBridging.isValidToUseWithQuerySetCommandEncoder(occlusionQuery, self) {
                return "occlusion query does not match the device"
            }
            if occlusionQuery.type() != WGPUQueryType_Occlusion {
                return "occlusion query type is not occlusion"
            }
        }
        let collection = CollectionOfOne(descriptor)
        let descriptorSpan = collection.span
        if let timestampWrites = wgpuGetRenderPassDescriptorTimestampWrites(descriptorSpan)?[0] {
            return errorValidatingTimestampWrites(
                timestampWrites: WGPUComputePassTimestampWrites(
                    querySet: timestampWrites.querySet,
                    beginningOfPassWriteIndex: timestampWrites.beginningOfPassWriteIndex,
                    endOfPassWriteIndex: timestampWrites.endOfPassWriteIndex
                )
            )
        }
        return nil
    }

    private func errorValidatingTimestampWrites(timestampWrites: WGPUComputePassTimestampWrites) -> String? {
        if !m_device.ptr().hasFeature(WGPUFeatureName_TimestampQuery) {
            return "device does not have timestamp query feature"
        }

        let querySet = WebGPU.fromAPI(timestampWrites.querySet)
        if querySet.type() != WGPUQueryType_Timestamp {
            return "query type is not timestamp but \(querySet.type())"
        }

        if !CxxBridging.isValidToUseWithQuerySetCommandEncoder(querySet, self) {
            return "device mismatch"
        }

        let querySetCount = querySet.count()
        let beginningOfPassWriteIndex = timestampWriteIndex(writeIndex: timestampWrites.beginningOfPassWriteIndex)
        let endOfPassWriteIndex = timestampWriteIndex(writeIndex: timestampWrites.endOfPassWriteIndex)
        if beginningOfPassWriteIndex >= querySetCount || endOfPassWriteIndex >= querySetCount
            || timestampWrites.beginningOfPassWriteIndex == timestampWrites.endOfPassWriteIndex
        {
            return
                "writeIndices mismatch: beginningOfPassWriteIndex(\(beginningOfPassWriteIndex) >= querySetCount(\(querySetCount) || endOfPassWriteIndex(\(endOfPassWriteIndex)) >= querySetCount(\(querySetCount)) || timestampWrite.beginningOfPassWriteIndex(\(timestampWrites.beginningOfPassWriteIndex) == timestampWrite.endOfPassWriteIndex(\(timestampWrites.endOfPassWriteIndex))"
        }

        return nil
    }

    private func errorValidatingComputePassDescriptor(descriptor: WGPUComputePassDescriptor) -> String? {
        let collection = CollectionOfOne(descriptor)
        if let timestampWrites = wgpuGetComputePassDescriptorTimestampWrites(collection.span)?[0] {
            return errorValidatingTimestampWrites(timestampWrites: timestampWrites)
        }
        return nil
    }

    private func loadAction(loadOp: WGPULoadOp) -> MTLLoadAction {
        switch loadOp {
        case WGPULoadOp_Load:
            return .load
        case WGPULoadOp_Clear:
            return .clear
        case WGPULoadOp_Undefined:
            return .dontCare
        case WGPULoadOp_Force32:
            assertionFailure()
            return .dontCare
        default:
            assertionFailure()
            return .dontCare
        }
    }

    private func storeAction(storeOp: WGPUStoreOp, hasResolveTarget: Bool = false) -> MTLStoreAction {
        switch storeOp {
        case WGPUStoreOp_Store:
            return hasResolveTarget ? .storeAndMultisampleResolve : .store
        case WGPUStoreOp_Discard:
            return hasResolveTarget ? .multisampleResolve : .dontCare
        case WGPUStoreOp_Undefined:
            return hasResolveTarget ? .multisampleResolve : .dontCare
        case WGPUStoreOp_Force32:
            assertionFailure()
            return .dontCare
        default:
            assertionFailure()
            return .dontCare
        }
    }

    private func isMultisampleTexture(texture: any MTLTexture) -> Bool {
        texture.textureType == .type2DMultisample || texture.textureType == .type2DMultisampleArray
    }

    func beginRenderPass(descriptor: WGPURenderPassDescriptor) -> CxxBridging.RefRenderPassEncoder {
        let collection = CollectionOfOne(descriptor)
        let descriptorSpan = collection.span
        let maxDrawCount = descriptorSpan[0].maxDrawCount

        guard prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "encoder state is not valid")
        }

        if let error = errorValidatingRenderPassDescriptor(descriptor: descriptor) {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), error)
        }

        if let commandBuffer = m_commandBuffer, commandBuffer.status.rawValue >= MTLCommandBufferStatus.enqueued.rawValue {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "command buffer has already been committed")
        }

        let mtlDescriptor = MTLRenderPassDescriptor()
        var counterSampleBuffer = WebGPU.QuerySet.CounterSampleBuffer()
        if let wgpuTimestampWrites = wgpuGetRenderPassDescriptorTimestampWrites(descriptorSpan)?[0] {
            let wgpuQuerySet = wgpuTimestampWrites.querySet
            let timestampsWrites = WebGPU.fromAPI(wgpuQuerySet)
            counterSampleBuffer = timestampsWrites.counterSampleBufferWithOffset()
            timestampsWrites.setCommandEncoder(self)
        }

        if m_device.ptr().enableEncoderTimestamps() || counterSampleBuffer.buffer != nil {
            if let buffer = counterSampleBuffer.buffer {
                // FIXME: (rdar://170907276) Prove that the result of `wgpuGetRenderPassDescriptorTimestampWrites` can never be nil.
                // swift-format-ignore: NeverForceUnwrap
                let timestampWrites = wgpuGetRenderPassDescriptorTimestampWrites(descriptorSpan)![0]
                mtlDescriptor.sampleBufferAttachments[0].sampleBuffer = buffer
                mtlDescriptor.sampleBufferAttachments[0].startOfVertexSampleIndex = timestampWriteIndex(
                    writeIndex: timestampWrites.beginningOfPassWriteIndex,
                    defaultValue: MTLCounterDontSample,
                    offset: counterSampleBuffer.offset
                )
                mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex = timestampWriteIndex(
                    writeIndex: timestampWrites.endOfPassWriteIndex,
                    defaultValue: MTLCounterDontSample,
                    offset: counterSampleBuffer.offset
                )
                mtlDescriptor.sampleBufferAttachments[0].startOfFragmentSampleIndex =
                    mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex
                mtlDescriptor.sampleBufferAttachments[0].endOfFragmentSampleIndex =
                    mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex
                m_device.ptr().trackTimestampsBuffer(m_commandBuffer, buffer)
            } else {
                mtlDescriptor.sampleBufferAttachments[0].sampleBuffer =
                    counterSampleBuffer.buffer ?? m_device.ptr().timestampsBuffer(m_commandBuffer, 4)
                mtlDescriptor.sampleBufferAttachments[0].startOfVertexSampleIndex = 0
                mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex = 1
                mtlDescriptor.sampleBufferAttachments[0].startOfFragmentSampleIndex = 2
                mtlDescriptor.sampleBufferAttachments[0].endOfFragmentSampleIndex = 3
            }
        }

        guard descriptor.colorAttachmentCount <= 8 else {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "color attachment count is > 8")
        }

        finalizeBlitCommandEncoder()

        var attachmentsToClear: [NSNumber: TextureAndClearColor] = [:]
        var zeroColorTargets = true
        var bytesPerSample: UInt32 = 0
        let maxColorAttachmentBytesPerSample = m_device.ptr().limitsCopy().maxColorAttachmentBytesPerSample
        var textureWidth: UInt32 = 0
        var textureHeight: UInt32 = 0
        var sampleCount: UInt32 = 0

        var depthSlices: [UInt64: Set<UInt64>] = [:]
        // FIXME: it shouldn't be necessary to pass colorAttachmentCount here
        var compositorTextureSlice: UInt32 = 0
        if descriptor.colorAttachmentCount != 0 {
            let attachments = wgpuGetRenderPassDescriptorColorAttachments(descriptorSpan, descriptor.colorAttachmentCount)
            for i in 0..<attachments.count {
                let attachment = attachments[i]

                if attachment.view == nil && attachment.texture == nil {
                    continue
                }

                // MTLRenderPassColorAttachmentDescriptorArray is bounds-checked internally, so this is guaranteed to be non-nil.
                // swift-format-ignore: NeverForceUnwrap
                let mtlAttachment = mtlDescriptor.colorAttachments[i]!

                mtlAttachment.clearColor = MTLClearColor(
                    red: attachment.clearValue.r,
                    green: attachment.clearValue.g,
                    blue: attachment.clearValue.b,
                    alpha: attachment.clearValue.a
                )

                var texture = WebGPU.TextureOrTextureView(attachment)
                if !CxxBridging.isValidToUseWith(texture, self) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "device mismatch")
                }

                if textureWidth != 0
                    && (texture.width() != textureWidth || texture.height() != textureHeight || sampleCount != texture.sampleCount())
                {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "texture size does not match")
                }

                textureWidth = texture.width()
                textureHeight = texture.height()
                sampleCount = texture.sampleCount()
                let textureFormat = texture.format()
                bytesPerSample = roundUpToMultipleOfNonPowerOfTwoUInt32UInt32(
                    WebGPU.Texture.renderTargetPixelByteAlignment(textureFormat),
                    bytesPerSample
                )
                bytesPerSample += WebGPU.Texture.renderTargetPixelByteCost(textureFormat)
                if bytesPerSample > maxColorAttachmentBytesPerSample {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "total bytes per sample exceeds limit")
                }

                let textureIsDestroyed = texture.isDestroyed()
                if !textureIsDestroyed {
                    if (texture.usage() & WGPUTextureUsage_RenderAttachment.rawValue) == 0
                        || !WebGPU.Texture.isColorRenderableFormat(textureFormat, m_device.ptr())
                    {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "color attachment is not renderable")
                    }

                    if !WebGPU.isRenderableTextureView(texture) {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "texture view is not renderable")
                    }
                }
                texture.setCommandEncoder(self)

                let mtlTexture = texture.texture()
                mtlAttachment.texture = mtlTexture
                if mtlAttachment.texture == nil {
                    if !textureIsDestroyed {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "color attachment's texture is nil")
                    }
                    continue
                }
                mtlAttachment.level = 0
                mtlAttachment.slice = 0
                var depthSliceOrArrayLayer: UInt64 = 0
                // FIMXE: (rdar://170907318) This should be changed to `if let` when possible.
                if var depthSlice = Optional(fromCxx: attachment.depthSlice) {
                    if !texture.is3DTexture() {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depthSlice specified on 2D texture")
                    }
                    depthSliceOrArrayLayer = textureIsDestroyed ? 0 : UInt64(depthSlice)
                    if depthSliceOrArrayLayer >= texture.depthOrArrayLayers() {
                        return WebGPU.RenderPassEncoder.createInvalid(
                            self,
                            m_device.ptr(),
                            "depthSlice is greater than texture's depth or array layers"
                        )
                    }
                } else {
                    if texture.is3DTexture() {
                        return WebGPU.RenderPassEncoder.createInvalid(
                            self,
                            m_device.ptr(),
                            "textureDimension is 3D and no depth slice is specified"
                        )
                    }
                    depthSliceOrArrayLayer = UInt64(textureIsDestroyed ? 0 : texture.baseArrayLayer())
                }
                let bridgedTexture = texture.parentTexture().gpuResourceID._impl
                let baseMipLevel = textureIsDestroyed ? 0 : texture.baseMipLevel()
                let depthAndMipLevel: UInt64 = depthSliceOrArrayLayer | (UInt64(baseMipLevel) << 32)

                guard depthSlices[bridgedTexture, default: []].insert(depthAndMipLevel).inserted else {
                    return WebGPU.RenderPassEncoder.createInvalid(
                        self,
                        m_device.ptr(),
                        "attempting to render to overlapping color attachment"
                    )
                }

                mtlAttachment.depthPlane = texture.is3DTexture() ? Int(depthSliceOrArrayLayer) : 0
                mtlAttachment.slice = 0
                mtlAttachment.loadAction = loadAction(loadOp: attachment.loadOp)
                mtlAttachment.storeAction = storeAction(storeOp: attachment.storeOp, hasResolveTarget: attachment.resolveTarget != nil)

                zeroColorTargets = false
                var textureToClear: (any MTLTexture)? = nil
                if mtlAttachment.loadAction == .load && !texture.previouslyCleared() {
                    textureToClear = mtlAttachment.texture
                }

                var compositorTexture = texture
                if attachment.resolveTarget != nil || attachment.resolveTexture != nil {
                    var resolveTarget =
                        attachment.resolveTarget != nil
                        ? WebGPU.TextureOrTextureView(WebGPU.fromAPI(attachment.resolveTarget))
                        : WebGPU.TextureOrTextureView(WebGPU.fromAPI(attachment.resolveTexture))
                    compositorTexture = resolveTarget

                    if !CxxBridging.isValidToUseWith(resolveTarget, self) {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "resolve target created from different device")
                    }
                    resolveTarget.setCommandEncoder(self)
                    let resolveTexture = resolveTarget.texture()
                    // FIMXE: (rdar://170907318) This should be changed to `guard let` when possible.
                    guard var resolveTexture, var mtlTexture else {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "resolveTexture/mtlTexture is nil")
                    }

                    if mtlTexture.sampleCount == 1
                        || resolveTexture.sampleCount != 1
                        || isMultisampleTexture(texture: resolveTexture)
                        || !isMultisampleTexture(texture: mtlTexture)
                        || !WebGPU.isRenderableTextureView(resolveTarget)
                        || mtlTexture.pixelFormat != resolveTexture.pixelFormat
                        || !WebGPU.Texture.supportsResolve(resolveTarget.format(), m_device.ptr())
                    {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "resolve target is invalid")
                    }

                    mtlAttachment.resolveTexture = resolveTexture
                    mtlAttachment.resolveLevel = 0
                    mtlAttachment.resolveSlice = 0
                    mtlAttachment.resolveDepthPlane = 0
                    if resolveTarget.width() != texture.width() || resolveTarget.height() != texture.height() {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "resolve target dimensions are invalid")
                    }
                }

                if let rateMap = compositorTexture.rasterizationMapForSlice(compositorTexture.parentRelativeSlice()) {
                    mtlDescriptor.rasterizationRateMap = rateMap
                    compositorTextureSlice = compositorTexture.parentRelativeSlice()
                }

                // FIMXE: (rdar://170907318) This should be changed to `if let` when possible.
                if var textureToClear {
                    let textureWithResolve = TextureAndClearColor(texture: textureToClear)
                    attachmentsToClear[i as NSNumber] = textureWithResolve
                    texture.setPreviouslyCleared()
                    // FIMXE: (rdar://170907318) This should be changed to `if let` when possible.
                    if var resolveTarget = attachment.resolveTarget {
                        // FIXME: rdar://138042799 remove default argument.
                        WebGPU.fromAPI(resolveTarget).setPreviouslyCleared(0, 0)
                    }
                    // FIMXE: (rdar://170907318) This should be changed to `if let` when possible.
                    if var resolveTexture = attachment.resolveTexture {
                        WebGPU.fromAPI(resolveTexture).setPreviouslyCleared()
                    }
                }
            }
        }

        var depthReadOnly = false
        var stencilReadOnly = false
        var hasStencilComponent = false
        var depthStencilAttachmentToClear: (any MTLTexture)? = nil
        var depthAttachmentToClear = false
        if let attachment = wgpuGetRenderPassDescriptorDepthStencilAttachment(descriptorSpan)?[0] {
            let textureView = WebGPU.TextureOrTextureView(attachment)
            if !CxxBridging.isValidToUseWith(textureView, self) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture device mismatch")
            }
            let metalDepthStencilTexture = textureView.texture()
            let textureFormat = textureView.format()
            hasStencilComponent = WebGPU.Texture.containsStencilAspect(textureFormat)
            let hasDepthComponent = WebGPU.Texture.containsDepthAspect(textureFormat)
            let isDestroyed = textureView.isDestroyed()
            if !isDestroyed {
                if textureWidth != 0
                    && (textureView.width() != textureWidth || textureView.height() != textureHeight
                        || sampleCount != textureView.sampleCount())
                {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture dimensions mismatch")
                }
                if textureView.arrayLayerCount() > 1 || textureView.mipLevelCount() > 1 {
                    return WebGPU.RenderPassEncoder.createInvalid(
                        self,
                        m_device.ptr(),
                        "depth stencil texture has more than one array layer or mip level"
                    )
                }

                if !WebGPU.Texture.isDepthStencilRenderableFormat(
                    textureView.format(),
                    m_device.ptr()
                ) || !WebGPU.isRenderableTextureView(textureView) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture is not renderable")
                }
            }

            depthReadOnly = attachment.depthReadOnly != 0
            if hasDepthComponent {
                // This is safe because the `depthAttachment` property is `null_resettable` in Objective C.
                // swift-format-ignore: NeverForceUnwrap
                let mtlAttachment = mtlDescriptor.depthAttachment!

                let clearDepth = WebGPU.RenderPassEncoder
                    .quantizedDepthValue(Double(attachment.depthClearValue), textureView.format())
                    .clamped(to: 0...1)

                mtlAttachment.clearDepth = attachment.depthLoadOp == WGPULoadOp_Clear ? clearDepth : 1.0
                mtlAttachment.texture = metalDepthStencilTexture
                mtlAttachment.level = 0
                mtlAttachment.loadAction = loadAction(loadOp: attachment.depthLoadOp)
                mtlAttachment.storeAction = storeAction(storeOp: attachment.depthStoreOp)

                if mtlDescriptor.rasterizationRateMap != nil && metalDepthStencilTexture?.sampleCount ?? 1 > 1 {
                    if let depthTexture = m_device.ptr().getXRViewSubImageDepthTexture() {
                        mtlAttachment.resolveTexture = depthTexture
                        mtlAttachment.storeAction = storeAction(storeOp: attachment.depthStoreOp, hasResolveTarget: true)
                        mtlAttachment.resolveSlice = Int(compositorTextureSlice)
                    }
                }

                if mtlAttachment.loadAction == .load && mtlAttachment.storeAction == .dontCare && !textureView.previouslyCleared() {
                    depthStencilAttachmentToClear = mtlAttachment.texture
                    depthAttachmentToClear = mtlAttachment.texture != nil
                }
            }

            if !isDestroyed {
                if hasDepthComponent && !depthReadOnly {
                    if attachment.depthLoadOp == WGPULoadOp_Undefined || attachment.depthStoreOp == WGPUStoreOp_Undefined {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth load and store op were not specified")
                    }
                } else if attachment.depthLoadOp != WGPULoadOp_Undefined || attachment.depthStoreOp != WGPUStoreOp_Undefined {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth load and store op were specified")
                }
            }

            if attachment.depthLoadOp == WGPULoadOp_Clear && (attachment.depthClearValue < 0 || attachment.depthClearValue > 1) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth clear value is invalid")
            }

            if zeroColorTargets {
                // FIMXE: (rdar://170907318) This should be changed to `guard let` when possible.
                guard var metalDepthStencilTexture, metalDepthStencilTexture.sampleCount > 0 else {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "no color targets and depth-stencil texture is nil")
                }

                mtlDescriptor.defaultRasterSampleCount = metalDepthStencilTexture.sampleCount
                mtlDescriptor.renderTargetWidth = metalDepthStencilTexture.width
                mtlDescriptor.renderTargetHeight = metalDepthStencilTexture.height
            }
        }

        var stencilAttachmentToClear = false
        if let attachment = wgpuGetRenderPassDescriptorDepthStencilAttachment(descriptorSpan)?[0] {
            // This is safe because the `stencilAttachment` property is `null_resettable` in Objective C.
            // swift-format-ignore: NeverForceUnwrap
            let mtlAttachment = mtlDescriptor.stencilAttachment!
            stencilReadOnly = attachment.stencilReadOnly != 0
            var textureView = WebGPU.TextureOrTextureView(attachment)
            if hasStencilComponent {
                mtlAttachment.texture = textureView.texture()
            }
            mtlAttachment.clearStencil = attachment.stencilClearValue
            mtlAttachment.loadAction = loadAction(loadOp: attachment.stencilLoadOp)
            mtlAttachment.storeAction = storeAction(storeOp: attachment.stencilStoreOp)
            let isDestroyed = textureView.isDestroyed()
            if !isDestroyed {
                if hasStencilComponent && !stencilReadOnly {
                    if attachment.stencilLoadOp == WGPULoadOp_Undefined || attachment.stencilStoreOp == WGPUStoreOp_Undefined {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "stencil load and store op were not specified")
                    }
                } else if attachment.stencilLoadOp != WGPULoadOp_Undefined || attachment.stencilStoreOp != WGPUStoreOp_Undefined {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "stencil load and store op were specified")
                }
            }

            textureView.setCommandEncoder(self)

            if hasStencilComponent
                && mtlAttachment.loadAction == .load
                && mtlAttachment.storeAction == .dontCare
                && !textureView.previouslyCleared()
            {
                depthStencilAttachmentToClear = mtlAttachment.texture
                stencilAttachmentToClear = mtlAttachment.texture != nil
            }
        }

        if zeroColorTargets && mtlDescriptor.renderTargetWidth == 0 {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "zero color and depth targets")
        }

        var visibilityResultBufferSize: UInt = 0
        var visibilityResultBuffer: (any MTLBuffer)? = nil
        if let wgpuOcclusionQuery = descriptor.occlusionQuerySet {
            let occlusionQuery = WebGPU.fromAPI(wgpuOcclusionQuery)
            occlusionQuery.setCommandEncoder(self)
            if occlusionQuery.type() != WGPUQueryType_Occlusion {
                return WebGPU.RenderPassEncoder.createInvalid(
                    self,
                    m_device.ptr(),
                    "querySet for occlusion query was not of type occlusion"
                )
            }
            mtlDescriptor.visibilityResultBuffer = occlusionQuery.visibilityBuffer()
            visibilityResultBuffer = mtlDescriptor.visibilityResultBuffer
            visibilityResultBufferSize = occlusionQuery.isDestroyed() ? UInt.max : UInt(occlusionQuery.visibilityBuffer().length)
        }

        if attachmentsToClear.count != 0 || depthStencilAttachmentToClear != nil {
            let attachment = wgpuGetRenderPassDescriptorDepthStencilAttachment(descriptorSpan)?[0]
            if attachment != nil && depthStencilAttachmentToClear != nil {
                var texture = WebGPU.TextureOrTextureView(attachment)
                texture.setPreviouslyCleared()
            }

            // FIXME: rdar://138042799 remove default argument.
            runClearEncoder(
                attachmentsToClear: attachmentsToClear,
                depthStencilAttachmentToClear: &depthStencilAttachmentToClear,
                depthAttachmentToClear: depthAttachmentToClear,
                stencilAttachmentToClear: stencilAttachmentToClear,
                depthClearValue: 0,
                stencilClearValue: 0,
                existingEncoder: nil,
            )
        }

        if !m_device.ptr().isValid() {
            return WebGPU.RenderPassEncoder.createInvalid(
                self,
                m_device.ptr(),
                "GPUDevice was invalid, this will be an error submitting the command buffer"
            )
        }

        let mtlRenderCommandEncoder = m_commandBuffer?.makeRenderCommandEncoder(descriptor: mtlDescriptor)
        if m_existingCommandEncoder != nil {
            assertionFailure("!m_existingCommandEncoder")
        }
        setExistingEncoder(mtlRenderCommandEncoder)
        return WebGPU.RenderPassEncoder.create(
            mtlRenderCommandEncoder,
            descriptor,
            visibilityResultBufferSize,
            depthReadOnly,
            stencilReadOnly,
            self,
            visibilityResultBuffer,
            maxDrawCount,
            m_device.ptr(),
            mtlDescriptor
        )
    }

    static func hasValidDimensions(dimension: WGPUTextureDimension, width: UInt, height: UInt, depth: UInt) -> Bool {
        switch dimension {
        case WGPUTextureDimension_1D:
            width != 0
        case WGPUTextureDimension_2D:
            width != 0 && height != 0
        case WGPUTextureDimension_3D:
            width != 0 && height != 0 && depth != 0
        default:
            true
        }
    }

    func copyBufferToBuffer(
        source: WebGPU.Buffer,
        sourceOffset: UInt64,
        destination: WebGPU.Buffer,
        destinationOffset: UInt64,
        size: UInt64,
    ) {
        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertobuffer
        guard prepareTheEncoderState() else {
            generateInvalidEncoderStateError()
            return
        }

        if let error = self.errorValidatingCopyBufferToBuffer(
            source: source,
            sourceOffset: sourceOffset,
            destination: destination,
            destinationOffset: destinationOffset,
            size: size
        ) {
            makeInvalid(error)
            return
        }

        // FIXME: rdar://138042799 remove default argument.
        source.setCommandEncoder(self, false)
        destination.setCommandEncoder(self, false)
        destination.indirectBufferInvalidated(self)
        guard size != 0, !source.isDestroyed() && !destination.isDestroyed() else {
            return
        }

        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        blitCommandEncoder.copy(
            from: source.buffer(),
            sourceOffset: Int(sourceOffset),
            to: destination.buffer(),
            destinationOffset: Int(destinationOffset),
            size: Int(size)
        )
    }

    func copyTextureToBuffer(source: WGPUImageCopyTexture, destination: WGPUImageCopyBuffer, copySize: WGPUExtent3D) {
        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetobuffer

        guard prepareTheEncoderState() else {
            generateInvalidEncoderStateError()
            return
        }

        let sourceTexture = WebGPU.fromAPI(source.texture)
        if let error = errorValidatingCopyTextureToBuffer(source: source, destination: destination, copySize: copySize) {
            makeInvalid(error)
            return
        }

        let apiDestinationBuffer = WebGPU.fromAPI(destination.buffer)
        sourceTexture.setCommandEncoder(self)
        apiDestinationBuffer.setCommandEncoder(self, false)
        apiDestinationBuffer.indirectBufferInvalidated(self)
        guard !sourceTexture.isDestroyed() && !apiDestinationBuffer.isDestroyed() else {
            return
        }

        var options: MTLBlitOption = []
        switch source.aspect {
        case WGPUTextureAspect_All:
            break
        case WGPUTextureAspect_StencilOnly:
            options = .stencilFromDepthStencil
        case WGPUTextureAspect_DepthOnly:
            options = .depthFromDepthStencil
        case WGPUTextureAspect_Force32:
            return
        default:
            return
        }

        let logicalSize = sourceTexture.logicalMiplevelSpecificTextureExtent(source.mipLevel)
        let widthForMetal = logicalSize.width < source.origin.x ? 0 : min(copySize.width, logicalSize.width - source.origin.x)
        let heightForMetal = logicalSize.height < source.origin.y ? 0 : min(copySize.height, logicalSize.height - source.origin.y)
        let depthForMetal =
            logicalSize.depthOrArrayLayers < source.origin.z
            ? 0 : min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers - source.origin.z)

        guard let destinationBuffer = apiDestinationBuffer.buffer() else {
            return
        }
        var destinationBytesPerRow = UInt(destination.layout.bytesPerRow)
        if destinationBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED {
            destinationBytesPerRow = UInt(destinationBuffer.length)
        }

        let sourceTextureFormat = sourceTexture.format()
        let aspectSpecificFormat = WebGPU.Texture.aspectSpecificFormat(sourceTextureFormat, source.aspect)
        let blockSize = WebGPU.Texture.texelBlockSize(aspectSpecificFormat)
        let textureDimension = sourceTexture.dimension()
        var didOverflow: Bool
        switch textureDimension {
        case WGPUTextureDimension_1D:
            if !blockSize.hasOverflowed() {
                var product: UInt32 = blockSize.value()
                (product, didOverflow) = product.multipliedReportingOverflow(by: self.m_device.ptr().limitsCopy().maxTextureDimension1D)
                if !didOverflow {
                    destinationBytesPerRow = min(destinationBytesPerRow, UInt(product))
                }
            }
        case WGPUTextureDimension_2D, WGPUTextureDimension_3D:
            if !blockSize.hasOverflowed() {
                var product: UInt32 = blockSize.value()
                (product, didOverflow) = product.multipliedReportingOverflow(by: self.m_device.ptr().limitsCopy().maxTextureDimension2D)
                if !didOverflow {
                    destinationBytesPerRow = min(destinationBytesPerRow, UInt(product))
                }
            }
        case WGPUTextureDimension_Force32:
            break
        default:
            break
        }

        destinationBytesPerRow = roundUpToMultipleOfNonPowerOfTwoCheckedUInt32UnsignedLong(blockSize, destinationBytesPerRow)
        if textureDimension == WGPUTextureDimension_3D && copySize.depthOrArrayLayers <= 1 && copySize.height <= 1 {
            destinationBytesPerRow = 0
        }

        var rowsPerImage = destination.layout.rowsPerImage
        if rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED {
            rowsPerImage = heightForMetal != 0 ? heightForMetal : 1
        }
        var destinationBytesPerImage = UInt(rowsPerImage)
        (destinationBytesPerImage, didOverflow) = destinationBytesPerImage.multipliedReportingOverflow(by: destinationBytesPerRow)
        guard !didOverflow else {
            return
        }

        let maxDestinationBytesPerRow = textureDimension == WGPUTextureDimension_3D ? (2048 * blockSize.value()) : destinationBytesPerRow
        if destinationBytesPerRow > maxDestinationBytesPerRow {
            for z in 0..<copySize.depthOrArrayLayers {
                var zPlusOriginZ = z
                (zPlusOriginZ, didOverflow) = zPlusOriginZ.addingReportingOverflow(source.origin.z)
                guard !didOverflow else {
                    return
                }
                var zTimesDestinationBytesPerImage = z
                guard destinationBytesPerImage <= UInt32.max else {
                    return
                }
                (zTimesDestinationBytesPerImage, didOverflow) = zTimesDestinationBytesPerImage.multipliedReportingOverflow(
                    by: UInt32(destinationBytesPerImage)
                )
                guard !didOverflow else {
                    return
                }
                for y in 0..<copySize.height {
                    var yPlusOriginY = source.origin.y
                    (yPlusOriginY, didOverflow) = yPlusOriginY.addingReportingOverflow(y)
                    guard !didOverflow else {
                        return
                    }
                    var yTimesDestinationBytesPerRow = y
                    guard destinationBytesPerRow <= UInt32.max else {
                        return
                    }
                    (yTimesDestinationBytesPerRow, didOverflow) = yTimesDestinationBytesPerRow.multipliedReportingOverflow(
                        by: UInt32(destinationBytesPerRow)
                    )
                    guard !didOverflow else {
                        return
                    }
                    let newSource = WGPUImageCopyTexture(
                        texture: source.texture,
                        mipLevel: source.mipLevel,
                        origin: WGPUOrigin3D(x: source.origin.x, y: yPlusOriginY, z: zPlusOriginZ),
                        aspect: source.aspect
                    )
                    var tripleSum = UInt64(destination.layout.offset)
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(zTimesDestinationBytesPerImage))
                    guard !didOverflow else {
                        return
                    }
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(yTimesDestinationBytesPerRow))
                    guard !didOverflow else {
                        return
                    }
                    let newDestination = WGPUImageCopyBuffer(
                        layout: WGPUTextureDataLayout(
                            offset: tripleSum,
                            bytesPerRow: UInt32(WGPU_COPY_STRIDE_UNDEFINED),
                            rowsPerImage: UInt32(WGPU_COPY_STRIDE_UNDEFINED)
                        ),
                        buffer: destination.buffer
                    )
                    self.copyTextureToBuffer(
                        source: newSource,
                        destination: newDestination,
                        copySize: WGPUExtent3D(
                            width: copySize.width,
                            height: 1,
                            depthOrArrayLayers: 1
                        )
                    )
                }
            }
            return
        }

        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }

        for layer in 0..<copySize.depthOrArrayLayers {
            var originZPlusLayer = UInt(source.origin.z)
            (originZPlusLayer, didOverflow) = originZPlusLayer.addingReportingOverflow(UInt(layer))
            guard !didOverflow else {
                return
            }
            let sourceSlice = sourceTexture.dimension() == WGPUTextureDimension_3D ? 0 : originZPlusLayer
            if !sourceTexture.previouslyCleared(source.mipLevel, UInt32(sourceSlice)) {
                clearTextureIfNeeded(destination: source, slice: sourceSlice)
            }
        }

        guard
            Self.hasValidDimensions(
                dimension: sourceTexture.dimension(),
                width: UInt(widthForMetal),
                height: UInt(heightForMetal),
                depth: UInt(depthForMetal)
            )
        else {
            return
        }

        guard destinationBuffer.length >= WebGPU.Texture.bytesPerRow(aspectSpecificFormat, widthForMetal, sourceTexture.sampleCount())
        else {
            return
        }

        switch sourceTexture.dimension() {
        case WGPUTextureDimension_1D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
            // "When you copy to a 1D texture, height and depth must be 1."
            let sourceSize = MTLSize(width: Int(widthForMetal), height: 1, depth: 1)
            let sourceOrigin = MTLOrigin(x: Int(source.origin.x), y: 0, z: 0)
            for layer in 0..<copySize.depthOrArrayLayers {
                var layerTimesDestinationBytesPerImage = UInt(layer)
                (layerTimesDestinationBytesPerImage, didOverflow) = layerTimesDestinationBytesPerImage.multipliedReportingOverflow(
                    by: destinationBytesPerImage
                )
                guard !didOverflow else {
                    return
                }
                var destinationOffset = UInt(destination.layout.offset)
                (destinationOffset, didOverflow) = destinationOffset.addingReportingOverflow(layerTimesDestinationBytesPerImage)
                guard !didOverflow else {
                    return
                }
                var sourceSlice = UInt(source.origin.z)
                (sourceSlice, didOverflow) = sourceSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                var widthTimesBlockSize = UInt(widthForMetal)
                (widthTimesBlockSize, didOverflow) = widthTimesBlockSize.multipliedReportingOverflow(by: blockSize.value())
                guard !didOverflow else {
                    return
                }
                var sum = UInt(destinationOffset)
                (sum, didOverflow) = sum.addingReportingOverflow(UInt(widthTimesBlockSize))
                guard !didOverflow else {
                    return
                }
                if sum > destinationBuffer.length {
                    continue
                }
                blitCommandEncoder
                    .copy(
                        from: sourceTexture.texture(),
                        sourceSlice: Int(sourceSlice),
                        sourceLevel: Int(source.mipLevel),
                        sourceOrigin: sourceOrigin,
                        sourceSize: sourceSize,
                        to: destinationBuffer,
                        destinationOffset: Int(destinationOffset),
                        destinationBytesPerRow: Int(destinationBytesPerRow),
                        destinationBytesPerImage: Int(destinationBytesPerImage),
                        options: options
                    )
            }
        case WGPUTextureDimension_2D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
            // "When you copy to a 2D texture, depth must be 1."
            let sourceSize = MTLSizeMake(Int(widthForMetal), Int(heightForMetal), 1)
            let sourceOrigin = MTLOriginMake(Int(source.origin.x), Int(source.origin.y), 0)
            for layer in 0..<copySize.depthOrArrayLayers {
                var layerTimesBytesPerImage = UInt(layer)
                (layerTimesBytesPerImage, didOverflow) = layerTimesBytesPerImage.multipliedReportingOverflow(by: destinationBytesPerImage)
                guard !didOverflow else {
                    return
                }
                var destinationOffset = UInt(destination.layout.offset)
                (destinationOffset, didOverflow) = destinationOffset.addingReportingOverflow(layerTimesBytesPerImage)
                guard !didOverflow else {
                    return
                }
                var sourceSlice = UInt(source.origin.z)
                (sourceSlice, didOverflow) = sourceSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                blitCommandEncoder.copy(
                    from: sourceTexture.texture(),
                    sourceSlice: Int(sourceSlice),
                    sourceLevel: Int(source.mipLevel),
                    sourceOrigin: sourceOrigin,
                    sourceSize: sourceSize,
                    to: destinationBuffer,
                    destinationOffset: Int(destinationOffset),
                    destinationBytesPerRow: Int(destinationBytesPerRow),
                    destinationBytesPerImage: Int(destinationBytesPerImage),
                    options: options
                )
            }
        case WGPUTextureDimension_3D:
            let sourceSize = MTLSize(width: Int(widthForMetal), height: Int(heightForMetal), depth: Int(depthForMetal))
            let sourceOrigin = MTLOrigin(x: Int(source.origin.x), y: Int(source.origin.y), z: Int(source.origin.z))
            let destinationOffset = UInt(destination.layout.offset)
            blitCommandEncoder
                .copy(
                    from: sourceTexture.texture(),
                    sourceSlice: 0,
                    sourceLevel: Int(source.mipLevel),
                    sourceOrigin: sourceOrigin,
                    sourceSize: sourceSize,
                    to: destinationBuffer,
                    destinationOffset: Int(destinationOffset),
                    destinationBytesPerRow: Int(destinationBytesPerRow),
                    destinationBytesPerImage: Int(destinationBytesPerImage),
                    options: options,
                )
        case WGPUTextureDimension_Force32:
            return
        default:
            return
        }
    }

    func copyBufferToTexture(source: WGPUImageCopyBuffer, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
        guard prepareTheEncoderState() else {
            generateInvalidEncoderStateError()
            return
        }
        let destinationTexture = WebGPU.fromAPI(destination.texture)

        if let error = errorValidatingCopyBufferToTexture(source: source, destination: destination, copySize: copySize) {
            makeInvalid(error)
            return
        }
        let apiBuffer = WebGPU.fromAPI(source.buffer)
        apiBuffer.setCommandEncoder(self, false)
        destinationTexture.setCommandEncoder(self)

        guard
            copySize.width != 0 || copySize.height != 0 || copySize.depthOrArrayLayers != 0,
            !apiBuffer.isDestroyed(),
            !destinationTexture.isDestroyed()
        else {
            return
        }

        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        var sourceBytesPerRow = UInt(source.layout.bytesPerRow)
        guard let sourceBuffer = apiBuffer.buffer() else {
            return
        }
        if sourceBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED {
            sourceBytesPerRow = UInt(sourceBuffer.length)
        }
        let aspectSpecificFormat = WebGPU.Texture.aspectSpecificFormat(destinationTexture.format(), destination.aspect)
        let blockSize = WebGPU.Texture.texelBlockSize(aspectSpecificFormat)
        // Interesting that swift imports this.. becase I think it knows how to manage WebGPU.Device
        // It will not import raw pointers it does not know how to manage.
        let device = m_device.ptr()
        switch destinationTexture.dimension() {
        case WGPUTextureDimension_1D:
            if !blockSize.hasOverflowed() {
                // swift cannot infer .value()'s type
                let blockSizeValue: UInt32 = blockSize.value()
                let (result, didOverflow) = blockSizeValue.multipliedReportingOverflow(by: device.limitsCopy().maxTextureDimension1D)
                if !didOverflow {
                    sourceBytesPerRow = min(sourceBytesPerRow, UInt(result))
                }
            }
        case WGPUTextureDimension_2D, WGPUTextureDimension_3D:
            if !blockSize.hasOverflowed() {
                // swift cannot infer .value()'s type
                let blockSizeValue: UInt32 = blockSize.value()
                let (result, didOverflow) = blockSizeValue.multipliedReportingOverflow(by: device.limitsCopy().maxTextureDimension2D)
                if !didOverflow {
                    sourceBytesPerRow = min(sourceBytesPerRow, UInt(result))
                }
            }
        case WGPUTextureDimension_Force32:
            break
        default:
            break
        }

        var options: MTLBlitOption = []
        switch destination.aspect {
        case WGPUTextureAspect_StencilOnly:
            options = .stencilFromDepthStencil
        case WGPUTextureAspect_DepthOnly:
            options = .depthFromDepthStencil
        case WGPUTextureAspect_All:
            break
        case WGPUTextureAspect_Force32:
            return
        default:
            return
        }
        let logicalSize = WebGPU.fromAPI(destination.texture).logicalMiplevelSpecificTextureExtent(destination.mipLevel)
        let widthForMetal = logicalSize.width < destination.origin.x ? 0 : min(copySize.width, logicalSize.width - destination.origin.x)
        let heightForMetal = logicalSize.height < destination.origin.y ? 0 : min(copySize.height, logicalSize.height - destination.origin.y)
        let depthForMetal =
            logicalSize.depthOrArrayLayers < destination.origin.z
            ? 0 : min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers - destination.origin.z)
        var rowsPerImage = source.layout.rowsPerImage
        if rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED {
            rowsPerImage = heightForMetal != 0 ? rowsPerImage : 1
        }
        var sourceBytesPerImage: UInt
        var didOverflow: Bool
        (sourceBytesPerImage, didOverflow) = UInt(rowsPerImage).multipliedReportingOverflow(by: sourceBytesPerRow)
        guard !didOverflow else {
            return
        }
        let mtlDestinationTexture = destinationTexture.texture()
        let textureDimension = destinationTexture.dimension()

        let sliceCount = textureDimension == WGPUTextureDimension_3D ? 1 : copySize.depthOrArrayLayers
        for layer in 0..<sliceCount {
            var originPlusLayer = destination.origin.z
            (originPlusLayer, didOverflow) = originPlusLayer.addingReportingOverflow(layer)
            if didOverflow {
                return
            }
            let destinationSlice = destinationTexture.dimension() == WGPUTextureDimension_3D ? 0 : originPlusLayer

            guard let mtlDestinationTexture else {
                fatalError("mtlDestinationTexture is nil")
            }

            precondition(mtlDestinationTexture.parent == nil, "mtlDestinationTexture.parentTexture is not nil")

            if WebGPU.Queue.writeWillCompletelyClear(
                textureDimension,
                widthForMetal,
                logicalSize.width,
                heightForMetal,
                logicalSize.height,
                depthForMetal,
                logicalSize.depthOrArrayLayers
            ) {
                // FIXME: rdar://138042799 remove default argument.
                destinationTexture.setPreviouslyCleared(destination.mipLevel, destinationSlice, true)
            } else {
                clearTextureIfNeeded(destination: destination, slice: UInt(destinationSlice))
            }
        }
        let maxSourceBytesPerRow =
            textureDimension == WGPUTextureDimension_3D ? (2048 * blockSize.value()) : sourceBytesPerRow
        if textureDimension == WGPUTextureDimension_3D && copySize.depthOrArrayLayers <= 1 && copySize.height <= 1 {
            sourceBytesPerRow = 0
        }
        if sourceBytesPerRow > maxSourceBytesPerRow {
            for z in 0..<copySize.depthOrArrayLayers {
                var destinationOriginPlusZ = destination.origin.z
                (destinationOriginPlusZ, didOverflow) = destinationOriginPlusZ.addingReportingOverflow(z)
                guard !didOverflow else {
                    return
                }
                var zTimesSourceBytesPerImage = z
                guard let sourceBytesPerImageU32 = UInt32(exactly: sourceBytesPerImage) else {
                    return
                }
                (zTimesSourceBytesPerImage, didOverflow) = zTimesSourceBytesPerImage.multipliedReportingOverflow(by: sourceBytesPerImageU32)
                guard !didOverflow else {
                    return
                }
                guard let sourceBytesPerRowU32 = UInt32(exactly: sourceBytesPerRow) else {
                    return
                }
                for y in 0..<copySize.height {
                    var yTimesSourceBytesPerRow = y
                    (yTimesSourceBytesPerRow, didOverflow) = yTimesSourceBytesPerRow.multipliedReportingOverflow(by: sourceBytesPerRowU32)
                    guard !didOverflow else {
                        return
                    }
                    var tripleSum = UInt64(zTimesSourceBytesPerImage)
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(yTimesSourceBytesPerRow))
                    guard !didOverflow else {
                        return
                    }
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(source.layout.offset))
                    guard !didOverflow else {
                        return
                    }
                    let newSource = WGPUImageCopyBuffer(
                        layout: WGPUTextureDataLayout(
                            offset: tripleSum,
                            bytesPerRow: UInt32(WGPU_COPY_STRIDE_UNDEFINED),
                            rowsPerImage: UInt32(WGPU_COPY_STRIDE_UNDEFINED)
                        ),
                        buffer: source.buffer,
                    )
                    var destinationOriginPlusY = y
                    (destinationOriginPlusY, didOverflow) = destinationOriginPlusY.addingReportingOverflow(destination.origin.y)
                    guard !didOverflow else {
                        return
                    }
                    let newDestination = WGPUImageCopyTexture(
                        texture: destination.texture,
                        mipLevel: destination.mipLevel,
                        origin: WGPUOrigin3D(
                            x: destination.origin.x,
                            y: destinationOriginPlusY,
                            z: destinationOriginPlusZ
                        ),
                        aspect: destination.aspect
                    )
                    copyBufferToTexture(
                        source: newSource,
                        destination: newDestination,
                        copySize: WGPUExtent3D(width: copySize.width, height: 1, depthOrArrayLayers: 1),
                    )
                }
            }
            return
        }

        guard sourceBuffer.length >= WebGPU.Texture.bytesPerRow(aspectSpecificFormat, widthForMetal, destinationTexture.sampleCount())
        else {
            return
        }

        switch destinationTexture.dimension() {
        case WGPUTextureDimension_1D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
            // "When you copy to a 1D texture, height and depth must be 1."
            let sourceSize = MTLSize(width: Int(widthForMetal), height: 1, depth: 1)
            guard widthForMetal != 0 else {
                return
            }

            let destinationOrigin = MTLOrigin(x: Int(destination.origin.x), y: 0, z: 0)
            var widthTimesBlockSize = widthForMetal
            (widthTimesBlockSize, didOverflow) = widthTimesBlockSize.multipliedReportingOverflow(by: blockSize.value())
            guard !didOverflow else {
                return
            }
            sourceBytesPerRow = min(sourceBytesPerRow, UInt(widthTimesBlockSize))
            for layer in 0..<copySize.depthOrArrayLayers {
                var layerTimesSourceBytesPerImage = UInt(layer)
                (layerTimesSourceBytesPerImage, didOverflow) = layerTimesSourceBytesPerImage.multipliedReportingOverflow(
                    by: sourceBytesPerImage
                )
                guard !didOverflow else {
                    return
                }

                var sourceOffset = UInt(source.layout.offset)
                (sourceOffset, didOverflow) = sourceOffset.addingReportingOverflow(layerTimesSourceBytesPerImage)
                guard !didOverflow else {
                    return
                }
                var destinationSlice = UInt(destination.origin.z)
                (destinationSlice, didOverflow) = destinationSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                var sourceOffsetPlusSourceBytesPerRow = sourceOffset
                (sourceOffsetPlusSourceBytesPerRow, didOverflow) = sourceOffsetPlusSourceBytesPerRow.addingReportingOverflow(
                    sourceBytesPerRow
                )
                guard !didOverflow else {
                    return
                }
                if sourceOffsetPlusSourceBytesPerRow > sourceBuffer.length {
                    continue
                }

                // FIXME: (rdar://170907276) Prove that `mtlDestinationTexture` can never be nil.
                // swift-format-ignore: NeverForceUnwrap
                blitCommandEncoder
                    .copy(
                        from: sourceBuffer,
                        sourceOffset: Int(sourceOffset),
                        sourceBytesPerRow: Int(sourceBytesPerRow),
                        sourceBytesPerImage: Int(sourceBytesPerImage),
                        sourceSize: sourceSize,
                        to: mtlDestinationTexture!,
                        destinationSlice: Int(destinationSlice),
                        destinationLevel: Int(destination.mipLevel),
                        destinationOrigin: destinationOrigin,
                        options: options
                    )
            }
        case WGPUTextureDimension_2D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
            // "When you copy to a 2D texture, depth must be 1."
            let sourceSize = MTLSize(width: Int(widthForMetal), height: Int(heightForMetal), depth: 1)
            guard widthForMetal != 0 && heightForMetal != 0 else {
                return
            }

            let destinationOrigin = MTLOrigin(x: Int(destination.origin.x), y: Int(destination.origin.y), z: 0)
            for layer in 0..<copySize.depthOrArrayLayers {
                var layerTimesSourceBytesPerImage = UInt(layer)
                (layerTimesSourceBytesPerImage, didOverflow) = layerTimesSourceBytesPerImage.multipliedReportingOverflow(
                    by: sourceBytesPerImage
                )
                guard !didOverflow else {
                    return
                }
                var sourceOffset = UInt(source.layout.offset)
                (sourceOffset, didOverflow) = sourceOffset.addingReportingOverflow(layerTimesSourceBytesPerImage)
                guard !didOverflow else {
                    return
                }
                var destinationSlice = UInt(destination.origin.z)
                (destinationSlice, didOverflow) = destinationSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }

                // FIXME: (rdar://170907276) Prove that `mtlDestinationTexture` can never be nil.
                // swift-format-ignore: NeverForceUnwrap
                blitCommandEncoder
                    .copy(
                        from: sourceBuffer,
                        sourceOffset: Int(sourceOffset),
                        sourceBytesPerRow: Int(sourceBytesPerRow),
                        sourceBytesPerImage: Int(sourceBytesPerImage),
                        sourceSize: sourceSize,
                        to: mtlDestinationTexture!,
                        destinationSlice: Int(destinationSlice),
                        destinationLevel: Int(destination.mipLevel),
                        destinationOrigin: destinationOrigin,
                        options: options
                    )
            }

        case WGPUTextureDimension_3D:
            let sourceSize = MTLSize(width: Int(widthForMetal), height: Int(heightForMetal), depth: Int(depthForMetal))
            guard widthForMetal != 0 && heightForMetal != 0 && depthForMetal != 0 else {
                return
            }

            let destinationOrigin = MTLOrigin(x: Int(destination.origin.x), y: Int(destination.origin.y), z: Int(destination.origin.z))
            let sourceOffset = UInt(source.layout.offset)

            // FIXME: (rdar://170907276) Prove that `mtlDestinationTexture` can never be nil.
            // swift-format-ignore: NeverForceUnwrap
            blitCommandEncoder.copy(
                from: sourceBuffer,
                sourceOffset: Int(sourceOffset),
                sourceBytesPerRow: Int(sourceBytesPerRow),
                sourceBytesPerImage: Int(sourceBytesPerImage),
                sourceSize: sourceSize,
                to: mtlDestinationTexture!,
                destinationSlice: 0,
                destinationLevel: Int(destination.mipLevel),
                destinationOrigin: destinationOrigin,
                options: options
            )
        case WGPUTextureDimension_Force32:
            assertionFailure()
            return
        default:
            assertionFailure()
            return
        }
    }

    func clearBuffer(buffer: WebGPU.Buffer, offset: UInt64, size: inout UInt64) {
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        if size == .max {
            let initialSize = buffer.initialSize()
            let (subtractionResult, didOverflow) = initialSize.subtractingReportingOverflow(offset)
            if didOverflow {
                self.m_device.ptr().generateAValidationError("CommandEncoder::clearBuffer(): offset > buffer.size")
                return
            }
            size = subtractionResult
        }

        if !self.validateClearBuffer(buffer, offset, size) {
            self.makeInvalid("GPUCommandEncoder.clearBuffer validation failed")
            return
        }
        // FIXME: rdar://138042799 need to pass in the default argument.
        buffer.setCommandEncoder(self, false)
        buffer.indirectBufferInvalidated(self)
        guard let offsetInt = Int(exactly: offset), let sizeInt = Int(exactly: size) else {
            return
        }
        let range = offsetInt..<(offsetInt + sizeInt)
        if buffer.isDestroyed() || sizeInt == 0 || range.upperBound > buffer.buffer().length {
            return
        }
        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        blitCommandEncoder.fill(buffer: buffer.buffer(), range: range, value: 0)
    }

    func resolveQuerySet(
        _ querySet: WebGPU.QuerySet,
        firstQuery: UInt32,
        queryCount: UInt32,
        destination: WebGPU.Buffer,
        destinationOffset: UInt64
    ) {
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }

        let isValid = validateResolveQuerySet(
            querySet: querySet,
            firstQuery: firstQuery,
            queryCount: queryCount,
            destination: destination,
            destinationOffset: destinationOffset
        )

        guard
            isValid,
            CxxBridging.isValidToUseWithQuerySetCommandEncoder(querySet, self),
            CxxBridging.isValidToUseWithBufferCommandEncoder(destination, self)
        else {
            makeInvalid("GPUCommandEncoder.resolveQuerySet validation failed")
            return
        }

        querySet.setCommandEncoder(self)
        // FIXME: rdar://138042799 need to pass in the default argument.
        destination.setCommandEncoder(self, false)
        destination.indirectBufferInvalidated(self)

        if querySet.isDestroyed() || destination.isDestroyed() || queryCount == 0 {
            return
        }

        if querySet.type() == WGPUQueryType_Occlusion {
            guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
                return
            }
            guard
                let sourceOffset = Int(exactly: 8 * firstQuery),
                let destinationOffsetChecked = Int(exactly: destinationOffset),
                let size = Int(exactly: 8 * queryCount)
            else {
                return
            }
            blitCommandEncoder.copy(
                from: querySet.visibilityBuffer(),
                sourceOffset: sourceOffset,
                to: destination.buffer(),
                destinationOffset: destinationOffsetChecked,
                size: size
            )
        }

        if querySet.type() == WGPUQueryType_Timestamp {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=283385 - https://bugs.webkit.org/show_bug.cgi?id=283088 should be reverted when the blocking issue is resolved
            finalizeBlitCommandEncoder()
            let workaround = m_device.ptr().resolveTimestampsSharedEvent()
            // The signal value does not matter, the event alone prevents reordering.
            m_commandBuffer?.encodeSignalEvent(workaround, value: 1)
            m_commandBuffer?.encodeWaitForEvent(workaround, value: 1)
            guard ensureBlitCommandEncoder() != nil else {
                return
            }
            let counterSampleBuffer = querySet.counterSampleBufferWithOffset()
            guard let buffer = counterSampleBuffer.buffer else {
                return
            }

            m_blitCommandEncoder?
                .resolveCounters(
                    buffer,
                    range: Int(firstQuery + counterSampleBuffer.offset)..<Int(firstQuery + counterSampleBuffer.offset + queryCount),
                    destinationBuffer: destination.buffer(),
                    destinationOffset: Int(destinationOffset)
                )
        }
    }

    func validateResolveQuerySet(
        querySet: WebGPU.QuerySet,
        firstQuery: UInt32,
        queryCount: UInt32,
        destination: WebGPU.Buffer,
        destinationOffset: UInt64
    ) -> Bool {
        if !querySet.isDestroyed() && !querySet.isValid() {
            return false
        }
        if !destination.isDestroyed() && !destination.isValid() {
            return false
        }
        if (destination.usage() & WGPUBufferUsage_QueryResolve.rawValue) == 0 {
            return false
        }

        if firstQuery >= querySet.count() {
            return false
        }

        let (countEnd, didOverflow) = firstQuery.addingReportingOverflow(queryCount)
        if didOverflow || countEnd > querySet.count() {
            return false
        }

        if (destinationOffset % 256) != 0 {
            return false
        }

        let (queryCountTimes8, didOverflowMul) = UInt64(queryCount).multipliedReportingOverflow(by: 8)
        if didOverflowMul {
            return false
        }

        let (countTimes8PlusOffset, didOverflowSum) = destinationOffset.addingReportingOverflow(UInt64(queryCountTimes8))
        if didOverflowSum || countTimes8PlusOffset > destination.initialSize() {
            return false
        }

        return true
    }

    func copyTextureToTexture(source: WGPUImageCopyTexture, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetotexture

        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        if let error = self.errorValidatingCopyTextureToTexture(source: source, destination: destination, copySize: copySize) {
            self.makeInvalid(error)
            return
        }

        let sourceTexture = WebGPU.fromAPI(source.texture)
        let destinationTexture = WebGPU.fromAPI(destination.texture)
        sourceTexture.setCommandEncoder(self)
        destinationTexture.setCommandEncoder(self)

        guard !sourceTexture.isDestroyed(), !destinationTexture.isDestroyed() else {
            return
        }

        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }

        let destinationTextureDimension = destinationTexture.dimension()
        let sliceCount = destinationTextureDimension == WGPUTextureDimension_3D ? 1 : copySize.depthOrArrayLayers
        let destinationLogicalSize = destinationTexture.logicalMiplevelSpecificTextureExtent(destination.mipLevel)
        var didOverflow: Bool
        for layer in 0..<sliceCount {
            var sourceOriginPlusLayer = UInt(source.origin.z)
            (sourceOriginPlusLayer, didOverflow) = sourceOriginPlusLayer.addingReportingOverflow(UInt(layer))
            guard !didOverflow else {
                return
            }
            let sourceSlice = sourceTexture.dimension() == WGPUTextureDimension_3D ? 0 : sourceOriginPlusLayer
            self.clearTextureIfNeeded(destination: source, slice: sourceSlice)
            var destinationOriginPlusLayer = UInt(destination.origin.z)
            (destinationOriginPlusLayer, didOverflow) = destinationOriginPlusLayer.addingReportingOverflow(UInt(layer))
            guard !didOverflow else {
                return
            }
            let destinationSlice: UInt = destinationTexture.dimension() == WGPUTextureDimension_3D ? 0 : destinationOriginPlusLayer
            if WebGPU.Queue.writeWillCompletelyClear(
                destinationTextureDimension,
                copySize.width,
                destinationLogicalSize.width,
                copySize.height,
                destinationLogicalSize.height,
                copySize.depthOrArrayLayers,
                destinationLogicalSize.depthOrArrayLayers
            ) {
                guard let destinationSliceUInt32 = UInt32(exactly: destinationSlice) else {
                    return
                }
                // FIXME: rdar://138042799 remove default argument.
                destinationTexture.setPreviouslyCleared(destination.mipLevel, destinationSliceUInt32, true)
            } else {
                self.clearTextureIfNeeded(destination: destination, slice: destinationSlice)
            }
        }
        guard
            let mtlDestinationTexture = destinationTexture.texture(),
            let mtlSourceTexture = WebGPU.fromAPI(source.texture).texture()
        else {
            return
        }

        // FIXME(PERFORMANCE): Is it actually faster to use the -[MTLBlitCommandEncoder copyFromTexture:...toTexture:...levelCount:]
        // variant, where possible, rather than calling the other variant in a loop?
        switch sourceTexture.dimension() {
        case WGPUTextureDimension_1D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
            // "When you copy to a 1D texture, height and depth must be 1."
            let sourceSize = MTLSize(width: Int(copySize.width), height: 1, depth: 1)
            guard sourceSize.width != 0 else {
                return
            }

            let sourceOrigin = MTLOrigin(x: Int(source.origin.x), y: 0, z: 0)
            let destinationOrigin = MTLOrigin(x: Int(destination.origin.x), y: 0, z: 0)
            for layer in 0..<copySize.depthOrArrayLayers {
                var sourceSlice = UInt(source.origin.z)
                (sourceSlice, didOverflow) = sourceSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                var destinationSlice = UInt(destination.origin.z)
                (destinationSlice, didOverflow) = destinationSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                if destinationSlice >= mtlDestinationTexture.arrayLength || sourceSlice >= mtlSourceTexture.arrayLength {
                    continue
                }
                blitCommandEncoder.copy(
                    from: mtlSourceTexture,
                    sourceSlice: Int(sourceSlice),
                    sourceLevel: Int(source.mipLevel),
                    sourceOrigin: sourceOrigin,
                    sourceSize: sourceSize,
                    to: mtlDestinationTexture,
                    destinationSlice: Int(destinationSlice),
                    destinationLevel: Int(destination.mipLevel),
                    destinationOrigin: destinationOrigin
                )
            }
        case WGPUTextureDimension_2D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
            // "When you copy to a 2D texture, depth must be 1."
            let sourceSize = MTLSize(width: Int(copySize.width), height: Int(copySize.height), depth: 1)
            guard sourceSize.width != 0, sourceSize.height != 0 else {
                return
            }

            let sourceOrigin = MTLOrigin(x: Int(source.origin.x), y: Int(source.origin.y), z: 0)
            let destinationOrigin = MTLOrigin(x: Int(destination.origin.x), y: Int(destination.origin.y), z: 0)

            for layer in 0..<copySize.depthOrArrayLayers {
                var sourceSlice = UInt(source.origin.z)
                (sourceSlice, didOverflow) = sourceSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                var destinationSlice = UInt(destination.origin.z)
                (destinationSlice, didOverflow) = destinationSlice.addingReportingOverflow(UInt(layer))
                guard !didOverflow else {
                    return
                }
                if destinationSlice >= mtlDestinationTexture.arrayLength || sourceSlice >= mtlSourceTexture.arrayLength {
                    continue
                }
                blitCommandEncoder.copy(
                    from: mtlSourceTexture,
                    sourceSlice: Int(sourceSlice),
                    sourceLevel: Int(source.mipLevel),
                    sourceOrigin: sourceOrigin,
                    sourceSize: (sourceSize),
                    to: mtlDestinationTexture,
                    destinationSlice: Int(destinationSlice),
                    destinationLevel: Int(destination.mipLevel),
                    destinationOrigin: destinationOrigin
                )
            }
        case WGPUTextureDimension_3D:
            let sourceSize = MTLSize(width: Int(copySize.width), height: Int(copySize.height), depth: Int(copySize.depthOrArrayLayers))
            guard sourceSize.width != 0, sourceSize.height != 0, sourceSize.depth != 0 else {
                return
            }
            var originPlusSourceSize = UInt32(destination.origin.z)
            guard let sourceSizeDepthUInt32 = UInt32(exactly: sourceSize.depth) else {
                return
            }
            (originPlusSourceSize, didOverflow) = originPlusSourceSize.addingReportingOverflow(sourceSizeDepthUInt32)
            guard !didOverflow else {
                return
            }
            guard let mtlDestinationTextureDepthUInt32 = UInt32(exactly: mtlDestinationTexture.depth) else {
                return
            }
            guard originPlusSourceSize <= min(destinationLogicalSize.depthOrArrayLayers, mtlDestinationTextureDepthUInt32) else {
                makeInvalid(
                    "GPUCommandEncoder.copyTextureToTexture: destination.origin.z + sourceSize.depth > destinationLogicalSize.depthOrArrayLayers"
                )
                return
            }

            let sourceOrigin = MTLOrigin(x: Int(source.origin.x), y: Int(source.origin.y), z: Int(source.origin.z))
            let destinationOrigin = MTLOrigin(x: Int(destination.origin.x), y: Int(destination.origin.y), z: Int(destination.origin.z))
            blitCommandEncoder.copy(
                from: mtlSourceTexture,
                sourceSlice: 0,
                sourceLevel: Int(source.mipLevel),
                sourceOrigin: sourceOrigin,
                sourceSize: (sourceSize),
                to: mtlDestinationTexture,
                destinationSlice: 0,
                destinationLevel: Int(destination.mipLevel),
                destinationOrigin: destinationOrigin
            )
        case WGPUTextureDimension_Force32:
            assertionFailure()
            return
        default:
            assertionFailure()
            return
        }
    }

    func beginComputePass(descriptor: WGPUComputePassDescriptor) -> CxxBridging.RefComputePassEncoder {
        let collection = CollectionOfOne(descriptor)

        guard prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "encoder state is invalid")
        }

        if let error = self.errorValidatingComputePassDescriptor(descriptor: descriptor) {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), String(error))
        }

        // FIXME: Use accessor functions instead of member variables directly.
        // swift-format-ignore: AlwaysUseLowerCamelCase
        if let m_commandBuffer, m_commandBuffer.status.rawValue >= MTLCommandBufferStatus.enqueued.rawValue {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "command buffer has already been committed")
        }

        self.finalizeBlitCommandEncoder()

        guard m_device.ptr().isValid() else {
            return WebGPU.ComputePassEncoder.createInvalid(
                self,
                m_device.ptr(),
                "GPUDevice was invalid, this will be an error submitting the command buffer"
            )
        }
        let computePassDescriptor = MTLComputePassDescriptor()
        computePassDescriptor.dispatchType = .serial
        var counterSampleBuffer = WebGPU.QuerySet.CounterSampleBuffer()
        if let wgpuTimestampWrites = wgpuGetComputePassDescriptorTimestampWrites(collection.span)?[0] {
            let timestampsWrites = WebGPU.fromAPI(wgpuTimestampWrites.querySet)
            counterSampleBuffer = timestampsWrites.counterSampleBufferWithOffset()
            timestampsWrites.setCommandEncoder(self)
        }

        if m_device.ptr().enableEncoderTimestamps() || counterSampleBuffer.buffer != nil {
            // FIXME: (rdar://170907276) Prove that the result of `wgpuGetComputePassDescriptorTimestampWrites` can never be nil.
            // swift-format-ignore: NeverForceUnwrap
            let timestampWrites = wgpuGetComputePassDescriptorTimestampWrites(collection.span)![0]
            computePassDescriptor.sampleBufferAttachments[0].sampleBuffer =
                counterSampleBuffer.buffer ?? m_device.ptr().timestampsBuffer(m_commandBuffer, 2)

            computePassDescriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = timestampWriteIndex(
                writeIndex: timestampWrites.beginningOfPassWriteIndex,
                defaultValue: MTLCounterDontSample,
                offset: counterSampleBuffer.offset
            )

            computePassDescriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = timestampWriteIndex(
                writeIndex: timestampWrites.endOfPassWriteIndex,
                defaultValue: MTLCounterDontSample,
                offset: counterSampleBuffer.offset
            )

            if let buffer = counterSampleBuffer.buffer {
                m_device.ptr().trackTimestampsBuffer(m_commandBuffer, buffer)
            }
        }
        guard let computeCommandEncoder = m_commandBuffer?.makeComputeCommandEncoder(descriptor: computePassDescriptor) else {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "computeCommandEncoder is null")
        }

        self.setExistingEncoder(computeCommandEncoder)
        // FIXME: Figure out a way so that WTFString does not override String in the global
        //        namespace. At the moment it is and that's why we need this.
        computeCommandEncoder.label = CxxBridging.convertWTFStringToNSString(descriptor.label)

        return WebGPU.ComputePassEncoder.create(computeCommandEncoder, descriptor, self, m_device.ptr())
    }
}
