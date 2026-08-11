list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/gpu/dawn"
)

list(APPEND WebCore_SOURCES
    platform/graphics/gpu/dawn/GPUBindGroupAllocatorDawn.cpp
    platform/graphics/gpu/dawn/GPUBindGroupDawn.cpp
    platform/graphics/gpu/dawn/GPUBindGroupLayoutDawn.cpp
    platform/graphics/gpu/dawn/GPUBufferDawn.cpp
    platform/graphics/gpu/dawn/GPUCommandBufferDawn.cpp
    platform/graphics/gpu/dawn/GPUComputePassEncoderDawn.cpp
    platform/graphics/gpu/dawn/GPUComputePipelineDawn.cpp
    platform/graphics/gpu/dawn/GPUDeviceDawn.cpp
    platform/graphics/gpu/dawn/GPUProgrammablePassEncoderDawn.cpp
    platform/graphics/gpu/dawn/GPUQueueDawn.cpp
    platform/graphics/gpu/dawn/GPURenderPassEncoderDawn.cpp
    platform/graphics/gpu/dawn/GPURenderPipelineDawn.cpp
    platform/graphics/gpu/dawn/GPUSamplerDawn.cpp
    platform/graphics/gpu/dawn/GPUShaderModuleDawn.cpp
    platform/graphics/gpu/dawn/GPUSwapChainDawn.cpp
    platform/graphics/gpu/dawn/GPUTextureDawn.cpp
)

list(APPEND WebCore_PRIVATE_LIBRARIES
    Dawn::dawn
    Dawn::native
)
