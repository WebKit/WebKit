//
//  MTLDevice.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Availability.h>
#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLGPUAddress.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLTensor.h>
#import <Metal/MTLLibrary.h>
#import <IOSurface/IOSurfaceRef.h>
#import <Metal/MTLCounters.h>


#import <Metal/MTL4Compiler.h>
#import <Metal/MTL4Counters.h>
@protocol MTL4BinaryFunction;

NS_ASSUME_NONNULL_BEGIN
@protocol MTLCommandQueue;
@protocol MTLDevice;
@protocol MTLBuffer;
@protocol MTLDepthStencilState;
@protocol MTLFunction;
@protocol MTLLibrary;
@protocol MTLTexture;
@protocol MTLSamplerState;
@protocol MTLRenderPipelineState;
@protocol MTLComputePipelineState;
@protocol MTLHeap;
@protocol MTLFence;
@protocol MTLArgumentEncoder;
@protocol MTLRasterizationRateMap;
@class MTLRasterizationRateLayerDescriptor;
@class MTLRasterizationRateMapDescriptor;
@class MTLTileRenderPipelineDescriptor;
@class MTLTilePipelineColorAttachmentDescriptor;
@class MTLMeshRenderPipelineDescriptor;
@class MTLSamplerDescriptor;
@class MTLRenderPipelineColorAttachmentDescriptor;
@class MTLDepthStencilDescriptor;
@class MTLTextureDescriptor;
@class MTLCompileOptions;
@class MTLRenderPipelineDescriptor;
@class MTLRenderPassDescriptor;
@class MTLComputePassDescriptor;
@class MTLBlitPassDescriptor;
@class MTLRenderPipelineReflection;
@class MTLComputePipelineDescriptor;
@class MTLComputePipelineReflection;
@class MTLCommandQueueDescriptor;
@class MTLHeapDescriptor;
@class MTLIndirectCommandBufferDescriptor;
@protocol MTLIndirectRenderCommandEncoder;
@protocol MTLIndirectComputeCommandEncoder;
@protocol MTLIndirectCommandBuffer;
@class MTLSharedTextureHandle;
@protocol MTLEvent;
@protocol MTLSharedEvent;
@class MTLSharedEventHandle;
@protocol MTLDynamicLibrary;
@protocol MTLBinaryArchive;
@class MTLBinaryArchiveDescriptor;
@class MTLAccelerationStructureDescriptor;
@protocol MTLAccelerationStructure;
@protocol MTLFunctionHandle;
@protocol MTLVisibleFunctionTable;
@class MTLVisibleFunctionTableDescriptor;
@protocol MTLIntersectionFunctionTable;
@class MTLIntersectionFunctionTableDescriptor;
@class MTLStitchedLibraryDescriptor;
@protocol MTLLogState;
@class MTLLogStateDescriptor;
@protocol MTLArgumentEncoder;
@class MTLResidencySetDescriptor;
@protocol MTLResidencySet;


@class MTL4CommandQueueDescriptor;
@protocol MTL4CommandQueue;

@protocol MTL4CommandBuffer;

@class MTL4CommandAllocatorDescriptor;
@protocol MTL4CommandAllocator;

@class MTL4ArgumentTableDescriptor;
@protocol MTL4ArgumentTable;

@class MTLResourceViewPoolDescriptor;
@protocol MTLTextureViewPool;
@class MTL4CompilerDescriptor;
@protocol MTL4Compiler;
@protocol MTL4Archive;
@protocol MTL4PipelineDataSetSerializer;
@class MTL4PipelineDataSetSerializerDescriptor;


@class MTL4CounterHeapDescriptor;
@protocol MTL4CounterHeap;


@protocol MTLIOFileHandle;
@protocol MTLIOCommandQueue;
@class MTLIOCommandQueueDescriptor;


typedef NS_ENUM(NSInteger, MTLIOCompressionMethod) {
    MTLIOCompressionMethodZlib = 0,
    MTLIOCompressionMethodLZFSE = 1,
    MTLIOCompressionMethodLZ4 = 2,
    MTLIOCompressionMethodLZMA = 3,
    MTLIOCompressionMethodLZBitmap = 4,
};


/*!
 @brief Returns a reference to the preferred system default Metal device.
 @discussion On Mac OS X systems that support automatic graphics switching, calling
 this API to get a Metal device will cause the system to switch to the high power
 GPU.  On other systems that support more than one GPU it will return the GPU that
 is associated with the main display.
 */
MTL_EXTERN id <MTLDevice> __nullable MTLCreateSystemDefaultDevice(void) API_AVAILABLE(macos(10.11), ios(8.0)) NS_RETURNS_RETAINED;

/*!
 @brief Returns all Metal devices in the system.
 @discussion On macOS and macCatalyst, this API will not cause the system to switch devices and leaves the decision about which GPU to use up to the application based on whatever criteria it deems appropriate.
 On iOS, tvOS and visionOS, this API returns an array containing the same device that MTLCreateSystemDefaultDevice would have returned, or an empty array if it would have failed.
*/
MTL_EXTERN NSArray <id<MTLDevice>> *MTLCopyAllDevices(void) API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(18.0)) NS_RETURNS_RETAINED;

