//
//  MTLCommandEncoder.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;

/*!
 * @brief Describes how a resource will be used by a shader through an argument buffer
 */
typedef NS_OPTIONS(NSUInteger, MTLResourceUsage)
{
	MTLResourceUsageRead   = 1 << 0,
	MTLResourceUsageWrite  = 1 << 1,
	MTLResourceUsageSample API_DEPRECATED_WITH_REPLACEMENT("MTLResourceUsageRead", macos(10.13, 13.0), ios(11.0, 16.0)) = 1 << 2
} API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 * @brief Describes the types of resources that the a barrier operates on
 */
typedef NS_OPTIONS(NSUInteger, MTLBarrierScope)
{
    MTLBarrierScopeBuffers = 1 << 0,
    MTLBarrierScopeTextures = 1 << 1,
    MTLBarrierScopeRenderTargets API_AVAILABLE(macos(10.14), macCatalyst(13.0)) API_UNAVAILABLE(ios) = 1 << 2,
} API_AVAILABLE(macos(10.14), ios(12.0));

/// Describes stages of GPU work.
///
/// All commands you encoder into command buffers relate to one or more shader stages,
/// for example, a compute dispatch command from a compute command encoder relates to
/// stage ``MTLStageDispatch``.
///
/// Use these stages to issue barriers between shader stages to ensure Metal correctly
/// synchronizes GPU commands.
typedef NS_OPTIONS(NSUInteger, MTLStages)
{
    /// Represents all vertex shader stage work in a render pass.
    MTLStageVertex                = 1 << 0,
    
    /// Represents all fragment shader stage work in a render pass.
    MTLStageFragment              = 1 << 1,
    
    /// Represents all tile shading stage work in a render pass.
    MTLStageTile                  = 1 << 2,
    
    /// Represents all object shader stage work in a render pass.
    MTLStageObject                = 1 << 3,
    
    /// Represents all mesh shader stage work work in a render pass.
    MTLStageMesh                  = 1 << 4,
    
    /// Represents all sparse and placement sparse resource mapping updates.
    MTLStageResourceState         = 1 << 26,
    
    /// Represents all compute dispatches in a compute pass.
    MTLStageDispatch              = 1 << 27,
    
    /// Represents all blit operations in a pass.
    MTLStageBlit                  = 1 << 28,
    
    /// Represents all acceleration structure operations.
    MTLStageAccelerationStructure = 1 << 29,
    
    /// Represents all machine learning network dispatch operations.
    MTLStageMachineLearning       = 1 << 30,
    
    /// Convenience mask representing all stages of GPU work.
    MTLStageAll                   = NSIntegerMax,
} API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @protocol MTLCommandEncoder
 @abstract MTLCommandEncoder is the common interface for objects that write commands into MTLCommandBuffers.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLCommandEncoder <NSObject>

/*!
 @property device
 @abstract The device this resource was created against.
 */
@property (readonly) id <MTLDevice> device;

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, copy, atomic) NSString *label;

/*!
 @method endEncoding
 @abstract Declare that all command generation from this encoder is complete, and detach from the MTLCommandBuffer.
 */
- (void)endEncoding;

/// Encodes a consumer barrier on work you commit to the same command queue.
///
/// Encode a barrier that guarantees that any subsequent work you encode in the current command encoder that corresponds
/// to the `beforeStages` stages doesn't proceed until Metal completes all work prior to the current command encoder
/// corresponding to the `afterQueueStages` stages, completes.
///
/// Metal can reorder the exact point where it applies the barrier, so use this method for synchronizing between different passes.
///
/// If you need to synchronize work within a pass that you encode with an instance of a subclass of ``MTLCommandEncoder``,
/// use memory barriers instead. For subclasses of ``MTL4CommandEncoder``, use encoder barriers.
///
/// You can specify `afterQueueStages` and `beforeStages` that contain ``MTLStages`` unrelated to the current command
/// encoder.
///
/// - Parameters:
///   - afterQueueStages: ``MTLStages`` mask that represents the stages of work to wait for.
///                        This argument applies to work corresponding to these stages you
///                        encode in prior command encoders, and not for the current encoder.
///   - beforeStages:     ``MTLStages`` mask that represents the stages of work that wait.
///                        This argument applies to work you encode in the current command encoder.
- (void)barrierAfterQueueStages:(MTLStages)afterQueueStages
                   beforeStages:(MTLStages)beforeStages
API_AVAILABLE(macos(26.0), ios(26.0));

/* Debug Support */

/*!
 @method insertDebugSignpost:
 @abstract Inserts a debug string into the command buffer.  This does not change any API behavior, but can be useful when debugging.
 */
- (void)insertDebugSignpost:(NSString *)string;

/*!
 @method pushDebugGroup:
 @abstract Push a new named string onto a stack of string labels.
 */
- (void)pushDebugGroup:(NSString *)string;

/*!
 @method popDebugGroup
 @abstract Pop the latest named string off of the stack.
*/
- (void)popDebugGroup;

@end
NS_ASSUME_NONNULL_END
