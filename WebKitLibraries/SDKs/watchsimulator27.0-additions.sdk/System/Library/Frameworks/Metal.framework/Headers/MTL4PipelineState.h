//
//  MTL4PipelineState.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4PipelineState_h
#define MTL4PipelineState_h

#import <Metal/MTLDefines.h>


#import <Foundation/Foundation.h>
#import <Metal/MTLPipeline.h>
#import <Metal/MTLTypes.h>

NS_ASSUME_NONNULL_BEGIN

/// Option mask for requesting reflection information at pipeline build time.
typedef NS_OPTIONS(NSUInteger, MTL4ShaderReflection) {
    
    /// Requests no information.
    MTL4ShaderReflectionNone           = 0,
    
    /// Requests reflection information for bindings.
    MTL4ShaderReflectionBindingInfo    = 1 << 0,
    
    /// Requests reflection information for buffer types.
    MTL4ShaderReflectionBufferTypeInfo = 1 << 1,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Enumeration for controlling alpha-to-one state of a pipeline state object.
typedef NS_ENUM(NSInteger, MTL4AlphaToOneState) {
    
    /// Disables alpha-to-one.
    MTL4AlphaToOneStateDisabled = 0,
    
    /// Enables alpha-to-one.
    MTL4AlphaToOneStateEnabled  = 1,

} API_AVAILABLE(macos(26.0), ios(26.0));

/// Enumeration for controlling alpha-to-coverage state of a pipeline state object.
typedef NS_ENUM(NSInteger, MTL4AlphaToCoverageState) {
    
    /// Disables alpha-to-coverage.
    MTL4AlphaToCoverageStateDisabled = 0,
    
    /// Enables alpha-to-coverage.
    MTL4AlphaToCoverageStateEnabled  = 1,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Enumeration for controlling the blend state of a pipeline state object.
typedef NS_ENUM(NSInteger, MTL4BlendState) {
    
    /// Disables blending.
    MTL4BlendStateDisabled = 0,
    
    /// Enables blending.
    MTL4BlendStateEnabled  = 1,
    
    /// Defers determining the blending stage.
    ///
    /// Behaves as ``MTL4BlendStateDisabled`` until you specialize this pipeline value.
    MTL4BlendStateUnspecialized API_AVAILABLE(macos(26.0), ios(26.0)) = 2,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Enumeration for controlling support for ``MTLIndirectCommandBuffer``.
typedef NS_ENUM(NSInteger, MTL4IndirectCommandBufferSupportState) {
    
    /// Disables support for indirect command buffers.
    MTL4IndirectCommandBufferSupportStateDisabled = 0,
    
    /// Enables support for indirect command buffers.
    MTL4IndirectCommandBufferSupportStateEnabled  = 1,
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Provides options controlling how to compile a pipeline state.
///
/// You provide these options through the ``MTL4PipelineDescriptor`` class at compilation time.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4PipelineOptions : NSObject<NSCopying>

/// Controls whether to enable or disable Metal Shader Validation for the pipeline.
@property (readwrite, nonatomic) MTLShaderValidation shaderValidation;

/// Controls whether to include Metal shader reflection in this pipeline.
@property (readwrite, nonatomic) MTL4ShaderReflection shaderReflection;
@end

/// Base type for descriptors you use for building pipeline state objects.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4PipelineDescriptor : NSObject<NSCopying>

/// Assigns an optional string that uniquely identifies a pipeline descriptor.
///
/// After you provide this label, you can use it to look up a pipeline state object by name in a binary archive.
@property (nullable, copy, nonatomic) NSString* label;

/// Provides compile-time options when you build the pipeline.
@property (nullable, readwrite, nonatomic, retain) MTL4PipelineOptions* options;

@end

NS_ASSUME_NONNULL_END

#endif // MTL4PipelineState_h
