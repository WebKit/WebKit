/*
 * Copyright (c) 2021-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import Metal
import WebGPU_Internal

// FIXME: rdar://140819194
private let WGPU_COPY_STRIDE_UNDEFINED = WGPU_COPY_STRIDE_UNDEFINED_

// FIXME: rdar://140819448
private let MTLBlitOptionNone = MTLBlitOptionNone_

public func clearBuffer(
    commandEncoder: WebGPU.CommandEncoder, buffer: WebGPU.Buffer, offset: UInt64, size: inout UInt64
) {
    commandEncoder.clearBuffer(buffer: buffer, offset: offset, size: &size)
}
public func resolveQuerySet(commandEncoder: WebGPU.CommandEncoder, querySet: WebGPU.QuerySet, firstQuery: UInt32, queryCount:UInt32, destination: WebGPU.Buffer, destinationOffset: UInt64)
{
    commandEncoder.resolveQuerySet(querySet, firstQuery: firstQuery, queryCount: queryCount, destination: destination, destinationOffset: destinationOffset)
}

@_expose(Cxx)
public func CommandEncoder_copyBufferToTexture_thunk(commandEncoder: WebGPU.CommandEncoder, source: WGPUImageCopyBuffer, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D){
    commandEncoder.copyBufferToTexture(source: source, destination: destination, copySize: copySize)
}

@_expose(Cxx)
public func CommandEncoder_copyTextureToBuffer_thunk(commandEncoder: WebGPU.CommandEncoder, source: WGPUImageCopyTexture, destination: WGPUImageCopyBuffer, copySize: WGPUExtent3D){
    commandEncoder.copyTextureToBuffer(source: source, destination: destination, copySize: copySize)
}

@_expose(Cxx)
public func CommandEncoder_copyTextureToTexture_thunk(commandEncoder: WebGPU.CommandEncoder, source: WGPUImageCopyTexture, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
    commandEncoder.copyTextureToTexture(source: source, destination: destination, copySize: copySize)
}

@_expose(Cxx)
public func CommandEncoder_copyBufferToBuffer_thunk(commandEncoder: WebGPU.CommandEncoder, source: WebGPU.Buffer, sourceOffset: UInt64, destination: WebGPU.Buffer, destinationOffset: UInt64, size: UInt64) {
    commandEncoder.copyBufferToBuffer(source: source, sourceOffset: sourceOffset, destination: destination, destinationOffset: destinationOffset, size: size)
}

@_expose(Cxx)
public func CommandEncoder_beginRenderPass_thunk(commandEncoder: WebGPU.CommandEncoder, descriptor: WGPURenderPassDescriptor) -> WebGPU_Internal.RefRenderPassEncoder {
    return commandEncoder.beginRenderPass(descriptor: descriptor)
}

@_expose(Cxx)
public func CommandEncoder_beginComputePass_thunk(commandEncoder: WebGPU.CommandEncoder, descriptor: WGPUComputePassDescriptor) -> WebGPU_Internal.RefComputePassEncoder {
    return commandEncoder.beginComputePass(descriptor: descriptor)
}

extension WebGPU.CommandEncoder {
    private func isRenderableTextureView(texture: WebGPU.TextureView) -> Bool {
        let textureDimension = texture.dimension()

        return (texture.usage() & WGPUTextureUsage_RenderAttachment.rawValue) != 0 && (textureDimension == WGPUTextureViewDimension_2D || textureDimension == WGPUTextureViewDimension_2DArray || textureDimension == WGPUTextureViewDimension_3D) && texture.mipLevelCount() == 1 && texture.arrayLayerCount() <= 1
    }
    private func loadAction(loadOp: WGPULoadOp) -> MTLLoadAction {
        switch (loadOp) {
        case WGPULoadOp_Load:
            return MTLLoadAction.load
        case WGPULoadOp_Clear:
            return MTLLoadAction.clear
        case WGPULoadOp_Undefined:
            return MTLLoadAction.dontCare
        case WGPULoadOp_Force32:
            assertionFailure("ASSERT_NOT_REACHED")
            return MTLLoadAction.dontCare
        default:
            assertionFailure("ASSERT_NOT_REACHED")
            return MTLLoadAction.dontCare
            
        }
    }

    private func storeAction(storeOp: WGPUStoreOp, hasResolveTarget: Bool = false) -> MTLStoreAction {
        switch (storeOp) {
        case WGPUStoreOp_Store:
            return hasResolveTarget ? MTLStoreAction.storeAndMultisampleResolve : MTLStoreAction.store
        case WGPUStoreOp_Discard:
            return hasResolveTarget ? MTLStoreAction.multisampleResolve : MTLStoreAction.dontCare
        case WGPUStoreOp_Undefined:
            return hasResolveTarget ? MTLStoreAction.multisampleResolve : MTLStoreAction.dontCare
        case WGPUStoreOp_Force32:
            assertionFailure("ASSERT_NOT_REACHED")
            return MTLStoreAction.dontCare
        default:
            assertionFailure("ASSERT_NOT_REACHED")
            return MTLStoreAction.dontCare
        }
    }
    private func isMultisampleTexture(texture: MTLTexture) -> Bool {
        return texture.textureType == MTLTextureType.type2DMultisample || texture.textureType == MTLTextureType.type2DMultisampleArray
    }


    public func beginRenderPass(descriptor: WGPURenderPassDescriptor) -> WebGPU_Internal.RefRenderPassEncoder {
        var maxDrawCount = UInt64.max
        if descriptor.nextInChain != nil {
            if descriptor.nextInChain.pointee.sType != WGPUSType_RenderPassDescriptorMaxDrawCount {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "descriptor is corrupted")
            }
            descriptor.nextInChain.withMemoryRebound(to: WGPURenderPassDescriptorMaxDrawCount.self, capacity: 1) {
                maxDrawCount = $0.pointee.maxDrawCount
            }
        }

        guard prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "encoder state is not valid")
        }

        if let error = errorValidatingRenderPassDescriptor(descriptor) {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), error)
        }

        guard m_commandBuffer.status.rawValue < MTLCommandBufferStatus.enqueued.rawValue else {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "command buffer has already been committed")
        }

        let mtlDescriptor = MTLRenderPassDescriptor()
        var counterSampleBuffer: MTLCounterSampleBuffer? = nil
        if let wgpuTimestampWrites = descriptor.timestampWrites {
            counterSampleBuffer = WebGPU.fromAPI(wgpuTimestampWrites.pointee.querySet).counterSampleBuffer()
        }

        if (m_device.ptr().enableEncoderTimestamps() || counterSampleBuffer != nil) {
            if (counterSampleBuffer != nil) {
                mtlDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer;
                mtlDescriptor.sampleBufferAttachments[0].startOfVertexSampleIndex = Int(descriptor.timestampWrites.pointee.beginningOfPassWriteIndex)
                mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex = Int(descriptor.timestampWrites.pointee.endOfPassWriteIndex)
                mtlDescriptor.sampleBufferAttachments[0].startOfFragmentSampleIndex = Int(descriptor.timestampWrites.pointee.endOfPassWriteIndex)
                mtlDescriptor.sampleBufferAttachments[0].endOfFragmentSampleIndex = Int(descriptor.timestampWrites.pointee.endOfPassWriteIndex)
            } else {
                mtlDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer != nil ? counterSampleBuffer : m_device.ptr().timestampsBuffer(m_commandBuffer, 4)
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

        let attachmentsToClear = NSMutableDictionary()
        var zeroColorTargets = true
        var bytesPerSample: UInt32 = 0
        let maxColorAttachmentBytesPerSample = m_device.ptr().limitsCopy().maxColorAttachmentBytesPerSample
        var textureWidth: UInt32 = 0, textureHeight: UInt32  = 0, sampleCount: UInt32 = 0

        var depthSlices: [UnsafeRawPointer: Set<UInt64>] = [:]
        for i in 0..<descriptor.colorAttachmentsSpan().count {
            let attachment = descriptor.colorAttachmentsSpan()[i]

            if (attachment.view == nil) {
                continue
            }

            // MTLRenderPassColorAttachmentDescriptorArray is bounds-checked internally.
            let mtlAttachment = mtlDescriptor.colorAttachments[i]!;

            mtlAttachment.clearColor = MTLClearColorMake(attachment.clearValue.r,
                attachment.clearValue.g,
                attachment.clearValue.b,
                attachment.clearValue.a)

            let texture = WebGPU.fromAPI(attachment.view)
            if (!WebGPU_Internal.isValidToUseWithTextureViewCommandEncoder(texture, self)) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "device mismatch")
            }
            if (textureWidth != 0 && (texture.width() != textureWidth || texture.height() != textureHeight || sampleCount != texture.sampleCount())) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "texture size does not match")
            }

            textureWidth = texture.width()
            textureHeight = texture.height()
            sampleCount = texture.sampleCount()
            let textureFormat = texture.format()
            bytesPerSample = WebGPU_Internal.roundUpToMultipleOfNonPowerOfTwoUInt32UInt32(WebGPU.Texture.renderTargetPixelByteAlignment(textureFormat), bytesPerSample)
            bytesPerSample += WebGPU.Texture.renderTargetPixelByteCost(textureFormat)
            if (bytesPerSample > maxColorAttachmentBytesPerSample) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "total bytes per sample exceeds limit")
            }

            let textureIsDestroyed = texture.isDestroyed()
            if (!textureIsDestroyed) {
                if ((texture.usage() & WGPUTextureUsage_RenderAttachment.rawValue) == 0 || !WebGPU.Texture.isColorRenderableFormat(textureFormat, m_device.ptr())) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "color attachment is not renderable")
                }

                if (!isRenderableTextureView(texture: texture)) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "texture view is not renderable")
                }
            }
            texture.setCommandEncoder(self)

            let mtlTexture = texture.texture()
            mtlAttachment.texture = mtlTexture
            if (mtlAttachment.texture == nil) {
                if (!textureIsDestroyed) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "color attachment's texture is nil")
                }
                continue
            }
            mtlAttachment.level = 0
            mtlAttachment.slice = 0
            var depthSliceOrArrayLayer: UInt64 = 0
            let textureDimension = texture.dimension()
            if attachment.depthSlice.hasValue {
                if textureDimension != WGPUTextureViewDimension_3D {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depthSlice specified on 2D texture")
                }
                depthSliceOrArrayLayer = textureIsDestroyed ? 0 : UInt64(attachment.depthSlice.value!)
                if depthSliceOrArrayLayer >= texture.depthOrArrayLayers() {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depthSlice is greater than texture's depth or array layers")
                }
            } else {
                if textureDimension == WGPUTextureViewDimension_3D {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "textureDimension is 3D and no depth slice is specified")
                }
                depthSliceOrArrayLayer = UInt64(textureIsDestroyed ? 0 : texture.baseArrayLayer())
            }
            var bridgedTexture: UnsafeRawPointer? = nil
            withUnsafePointer(to: texture.parentTexture()) {
                bridgedTexture = UnsafeRawPointer($0)
            }
            guard bridgedTexture != nil else {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "parent texture is nil")
            }
            let baseMipLevel = textureIsDestroyed ? 0 : texture.baseMipLevel()
            //continue review here
            let depthAndMipLevel: UInt64 = depthSliceOrArrayLayer | (UInt64(baseMipLevel) << 32)
            if var setValue = depthSlices[bridgedTexture!] {
            //if (auto it = depthSlices.find(bridgedTexture); it != depthSlices.end()) {
                if depthSlices[bridgedTexture!]!.contains(depthAndMipLevel) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "attempting to render to overlapping color attachment")
                }
                depthSlices[bridgedTexture!]!.insert(depthAndMipLevel)
            } else {
                depthSlices[bridgedTexture!] = [ depthAndMipLevel ]
            }

            mtlAttachment.depthPlane = Int(textureDimension == WGPUTextureViewDimension_3D ? depthSliceOrArrayLayer : 0)
            mtlAttachment.slice = 0
            mtlAttachment.loadAction = loadAction(loadOp: attachment.loadOp)
            mtlAttachment.storeAction = storeAction(storeOp: attachment.storeOp, hasResolveTarget: attachment.resolveTarget != nil)

            zeroColorTargets = false
            var textureToClear: MTLTexture? = nil
            if mtlAttachment.loadAction == MTLLoadAction.load && !texture.previouslyCleared() {
                textureToClear = mtlAttachment.texture
            }

            if attachment.resolveTarget != nil {
                let resolveTarget = WebGPU.fromAPI(attachment.resolveTarget)
                if !WebGPU_Internal.isValidToUseWith_TextureView_CommandEncoder(resolveTarget, self) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "resolve target created from different device")
                }
                resolveTarget.setCommandEncoder(self)
                let resolveTexture = resolveTarget.texture()
                if (resolveTexture == nil || mtlTexture == nil) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "resolveTexture/mtlTexture is nil")
                }
                if mtlTexture!.sampleCount == 1 || resolveTexture!.sampleCount != 1 || isMultisampleTexture(texture: resolveTexture!) || !isMultisampleTexture(texture: mtlTexture!) || !isRenderableTextureView(texture: resolveTarget) || mtlTexture!.pixelFormat != resolveTexture!.pixelFormat || WebGPU.Texture.supportsResolve(resolveTarget.format(), m_device.ptr()) {
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
            if (textureToClear != nil) {
                let textureWithResolve = TextureAndClearColor(texture: textureToClear!)
                attachmentsToClear[i] = textureWithResolve
                if (textureToClear != nil) {
                    // FIXME: rdar://138042799 remove default argument.
                    texture.setPreviouslyCleared(0, 0)
                }
                if (attachment.resolveTarget != nil) {
                    // FIXME: rdar://138042799 remove default argument.
                    WebGPU.fromAPI(attachment.resolveTarget).setPreviouslyCleared(0, 0)
                }
            }

        }
        var depthReadOnly = false, stencilReadOnly = false
        var hasStencilComponent = false
        var depthStencilAttachmentToClear: MTLTexture? = nil
        var depthAttachmentToClear = false
        if let attachment = descriptor.depthStencilAttachment {
            let textureView = WebGPU.fromAPI(attachment.pointee.view)
            if (!WebGPU_Internal.isValidToUseWithTextureViewCommandEncoder(textureView, self)) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture device mismatch")
            }
            let metalDepthStencilTexture = textureView.texture()
            let textureFormat = textureView.format()
            hasStencilComponent = WebGPU.Texture.containsStencilAspect(textureFormat)
            let hasDepthComponent = WebGPU.Texture.containsDepthAspect(textureFormat)
            let isDestroyed = textureView.isDestroyed()
            if !isDestroyed {
                if textureWidth != 0 && (textureView.width() != textureWidth || textureView.height() != textureHeight || sampleCount != textureView.sampleCount()) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture dimensions mismatch")
                }
                if textureView.arrayLayerCount() > 1 || textureView.mipLevelCount() > 1 {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture has more than one array layer or mip level")
                }

                if !WebGPU.Texture.isDepthStencilRenderableFormat(textureView.format(), m_device.ptr()) || !isRenderableTextureView(texture: textureView) {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth stencil texture is not renderable")
                }
            }

            depthReadOnly = attachment.pointee.depthReadOnly != 0
            if hasDepthComponent {
                let mtlAttachment = mtlDescriptor.depthAttachment
                let clearDepth: Double = WebGPU_Internal.clampDouble(WebGPU.RenderPassEncoder.quantizedDepthValue(Double(attachment.pointee.depthClearValue), textureView.format()), 0.0, 1.0)
                mtlAttachment!.clearDepth = attachment.pointee.depthLoadOp == WGPULoadOp_Clear ? clearDepth : 1.0
                mtlAttachment!.texture = metalDepthStencilTexture
                mtlAttachment!.level = 0
                mtlAttachment!.loadAction = loadAction(loadOp: attachment.pointee.depthLoadOp)
                mtlAttachment!.storeAction = storeAction(storeOp: attachment.pointee.depthStoreOp)

                if mtlAttachment!.loadAction == MTLLoadAction.load && mtlAttachment!.storeAction == MTLStoreAction.dontCare && !textureView.previouslyCleared() {
                    depthStencilAttachmentToClear = mtlAttachment!.texture
                    depthAttachmentToClear = mtlAttachment!.texture != nil
                }
            }

            if !isDestroyed {
                if hasDepthComponent && !depthReadOnly {
                    if attachment.pointee.depthLoadOp == WGPULoadOp_Undefined || attachment.pointee.depthStoreOp == WGPUStoreOp_Undefined {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth load and store op were not specified")
                    }
                } else if attachment.pointee.depthLoadOp != WGPULoadOp_Undefined || attachment.pointee.depthStoreOp != WGPUStoreOp_Undefined {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth load and store op were specified")
                }
            }

            if attachment.pointee.depthLoadOp == WGPULoadOp_Clear && (attachment.pointee.depthClearValue < 0 || attachment.pointee.depthClearValue > 1) {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "depth clear value is invalid")
            }

            if zeroColorTargets {
                mtlDescriptor.defaultRasterSampleCount = metalDepthStencilTexture!.sampleCount
                if mtlDescriptor.defaultRasterSampleCount == 0 {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "no color targets and depth-stencil texture is nil")
                }
                mtlDescriptor.renderTargetWidth = metalDepthStencilTexture!.width
                mtlDescriptor.renderTargetHeight = metalDepthStencilTexture!.height
            }
        }

        var stencilAttachmentToClear = false
        if let attachment = descriptor.depthStencilAttachment {
            let mtlAttachment = mtlDescriptor.stencilAttachment
            stencilReadOnly = attachment.pointee.stencilReadOnly != 0
            let textureView = WebGPU.fromAPI(attachment.pointee.view)
            if hasStencilComponent {
                mtlAttachment!.texture = textureView.texture()
            }
            mtlAttachment!.clearStencil = attachment.pointee.stencilClearValue
            mtlAttachment!.loadAction = loadAction(loadOp: attachment.pointee.stencilLoadOp)
            mtlAttachment!.storeAction = storeAction(storeOp: attachment.pointee.stencilStoreOp)
            let isDestroyed = textureView.isDestroyed()
            if !isDestroyed {
                if hasStencilComponent && !stencilReadOnly {
                    if attachment.pointee.stencilLoadOp == WGPULoadOp_Undefined || attachment.pointee.stencilStoreOp == WGPUStoreOp_Undefined {
                        return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "stencil load and store op were not specified")
                    }
                } else if attachment.pointee.stencilLoadOp != WGPULoadOp_Undefined || attachment.pointee.stencilStoreOp != WGPUStoreOp_Undefined {
                    return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "stencil load and store op were specified")
                }
            }

            textureView.setCommandEncoder(self)

            if hasStencilComponent && mtlAttachment!.loadAction == MTLLoadAction.load && mtlAttachment!.storeAction == MTLStoreAction.dontCare && !textureView.previouslyCleared() {
                depthStencilAttachmentToClear = mtlAttachment!.texture
                stencilAttachmentToClear = mtlAttachment!.texture != nil
            }
        }

        if zeroColorTargets && mtlDescriptor.renderTargetWidth == 0 {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "zero color and depth targets")
        }

        var visibilityResultBufferSize: UInt = 0
        var visibilityResultBuffer: MTLBuffer? = nil
        if let wgpuOcclusionQuery = descriptor.occlusionQuerySet {
            let occlusionQuery = WebGPU.fromAPI(wgpuOcclusionQuery)
            occlusionQuery.setCommandEncoder(self)
            if occlusionQuery.type() != WGPUQueryType_Occlusion {
                return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "querySet for occlusion query was not of type occlusion")
            }
            mtlDescriptor.visibilityResultBuffer = occlusionQuery.visibilityBuffer()
            visibilityResultBuffer = mtlDescriptor.visibilityResultBuffer
            visibilityResultBufferSize = occlusionQuery.isDestroyed() ? UInt.max : UInt(occlusionQuery.visibilityBuffer().length)
        }

        if (attachmentsToClear.count != 0 || depthStencilAttachmentToClear != nil) {
            let attachment = descriptor.depthStencilAttachment
            if attachment != nil && depthStencilAttachmentToClear != nil {
                // FIXME: rdar://138042799 remove default argument.
                WebGPU.fromAPI(attachment.pointee.view).setPreviouslyCleared(0, 0)
            }
            // FIXME: rdar://138042799 remove default argument.
            runClearEncoder(attachmentsToClear, depthStencilAttachmentToClear, depthAttachmentToClear, stencilAttachmentToClear, 0 ,0 ,nil)
        }

        if !m_device.ptr().isValid() {
            return WebGPU.RenderPassEncoder.createInvalid(self, m_device.ptr(), "GPUDevice was invalid, this will be an error submitting the command buffer")
        }

        let mtlRenderCommandEncoder = m_commandBuffer.makeRenderCommandEncoder(descriptor: mtlDescriptor)
        if (m_existingCommandEncoder != nil)  {
            assertionFailure("!m_existingCommandEncoder")
        }
        setExistingEncoder(mtlRenderCommandEncoder)
        return WebGPU.RenderPassEncoder.create(mtlRenderCommandEncoder, descriptor, visibilityResultBufferSize, depthReadOnly, stencilReadOnly, self, visibilityResultBuffer, maxDrawCount, m_device.ptr(), mtlDescriptor)

    }
    static func hasValidDimensions(dimension: WGPUTextureDimension, width: UInt, height: UInt, depth: UInt) -> Bool {
        switch (dimension.rawValue) {
            case WGPUTextureDimension_1D.rawValue:
                return width != 0
            case WGPUTextureDimension_2D.rawValue:
                return width != 0 && height != 0
            case WGPUTextureDimension_3D.rawValue:
                return width != 0 && height != 0 && depth != 0
            default:
                return true
        }
    }

    public func copyBufferToBuffer(source: WebGPU.Buffer, sourceOffset: UInt64, destination: WebGPU.Buffer, destinationOffset: UInt64, size: UInt64) {
        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertobuffer
        guard prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        let error = self.errorValidatingCopyBufferToBuffer(source, sourceOffset, destination, destinationOffset, size)
        guard error != nil else {
            self.makeInvalid(error)
            return
        }

        // FIXME: rdar://138042799 remove default argument.
        source.setCommandEncoder(self, false)
        destination.setCommandEncoder(self, false)
        destination.indirectBufferInvalidated()
        guard size != 0, !source.isDestroyed() && !destination.isDestroyed() else {
            return
        }

        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        blitCommandEncoder.copy(from: source.buffer(), sourceOffset: Int(sourceOffset), to: destination.buffer(), destinationOffset: Int(destinationOffset), size: Int(size))
    }

    public func copyTextureToBuffer(source: WGPUImageCopyTexture, destination: WGPUImageCopyBuffer, copySize: WGPUExtent3D) {
        guard source.nextInChain == nil && destination.nextInChain == nil && destination.layout.nextInChain == nil else {
            return
        }

        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetobuffer

        guard prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }

        let sourceTexture = WebGPU.fromAPI(source.texture);
        let error = self.errorValidatingCopyTextureToBuffer(source, destination, copySize)
        guard error != nil else {
            self.makeInvalid(error)
            return
        }

        let apiDestinationBuffer = WebGPU.fromAPI(destination.buffer)
        sourceTexture.setCommandEncoder(self)
        apiDestinationBuffer.setCommandEncoder(self, false)
        apiDestinationBuffer.indirectBufferInvalidated()
        guard !sourceTexture.isDestroyed() && !apiDestinationBuffer.isDestroyed() else {
            return
        }

        var options = MTLBlitOption(rawValue: MTLBlitOptionNone.rawValue)
        switch (source.aspect.rawValue) {
        case WGPUTextureAspect_All.rawValue:
            break
        case WGPUTextureAspect_StencilOnly.rawValue:
            options = MTLBlitOption.stencilFromDepthStencil
        case WGPUTextureAspect_DepthOnly.rawValue:
            options = MTLBlitOption.depthFromDepthStencil
        case WGPUTextureAspect_Force32.rawValue:
            return
        default:
            return
        }

        let logicalSize = sourceTexture.logicalMiplevelSpecificTextureExtent(source.mipLevel)
        let widthForMetal = logicalSize.width < source.origin.x ? 0 : min(copySize.width, logicalSize.width - source.origin.x)
        let heightForMetal = logicalSize.height < source.origin.y ? 0 : min(copySize.height, logicalSize.height - source.origin.y)
        let depthForMetal = logicalSize.depthOrArrayLayers < source.origin.z ? 0 : min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers - source.origin.z)

        guard let destinationBuffer = apiDestinationBuffer.buffer() else {
            return
        }
        var destinationBytesPerRow = UInt(destination.layout.bytesPerRow)
        if (destinationBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED) {
            destinationBytesPerRow = UInt(destinationBuffer.length)
        }

        let sourceTextureFormat = sourceTexture.format()
        let aspectSpecificFormat = WebGPU.Texture.aspectSpecificFormat(sourceTextureFormat, source.aspect)
        let blockSize = WebGPU.Texture.texelBlockSize(aspectSpecificFormat)
        let textureDimension = sourceTexture.dimension()
        var didOverflow: Bool
        switch (textureDimension.rawValue) {
            case WGPUTextureDimension_1D.rawValue:
                if !blockSize.hasOverflowed() {
                    var product: UInt32 = blockSize.value()
                    (product, didOverflow) = product.multipliedReportingOverflow(by: self.m_device.ptr().limitsCopy().maxTextureDimension1D)
                    if !didOverflow {
                        destinationBytesPerRow = min(destinationBytesPerRow, UInt(product))
                    }
                }
            case WGPUTextureDimension_2D.rawValue, WGPUTextureDimension_3D.rawValue:
                if !blockSize.hasOverflowed() {
                    var product: UInt32 = blockSize.value()
                    (product, didOverflow) = product.multipliedReportingOverflow(by: self.m_device.ptr().limitsCopy().maxTextureDimension2D)
                    if !didOverflow {
                        destinationBytesPerRow = min(destinationBytesPerRow, UInt(product))
                    }
                }
            case WGPUTextureDimension_Force32.rawValue:
                break
            default:
                break
        }

        destinationBytesPerRow = WebGPU_Internal.roundUpToMultipleOfNonPowerOfTwoCheckedUInt32UnsignedLong(blockSize, destinationBytesPerRow)
        if textureDimension.rawValue == WGPUTextureDimension_3D.rawValue && copySize.depthOrArrayLayers <= 1 && copySize.height <= 1 {
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

        let maxDestinationBytesPerRow: UInt = textureDimension.rawValue == WGPUTextureDimension_3D.rawValue ? (2048 * blockSize.value()) : destinationBytesPerRow
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
                (zTimesDestinationBytesPerImage, didOverflow) = zTimesDestinationBytesPerImage.multipliedReportingOverflow(by: UInt32(destinationBytesPerImage))
                guard !didOverflow else {
                    return
                }
                for y in 0..<copySize.height {
                    var yPlusOriginY = source.origin.y
                    (yPlusOriginY, didOverflow) = yPlusOriginY.addingReportingOverflow(y)
                    guard !didOverflow else {
                        return
                    }
                    var yTimesDestinationBytesPerImage = y
                    guard destinationBytesPerImage <= UInt32.max else {
                        return
                    }
                    (yTimesDestinationBytesPerImage, didOverflow) = yTimesDestinationBytesPerImage.multipliedReportingOverflow(by: UInt32(destinationBytesPerImage))
                    guard !didOverflow else {
                        return
                    }
                    let newSource = WGPUImageCopyTexture(
                        nextInChain: nil,
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
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(yTimesDestinationBytesPerImage))
                    guard !didOverflow else {
                        return
                    }
                    let newDestination = WGPUImageCopyBuffer(
                        nextInChain: nil,
                        layout: WGPUTextureDataLayout(
                            nextInChain: nil,
                            offset: tripleSum,
                            bytesPerRow: UInt32(WGPU_COPY_STRIDE_UNDEFINED),
                            rowsPerImage: UInt32(WGPU_COPY_STRIDE_UNDEFINED)
                        ),
                        buffer: destination.buffer
                    )
                    self.copyTextureToBuffer(source: newSource, destination: newDestination, copySize: WGPUExtent3D(
                        width: copySize.width,
                        height: 1,
                        depthOrArrayLayers: 1
                    ))
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
            let sourceSlice = sourceTexture.dimension().rawValue == WGPUTextureDimension_3D.rawValue ? 0 : originZPlusLayer
            if !sourceTexture.previouslyCleared(source.mipLevel, UInt32(sourceSlice)) {
                clearTextureIfNeeded(source, sourceSlice)
            }
        }

        guard Self.hasValidDimensions(dimension: sourceTexture.dimension(), width: UInt(widthForMetal), height: UInt(heightForMetal), depth: UInt(depthForMetal)) else {
            return
        }

        guard destinationBuffer.length >= WebGPU.Texture.bytesPerRow(aspectSpecificFormat, widthForMetal, sourceTexture.sampleCount()) else {
            return
        }

        switch (sourceTexture.dimension()) {
            case WGPUTextureDimension_1D:
                // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
                // "When you copy to a 1D texture, height and depth must be 1."
                let sourceSize = MTLSizeMake(Int(widthForMetal), 1, 1)
                let sourceOrigin = MTLOriginMake(Int(source.origin.x), 0, 0)
                for layer in 0..<copySize.depthOrArrayLayers {
                    var layerTimesDestinationBytesPerImage = UInt(layer)
                    (layerTimesDestinationBytesPerImage, didOverflow) = layerTimesDestinationBytesPerImage.multipliedReportingOverflow(by: destinationBytesPerImage)
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
                    let sum = UInt(destinationOffset) + UInt(widthTimesBlockSize)
                    if sum > destinationBuffer.length {
                        continue
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
                        options: options)
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
                        options: options)
                }
            case WGPUTextureDimension_3D:
                let sourceSize = MTLSizeMake(Int(widthForMetal), Int(heightForMetal), Int(depthForMetal))
                let sourceOrigin = MTLOriginMake(Int(source.origin.x), Int(source.origin.y), Int(source.origin.z))
                let destinationOffset = UInt(destination.layout.offset)
                blitCommandEncoder.copy(
                    from: sourceTexture.texture(),
                    sourceSlice: 0,
                    sourceLevel: Int(source.mipLevel),
                    sourceOrigin: sourceOrigin,
                    sourceSize: sourceSize,
                    to: destinationBuffer,
                    destinationOffset: Int(destinationOffset),
                    destinationBytesPerRow: Int(destinationBytesPerRow),
                    destinationBytesPerImage: Int(destinationBytesPerImage),
                    options: options)
            case WGPUTextureDimension_Force32:
                return
            default:
                return
        }
    }

    public func copyBufferToTexture(source: WGPUImageCopyBuffer, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
        guard source.nextInChain == nil && source.layout.nextInChain == nil && destination.nextInChain == nil else {
            return
        }
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        let destinationTexture = WebGPU.fromAPI(destination.texture)

        let error = self.errorValidatingCopyBufferToTexture(source, destination, copySize)
        guard error != nil else {
            self.makeInvalid(error)
            return
        }
        let apiBuffer = WebGPU.fromAPI(source.buffer)
        apiBuffer.setCommandEncoder(self, false) 
        destinationTexture.setCommandEncoder(self)
        guard copySize.width != 0 || copySize.height != 0 || copySize.depthOrArrayLayers != 0, !apiBuffer.isDestroyed(), !destinationTexture.isDestroyed() else {
            return
        }
        guard let blitCommandEncoder = self.ensureBlitCommandEncoder() else {
            return 
        }
        var sourceBytesPerRow: UInt = UInt(source.layout.bytesPerRow)
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
                        sourceBytesPerRow = UInt(result)
                    }
                }
            case WGPUTextureDimension_2D, WGPUTextureDimension_3D:
                if !blockSize.hasOverflowed() {
                    // swift cannot infer .value()'s type
                    let blockSizeValue: UInt32 = blockSize.value()
                    let (result, didOverflow) = blockSizeValue.multipliedReportingOverflow(by: device.limitsCopy().maxTextureDimension2D)
                    if !didOverflow {
                        sourceBytesPerRow = UInt(result)
                    }
                }
            case WGPUTextureDimension_Force32:
                break
            default:
                break
        }

        var options: MTLBlitOption = MTLBlitOption(rawValue: MTLBlitOptionNone.rawValue)
        switch destination.aspect {
            case WGPUTextureAspect_StencilOnly:
                options = MTLBlitOption.stencilFromDepthStencil
            case WGPUTextureAspect_DepthOnly:
                options = MTLBlitOption.depthFromDepthStencil
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
        let depthForMetal = logicalSize.depthOrArrayLayers < destination.origin.z ? 0 : min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers - destination.origin.z)
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

        let sliceCount: UInt32 = textureDimension.rawValue == WGPUTextureDimension_3D.rawValue ? 1 : copySize.depthOrArrayLayers
        for layer in 0..<sliceCount {
            var originPlusLayer = destination.origin.z
            (originPlusLayer, didOverflow) = originPlusLayer.addingReportingOverflow(layer)
            if didOverflow {
                return
            }
            let destinationSlice = destinationTexture.dimension().rawValue == WGPUTextureDimension_3D.rawValue ? 0 : originPlusLayer
            precondition(mtlDestinationTexture != nil, "mtlDestinationTexture is nil")
            precondition(mtlDestinationTexture!.parent == nil, "mtlDestinationTexture.parentTexture is not nil")
            if WebGPU.Queue.writeWillCompletelyClear(textureDimension, widthForMetal, logicalSize.width, heightForMetal, logicalSize.height, depthForMetal, logicalSize.depthOrArrayLayers) {
                // FIXME: rdar://138042799 remove default argument.
                destinationTexture.setPreviouslyCleared(destination.mipLevel, destinationSlice, true)
            } else {
                self.clearTextureIfNeeded(destination, UInt(destinationSlice))
            }
        }
        let maxSourceBytesPerRow: UInt = textureDimension.rawValue == WGPUTextureDimension_3D.rawValue ? (2048 * blockSize.value()) : sourceBytesPerRow
        if textureDimension.rawValue == WGPUTextureDimension_3D.rawValue && copySize.depthOrArrayLayers <= 1 && copySize.height <= 1 {
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
                guard let sourceBytesPerRowU32 = UInt32(exactly: sourceBytesPerRow) else {
                    return
                }
                (zTimesSourceBytesPerImage, didOverflow) = zTimesSourceBytesPerImage.multipliedReportingOverflow(by: sourceBytesPerRowU32)
                guard !didOverflow else {
                    return
                }
                for y in 0..<copySize.height {
                    var yTimesSourceBytesPerImage = y
                    (yTimesSourceBytesPerImage, didOverflow) = yTimesSourceBytesPerImage.multipliedReportingOverflow(by: sourceBytesPerRowU32)
                    guard !didOverflow else {
                        return
                    }
                    var tripleSum = UInt64(zTimesSourceBytesPerImage)
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(yTimesSourceBytesPerImage))
                    guard !didOverflow else {
                        return
                    }
                    (tripleSum, didOverflow) = tripleSum.addingReportingOverflow(UInt64(source.layout.offset))
                    guard !didOverflow else {
                        return
                    }
                    let newSource = WGPUImageCopyBuffer(
                        nextInChain: nil,
                        layout: WGPUTextureDataLayout(
                            nextInChain: nil,
                            offset: tripleSum,
                            bytesPerRow: UInt32(WGPU_COPY_STRIDE_UNDEFINED),
                            rowsPerImage: UInt32(WGPU_COPY_STRIDE_UNDEFINED)
                        ),
                        buffer: source.buffer)
                    var destinationOriginPlusY = y
                    (destinationOriginPlusY, didOverflow) = destinationOriginPlusY.addingReportingOverflow(destination.origin.y)
                    guard !didOverflow else {
                        return
                    }
                    let newDestination = WGPUImageCopyTexture(
                        nextInChain: nil,
                        texture: destination.texture,
                        mipLevel: destination.mipLevel,
                        origin: WGPUOrigin3D(
                            x: destination.origin.x,
                            y: destinationOriginPlusY,
                            z: destinationOriginPlusZ),
                        aspect: destination.aspect
                    )
                    self.copyBufferToTexture(source: newSource, destination: newDestination, copySize: WGPUExtent3D(width: copySize.width, height: 1, depthOrArrayLayers: 1))
                }
            }
            return
        }
        guard sourceBuffer.length >= WebGPU.Texture.bytesPerRow(aspectSpecificFormat, widthForMetal, destinationTexture.sampleCount()) else {
            return
        }
        switch destinationTexture.dimension() {
            case WGPUTextureDimension_1D:
                // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
                // "When you copy to a 1D texture, height and depth must be 1."
                let sourceSize = MTLSizeMake(Int(widthForMetal), 1, 1);
                guard widthForMetal != 0 else {
                    return
                }

                let destinationOrigin = MTLOriginMake(Int(destination.origin.x), 0, 0);
                var widthTimesBlockSize: UInt32 = widthForMetal
                (widthTimesBlockSize, didOverflow) = widthTimesBlockSize.multipliedReportingOverflow(by: blockSize.value())
                guard !didOverflow else {
                    return
                }
                sourceBytesPerRow = min(sourceBytesPerRow, UInt(widthTimesBlockSize));
                for layer in 0..<copySize.depthOrArrayLayers {
                    var layerTimesSourceBytesPerImage = UInt(layer)
                    (layerTimesSourceBytesPerImage, didOverflow) = layerTimesSourceBytesPerImage.multipliedReportingOverflow(by: sourceBytesPerImage)
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
                    (sourceOffsetPlusSourceBytesPerRow, didOverflow) = sourceOffsetPlusSourceBytesPerRow.addingReportingOverflow(sourceBytesPerRow)
                    guard !didOverflow else {
                        return
                    }
                    guard sourceOffsetPlusSourceBytesPerRow <= sourceBuffer.length else {
                        return
                    }
                    blitCommandEncoder.copy(
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
                let sourceSize = MTLSizeMake(Int(widthForMetal), Int(heightForMetal), 1);
                guard widthForMetal != 0 && heightForMetal != 0 else {
                    return
                }

                let destinationOrigin = MTLOriginMake(Int(destination.origin.x), Int(destination.origin.y), 0);
                for layer in 0..<copySize.depthOrArrayLayers {
                    var layerTimesSourceBytesPerImage = UInt(layer)
                    (layerTimesSourceBytesPerImage, didOverflow) = layerTimesSourceBytesPerImage.addingReportingOverflow(sourceBytesPerImage)
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
                    blitCommandEncoder.copy(
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
                let sourceSize = MTLSizeMake(Int(widthForMetal), Int(heightForMetal), Int(depthForMetal));
                guard widthForMetal != 0 && heightForMetal != 0 && depthForMetal != 0 else {
                    return
                }

                let destinationOrigin = MTLOriginMake(Int(destination.origin.x), Int(destination.origin.y), Int(destination.origin.z))
                let sourceOffset = UInt(source.layout.offset)
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
                assertionFailure("ASSERT_NOT_REACHED")
                return
            default:
                assertionFailure("ASSERT_NOT_REACHED")
                return
        }

    }

    public func clearBuffer(buffer: WebGPU.Buffer, offset: UInt64, size: inout UInt64) {
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        if size == UInt64.max {
            let initialSize = buffer.initialSize()
            let (subtractionResult, didOverflow) = initialSize.subtractingReportingOverflow(offset)
            if didOverflow {
                self.m_device.ptr().generateAValidationError(
                    "CommandEncoder::clearBuffer(): offset > buffer.size")
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
        buffer.indirectBufferInvalidated()
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

    public func resolveQuerySet(_ querySet: WebGPU.QuerySet, firstQuery: UInt32, queryCount: UInt32, destination: WebGPU.Buffer, destinationOffset:UInt64)
    {
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        guard self.validateResolveQuerySet(querySet: querySet, firstQuery: firstQuery, queryCount: queryCount, destination: destination, destinationOffset: destinationOffset) else {
            self.makeInvalid("GPUCommandEncoder.resolveQuerySet validation failed")
            return
        }
        querySet.setCommandEncoder(self)
        // FIXME: rdar://138042799 need to pass in the default argument.
        destination.setCommandEncoder(self, false)
        destination.indirectBufferInvalidated();
        guard !(querySet.isDestroyed() || destination.isDestroyed() || queryCount == 0) else {
            return
        }
        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        if querySet.type().rawValue == WGPUQueryType_Occlusion.rawValue {
            guard let sourceOffset = Int(exactly: 8 * firstQuery), let destinationOffsetChecked = Int(exactly: destinationOffset), let size = Int(exactly: 8 * queryCount) else {
                return
            }
            blitCommandEncoder.copy(from: querySet.visibilityBuffer(), sourceOffset: sourceOffset, to: destination.buffer(), destinationOffset: destinationOffsetChecked, size: size)
        }
    }
    public func validateResolveQuerySet(querySet: WebGPU.QuerySet, firstQuery: UInt32, queryCount: UInt32, destination: WebGPU.Buffer, destinationOffset: UInt64) -> Bool
    {
        guard (destinationOffset % 256) == 0 else {
            return false
        }
        guard querySet.isDestroyed() || querySet.isValid() else {
            return false
        }
        guard destination.isDestroyed() || destination.isValid() else {
            return false
        }
        guard (destination.usage() & WGPUBufferUsage_QueryResolve.rawValue) != 0 else {
            return false
        }
        guard firstQuery < querySet.count() else {
            return false
        }
        let countEnd: UInt32 = firstQuery
        var (additionResult, didOverflow) = countEnd.addingReportingOverflow(queryCount)
        guard !didOverflow && additionResult <= querySet.count() else {
            return false
        }
        let countTimes8PlusOffset = destinationOffset
        let additionResult64: UInt64
        (additionResult64, didOverflow) = countTimes8PlusOffset.addingReportingOverflow(8 * UInt64(queryCount))
        guard !didOverflow && destination.initialSize() >= additionResult64 else {
            return false
        }
        return true
    }

    public func copyTextureToTexture(source: WGPUImageCopyTexture, destination: WGPUImageCopyTexture, copySize: WGPUExtent3D) {
        guard source.nextInChain == nil, destination.nextInChain == nil else {
            return
        }

        // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetotexture

        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        let error = self.errorValidatingCopyTextureToTexture(source, destination, copySize)
        guard error == nil else {
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
        let sliceCount: UInt32 = destinationTextureDimension == WGPUTextureDimension_3D ? 1 : copySize.depthOrArrayLayers
        let destinationLogicalSize = destinationTexture.logicalMiplevelSpecificTextureExtent(destination.mipLevel)
        var didOverflow: Bool
        for layer in 0..<sliceCount {

            var sourceOriginPlusLayer = UInt(source.origin.z)
            (sourceOriginPlusLayer, didOverflow) = sourceOriginPlusLayer.addingReportingOverflow(UInt(layer))
            guard !didOverflow else {
                return
            }
            let sourceSlice: UInt = sourceTexture.dimension() == WGPUTextureDimension_3D ? 0 : sourceOriginPlusLayer
            self.clearTextureIfNeeded(source, sourceSlice)
            var destinationOriginPlusLayer = UInt(destination.origin.z)
            (destinationOriginPlusLayer, didOverflow) = destinationOriginPlusLayer.addingReportingOverflow(UInt(layer))
            guard !didOverflow else {
                return
            }
            let destinationSlice: UInt = destinationTexture.dimension() == WGPUTextureDimension_3D ? 0 : destinationOriginPlusLayer
            if WebGPU.Queue.writeWillCompletelyClear(destinationTextureDimension, copySize.width, destinationLogicalSize.width, copySize.height, destinationLogicalSize.height, copySize.depthOrArrayLayers, destinationLogicalSize.depthOrArrayLayers) {
                guard let destinationSliceUInt32 = UInt32(exactly: destinationSlice) else {
                    return
                }
                // FIXME: rdar://138042799 remove default argument.
                destinationTexture.setPreviouslyCleared(destination.mipLevel, destinationSliceUInt32, true)
            } else {
                self.clearTextureIfNeeded(destination, destinationSlice)
            }
        }
        guard let mtlDestinationTexture = destinationTexture.texture(), let mtlSourceTexture = sourceTexture.texture() else {
            return
        }

        // FIXME(PERFORMANCE): Is it actually faster to use the -[MTLBlitCommandEncoder copyFromTexture:...toTexture:...levelCount:]
        // variant, where possible, rather than calling the other variant in a loop?
        switch (sourceTexture.dimension()) {
        case WGPUTextureDimension_1D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
            // "When you copy to a 1D texture, height and depth must be 1."
            let sourceSize = MTLSizeMake(Int(copySize.width), 1, 1)
            guard sourceSize.width != 0 else {
                return
            }

            let sourceOrigin = MTLOriginMake(Int(source.origin.x), 0, 0)
            let destinationOrigin = MTLOriginMake(Int(destination.origin.x), 0, 0)
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
        case WGPUTextureDimension_2D:
            // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
            // "When you copy to a 2D texture, depth must be 1."
            let sourceSize = MTLSizeMake(Int(copySize.width), Int(copySize.height), 1)
            guard sourceSize.width != 0, sourceSize.height != 0 else {
                return
            }

            let sourceOrigin = MTLOriginMake(Int(source.origin.x), Int(source.origin.y), 0)
            let destinationOrigin = MTLOriginMake(Int(destination.origin.x), Int(destination.origin.y), 0)

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
            let sourceSize = MTLSizeMake(Int(copySize.width), Int(copySize.height), Int(copySize.depthOrArrayLayers))
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
                self.makeInvalid("GPUCommandEncoder.copyTextureToTexture: destination.origin.z + sourceSize.depth > destinationLogicalSize.depthOrArrayLayers")
                return
            }

            let sourceOrigin = MTLOriginMake(Int(source.origin.x), Int(source.origin.y), Int(source.origin.z))
            let destinationOrigin = MTLOriginMake(Int(destination.origin.x), Int(destination.origin.y), Int(destination.origin.z))
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

    public func beginComputePass(descriptor: WGPUComputePassDescriptor) -> WebGPU_Internal.RefComputePassEncoder {
        guard descriptor.nextInChain == nil else {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "descriptor is corrupted")
        }

        guard prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "encoder state is invalid")
        }

        let error = self.errorValidatingComputePassDescriptor(descriptor)
        guard error == nil else {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), error)
        }

        guard m_commandBuffer.status.rawValue < MTLCommandBufferStatus.enqueued.rawValue else {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "command buffer has already been committed")
        }

        self.finalizeBlitCommandEncoder()

        guard m_device.ptr().isValid() else {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "GPUDevice was invalid, this will be an error submitting the command buffer")
        }
        let computePassDescriptor = MTLComputePassDescriptor()
        computePassDescriptor.dispatchType = MTLDispatchType.serial
        var counterSampleBuffer: MTLCounterSampleBuffer? = nil
        if let wgpuTimestampWrites = descriptor.timestampWrites {
            counterSampleBuffer = WebGPU.fromAPI(wgpuTimestampWrites.pointee.querySet).counterSampleBuffer()
        }

        if m_device.ptr().enableEncoderTimestamps() || counterSampleBuffer != nil {
            let timestampWrites = descriptor.timestampWrites
            computePassDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer != nil ? computePassDescriptor.sampleBufferAttachments[0].sampleBuffer : m_device.ptr().timestampsBuffer(m_commandBuffer, 2)
            computePassDescriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = timestampWrites != nil ? Int(timestampWrites.pointee.beginningOfPassWriteIndex) : 0
            computePassDescriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = timestampWrites != nil ? Int(timestampWrites.pointee.endOfPassWriteIndex) : 1
        }
        guard let computeCommandEncoder = m_commandBuffer.makeComputeCommandEncoder(descriptor: computePassDescriptor) else {
            return WebGPU.ComputePassEncoder.createInvalid(self, m_device.ptr(), "computeCommandEncoder is null")
        }

        self.setExistingEncoder(computeCommandEncoder)
        // FIXME: Figure out a way so that WTFString does not override Swift.String in the global
        //        namespace. At the moment it is and that's why we need this.
        computeCommandEncoder.label = Swift.String(cString: descriptor.label)

        return WebGPU.ComputePassEncoder.create(computeCommandEncoder, descriptor, self, m_device.ptr())

    }
}