typedef NS_ENUM(NSUInteger, MTLFeatureSet)
{
    MTLFeatureSet_iOS_GPUFamily1_v1 API_AVAILABLE(ios(8.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 0,
    MTLFeatureSet_iOS_GPUFamily2_v1 API_AVAILABLE(ios(8.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 1,

    MTLFeatureSet_iOS_GPUFamily1_v2 API_AVAILABLE(ios(9.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 2,
    MTLFeatureSet_iOS_GPUFamily2_v2 API_AVAILABLE(ios(9.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 3,
    MTLFeatureSet_iOS_GPUFamily3_v1 API_AVAILABLE(ios(9.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 4,

    MTLFeatureSet_iOS_GPUFamily1_v3 API_AVAILABLE(ios(10.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 5,
    MTLFeatureSet_iOS_GPUFamily2_v3 API_AVAILABLE(ios(10.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 6,
    MTLFeatureSet_iOS_GPUFamily3_v2 API_AVAILABLE(ios(10.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 7,

    MTLFeatureSet_iOS_GPUFamily1_v4 API_AVAILABLE(ios(11.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 8,
    MTLFeatureSet_iOS_GPUFamily2_v4 API_AVAILABLE(ios(11.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 9,
    MTLFeatureSet_iOS_GPUFamily3_v3 API_AVAILABLE(ios(11.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 10,
    MTLFeatureSet_iOS_GPUFamily4_v1 API_AVAILABLE(ios(11.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 11,
    
    MTLFeatureSet_iOS_GPUFamily1_v5 API_AVAILABLE(ios(12.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 12,
    MTLFeatureSet_iOS_GPUFamily2_v5 API_AVAILABLE(ios(12.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 13,
    MTLFeatureSet_iOS_GPUFamily3_v4 API_AVAILABLE(ios(12.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 14,
    MTLFeatureSet_iOS_GPUFamily4_v2 API_AVAILABLE(ios(12.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 15,
    MTLFeatureSet_iOS_GPUFamily5_v1 API_AVAILABLE(ios(12.0)) API_UNAVAILABLE(macos, macCatalyst, tvos) = 16,

    MTLFeatureSet_macOS_GPUFamily1_v1 API_AVAILABLE(macos(10.11)) API_UNAVAILABLE(ios) API_UNAVAILABLE(macCatalyst) = 10000,
    MTLFeatureSet_OSX_GPUFamily1_v1 API_AVAILABLE(macos(10.11)) API_UNAVAILABLE(ios) = MTLFeatureSet_macOS_GPUFamily1_v1, // deprecated

    MTLFeatureSet_macOS_GPUFamily1_v2 API_AVAILABLE(macos(10.12)) API_UNAVAILABLE(ios) API_UNAVAILABLE(macCatalyst) = 10001,
    MTLFeatureSet_OSX_GPUFamily1_v2 API_AVAILABLE(macos(10.12)) API_UNAVAILABLE(ios) = MTLFeatureSet_macOS_GPUFamily1_v2, // deprecated
    MTLFeatureSet_macOS_ReadWriteTextureTier2 API_AVAILABLE(macos(10.12)) API_UNAVAILABLE(ios) = 10002,
    MTLFeatureSet_OSX_ReadWriteTextureTier2 API_AVAILABLE(macos(10.12)) API_UNAVAILABLE(ios) = MTLFeatureSet_macOS_ReadWriteTextureTier2, // deprecated

    MTLFeatureSet_macOS_GPUFamily1_v3 API_AVAILABLE(macos(10.13)) API_UNAVAILABLE(ios) API_UNAVAILABLE(macCatalyst) = 10003,
    
    MTLFeatureSet_macOS_GPUFamily1_v4 API_AVAILABLE(macos(10.14)) API_UNAVAILABLE(ios) API_UNAVAILABLE(macCatalyst) = 10004,
    MTLFeatureSet_macOS_GPUFamily2_v1 API_AVAILABLE(macos(10.14)) API_UNAVAILABLE(ios) API_UNAVAILABLE(macCatalyst) = 10005,


    MTLFeatureSet_tvOS_GPUFamily1_v1 API_AVAILABLE(tvos(9.0)) API_UNAVAILABLE(macos, ios) = 30000,
    MTLFeatureSet_TVOS_GPUFamily1_v1 API_AVAILABLE(tvos(9.0)) API_UNAVAILABLE(macos, ios) = MTLFeatureSet_tvOS_GPUFamily1_v1, // deprecated
    
    MTLFeatureSet_tvOS_GPUFamily1_v2 API_AVAILABLE(tvos(10.0)) API_UNAVAILABLE(macos, ios) = 30001,
    
    MTLFeatureSet_tvOS_GPUFamily1_v3 API_AVAILABLE(tvos(11.0)) API_UNAVAILABLE(macos, ios) = 30002,
    MTLFeatureSet_tvOS_GPUFamily2_v1 API_AVAILABLE(tvos(11.0)) API_UNAVAILABLE(macos, ios) = 30003,
    
    MTLFeatureSet_tvOS_GPUFamily1_v4 API_AVAILABLE(tvos(12.0)) API_UNAVAILABLE(macos, ios) = 30004,
    MTLFeatureSet_tvOS_GPUFamily2_v2 API_AVAILABLE(tvos(12.0)) API_UNAVAILABLE(macos, ios) = 30005,
} API_DEPRECATED("Use MTLGPUFamily instead", macos(10.11, 13.0), ios(8.0, 16.0), tvos(9.0, 16.0));

typedef NS_ENUM(NSInteger, MTLGPUFamily)
{
    MTLGPUFamilyApple1  = 1001,
    MTLGPUFamilyApple2  = 1002,
    MTLGPUFamilyApple3  = 1003,
    MTLGPUFamilyApple4  = 1004,
    MTLGPUFamilyApple5  = 1005,
    MTLGPUFamilyApple6  = 1006,
    MTLGPUFamilyApple7  = 1007,
    MTLGPUFamilyApple8  = 1008,
    MTLGPUFamilyApple9  = 1009,
    MTLGPUFamilyApple10 = 1010,

    MTLGPUFamilyMac1 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple7", macos(10.15, 13.0), ios(13.0, 16.0)) = 2001,
    MTLGPUFamilyMac2 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple7", macos(10.15, 27.0), ios(13.0, 27.0)) = 2002,
    
    MTLGPUFamilyCommon1 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple1", macos(10.15, 27.0), ios(13.0, 27.0)) = 3001,
    MTLGPUFamilyCommon2 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple3", macos(10.15, 27.0), ios(13.0, 27.0)) = 3002,
    MTLGPUFamilyCommon3 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple5", macos(10.15, 27.0), ios(13.0, 27.0)) = 3003,
    
    MTLGPUFamilyMacCatalyst1 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple7", macos(10.15, 13.0), ios(13.0, 16.0)) = 4001,
    MTLGPUFamilyMacCatalyst2 API_DEPRECATED_WITH_REPLACEMENT("MTLGPUFamilyApple7", macos(10.15, 13.0), ios(13.0, 16.0)) = 4002,
    
    MTLGPUFamilyMetal3 API_AVAILABLE(macos(13.0), ios(16.0)) = 5001,
    MTLGPUFamilyMetal4 API_AVAILABLE(macos(26.0), ios(26.0)) = 5002,
} API_AVAILABLE(macos(10.15), ios(13.0));


/*!
 @enum MTLPipelineOption
 @abstract Controls the creation of the pipeline
 */
typedef NS_OPTIONS(NSUInteger, MTLPipelineOption)
{
    MTLPipelineOptionNone               = 0,
    MTLPipelineOptionArgumentInfo API_DEPRECATED_WITH_REPLACEMENT("MTLPipelineOptionBindingInfo", macos(10.11, 13.0), ios(8.0, 16.0)) = 1 << 0,
    MTLPipelineOptionBindingInfo        = 1 << 0,
    MTLPipelineOptionBufferTypeInfo     = 1 << 1,
    MTLPipelineOptionFailOnBinaryArchiveMiss API_AVAILABLE(macos(11.0), ios(14.0)) = 1 << 2,
} API_AVAILABLE(macos(10.11), ios(8.0));

/*!
 @enum MTLReadWriteTextureTier
 @abstract MTLReadWriteTextureTier determines support level for read-write texture formats.
 */
typedef NS_ENUM(NSUInteger, MTLReadWriteTextureTier)
{
    MTLReadWriteTextureTierNone = 0,
    MTLReadWriteTextureTier1 = 1,
    MTLReadWriteTextureTier2 = 2,
} API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @enum MTLArgumentBuffersTier
 @abstract MTLArgumentBuffersTier determines support level for argument buffers.
 */
typedef NS_ENUM(NSUInteger, MTLArgumentBuffersTier)
{
    MTLArgumentBuffersTier1 = 0,
    MTLArgumentBuffersTier2 = 1,
} API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @enum MTLSparseTextureRegionAlignmentMode
 @abstract MTLSparseTextureRegionAlignmentMode determines type of alignment used when converting from pixel region to tile region.
 */
typedef NS_ENUM(NSUInteger, MTLSparseTextureRegionAlignmentMode)
{
    MTLSparseTextureRegionAlignmentModeOutward   = 0,
    MTLSparseTextureRegionAlignmentModeInward    = 1,
} API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/**
 * @brief Describes the memory requirements for an acceleration structure
 */
typedef struct {
    /**
     * @brief The required size, in bytes, of the built acceleration structure
     */
    NSUInteger accelerationStructureSize;
    
    /**
     * @brief The required size, in bytes, of the scratch buffer used to build the acceleration structure
     */
    NSUInteger buildScratchBufferSize;
    
    /**
     * @brief The required size, in bytes, of the scratch buffer used to refit the acceleration structure
     */
    NSUInteger refitScratchBufferSize;
} MTLAccelerationStructureSizes;

/*!
 @enum MTLCounterSamplingPoint
 @abstract MTLCounterSamplingPoint determines type of sampling points that are supported on given device.

 @constant MTLCounterSamplingPointAtStageBoundary
 Counter sampling points at render, compute, and blit command encoder stage boundary are supported.

 @constant MTLCounterSamplingPointAtDrawBoundary
 Counter sampling at draw boundary is supported, render encoder method sampleCountersInBuffer can be used for sampling.

 @constant MTLCounterSamplingPointAtDispatchBoundary
 Counter sampling at compute dispatch boundary is supported, compute encoder method sampleCountersInBuffer can be used for sampling.

 @constant MTLCounterSamplingPointAtTileDispatchBoundary
 Counter sampling at tile shader dispatch boundary is supported.

 @constant MTLCounterSamplingPointAtBlitBoundary
 Counter sampling at blit boundary is supported, blit encoder method sampleCountersInBuffer can be used for sampling.
*/
typedef NS_ENUM(NSUInteger, MTLCounterSamplingPoint)
{
    MTLCounterSamplingPointAtStageBoundary,
    MTLCounterSamplingPointAtDrawBoundary,
    MTLCounterSamplingPointAtDispatchBoundary,
    MTLCounterSamplingPointAtTileDispatchBoundary,
    MTLCounterSamplingPointAtBlitBoundary
} API_AVAILABLE(macos(11.0), ios(14.0));


/// Enumeration of kinds of errors that methods on MTLDevice can produce.
typedef NS_ENUM(NSInteger, MTLDeviceError)
{
    /// Indicates the absence of any problems.
    MTLDeviceErrorNone         = 0,
    
    /// Indicates the requested feature is not supported by the current device.
    MTLDeviceErrorNotSupported = 1,
} API_AVAILABLE(macos(26.4), ios(26.4));

API_AVAILABLE(macos(26.4), ios(26.4))
MTL_EXTERN NSErrorDomain const MTLDeviceErrorDomain;


/*!
 @abstract Represent a memory size and alignment in bytes.
 */
typedef struct {
    NSUInteger size;
    NSUInteger align;
} MTLSizeAndAlign;



/*!
 * @class MTLArgumentDescriptor
 * @abstract Represents a member of an argument buffer
 */
MTL_EXPORT API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLArgumentDescriptor : NSObject <NSCopying>

/*!
 * @method argumentDescriptor
 * @abstract Create an autoreleased default argument descriptor
 */
+ (MTLArgumentDescriptor *)argumentDescriptor;

/*!
 * @property dataType
 * @abstract For constants, the data type. Otherwise, MTLDataTypeTexture, MTLDataTypeSampler, or
 * MTLDataTypePointer.
 */
@property (nonatomic) MTLDataType dataType;

/*!
 * @property index
 * @abstract The binding point index of the argument
 */
@property (nonatomic) NSUInteger index;

/*!
 * @property arrayLength
 * @abstract The length of an array of constants, textures, or samplers, or 0 for non-array arguments
 */
@property (nonatomic) NSUInteger arrayLength;

/*!
 * @property access
 * @abstract Access flags for the argument
 */
@property (nonatomic) MTLBindingAccess access;

/*!
 * @property textureType
 * @abstract For texture arguments, the texture type
 */
@property (nonatomic) MTLTextureType textureType;

/*!
 @property constantBlockAlignment
 @abstract if set forces the constant block to be aligned to the given alignment
 @discussion Should only be set on the first constant of the block and is only valid if a corresponding
     explicit "alignas" is applied to the constant in the metal shader language.
 */
@property (nonatomic) NSUInteger constantBlockAlignment;

@end


/*!
 @class MTLArchitecture
 @abstract Contains information about the device's architecture
 */
MTL_EXPORT API_AVAILABLE(macos(14.0), ios(17.0))
@interface MTLArchitecture : NSObject <NSCopying>
/*!
 @property name
 @abstract The device's architecture name.
 */
@property (readonly, nonnull) NSString *name;
@end

/*!
 @protocol MTLDevice
 @abstract MTLDevice represents a processor capable of data parallel computations
 */
API_AVAILABLE(macos(10.11), ios(8.0)) NS_SWIFT_SENDABLE
@protocol MTLDevice <NSObject>

/*!
 @property name
 @abstract The full name of the vendor device.
 */
@property (nonnull, readonly) NSString *name;

/*!
 @property registryID
 @abstract Returns the IORegistry ID for the Metal device
 @discussion The registryID value for a Metal device is global to all tasks, and may be used
 to identify the GPU across task boundaries.
*/
@property (readonly) uint64_t registryID API_AVAILABLE(macos(10.13), ios(11.0)) ;

/*!
 @property architecture
 @abstract Returns the device's architecture information.
 */
@property (nonnull, readonly) MTLArchitecture *architecture API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 @property maxThreadsPerThreadgroup
 @abstract The maximum number of threads along each dimension.
 */
@property (readonly) MTLSize maxThreadsPerThreadgroup API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @property lowPower
 @abstract On systems that support automatic graphics switching, this will return YES for the the low power device.
 */
@property (readonly, getter=isLowPower) BOOL lowPower API_DEPRECATED("Not applicable on Apple Silicon", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @property headless
 @abstract On systems that include more that one GPU, this will return YES for any device that does not support any displays.  Only available on Mac OS X.
 */
@property (readonly, getter=isHeadless) BOOL headless API_DEPRECATED("Not applicable on Apple Silicon", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @property removable
 @abstract If this GPU is removable, this property will return YES.  
 @discussion If a GPU is is removed without warning, APIs may fail even with good input, even before a notification can get posted informing
 the application that the device has been removed.
 */
@property (readonly, getter=isRemovable) BOOL removable API_DEPRECATED("Not applicable on Apple Silicon", macos(10.13, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @property hasUnifiedMemory
 @abstract Returns YES if this GPU shares its memory with the rest of the machine (CPU, etc.)
 @discussion Some GPU architectures do not have dedicated local memory and instead only use the same memory shared with the rest
 of the machine.  This property will return YES for GPUs that fall into that category.
*/
@property (readonly) BOOL hasUnifiedMemory API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @property recommendedMaxWorkingSetSize
 @abstract Returns an approximation of how much memory this device can use with good performance.
 @discussion Performance may be improved by keeping the total size of all resources (texture and buffers)
 and heaps less than this threshold, beyond which the device is likely to be overcommitted and incur a
 performance penalty. */
@property (readonly) uint64_t recommendedMaxWorkingSetSize API_AVAILABLE(macos(10.12), macCatalyst(13.0), ios(16.0));


/*!
 @property depth24Stencil8PixelFormatSupported
 @abstract If YES, device supports MTLPixelFormatDepth24Unorm_Stencil8.
 */
@property (readonly, getter=isDepth24Stencil8PixelFormatSupported) BOOL depth24Stencil8PixelFormatSupported API_DEPRECATED("Never supported on Apple Silicon", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios);

/*!
 @property readWriteTextureSupport
 @abstract Query support tier for read-write texture formats.
 @return MTLReadWriteTextureTier enum value.
 */
@property (readonly) MTLReadWriteTextureTier readWriteTextureSupport API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @property argumentBuffersSupport
 @abstract Query support tier for Argument Buffers.
 @return MTLArgumentBuffersTier enum value.
 */
@property (readonly) MTLArgumentBuffersTier argumentBuffersSupport API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @property rasterOrderGroupsSupported
 @abstract Query device for raster order groups support.
 @return BOOL value. If YES, the device supports raster order groups. If NO, the device does not.
 */
@property (readonly, getter=areRasterOrderGroupsSupported) BOOL rasterOrderGroupsSupported API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @property supports32BitFloatFiltering
 @abstract Query device for 32-bit Float texture filtering support. Specifically, R32Float, RG32Float, and RGBA32Float.
 @return BOOL value. If YES, the device supports filtering 32-bit Float textures. If NO, the device does not.
 */
@property(readonly) BOOL supports32BitFloatFiltering API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @property supports32BitMSAA
 @abstract Query device for 32-bit MSAA texture support. Specifically, added support for allocating 32-bit Integer format textures (R32Uint, R32Sint, RG32Uint, RG32Sint, RGBA32Uint, and RGBA32Sint) and resolving 32-bit Float format textures (R32Float, RG32Float, and RGBA32Float).
 @return BOOL value. If YES, the device supports these additional 32-bit MSAA texture capabilities. If NO, the devices does not.
 */
@property(readonly) BOOL supports32BitMSAA API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
@property supportsQueryTextureLOD
@abstract Query device for whether it supports the `calculate_clampled_lod` and `calculate_unclamped_lod` Metal shading language functionality.
@return BOOL value. If YES, the device supports the calculate LOD functionality. If NO, the device does not.
*/
@property (readonly) BOOL supportsQueryTextureLOD API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @property supportsBCTextureCompression
 @abstract Query device for BC Texture format support
 @return BOOL value. If YES, the device supports compressed BC Texture formats. If NO, the device does not.
 */
 @property (readonly) BOOL supportsBCTextureCompression API_AVAILABLE(macos(11.0), ios(16.4));

/*!
 @property supportsPullModelInterpolation
 @abstract Query device for pull model interpolation support which allows a fragment shader to compute multiple interpolations (at center, at centroid, at offset, at sample) of a fragment input.
 @return BOOL value. If YES, the device supports pull model interpolation. If NO, the device does not.
 */
@property(readonly) BOOL supportsPullModelInterpolation API_AVAILABLE(macos(11.0),ios(14.0));

/*!
 @property barycentricsSupported
 @abstract Query device for Barycentric coordinates support; deprecated, use supportsShaderBarycentricCoordinates
 @return BOOL value. If YES, the device barycentric coordinates
 */
@property(readonly, getter=areBarycentricCoordsSupported) BOOL barycentricCoordsSupported API_DEPRECATED_WITH_REPLACEMENT("supportsShaderBarycentricCoordinates", macos(10.15, 13.0), ios(14.0, 16.0)) API_UNAVAILABLE(tvos);

/*!
 @property supportsShaderBarycentricCoordinates
 @abstract Query device for Barycentric Coordinates support.
 @return BOOL value. If YES, the device supports barycentric coordinates. If NO, the device does not.
 */
 @property (readonly) BOOL supportsShaderBarycentricCoordinates API_AVAILABLE(macos(10.15), ios(14.0), tvos(16.0));

/*!
 @property currentAllocatedSize
 @abstract The current size in bytes of all resources allocated by this device
 */
@property (readonly) NSUInteger currentAllocatedSize API_AVAILABLE(macos(10.13), ios(11.0));



/*!
 @method newLogStateWithDescriptor
 @abstract This method will create a new MTLLogState.
 */
- (nullable id <MTLLogState>) newLogStateWithDescriptor:(MTLLogStateDescriptor* _Nonnull) descriptor error:(NSError** )error API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @method newCommandQueue
 @brief Create and return a new command queue.   Command Queues created via this method will only allow up to 64 non-completed command buffers.
 @return The new command queue object
 */
- (nullable id <MTLCommandQueue>)newCommandQueue;

/*!
 @method newCommandQueueWithMaxCommandBufferCount
 @brief Create and return a new command queue with a given upper bound on non-completed command buffers.
 @return The new command queue object
 */
- (nullable id <MTLCommandQueue>)newCommandQueueWithMaxCommandBufferCount:(NSUInteger)maxCommandBufferCount;

/*!
 @method newCommandQueueWithDescriptor:
 @brief Create a MTLCommandQueue according to MTLCommandQueueDescriptor.
 */
- (nullable id <MTLCommandQueue>)newCommandQueueWithDescriptor:(MTLCommandQueueDescriptor *)descriptor API_AVAILABLE(macos(15.0), ios(18.0));

/*!
 @method heapTextureSizeAndAlignWithDescriptor:
 @abstract Determine the byte size of textures when sub-allocated from a heap.
 @discussion This method can be used to help determine the required heap size.
 */
- (MTLSizeAndAlign)heapTextureSizeAndAlignWithDescriptor:(MTLTextureDescriptor *)desc API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method heapBufferSizeAndAlignWithLength:options:
 @abstract Determine the byte size of buffers when sub-allocated from a heap.
 @discussion This method can be used to help determine the required heap size.
 */
- (MTLSizeAndAlign)heapBufferSizeAndAlignWithLength:(NSUInteger)length options:(MTLResourceOptions)options API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method newHeapWithDescriptor:
 @abstract Create a new heap with the given descriptor.
 */
- (nullable id <MTLHeap>)newHeapWithDescriptor:(MTLHeapDescriptor *)descriptor API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method newBufferWithLength:options:
 @brief Create a buffer by allocating new memory.
 */
- (nullable id <MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;

/*!
 @method newBufferWithBytes:length:options:
 @brief Create a buffer by allocating new memory and specifing the initial contents to be copied into it.
 */
- (nullable id <MTLBuffer>)newBufferWithBytes:(const void *)pointer length:(NSUInteger)length options:(MTLResourceOptions)options;

/*!
 @method newBufferWithBytesNoCopy:length:options:deallocator:
 @brief Create a buffer by wrapping an existing part of the address space.
 */
- (nullable id <MTLBuffer>)newBufferWithBytesNoCopy:(void *)pointer length:(NSUInteger)length options:(MTLResourceOptions)options deallocator:(void (^ NS_SWIFT_SENDABLE __nullable)(void *pointer, NSUInteger length))deallocator;

/*!
 @method newDepthStencilStateWithDescriptor:
 @brief Create a depth/stencil test state object.
 */
- (nullable id <MTLDepthStencilState>)newDepthStencilStateWithDescriptor:(MTLDepthStencilDescriptor *)descriptor;

/*!
 @method newTextureWithDescriptor:
 @abstract Allocate a new texture with privately owned storage.
 */
- (nullable id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor;

/*!
 @method newTextureWithDescriptor:iosurface:plane
 @abstract Create a new texture from an IOSurface.
 @param descriptor A description of the properties for the new texture.
 @param iosurface The IOSurface to use as storage for the new texture.
 @param plane The plane within the IOSurface to use.
 @return A new texture object.
 */
- (nullable id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)descriptor iosurface:(IOSurfaceRef)iosurface plane:(NSUInteger)plane API_AVAILABLE(macos(10.11), ios(11.0));


/*!
 @method newSharedTextureWithDescriptor
 @abstract Create a new texture that can be shared across process boundaries.
 @discussion This texture can be shared between process boundaries
 but not between different GPUs, by passing its MTLSharedTextureHandle.
 @param descriptor A description of the properties for the new texture.
 @return A new texture object.
 */
- (nullable id <MTLTexture>)newSharedTextureWithDescriptor:(MTLTextureDescriptor *)descriptor API_AVAILABLE(macos(10.14), ios(13.0));

/*!
 @method newSharedTextureWithHandle
 @abstract Recreate shared texture from received texture handle.
 @discussion This texture was shared between process boundaries by other
 process using MTLSharedTextureHandle. Current process will now share
 it with other processes and will be able to interact with it (but still
 in scope of the same GPUs).
 @param sharedHandle Handle to shared texture in this process space.
 @return A new texture object.
 */
- (nullable id <MTLTexture>)newSharedTextureWithHandle:(MTLSharedTextureHandle *)sharedHandle API_AVAILABLE(macos(10.14), ios(13.0));

/*!
 @method newSamplerStateWithDescriptor:
 @abstract Create a new sampler.
*/
- (nullable id <MTLSamplerState>)newSamplerStateWithDescriptor:(MTLSamplerDescriptor *)descriptor;

/*!
 @method newDefaultLibrary
 @abstract Returns the default library for the main bundle.
 @discussion use newDefaultLibraryWithBundle:error: to get an NSError in case of failure.
 */
- (nullable id <MTLLibrary>)newDefaultLibrary;

/*
 @method newDefaultLibraryWithBundle:error:
 @abstract Returns the default library for a given bundle.
 @return A pointer to the library, nil if an error occurs.
*/
- (nullable id <MTLLibrary>)newDefaultLibraryWithBundle:(NSBundle *)bundle error:(__autoreleasing NSError **)error API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method newLibraryWithFile:
 @abstract Load a MTLLibrary from a metallib file.
 */
- (nullable id <MTLLibrary>)newLibraryWithFile:(NSString *)filepath error:(__autoreleasing NSError **)error
API_DEPRECATED("Use -newLibraryWithURL:error: instead", macos(10.11, 13.0), ios(8.0, 16.0));

/*!
 @method newLibraryWithURL:
 @abstract Load a MTLLibrary from a metallib file.
 */
- (nullable id <MTLLibrary>)newLibraryWithURL:(NSURL *)url error:(__autoreleasing NSError **)error API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @method newLibraryWithData:
 @abstract Load a MTLLibrary from a dispatch_data_t
 @param data A metallib file already loaded as data in the form of dispatch_data_t.
 @param error An error if we fail to open the metallib data.
 */
- (nullable id <MTLLibrary>)newLibraryWithData:(dispatch_data_t)data error:(__autoreleasing NSError **)error;

/*!
 @method newLibraryWithSource:options:error:
 @abstract Load a MTLLibrary from source.
 */
- (nullable id <MTLLibrary>)newLibraryWithSource:(NSString *)source options:(nullable MTLCompileOptions *)options error:(__autoreleasing NSError **)error;

/*!
 @method newLibraryWithSource:options:completionHandler:
 @abstract Load a MTLLibrary from source.
 */
- (void)newLibraryWithSource:(NSString *)source options:(nullable MTLCompileOptions *)options completionHandler:(MTLNewLibraryCompletionHandler)completionHandler;

/*!
 @method newLibraryWithStitchedDescriptor:error:
 @abstract Returns a library generated using the graphs in the descriptor.
 */
- (nullable id <MTLLibrary>)newLibraryWithStitchedDescriptor:(MTLStitchedLibraryDescriptor *)descriptor error:(__autoreleasing NSError **)error API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @method newLibraryWithStitchedDescriptor:completionHandler:
 @abstract Generates a new library using the graphs in the descriptor.
 */
- (void)newLibraryWithStitchedDescriptor:(MTLStitchedLibraryDescriptor *)descriptor completionHandler:(MTLNewLibraryCompletionHandler)completionHandler API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @method newRenderPipelineStateWithDescriptor:error:
 @abstract Create and compile a new MTLRenderPipelineState object synchronously.
 */
- (nullable id <MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor error:(__autoreleasing NSError **)error;

/*!
 @method newRenderPipelineStateWithDescriptor:options:reflection:error:
 @abstract Create and compile a new MTLRenderPipelineState object synchronously and returns additional reflection information.
 */
- (nullable id <MTLRenderPipelineState>)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)options reflection:(MTLAutoreleasedRenderPipelineReflection * __nullable)reflection error:(__autoreleasing NSError **)error;

/*!
 @method newRenderPipelineState:completionHandler:
 @abstract Create and compile a new MTLRenderPipelineState object asynchronously.
 */
- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor completionHandler:(MTLNewRenderPipelineStateCompletionHandler)completionHandler;

/*!
 @method newRenderPipelineState:options:completionHandler:
 @abstract Create and compile a new MTLRenderPipelineState object asynchronously and returns additional reflection information
 */
- (void)newRenderPipelineStateWithDescriptor:(MTLRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)options completionHandler:(MTLNewRenderPipelineStateWithReflectionCompletionHandler)completionHandler;

/*!
 @method newComputePipelineStateWithDescriptor:error:
 @abstract Create and compile a new MTLComputePipelineState object synchronously.
 */
- (nullable id <MTLComputePipelineState>)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction error:(__autoreleasing NSError **)error;

/*!
 @method newComputePipelineStateWithDescriptor:options:reflection:error:
 @abstract Create and compile a new MTLComputePipelineState object synchronously.
 */
- (nullable id <MTLComputePipelineState>)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction options:(MTLPipelineOption)options reflection:(MTLAutoreleasedComputePipelineReflection * __nullable)reflection error:(__autoreleasing NSError **)error;

/*!
 @method newComputePipelineStateWithDescriptor:completionHandler:
 @abstract Create and compile a new MTLComputePipelineState object asynchronously.
 */
- (void)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction completionHandler:(MTLNewComputePipelineStateCompletionHandler)completionHandler;

/*!
 @method newComputePipelineStateWithDescriptor:options:completionHandler:
 @abstract Create and compile a new MTLComputePipelineState object asynchronously.
 */
- (void)newComputePipelineStateWithFunction:(id <MTLFunction>)computeFunction options:(MTLPipelineOption)options completionHandler:(MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler;

/*!
 @method newComputePipelineStateWithDescriptor:options:reflection:error:
 @abstract Create and compile a new MTLComputePipelineState object synchronously.
 */
- (nullable id <MTLComputePipelineState>)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor options:(MTLPipelineOption)options reflection:(MTLAutoreleasedComputePipelineReflection * __nullable)reflection error:(__autoreleasing NSError **)error API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method newComputePipelineStateWithDescriptor:options:completionHandler:
 @abstract Create and compile a new MTLComputePipelineState object asynchronously.
 */
- (void)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor options:(MTLPipelineOption)options completionHandler:(MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method newFence
 @abstract Create a new MTLFence object
 */
- (nullable id <MTLFence>)newFence API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method supportsFeatureSet:
 @abstract Returns TRUE if the feature set is supported by this MTLDevice.
 */
- (BOOL)supportsFeatureSet:(MTLFeatureSet)featureSet API_DEPRECATED("Use supportsFamily instead", macos(10.11, 13.0), ios(8.0, 16.0), tvos(9.0, 16.0));

/*!
 @method supportsFamily:
 @abstract Returns TRUE if the GPU Family is supported by this MTLDevice.
 */
- (BOOL)supportsFamily:(MTLGPUFamily)gpuFamily API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 @method supportsTextureSampleCount:
 @brief Query device if it support textures with a given sampleCount.
 @return BOOL value. If YES, device supports the given sampleCount for textures. If NO, device does not support the given sampleCount.
 */
- (BOOL)supportsTextureSampleCount:(NSUInteger)sampleCount API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method minimumLinearTextureAlignmentForPixelFormat:
 @abstract Returns the minimum alignment required for offset and rowBytes when creating a linear texture. An error is thrown for queries with invalid pixel formats (depth, stencil, or compressed formats).
 */
- (NSUInteger)minimumLinearTextureAlignmentForPixelFormat:(MTLPixelFormat)format API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @method minimumTextureBufferAlignmentForPixelFormat:
 @abstract Returns the minimum alignment required for offset and rowBytes when creating a texture buffer from a buffer.
 */
- (NSUInteger)minimumTextureBufferAlignmentForPixelFormat:(MTLPixelFormat)format API_AVAILABLE(macos(10.14), ios(12.0));


/*!
 @method newRenderPipelineStateWithTileDescriptor:options:reflection:error:
 @abstract Create and compile a new MTLRenderPipelineState object synchronously given a MTLTileRenderPipelineDescriptor.
 */
- (nullable id <MTLRenderPipelineState>)newRenderPipelineStateWithTileDescriptor:(MTLTileRenderPipelineDescriptor*)descriptor options:(MTLPipelineOption)options reflection:(MTLAutoreleasedRenderPipelineReflection * __nullable)reflection error:(__autoreleasing NSError **)error API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));


/*!
 @method newRenderPipelineStateWithTileDescriptor:options:completionHandler:
 @abstract Create and compile a new MTLRenderPipelineState object asynchronously given a MTLTileRenderPipelineDescriptor.
 */
- (void)newRenderPipelineStateWithTileDescriptor:(MTLTileRenderPipelineDescriptor *)descriptor options:(MTLPipelineOption)options completionHandler:(MTLNewRenderPipelineStateWithReflectionCompletionHandler)completionHandler API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method newRenderPipelineStateWithMeshDescriptor:options:reflection:error:
 @abstract Create and compile a new MTLRenderPipelineState object synchronously given a MTLMeshRenderPipelineDescriptor.
 */
- (nullable id <MTLRenderPipelineState>)newRenderPipelineStateWithMeshDescriptor:(MTLMeshRenderPipelineDescriptor*)descriptor
                                                                         options:(MTLPipelineOption)options
                                                                      reflection:(MTLAutoreleasedRenderPipelineReflection * __nullable)reflection
                                                                           error:(__autoreleasing NSError **)error API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method newRenderPipelineStateWithMeshDescriptor:options:completionHandler:
 @abstract Create and compile a new MTLRenderPipelineState object asynchronously given a MTLMeshRenderPipelineDescriptor.
 */
- (void)newRenderPipelineStateWithMeshDescriptor:(MTLMeshRenderPipelineDescriptor *)descriptor
                                         options:(MTLPipelineOption)options
                               completionHandler:(MTLNewRenderPipelineStateWithReflectionCompletionHandler)completionHandler API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @property maxThreadgroupMemoryLength
 @abstract The maximum threadgroup memory available, in bytes.
 */
@property (readonly) NSUInteger maxThreadgroupMemoryLength API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @property maxArgumentBufferSamplerCount
 @abstract The maximum number of unique argument buffer samplers per app.
 @discussion This limit is only applicable to samplers that have their supportArgumentBuffers property set to true. A MTLSamplerState object is considered unique if the configuration of its originating MTLSamplerDescriptor properties is unique. For example, two samplers with equal minFilter values but different magFilter values are considered unique.
 */
@property (readonly) NSUInteger maxArgumentBufferSamplerCount API_AVAILABLE(macos(10.14), ios(12.0));


/*!
 @property programmableSaplePositionsSupported
 @abstract Query device for programmable sample position support.
 @return BOOL value. If YES, the device supports programmable sample positions. If NO, the device does not.
 */
@property (readonly, getter=areProgrammableSamplePositionsSupported) BOOL programmableSamplePositionsSupported API_AVAILABLE(macos(10.13), ios(11.0));


/*!
 @method getDefaultSamplePositions:count:
 @abstract Retrieve the default sample positions.
 @param positions The destination array for default sample position data.
 @param count Specifies the sample count for which to retrieve the default positions, the length of the positions array, and must be set to a valid sample count.
 */
- (void)getDefaultSamplePositions:(MTLSamplePosition *)positions count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 * @method newArgumentEncoderWithArguments:count:
 * @abstract Creates an argument encoder for an array of argument descriptors which will be encoded sequentially.
 */
- (nullable id <MTLArgumentEncoder>)newArgumentEncoderWithArguments:(NSArray <MTLArgumentDescriptor *> *)arguments API_AVAILABLE(macos(10.13), ios(11.0));


/*!
 @method supportsRasterizationRateMapWithLayerCount:
 @abstract Query device for variable rasterization rate support with the given number of layers.
 @param layerCount The number of layers for which to query device support.
 @return YES if the device supports creation of rendering using a MTLRasterizationRateMap with the given number of layers.
 */
-(BOOL)supportsRasterizationRateMapWithLayerCount:(NSUInteger)layerCount API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

/*!
 @method newRasterizationRateMapWithDescriptor:
 @abstract Creates a new variable rasterization rate map with the given descriptor.
 @discussion If '[self supportsRasterizationRateMapWithLayerCount:descriptor.layerCount]' returns NO, or descriptor.screenSize describes an empty region, the result will always be nil.
 @return A MTLRasterizationRateMap instance that can be used for rendering on this MTLDevice, or nil if the device does not support the combination of parameters stored in the descriptor.
 */
-(nullable id<MTLRasterizationRateMap>)newRasterizationRateMapWithDescriptor:(MTLRasterizationRateMapDescriptor*)descriptor API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

/*!
 * @method newIndirectCommandBufferWithDescriptor:maxCommandCount:options
 * @abstract Creates a new indirect command buffer with the given descriptor and count.
 * @param descriptor The descriptor encodes the maximum logical stride of each command.
 * @param maxCount The maximum number of commands that this buffer can contain.
 * @param options The options for the indirect command buffer.
 * @discussion The returned buffer can be safely executed without first encoding into (but is wasteful).
 */
- (nullable id <MTLIndirectCommandBuffer>)newIndirectCommandBufferWithDescriptor:(MTLIndirectCommandBufferDescriptor*)descriptor maxCommandCount:(NSUInteger)maxCount options:(MTLResourceOptions)options API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method newEvent
 @abstract Returns a new single-device non-shareable Metal event object
*/
- (nullable id <MTLEvent>)newEvent API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method newSharedEvent
 @abstract Returns a shareable multi-device event.
 */
- (nullable id <MTLSharedEvent>)newSharedEvent API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @method newSharedEventWithHandle
 @abstract Creates a shareable multi-device event from an existing shared event handle.
*/
- (nullable id <MTLSharedEvent>)newSharedEventWithHandle:(MTLSharedEventHandle *)sharedEventHandle API_AVAILABLE(macos(10.14), ios(12.0));






/*!
 * @method newIOHandleWithURL:url:error:
 * @abstract Create and return a handle that points to a raw file on disk. This object can be used by
 * MTLIOCommandBuffer load commands to source data for MTLResources. If the creation
 * of the handle fails the return value will be nil and the optional error if passed in will be non-nil
 * with details of the error.
 */
-(nullable id<MTLIOFileHandle>)newIOHandleWithURL:(NSURL *)url
                                            error:(NSError **)error API_DEPRECATED_WITH_REPLACEMENT("Use newIOFileHandleWithURL:error: instead", macos(13.0, 14.0), ios(16.0, 17.0));


/*!
 * @method newIOCommandQueueWithDescriptor:descriptor:error:
 * @abstract Create and return an IO queue. If the creation
 * of the queue fails the return value will be nil and the optional error if passed in will be non-nil
 * with details of the error.
 */
-(nullable id<MTLIOCommandQueue>)newIOCommandQueueWithDescriptor:(MTLIOCommandQueueDescriptor*)descriptor
                                                           error:(NSError **)error API_AVAILABLE(macos(13.0), ios(16.0));


/*!
 * @method newIOHandleWithURL:url:compressionMethod:error:
 * @abstract Create and return a handle that points to a compressed file on disk (a file that was
 * created with MTLIOCompressionContext). This object can be used by
 * MTLIOCommandBuffer load commands to source data for MTLResources. If the creation
 * of the handle fails the return value will be nil and the optional error if passed in will be non-nil
 * with details of the error.
 */
-(nullable id<MTLIOFileHandle>)newIOHandleWithURL:(NSURL *)url
                                compressionMethod:(MTLIOCompressionMethod)compressionMethod
                                            error:(NSError **)error API_DEPRECATED_WITH_REPLACEMENT("Use newIOFileHandleWithURL:compressionMethod:error: instead", macos(13.0, 14.0), ios(16.0, 17.0));

/*!
 * @method newIOFileHandleWithURL:error:
 * @abstract Create and return a handle that points to a raw file on disk. This object can be used by
 * MTLIOCommandBuffer load commands to source data for MTLResources. If the creation
 * of the handle fails the return value will be nil and the optional error if passed in will be non-nil
 * with details of the error.
 */
-(nullable id<MTLIOFileHandle>)newIOFileHandleWithURL:(NSURL *)url
                                                error:(NSError **)error API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 * @method newIOFileHandleWithURL:compressionMethod:error:
 * @abstract Create and return a handle that points to a compressed file on disk (a file that was
 * created with MTLIOCompressionContext). This object can be used by
 * MTLIOCommandBuffer load commands to source data for MTLResources. If the creation
 * of the handle fails the return value will be nil and the optional error if passed in will be non-nil
 * with details of the error.
 */
-(nullable id<MTLIOFileHandle>)newIOFileHandleWithURL:(NSURL *)url
                                    compressionMethod:(MTLIOCompressionMethod)compressionMethod
                                                error:(NSError **)error API_AVAILABLE(macos(14.0), ios(17.0));



/*!
 * @method sparseTileSizeWithTextureType:pixelFormat:sampleCount:
 * @abstract Returns tile size for sparse texture with given type, pixel format and sample count.
 */
-(MTLSize) sparseTileSizeWithTextureType:(MTLTextureType)textureType
                             pixelFormat:(MTLPixelFormat)pixelFormat
                             sampleCount:(NSUInteger)sampleCount API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @property sparseTileSizeInBytes
 @abstract Returns the number of bytes required to map one sparse texture tile.
 */
@property (readonly) NSUInteger sparseTileSizeInBytes API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

@optional
/*!
 * @method convertSparsePixelRegions:toTileRegions:withTileSize:alignmentMode:numRegions:
 * @abstract Converts regions in pixels to regions in sparse tiles using specified alignment mode.
   Tile size can be obtained from tileSizeWithTextureType:pixelFormat:sampleCount: method.
 */
-(void) convertSparsePixelRegions:(const MTLRegion[_Nonnull])pixelRegions
                    toTileRegions:(MTLRegion[_Nonnull])tileRegions
                     withTileSize:(MTLSize)tileSize
                    alignmentMode:(MTLSparseTextureRegionAlignmentMode)mode
                       numRegions:(NSUInteger)numRegions API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 * @method convertSparseTileRegions:toPixelRegions:withTileSize:numRegions:
 * @abstract Convertes region in sparse tiles to region in pixels
   Tile size can be obtained from tileSizeWithTextureType:pixelFormat:sampleCount: method.
 */
-(void) convertSparseTileRegions:(const MTLRegion[_Nonnull])tileRegions
                  toPixelRegions:(MTLRegion[_Nonnull])pixelRegions
                    withTileSize:(MTLSize)tileSize
                      numRegions:(NSUInteger)numRegions API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));
@required


/*!
 @property sparseTileSizeInBytesForSparsePageSize
 @abstract Returns the number of bytes required to map one sparse texture tile for a given MTLSparsePageSize
 */
- (NSUInteger) sparseTileSizeInBytesForSparsePageSize:(MTLSparsePageSize)sparsePageSize API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 * @method sparseTileSizeWithTextureType:pixelFormat:sampleCount:sparsePageSize
 * @abstract Returns tile size for sparse texture with given type, pixel format and sample count.
 */
-(MTLSize) sparseTileSizeWithTextureType:(MTLTextureType)textureType
                             pixelFormat:(MTLPixelFormat)pixelFormat
                             sampleCount:(NSUInteger)sampleCount
                          sparsePageSize:(MTLSparsePageSize)sparsePageSize API_AVAILABLE(macos(13.0), ios(16.0));



@property (readonly) NSUInteger maxBufferLength API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 @property counterSets
 @abstract Returns the set of Counter Sets exposed by the device.
 */
@property (readonly, nullable) NSArray<id<MTLCounterSet>>* counterSets API_AVAILABLE(macos(10.15), ios(14.0));


/*!
 @method newCounterSampleBufferWithDescriptor:error:
 @abstract Given a counter sample buffer descriptor, allocate a new counter
 sample buffer.
 This may return nil if the counters may not all be collected simultaneously.
 @param descriptor The descriptor to create a sample buffer for
 @param error An error return on failure.
 */
- (nullable id<MTLCounterSampleBuffer>) newCounterSampleBufferWithDescriptor:(MTLCounterSampleBufferDescriptor*)descriptor
                                                              error:(NSError**)error
    API_AVAILABLE(macos(10.15), ios(14.0));

#if defined(MTL_TIMESTAMP_AS_NSUINTEGER) && MTL_TIMESTAMP_AS_NSUINTEGER
typedef NSUInteger MTLTimestamp;
#else
typedef uint64_t MTLTimestamp;
#endif
/*!
 @method sampleTimestamps:gpuTimestamp:
 @abstract Sample the CPU and GPU timestamps as closely as possible.
 @param cpuTimestamp The timestamp on the CPU
 @param gpuTimestamp The timestamp on the GPU
 */
-(void)sampleTimestamps:(MTLTimestamp *)cpuTimestamp
           gpuTimestamp:(MTLTimestamp *)gpuTimestamp
    API_AVAILABLE(macos(10.15), ios(14.0));


-(id<MTLArgumentEncoder>)newArgumentEncoderWithBufferBinding:(id<MTLBufferBinding>)bufferBinding API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method supportsCounterSampling:
 @abstract Query device for counter sampling points support.
 @param samplingPoint Query index
 @return BOOL value. If YES, the device supports counter sampling at given point.
*/
- (BOOL)supportsCounterSampling:(MTLCounterSamplingPoint)samplingPoint API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property supportsVertexAmplificationCount:
 @abstract Query device for vertex amplification support.
 @param count The amplification count to check
 @return BOOL value. If YES, the device supports vertex amplification with the given count. If NO, the device does not.
 */
- (BOOL)supportsVertexAmplificationCount:(NSUInteger)count API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

/*!
 @property supportsDynamicLibraries
 @abstract Query device support for creating and using dynamic libraries in a compute pipeline.
 @return BOOL value. If YES, the device supports creating and using dynamic libraries in a compute pipeline. If NO, the device does not.
 */
@property(readonly) BOOL supportsDynamicLibraries API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @property supportsRenderDynamicLibraries
 @abstract Query device support for creating and using dynamic libraries in render pipeline stages.
 @return BOOL value. If YES, the device supports creating and using dynamic libraries in render pipeline stages. If NO, the device does not.
 */
@property (readonly) BOOL supportsRenderDynamicLibraries API_AVAILABLE(macos(12.0), ios(15.0));

/*!
 @method newDynamicLibrary:error:
 @abstract Creates a MTLDynamicLibrary by compiling the code in a MTLLibrary.
 @see MTLDynamicLibrary
 @param library The MTLLibrary from which to compile code. This library must have .type set to MTLLibraryTypeDynamic.
 @param error If an error occurs during creation, this parameter is updated to describe the failure.
 @return On success, the MTLDynamicLibrary containing compiled code. On failure, nil.
 */
- (nullable id<MTLDynamicLibrary>) newDynamicLibrary:(id<MTLLibrary>)library error:(NSError **) error API_AVAILABLE(macos(11.0), ios(14.0));

/*!
 @method newDynamicLibraryWithURL:error:
 @abstract Creates a MTLDynamicLibrary by loading compiled code from a file.
 @see MTLDynamicLibrary
 @param url The file URL from which to load. If the file contains no compiled code for this device, compilation is attempted as with newDynamicLibrary:error:
 @param error If an error occurs during creation, this parameter is updated to describe the failure.
 @return On success, the MTLDynamicLibrary containing compiled code (either loaded or compiled). On failure, nil.
 */
- (nullable id<MTLDynamicLibrary>) newDynamicLibraryWithURL:(NSURL *)url error:(NSError **) error API_AVAILABLE(macos(11.0), ios(14.0));
/*!
 @method newBinaryArchiveWithDescriptor:error:
 @abstract Creates a MTLBinaryArchive using the configuration in the descriptor.
 @see MTLBinaryArchive
 @param descriptor The descriptor for the configuration of the binary archive to create.
 @param error If an error occurs during creation, this parameter is updated to describe the failure.
 @return On success, the created MTLBinaryArchive. On failure, nil.
 */
- (nullable id<MTLBinaryArchive>) newBinaryArchiveWithDescriptor:(MTLBinaryArchiveDescriptor*)descriptor
                                                           error:(NSError**)error API_AVAILABLE(macos(11.0), ios(14.0));

/// A Boolean value that indicates whether the device supports placement sparse resources.
@property (readonly) BOOL supportsPlacementSparse API_AVAILABLE(macos(26.4), ios(26.4));
/*!
@property supportsRaytracing
@abstract Query device support for using ray tracing from compute pipelines.
@return BOOL value. If YES, the device supports ray tracing from compute pipelines. If NO, the device does not.
*/
@property (readonly) BOOL supportsRaytracing API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

- (MTLAccelerationStructureSizes)accelerationStructureSizesWithDescriptor:(MTLAccelerationStructureDescriptor *)descriptor API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

- (nullable id <MTLAccelerationStructure>)newAccelerationStructureWithSize:(NSUInteger)size API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));
- (nullable id <MTLAccelerationStructure>)newAccelerationStructureWithDescriptor:(MTLAccelerationStructureDescriptor *)descriptor API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


/*!
 @method heapAccelerationStructureSizeAndAlignWithSize:
 @abstract Determine the byte size of acceleration structures when sub-allocated from a heap.
 @discussion This method can be used to help determine the required heap size.
 */
- (MTLSizeAndAlign)heapAccelerationStructureSizeAndAlignWithSize:(NSUInteger)size API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method heapAccelerationStructureSizeAndAlignWithDescriptor:
 @abstract Determine the byte size of acceleration structures when sub-allocated from a heap. This is a convenience method which computes the acceleration structure size based on the descriptor.
 @discussion This method can be used to help determine the required heap size.
 */
- (MTLSizeAndAlign)heapAccelerationStructureSizeAndAlignWithDescriptor:(MTLAccelerationStructureDescriptor *)descriptor API_AVAILABLE(macos(13.0), ios(16.0));


/*!
 @property supportsFunctionPointers
 @abstract Query device support for using function pointers from compute pipelines.
 @return BOOL value. If YES, the device supports function pointers from compute pipelines. If NO, the device does not.
 */
@property (readonly) BOOL supportsFunctionPointers API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
 @property supportsFunctionPointersFromRender
 @abstract Query device support for using function pointers from render pipeline stages.
 @return BOOL value. If YES, the device supports function pointers from render pipeline stages. If NO, the device does not.
 */
@property (readonly) BOOL supportsFunctionPointersFromRender API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property supportsRaytracingFromRender
 @abstract Query device support for using ray tracing from render pipeline stages.
 @return BOOL value. If YES, the device supports ray tracing from render pipeline stages. If NO, the device does not.
 */
@property (readonly) BOOL supportsRaytracingFromRender API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property supportsPrimitiveMotionBlur
 @abstract Query device support for using ray tracing primitive motion blur.
 @return BOOL value. If YES, the device supports the primitive motion blur api. If NO, the device does not.
 */
@property (readonly) BOOL supportsPrimitiveMotionBlur API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));


/*!
 @property maximumConcurrentCompilationTaskCount
 @abstract Returns the maximum count of concurrent executing compilation tasks.
 @discussion The property returns a different value depending on the value of the property `shouldMaximizeConcurrentCompilation`.
 */
@property (readonly) NSUInteger maximumConcurrentCompilationTaskCount
        API_AVAILABLE(macos(13.3), ios(26.0));

/*!
 @method newResidencySetWithDescriptor
 @abstract Creates a new residency set with a descriptor.
 */
- (nullable id<MTLResidencySet>) newResidencySetWithDescriptor:(MTLResidencySetDescriptor *)desc
                                            error:(NSError *__nullable*)error
                                            API_AVAILABLE(macos(15.0), ios(18.0));

/// Determines the size and alignment required to hold the data plane of a tensor you create with a descriptor in a buffer.
///
/// This method requires that `descriptor` does not configure any auxiliary planes.
///
/// - Parameters:
///    - descriptor: The tensor descriptor configuring the data plane.
/// - Returns: The size and alignment required to hold the data plane of a tensor you create with `descriptor` in a buffer.
- (MTLSizeAndAlign)tensorSizeAndAlignWithDescriptor:(MTLTensorDescriptor *)descriptor API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a tensor with the specified descriptor.
///
/// This method validates the constraints documented on ``MTLTensorDescriptor``.
///
/// - Parameters:
///   - descriptor: The tensor descriptor configuring the data plane and
///     auxiliary planes.
///   - error: On failure, an NSError instance that describes the validation failure.
/// - Returns: A tensor, or `nil` if validation fails.
- (nullable id <MTLTensor>)newTensorWithDescriptor:(MTLTensorDescriptor *)descriptor
                                             error:(__autoreleasing NSError * _Nullable * _Nullable)error API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a tensor with the specified descriptor and per-plane buffer backing storage.
///
/// This method validates the constraints documented on ``MTLTensorDescriptor``
/// and ``MTLTensorBufferAttachments``, and additionally requires that every
/// plane configured in `descriptor` (data plane and all auxiliary planes) has
/// a corresponding entry in `attachments`.
///
/// - Parameters:
///   - descriptor: The tensor descriptor configuring the data plane and
///     auxiliary planes.
///   - attachments: The per-plane buffer backing storage. Must not be `nil`.
///   - error: On failure, an NSError instance that describes the validation failure.
/// - Returns: A tensor, or `nil` if validation fails.
-(nullable id<MTLTensor>)newTensorWithDescriptor:(MTLTensorDescriptor *)descriptor
                                     attachments:(MTLTensorBufferAttachments *)attachments
                                           error:(NSError **)error API_AVAILABLE(macos(27.0), ios(27.0));


/*!
 @method functionHandleWithFunction:
 @abstract Returns the function handle for a function that was compiled with MTLFunctionOptionPipelineIndependent and MTLFunctionOptionCompileToBinary.
 */
- (nullable id<MTLFunctionHandle>)functionHandleWithFunction:(id<MTLFunction>)function API_AVAILABLE(macos(26.0), ios(26.0));



/// Creates a new command allocator.
///
/// - Returns: A ``MTL4CommandAllocator`` instance, or `nil` if the function failed.
 - (nullable id<MTL4CommandAllocator>)newCommandAllocator
API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new command allocator from a command allocator descriptor.
///
/// - Parameters:
///   - descriptor: A ``MTL4CommandAllocatorDescriptor`` instance that configures the
///                 ``MTL4CommandAllocator`` instance.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTL4CommandAllocator`` instance, or `nil` if the function failed.
- (nullable id<MTL4CommandAllocator>)newCommandAllocatorWithDescriptor:(MTL4CommandAllocatorDescriptor *)descriptor
                                                                 error:(NSError**)error
API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new command queue.
///
/// - Returns: A ``MTL4CommandQueue`` instance, or `nil` if the function failed.
- (nullable id<MTL4CommandQueue>)newMTL4CommandQueue
API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new command queue from a queue descriptor.
///
/// - Parameters:
///   - descriptor: A ``MTL4CommandQueueDescriptor`` instance that configures the
///                 ``MTL4CommandQueue`` instance.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTL4CommandQueue`` instance, or `nil` if the function failed.
- (nullable id<MTL4CommandQueue>)newMTL4CommandQueueWithDescriptor:(MTL4CommandQueueDescriptor *)descriptor
                                                             error:(__autoreleasing NSError * _Nullable * _Nullable)error
API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new command buffer.
///
/// - Returns: A ``MTL4CommandBuffer`` instance, or `nil` if the function failed.
 - (nullable id<MTL4CommandBuffer>)newCommandBuffer
API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new argument table from an argument table descriptor.
///
/// - Parameters:
///   - descriptor: A ``MTL4ArgumentTableDescriptor`` instance that configures the
///                 ``MTL4ArgumentTable`` instance.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTL4ArgumentTable`` instance, or `nil` if the function failed.
- (nullable id<MTL4ArgumentTable>)newArgumentTableWithDescriptor:(MTL4ArgumentTableDescriptor *)descriptor
                                                           error:(NSError * _Nullable *)error
API_AVAILABLE(macos(26.0), ios(26.0));



/// Creates a new texture view pool from a resource view pool descriptor.
///
/// - Parameters:
///   - descriptor: A ``MTLResourceViewPoolDescriptor`` instance that configures the
///                 ``MTLTextureViewPool`` instance.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTLTextureViewPool`` instance, or `nil` if the function failed.
- (nullable id<MTLTextureViewPool>)newTextureViewPoolWithDescriptor:(MTLResourceViewPoolDescriptor *)descriptor
                                                              error:(NSError * _Nullable *)error
API_AVAILABLE(macos(26.0), ios(26.0));




/// Creates a new compiler from a compiler descriptor.
///
/// - Parameters:
///   - descriptor: A ``MTL4CompilerDescriptor`` instance that configures the
///                 ``MTL4Compiler`` instance.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTL4Compiler`` instance, or `nil` if the function failed.
- (nullable id<MTL4Compiler>)newCompilerWithDescriptor:(MTL4CompilerDescriptor *)descriptor
                                                 error:(NSError **)error
API_AVAILABLE(macos(26.0), ios(26.0));

/// Creates a new archive from data available at an `NSURL` address.
///
/// - Parameters:
///   - url:   An `NSURL` instance that represents the path from which the device loads the ``MTL4Archive``.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTL4Archive`` instance, or `nil` if the function failed.
- (nullable id<MTL4Archive>)newArchiveWithURL:(NSURL *)url
                                        error:(NSError **)error
API_AVAILABLE(macos(26.0), ios(26.0));


/// Creates a new pipeline data set serializer instance from a descriptor.
///
/// - Parameter descriptor: A ``MTL4PipelineDataSetSerializerDescriptor`` instance that configures
///                         the new ``MTL4PipelineDataSetSerializer`` instance.
///
/// - Returns: A ``MTL4PipelineDataSetSerializer`` instance, or `nil` if the function failed.
- (id<MTL4PipelineDataSetSerializer>)newPipelineDataSetSerializerWithDescriptor:
    (MTL4PipelineDataSetSerializerDescriptor *)descriptor
API_AVAILABLE(macos(26.0), ios(26.0));


/// Creates a new placement sparse buffer of a specific length.
///
/// This method creates a new placement sparse ``MTLBuffer`` of a specific length. You assign memory to
/// placement sparse buffers using a ``MTLHeap`` of type ``MTLHeapType/MTLHeapTypePlacement``.
///
/// - Parameters:
///   - length:                  The size of the ``MTLBuffer``, in bytes.
///   - options:                 A ``MTLResourceOptions`` instance that establishes the buffer’s storage modes.
///   - placementSparsePageSize: ``MTLSparsePageSize`` to use for the placement sparse buffer.
///
/// - Returns: A ``MTLBuffer`` instance, or `nil` if the function failed.
- (nullable id<MTLBuffer>)newBufferWithLength:(NSUInteger)length
                                      options:(MTLResourceOptions)options
                      placementSparsePageSize:(MTLSparsePageSize)placementSparsePageSize
API_AVAILABLE(macos(26.0), ios(26.0));


/// Creates a new counter heap configured from a counter heap descriptor.
///
/// - Parameters:
///   - descriptor: ``MTL4CounterHeapDescriptor`` instance that configures the ``MTL4CounterHeap`` instance.
///   - error:      Optional pointer to a `NSError` instance that Metal uses to describe the failure
///                 if this function fails.
///
/// - Returns: A ``MTL4CounterHeap`` instance, or `nil` if the function failed.
- (nullable id<MTL4CounterHeap>)newCounterHeapWithDescriptor:(MTL4CounterHeapDescriptor *)descriptor
                                                       error:(NSError *__nullable*)error
                                            API_AVAILABLE(macos(26.0), ios(26.0));

/// Returns the size, in bytes, of each entry in a counter heap of a specific counter heap type when
/// your app resolves it into a usable format.
///
/// In order to use the data available in a ``MTL4CounterHeap``, your app first resolves it either in the CPU timeline
/// or in the GPU timeline. When your app calls ``MTL4CommandBuffer/resolveCounterHeap:withRange:intoBuffer:waitFence:updateFence:``
/// to resolve counter data in the GPU timeline, Metal writes the data into a ``MTLBuffer``.
///
/// During this process, Metal transform the data in the heap into a format consisting of entries of the size
/// that this method advertises, based on the ``MTL4CounterHeapType``.
///
/// - Parameters:
///   - type: ``MTL4CounterHeapType`` value that represents the type of the ``MTL4CounterHeap`` to resolve.
///
/// - Returns: The size of the post-transformation entry from a ``MTL4CounterHeap`` of type ``MTL4CounterHeapType``.
- (NSUInteger)sizeOfCounterHeapEntry:(MTL4CounterHeapType)type
API_AVAILABLE(macos(26.0), ios(26.0));

/// Queries the frequency of the GPU timestamp in ticks per second.
///
/// - Returns: The frequency of the GPU timestamp in ticks per second.
- (uint64_t)queryTimestampFrequency
API_AVAILABLE(macos(26.0), ios(26.0));

/// Get the function handle for the specified binary-linked function from the pipeline state.
///
/// - Parameters:
///   - function: A ``MTL4BinaryFunction`` instance representing the function binary.
///
/// - Returns: A ``MTLFunctionHandle`` instance  for a binary function that was compiled with ``MTLFunctionOptionPipelineIndependent``, otherwise `nil`.
- (nullable id<MTLFunctionHandle>)functionHandleWithBinaryFunction:(id<MTL4BinaryFunction>)function
API_AVAILABLE(macos(26.0), ios(26.0));


@end
NS_ASSUME_NONNULL_END
