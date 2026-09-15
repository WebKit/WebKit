//
//  MTL4TileRenderPipeline.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4TileRenderPipeline_h
#define MTL4TileRenderPipeline_h

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>
#import <Metal/MTL4RenderPipeline.h>
#import <Metal/MTLFunctionHandle.h>
#import <Metal/MTLRenderPipeline.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN

/// Groups together properties you use to create a tile render pipeline state object.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4TileRenderPipelineDescriptor : MTL4PipelineDescriptor
/// Configures the tile function that the render pipeline executes for each tile in the tile shader stage.
@property (nullable, readwrite, nonatomic, copy) MTL4FunctionDescriptor* tileFunctionDescriptor;

/// Configures the number of samples per pixel used for multisampling.
@property (readwrite, nonatomic) NSUInteger rasterSampleCount;

/// Access an array of descriptors that configure the properties of each color attachment in the tile render
/// pipeline.
@property (readonly) MTLTileRenderPipelineColorAttachmentDescriptorArray* colorAttachments;

/// Indicating whether the size of the threadgroup matches the size of a tile in the render pipeline.
@property (readwrite, nonatomic) BOOL threadgroupSizeMatchesTileSize;

/// Sets the maximum number of threads that the GPU can execute simultaneously within a single threadgroup in
/// the tile render pipeline.
@property (readwrite, nonatomic) NSUInteger maxTotalThreadsPerThreadgroup;

/// Sets the required number of threads per threadgroup for tile dispatches.
///
/// This value is typically optional, except in the cases where the tile function that ``tileFunctionDescriptor``
/// references uses `CooperativeTensors`. In this case, you need to provide a non-zero value to this property.
///
/// Additionally, when you set this value, the `threadsPerTile` argument of any tile dispatch needs to match it.
///
/// Setting this value to a size of 0 in every dimension disables this property.
@property(readwrite, nonatomic) MTLSize requiredThreadsPerThreadgroup;

/// Configures an object that contains information about functions to link to the tile render pipeline
/// when Metal builds it.
@property (null_resettable, copy, nonatomic) MTL4StaticLinkingDescriptor* staticLinkingDescriptor;

/// Indicates whether the pipeline supports linking binary functions.
@property (readwrite, nonatomic) BOOL supportBinaryLinking;

/// Resets the descriptor to the default state.
- (void)reset;

@end

NS_ASSUME_NONNULL_END
#endif // MTL4TileRenderPipeline_h
