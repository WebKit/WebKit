set(PAL_PUBLIC_HEADERS
    ExportMacros.h
    FileSizeFormatter.h
    Gunzip.h
    HysteresisActivity.h
    Logging.h
    SessionID.h

    crypto/CryptoDigest.h

    graphics/WebGPU/WebGPU.h
    graphics/WebGPU/WebGPUAdapter.h
    graphics/WebGPU/WebGPUAddressMode.h
    graphics/WebGPU/WebGPUBindGroup.h
    graphics/WebGPU/WebGPUBindGroupDescriptor.h
    graphics/WebGPU/WebGPUBindGroupEntry.h
    graphics/WebGPU/WebGPUBindGroupLayout.h
    graphics/WebGPU/WebGPUBindGroupLayoutDescriptor.h
    graphics/WebGPU/WebGPUBindGroupLayoutEntry.h
    graphics/WebGPU/WebGPUBlendComponent.h
    graphics/WebGPU/WebGPUBlendFactor.h
    graphics/WebGPU/WebGPUBlendOperation.h
    graphics/WebGPU/WebGPUBlendState.h
    graphics/WebGPU/WebGPUBuffer.h
    graphics/WebGPU/WebGPUBufferBinding.h
    graphics/WebGPU/WebGPUBufferBindingLayout.h
    graphics/WebGPU/WebGPUBufferBindingType.h
    graphics/WebGPU/WebGPUBufferDescriptor.h
    graphics/WebGPU/WebGPUBufferUsage.h
    graphics/WebGPU/WebGPUCanvasCompositingAlphaMode.h
    graphics/WebGPU/WebGPUCanvasConfiguration.h
    graphics/WebGPU/WebGPUColor.h
    graphics/WebGPU/WebGPUColorTargetState.h
    graphics/WebGPU/WebGPUColorWrite.h
    graphics/WebGPU/WebGPUCommandBuffer.h
    graphics/WebGPU/WebGPUCommandBufferDescriptor.h
    graphics/WebGPU/WebGPUCommandEncoder.h
    graphics/WebGPU/WebGPUCommandEncoderDescriptor.h
    graphics/WebGPU/WebGPUCompareFunction.h
    graphics/WebGPU/WebGPUCompilationInfo.h
    graphics/WebGPU/WebGPUCompilationMessage.h
    graphics/WebGPU/WebGPUCompilationMessageType.h
    graphics/WebGPU/WebGPUComputePassDescriptor.h
    graphics/WebGPU/WebGPUComputePassEncoder.h
    graphics/WebGPU/WebGPUComputePassTimestampLocation.h
    graphics/WebGPU/WebGPUComputePassTimestampWrites.h
    graphics/WebGPU/WebGPUComputePipeline.h
    graphics/WebGPU/WebGPUComputePipelineDescriptor.h
    graphics/WebGPU/WebGPUCullMode.h
    graphics/WebGPU/WebGPUDepthStencilState.h
    graphics/WebGPU/WebGPUDevice.h
    graphics/WebGPU/WebGPUDeviceDescriptor.h
    graphics/WebGPU/WebGPUDeviceLostInfo.h
    graphics/WebGPU/WebGPUDeviceLostReason.h
    graphics/WebGPU/WebGPUError.h
    graphics/WebGPU/WebGPUErrorFilter.h
    graphics/WebGPU/WebGPUExtent3D.h
    graphics/WebGPU/WebGPUExternalTexture.h
    graphics/WebGPU/WebGPUExternalTextureBindingLayout.h
    graphics/WebGPU/WebGPUExternalTextureDescriptor.h
    graphics/WebGPU/WebGPUFeatureName.h
    graphics/WebGPU/WebGPUFilterMode.h
    graphics/WebGPU/WebGPUFragmentState.h
    graphics/WebGPU/WebGPUFrontFace.h
    graphics/WebGPU/WebGPUImageCopyBuffer.h
    graphics/WebGPU/WebGPUImageCopyExternalImage.h
    graphics/WebGPU/WebGPUImageCopyTexture.h
    graphics/WebGPU/WebGPUImageCopyTextureTagged.h
    graphics/WebGPU/WebGPUImageDataLayout.h
    graphics/WebGPU/WebGPUIndexFormat.h
    graphics/WebGPU/WebGPUIntegralTypes.h
    graphics/WebGPU/WebGPULoadOp.h
    graphics/WebGPU/WebGPUMapMode.h
    graphics/WebGPU/WebGPUMultisampleState.h
    graphics/WebGPU/WebGPUObjectDescriptorBase.h
    graphics/WebGPU/WebGPUOrigin2D.h
    graphics/WebGPU/WebGPUOrigin3D.h
    graphics/WebGPU/WebGPUOutOfMemoryError.h
    graphics/WebGPU/WebGPUPipelineDescriptorBase.h
    graphics/WebGPU/WebGPUPipelineLayout.h
    graphics/WebGPU/WebGPUPipelineLayoutDescriptor.h
    graphics/WebGPU/WebGPUPipelineStatisticName.h
    graphics/WebGPU/WebGPUPowerPreference.h
    graphics/WebGPU/WebGPUPredefinedColorSpace.h
    graphics/WebGPU/WebGPUPrimitiveState.h
    graphics/WebGPU/WebGPUPrimitiveTopology.h
    graphics/WebGPU/WebGPUProgrammableStage.h
    graphics/WebGPU/WebGPUQuerySet.h
    graphics/WebGPU/WebGPUQuerySetDescriptor.h
    graphics/WebGPU/WebGPUQueryType.h
    graphics/WebGPU/WebGPUQueue.h
    graphics/WebGPU/WebGPURenderBundle.h
    graphics/WebGPU/WebGPURenderBundleDescriptor.h
    graphics/WebGPU/WebGPURenderBundleEncoder.h
    graphics/WebGPU/WebGPURenderBundleEncoderDescriptor.h
    graphics/WebGPU/WebGPURenderPassColorAttachment.h
    graphics/WebGPU/WebGPURenderPassDepthStencilAttachment.h
    graphics/WebGPU/WebGPURenderPassDescriptor.h
    graphics/WebGPU/WebGPURenderPassEncoder.h
    graphics/WebGPU/WebGPURenderPassLayout.h
    graphics/WebGPU/WebGPURenderPassTimestampLocation.h
    graphics/WebGPU/WebGPURenderPassTimestampWrites.h
    graphics/WebGPU/WebGPURenderPipeline.h
    graphics/WebGPU/WebGPURenderPipelineDescriptor.h
    graphics/WebGPU/WebGPURequestAdapterOptions.h
    graphics/WebGPU/WebGPUSampler.h
    graphics/WebGPU/WebGPUSamplerBindingLayout.h
    graphics/WebGPU/WebGPUSamplerBindingType.h
    graphics/WebGPU/WebGPUSamplerDescriptor.h
    graphics/WebGPU/WebGPUShaderModule.h
    graphics/WebGPU/WebGPUShaderModuleDescriptor.h
    graphics/WebGPU/WebGPUShaderStage.h
    graphics/WebGPU/WebGPUStencilFaceState.h
    graphics/WebGPU/WebGPUStencilOperation.h
    graphics/WebGPU/WebGPUStorageTextureAccess.h
    graphics/WebGPU/WebGPUStorageTextureBindingLayout.h
    graphics/WebGPU/WebGPUStoreOp.h
    graphics/WebGPU/WebGPUSupportedFeatures.h
    graphics/WebGPU/WebGPUSupportedLimits.h
    graphics/WebGPU/WebGPUTexture.h
    graphics/WebGPU/WebGPUTextureAspect.h
    graphics/WebGPU/WebGPUTextureBindingLayout.h
    graphics/WebGPU/WebGPUTextureDescriptor.h
    graphics/WebGPU/WebGPUTextureDimension.h
    graphics/WebGPU/WebGPUTextureFormat.h
    graphics/WebGPU/WebGPUTextureSampleType.h
    graphics/WebGPU/WebGPUTextureUsage.h
    graphics/WebGPU/WebGPUTextureView.h
    graphics/WebGPU/WebGPUTextureViewDescriptor.h
    graphics/WebGPU/WebGPUTextureViewDimension.h
    graphics/WebGPU/WebGPUUncapturedErrorEvent.h
    graphics/WebGPU/WebGPUUncapturedErrorEventInit.h
    graphics/WebGPU/WebGPUValidationError.h
    graphics/WebGPU/WebGPUVertexAttribute.h
    graphics/WebGPU/WebGPUVertexBufferLayout.h
    graphics/WebGPU/WebGPUVertexFormat.h
    graphics/WebGPU/WebGPUVertexState.h
    graphics/WebGPU/WebGPUVertexStepMode.h

    system/Clock.h
    system/ClockGeneric.h
    system/SleepDisabler.h
    system/Sound.h
    system/SystemSleepListener.h

    text/KillRing.h
    text/UnencodableHandling.h
)

set(PAL_SOURCES
    FileSizeFormatter.cpp
    Logging.cpp
    SessionID.cpp

    system/SleepDisabler.cpp
    system/SystemSleepListener.cpp
)

set(PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PAL_DERIVED_SOURCES_DIR}"
    "${PAL_DIR}"
    "${PAL_DIR}/pal"
    "${PAL_DIR}/pal/crypto"
    "${PAL_DIR}/pal/system"
    "${PAL_DIR}/pal/text"
)

set(PAL_FRAMEWORKS WTF)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND PAL_FRAMEWORKS bmalloc)
endif ()

set(PAL_INTERFACE_LIBRARIES PAL)
set(PAL_INTERFACE_INCLUDE_DIRECTORIES ${PAL_FRAMEWORK_HEADERS_DIR})
set(PAL_INTERFACE_DEPENDENCIES PAL_CopyHeaders)

WEBKIT_FRAMEWORK_DECLARE(PAL)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_COPY_FILES(PAL_CopyHeaders
    DESTINATION ${PAL_FRAMEWORK_HEADERS_DIR}/pal
    FILES ${PAL_PUBLIC_HEADERS}
)

WEBKIT_FRAMEWORK(PAL)
WEBKIT_FRAMEWORK_TARGET(PAL)
