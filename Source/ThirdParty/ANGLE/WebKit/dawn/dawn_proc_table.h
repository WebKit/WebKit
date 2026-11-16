
#ifndef DAWN_DAWN_PROC_TABLE_H_
#define DAWN_DAWN_PROC_TABLE_H_

// Rewritten from "dawn/webgpu.h" when vendored, so this directory is
// self-contained and can be put on a header search path on its own.
#include "webgpu.h"

// Note: Often allocated as a static global. Do not add a complex constructor.
typedef struct DawnProcTable {
    uint8_t version[20];

    WGPUProcCreateInstance createInstance;
    WGPUProcGetInstanceFeatures getInstanceFeatures;
    WGPUProcGetInstanceLimits getInstanceLimits;
    WGPUProcHasInstanceFeature hasInstanceFeature;
    WGPUProcGetProcAddress getProcAddress;

    WGPUProcAdapterCreateDevice adapterCreateDevice;
    WGPUProcAdapterGetFeatures adapterGetFeatures;
    WGPUProcAdapterGetFormatCapabilities adapterGetFormatCapabilities;
    WGPUProcAdapterGetInfo adapterGetInfo;
    WGPUProcAdapterGetInstance adapterGetInstance;
    WGPUProcAdapterGetLimits adapterGetLimits;
    WGPUProcAdapterHasFeature adapterHasFeature;
    WGPUProcAdapterRequestDevice adapterRequestDevice;
    WGPUProcAdapterAddRef adapterAddRef;
    WGPUProcAdapterRelease adapterRelease;

    WGPUProcAdapterInfoFreeMembers adapterInfoFreeMembers;

    WGPUProcAdapterPropertiesMemoryHeapsFreeMembers adapterPropertiesMemoryHeapsFreeMembers;

    WGPUProcAdapterPropertiesSubgroupMatrixConfigsFreeMembers adapterPropertiesSubgroupMatrixConfigsFreeMembers;

    WGPUProcBindGroupSetLabel bindGroupSetLabel;
    WGPUProcBindGroupAddRef bindGroupAddRef;
    WGPUProcBindGroupRelease bindGroupRelease;

    WGPUProcBindGroupLayoutSetLabel bindGroupLayoutSetLabel;
    WGPUProcBindGroupLayoutAddRef bindGroupLayoutAddRef;
    WGPUProcBindGroupLayoutRelease bindGroupLayoutRelease;

    WGPUProcBufferCreateTexelView bufferCreateTexelView;
    WGPUProcBufferDestroy bufferDestroy;
    WGPUProcBufferGetConstMappedRange bufferGetConstMappedRange;
    WGPUProcBufferGetMappedRange bufferGetMappedRange;
    WGPUProcBufferGetMapState bufferGetMapState;
    WGPUProcBufferGetSize bufferGetSize;
    WGPUProcBufferGetUsage bufferGetUsage;
    WGPUProcBufferMapAsync bufferMapAsync;
    WGPUProcBufferReadMappedRange bufferReadMappedRange;
    WGPUProcBufferSetLabel bufferSetLabel;
    WGPUProcBufferUnmap bufferUnmap;
    WGPUProcBufferWriteMappedRange bufferWriteMappedRange;
    WGPUProcBufferAddRef bufferAddRef;
    WGPUProcBufferRelease bufferRelease;

    WGPUProcCommandBufferSetLabel commandBufferSetLabel;
    WGPUProcCommandBufferAddRef commandBufferAddRef;
    WGPUProcCommandBufferRelease commandBufferRelease;

    WGPUProcCommandEncoderBeginComputePass commandEncoderBeginComputePass;
    WGPUProcCommandEncoderBeginRenderPass commandEncoderBeginRenderPass;
    WGPUProcCommandEncoderClearBuffer commandEncoderClearBuffer;
    WGPUProcCommandEncoderCopyBufferToBuffer commandEncoderCopyBufferToBuffer;
    WGPUProcCommandEncoderCopyBufferToTexture commandEncoderCopyBufferToTexture;
    WGPUProcCommandEncoderCopyTextureToBuffer commandEncoderCopyTextureToBuffer;
    WGPUProcCommandEncoderCopyTextureToTexture commandEncoderCopyTextureToTexture;
    WGPUProcCommandEncoderFinish commandEncoderFinish;
    WGPUProcCommandEncoderInjectValidationError commandEncoderInjectValidationError;
    WGPUProcCommandEncoderInsertDebugMarker commandEncoderInsertDebugMarker;
    WGPUProcCommandEncoderPopDebugGroup commandEncoderPopDebugGroup;
    WGPUProcCommandEncoderPushDebugGroup commandEncoderPushDebugGroup;
    WGPUProcCommandEncoderResolveQuerySet commandEncoderResolveQuerySet;
    WGPUProcCommandEncoderSetLabel commandEncoderSetLabel;
    WGPUProcCommandEncoderWriteBuffer commandEncoderWriteBuffer;
    WGPUProcCommandEncoderWriteTimestamp commandEncoderWriteTimestamp;
    WGPUProcCommandEncoderAddRef commandEncoderAddRef;
    WGPUProcCommandEncoderRelease commandEncoderRelease;

    WGPUProcComputePassEncoderDispatchWorkgroups computePassEncoderDispatchWorkgroups;
    WGPUProcComputePassEncoderDispatchWorkgroupsIndirect computePassEncoderDispatchWorkgroupsIndirect;
    WGPUProcComputePassEncoderEnd computePassEncoderEnd;
    WGPUProcComputePassEncoderInsertDebugMarker computePassEncoderInsertDebugMarker;
    WGPUProcComputePassEncoderPopDebugGroup computePassEncoderPopDebugGroup;
    WGPUProcComputePassEncoderPushDebugGroup computePassEncoderPushDebugGroup;
    WGPUProcComputePassEncoderSetBindGroup computePassEncoderSetBindGroup;
    WGPUProcComputePassEncoderSetImmediates computePassEncoderSetImmediates;
    WGPUProcComputePassEncoderSetLabel computePassEncoderSetLabel;
    WGPUProcComputePassEncoderSetPipeline computePassEncoderSetPipeline;
    WGPUProcComputePassEncoderSetResourceTable computePassEncoderSetResourceTable;
    WGPUProcComputePassEncoderWriteTimestamp computePassEncoderWriteTimestamp;
    WGPUProcComputePassEncoderAddRef computePassEncoderAddRef;
    WGPUProcComputePassEncoderRelease computePassEncoderRelease;

    WGPUProcComputePipelineGetBindGroupLayout computePipelineGetBindGroupLayout;
    WGPUProcComputePipelineSetLabel computePipelineSetLabel;
    WGPUProcComputePipelineAddRef computePipelineAddRef;
    WGPUProcComputePipelineRelease computePipelineRelease;

    WGPUProcDawnDrmFormatCapabilitiesFreeMembers dawnDrmFormatCapabilitiesFreeMembers;

    WGPUProcDeviceCreateBindGroup deviceCreateBindGroup;
    WGPUProcDeviceCreateBindGroupLayout deviceCreateBindGroupLayout;
    WGPUProcDeviceCreateBuffer deviceCreateBuffer;
    WGPUProcDeviceCreateCommandEncoder deviceCreateCommandEncoder;
    WGPUProcDeviceCreateComputePipeline deviceCreateComputePipeline;
    WGPUProcDeviceCreateComputePipelineAsync deviceCreateComputePipelineAsync;
    WGPUProcDeviceCreateErrorBuffer deviceCreateErrorBuffer;
    WGPUProcDeviceCreateErrorExternalTexture deviceCreateErrorExternalTexture;
    WGPUProcDeviceCreateErrorShaderModule deviceCreateErrorShaderModule;
    WGPUProcDeviceCreateErrorTexture deviceCreateErrorTexture;
    WGPUProcDeviceCreateExternalTexture deviceCreateExternalTexture;
    WGPUProcDeviceCreatePipelineLayout deviceCreatePipelineLayout;
    WGPUProcDeviceCreateQuerySet deviceCreateQuerySet;
    WGPUProcDeviceCreateRenderBundleEncoder deviceCreateRenderBundleEncoder;
    WGPUProcDeviceCreateRenderPipeline deviceCreateRenderPipeline;
    WGPUProcDeviceCreateRenderPipelineAsync deviceCreateRenderPipelineAsync;
    WGPUProcDeviceCreateResourceTable deviceCreateResourceTable;
    WGPUProcDeviceCreateSampler deviceCreateSampler;
    WGPUProcDeviceCreateShaderModule deviceCreateShaderModule;
    WGPUProcDeviceCreateTexture deviceCreateTexture;
    WGPUProcDeviceDestroy deviceDestroy;
    WGPUProcDeviceForceLoss deviceForceLoss;
    WGPUProcDeviceGetAdapter deviceGetAdapter;
    WGPUProcDeviceGetAdapterInfo deviceGetAdapterInfo;
    WGPUProcDeviceGetAHardwareBufferProperties deviceGetAHardwareBufferProperties;
    WGPUProcDeviceGetFeatures deviceGetFeatures;
    WGPUProcDeviceGetLimits deviceGetLimits;
    WGPUProcDeviceGetLostFuture deviceGetLostFuture;
    WGPUProcDeviceGetQueue deviceGetQueue;
    WGPUProcDeviceHasFeature deviceHasFeature;
    WGPUProcDeviceImportSharedBufferMemory deviceImportSharedBufferMemory;
    WGPUProcDeviceImportSharedFence deviceImportSharedFence;
    WGPUProcDeviceImportSharedTextureMemory deviceImportSharedTextureMemory;
    WGPUProcDeviceInjectError deviceInjectError;
    WGPUProcDevicePopErrorScope devicePopErrorScope;
    WGPUProcDevicePushErrorScope devicePushErrorScope;
    WGPUProcDeviceSetLabel deviceSetLabel;
    WGPUProcDeviceSetLoggingCallback deviceSetLoggingCallback;
    WGPUProcDeviceTick deviceTick;
    WGPUProcDeviceValidateTextureDescriptor deviceValidateTextureDescriptor;
    WGPUProcDeviceAddRef deviceAddRef;
    WGPUProcDeviceRelease deviceRelease;

    WGPUProcExternalTextureDestroy externalTextureDestroy;
    WGPUProcExternalTextureExpire externalTextureExpire;
    WGPUProcExternalTextureRefresh externalTextureRefresh;
    WGPUProcExternalTextureSetLabel externalTextureSetLabel;
    WGPUProcExternalTextureAddRef externalTextureAddRef;
    WGPUProcExternalTextureRelease externalTextureRelease;

    WGPUProcInstanceCreateSurface instanceCreateSurface;
    WGPUProcInstanceGetWGSLLanguageFeatures instanceGetWGSLLanguageFeatures;
    WGPUProcInstanceHasWGSLLanguageFeature instanceHasWGSLLanguageFeature;
    WGPUProcInstanceProcessEvents instanceProcessEvents;
    WGPUProcInstanceRequestAdapter instanceRequestAdapter;
    WGPUProcInstanceWaitAny instanceWaitAny;
    WGPUProcInstanceAddRef instanceAddRef;
    WGPUProcInstanceRelease instanceRelease;

    WGPUProcPipelineLayoutSetLabel pipelineLayoutSetLabel;
    WGPUProcPipelineLayoutAddRef pipelineLayoutAddRef;
    WGPUProcPipelineLayoutRelease pipelineLayoutRelease;

    WGPUProcQuerySetDestroy querySetDestroy;
    WGPUProcQuerySetGetCount querySetGetCount;
    WGPUProcQuerySetGetType querySetGetType;
    WGPUProcQuerySetSetLabel querySetSetLabel;
    WGPUProcQuerySetAddRef querySetAddRef;
    WGPUProcQuerySetRelease querySetRelease;

    WGPUProcQueueCopyExternalTextureForBrowser queueCopyExternalTextureForBrowser;
    WGPUProcQueueCopyTextureForBrowser queueCopyTextureForBrowser;
    WGPUProcQueueOnSubmittedWorkDone queueOnSubmittedWorkDone;
    WGPUProcQueueSetLabel queueSetLabel;
    WGPUProcQueueSubmit queueSubmit;
    WGPUProcQueueWriteBuffer queueWriteBuffer;
    WGPUProcQueueWriteTexture queueWriteTexture;
    WGPUProcQueueAddRef queueAddRef;
    WGPUProcQueueRelease queueRelease;

    WGPUProcRenderBundleSetLabel renderBundleSetLabel;
    WGPUProcRenderBundleAddRef renderBundleAddRef;
    WGPUProcRenderBundleRelease renderBundleRelease;

    WGPUProcRenderBundleEncoderDraw renderBundleEncoderDraw;
    WGPUProcRenderBundleEncoderDrawIndexed renderBundleEncoderDrawIndexed;
    WGPUProcRenderBundleEncoderDrawIndexedIndirect renderBundleEncoderDrawIndexedIndirect;
    WGPUProcRenderBundleEncoderDrawIndirect renderBundleEncoderDrawIndirect;
    WGPUProcRenderBundleEncoderFinish renderBundleEncoderFinish;
    WGPUProcRenderBundleEncoderInsertDebugMarker renderBundleEncoderInsertDebugMarker;
    WGPUProcRenderBundleEncoderPopDebugGroup renderBundleEncoderPopDebugGroup;
    WGPUProcRenderBundleEncoderPushDebugGroup renderBundleEncoderPushDebugGroup;
    WGPUProcRenderBundleEncoderSetBindGroup renderBundleEncoderSetBindGroup;
    WGPUProcRenderBundleEncoderSetImmediates renderBundleEncoderSetImmediates;
    WGPUProcRenderBundleEncoderSetIndexBuffer renderBundleEncoderSetIndexBuffer;
    WGPUProcRenderBundleEncoderSetLabel renderBundleEncoderSetLabel;
    WGPUProcRenderBundleEncoderSetPipeline renderBundleEncoderSetPipeline;
    WGPUProcRenderBundleEncoderSetResourceTable renderBundleEncoderSetResourceTable;
    WGPUProcRenderBundleEncoderSetVertexBuffer renderBundleEncoderSetVertexBuffer;
    WGPUProcRenderBundleEncoderAddRef renderBundleEncoderAddRef;
    WGPUProcRenderBundleEncoderRelease renderBundleEncoderRelease;

    WGPUProcRenderPassEncoderBeginOcclusionQuery renderPassEncoderBeginOcclusionQuery;
    WGPUProcRenderPassEncoderDraw renderPassEncoderDraw;
    WGPUProcRenderPassEncoderDrawIndexed renderPassEncoderDrawIndexed;
    WGPUProcRenderPassEncoderDrawIndexedIndirect renderPassEncoderDrawIndexedIndirect;
    WGPUProcRenderPassEncoderDrawIndirect renderPassEncoderDrawIndirect;
    WGPUProcRenderPassEncoderEnd renderPassEncoderEnd;
    WGPUProcRenderPassEncoderEndOcclusionQuery renderPassEncoderEndOcclusionQuery;
    WGPUProcRenderPassEncoderExecuteBundles renderPassEncoderExecuteBundles;
    WGPUProcRenderPassEncoderInsertDebugMarker renderPassEncoderInsertDebugMarker;
    WGPUProcRenderPassEncoderMultiDrawIndexedIndirect renderPassEncoderMultiDrawIndexedIndirect;
    WGPUProcRenderPassEncoderMultiDrawIndirect renderPassEncoderMultiDrawIndirect;
    WGPUProcRenderPassEncoderPixelLocalStorageBarrier renderPassEncoderPixelLocalStorageBarrier;
    WGPUProcRenderPassEncoderPopDebugGroup renderPassEncoderPopDebugGroup;
    WGPUProcRenderPassEncoderPushDebugGroup renderPassEncoderPushDebugGroup;
    WGPUProcRenderPassEncoderSetBindGroup renderPassEncoderSetBindGroup;
    WGPUProcRenderPassEncoderSetBlendConstant renderPassEncoderSetBlendConstant;
    WGPUProcRenderPassEncoderSetImmediates renderPassEncoderSetImmediates;
    WGPUProcRenderPassEncoderSetIndexBuffer renderPassEncoderSetIndexBuffer;
    WGPUProcRenderPassEncoderSetLabel renderPassEncoderSetLabel;
    WGPUProcRenderPassEncoderSetPipeline renderPassEncoderSetPipeline;
    WGPUProcRenderPassEncoderSetResourceTable renderPassEncoderSetResourceTable;
    WGPUProcRenderPassEncoderSetScissorRect renderPassEncoderSetScissorRect;
    WGPUProcRenderPassEncoderSetStencilReference renderPassEncoderSetStencilReference;
    WGPUProcRenderPassEncoderSetVertexBuffer renderPassEncoderSetVertexBuffer;
    WGPUProcRenderPassEncoderSetViewport renderPassEncoderSetViewport;
    WGPUProcRenderPassEncoderWriteTimestamp renderPassEncoderWriteTimestamp;
    WGPUProcRenderPassEncoderAddRef renderPassEncoderAddRef;
    WGPUProcRenderPassEncoderRelease renderPassEncoderRelease;

    WGPUProcRenderPipelineGetBindGroupLayout renderPipelineGetBindGroupLayout;
    WGPUProcRenderPipelineSetLabel renderPipelineSetLabel;
    WGPUProcRenderPipelineAddRef renderPipelineAddRef;
    WGPUProcRenderPipelineRelease renderPipelineRelease;

    WGPUProcResourceTableDestroy resourceTableDestroy;
    WGPUProcResourceTableGetSize resourceTableGetSize;
    WGPUProcResourceTableInsertBinding resourceTableInsertBinding;
    WGPUProcResourceTableRemoveBinding resourceTableRemoveBinding;
    WGPUProcResourceTableSetLabel resourceTableSetLabel;
    WGPUProcResourceTableUpdate resourceTableUpdate;
    WGPUProcResourceTableAddRef resourceTableAddRef;
    WGPUProcResourceTableRelease resourceTableRelease;

    WGPUProcSamplerSetLabel samplerSetLabel;
    WGPUProcSamplerAddRef samplerAddRef;
    WGPUProcSamplerRelease samplerRelease;

    WGPUProcShaderModuleGetCompilationInfo shaderModuleGetCompilationInfo;
    WGPUProcShaderModuleSetLabel shaderModuleSetLabel;
    WGPUProcShaderModuleAddRef shaderModuleAddRef;
    WGPUProcShaderModuleRelease shaderModuleRelease;

    WGPUProcSharedBufferMemoryBeginAccess sharedBufferMemoryBeginAccess;
    WGPUProcSharedBufferMemoryCreateBuffer sharedBufferMemoryCreateBuffer;
    WGPUProcSharedBufferMemoryEndAccess sharedBufferMemoryEndAccess;
    WGPUProcSharedBufferMemoryGetProperties sharedBufferMemoryGetProperties;
    WGPUProcSharedBufferMemoryIsDeviceLost sharedBufferMemoryIsDeviceLost;
    WGPUProcSharedBufferMemorySetLabel sharedBufferMemorySetLabel;
    WGPUProcSharedBufferMemoryAddRef sharedBufferMemoryAddRef;
    WGPUProcSharedBufferMemoryRelease sharedBufferMemoryRelease;

    WGPUProcSharedBufferMemoryEndAccessStateFreeMembers sharedBufferMemoryEndAccessStateFreeMembers;

    WGPUProcSharedFenceExportInfo sharedFenceExportInfo;
    WGPUProcSharedFenceSetLabel sharedFenceSetLabel;
    WGPUProcSharedFenceAddRef sharedFenceAddRef;
    WGPUProcSharedFenceRelease sharedFenceRelease;

    WGPUProcSharedTextureMemoryBeginAccess sharedTextureMemoryBeginAccess;
    WGPUProcSharedTextureMemoryCreateTexture sharedTextureMemoryCreateTexture;
    WGPUProcSharedTextureMemoryEndAccess sharedTextureMemoryEndAccess;
    WGPUProcSharedTextureMemoryGetProperties sharedTextureMemoryGetProperties;
    WGPUProcSharedTextureMemoryIsDeviceLost sharedTextureMemoryIsDeviceLost;
    WGPUProcSharedTextureMemorySetLabel sharedTextureMemorySetLabel;
    WGPUProcSharedTextureMemoryAddRef sharedTextureMemoryAddRef;
    WGPUProcSharedTextureMemoryRelease sharedTextureMemoryRelease;

    WGPUProcSharedTextureMemoryEndAccessStateFreeMembers sharedTextureMemoryEndAccessStateFreeMembers;

    WGPUProcSupportedFeaturesFreeMembers supportedFeaturesFreeMembers;

    WGPUProcSupportedInstanceFeaturesFreeMembers supportedInstanceFeaturesFreeMembers;

    WGPUProcSupportedWGSLLanguageFeaturesFreeMembers supportedWGSLLanguageFeaturesFreeMembers;

    WGPUProcSurfaceConfigure surfaceConfigure;
    WGPUProcSurfaceGetCapabilities surfaceGetCapabilities;
    WGPUProcSurfaceGetCurrentTexture surfaceGetCurrentTexture;
    WGPUProcSurfacePresent surfacePresent;
    WGPUProcSurfaceSetLabel surfaceSetLabel;
    WGPUProcSurfaceUnconfigure surfaceUnconfigure;
    WGPUProcSurfaceAddRef surfaceAddRef;
    WGPUProcSurfaceRelease surfaceRelease;

    WGPUProcSurfaceCapabilitiesFreeMembers surfaceCapabilitiesFreeMembers;

    WGPUProcTexelBufferViewSetLabel texelBufferViewSetLabel;
    WGPUProcTexelBufferViewAddRef texelBufferViewAddRef;
    WGPUProcTexelBufferViewRelease texelBufferViewRelease;

    WGPUProcTextureCreateErrorView textureCreateErrorView;
    WGPUProcTextureCreateView textureCreateView;
    WGPUProcTextureDestroy textureDestroy;
    WGPUProcTextureGetDepthOrArrayLayers textureGetDepthOrArrayLayers;
    WGPUProcTextureGetDimension textureGetDimension;
    WGPUProcTextureGetFormat textureGetFormat;
    WGPUProcTextureGetHeight textureGetHeight;
    WGPUProcTextureGetMipLevelCount textureGetMipLevelCount;
    WGPUProcTextureGetSampleCount textureGetSampleCount;
    WGPUProcTextureGetTextureBindingViewDimension textureGetTextureBindingViewDimension;
    WGPUProcTextureGetUsage textureGetUsage;
    WGPUProcTextureGetWidth textureGetWidth;
    WGPUProcTexturePin texturePin;
    WGPUProcTextureSetLabel textureSetLabel;
    WGPUProcTextureSetOwnershipForMemoryDump textureSetOwnershipForMemoryDump;
    WGPUProcTextureUnpin textureUnpin;
    WGPUProcTextureAddRef textureAddRef;
    WGPUProcTextureRelease textureRelease;

    WGPUProcTextureViewSetLabel textureViewSetLabel;
    WGPUProcTextureViewAddRef textureViewAddRef;
    WGPUProcTextureViewRelease textureViewRelease;


} DawnProcTable;

#endif  // DAWN_DAWN_PROC_TABLE_H_
