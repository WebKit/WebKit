//
//  MTLCaptureManager.h
//  Metal
//
//  Copyright © 2017 Apple, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

#import <Metal/MTL4CommandQueue.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLCaptureScope;
@protocol MTLCommandQueue;
@protocol MTLDevice;

API_AVAILABLE(macos(10.15), ios(13.0))
MTL_EXTERN NSErrorDomain const MTLCaptureErrorDomain;

typedef NS_ENUM(NSInteger, MTLCaptureError)
{
    /// Capturing is not supported, maybe the destination is not supported.
    MTLCaptureErrorNotSupported = 1,
    /// A capture is already in progress.
    MTLCaptureErrorAlreadyCapturing,
    /// The MTLCaptureDescriptor contains an invalid parameters.
    MTLCaptureErrorInvalidDescriptor,
} API_AVAILABLE(macos(10.15), ios(13.0));

/// The destination where you want the GPU trace to be captured to.
typedef NS_ENUM(NSInteger, MTLCaptureDestination)
{
    /// Capture to Developer Tools (Xcode) and stop the execution after capturing.
    MTLCaptureDestinationDeveloperTools = 1,
    /// Capture to a GPU Trace document and continue execution after capturing.
    MTLCaptureDestinationGPUTraceDocument,
} API_AVAILABLE(macos(10.15), ios(13.0));

MTL_EXTERN API_AVAILABLE(macos(10.15), ios(13.0))
@interface MTLCaptureDescriptor: NSObject <NSCopying>

/**
    @brief The object that is captured.

    Must be one of the following:
 
    MTLDevice captures all command queues of the device.
 
    MTLCommandQueue captures a single command queue.
 
    MTLCaptureScope captures between the next begin and end of the scope.
 */
@property (nonatomic, strong, nullable) id captureObject;

/// The destination you want the GPU trace to be captured to.
@property (nonatomic, assign) MTLCaptureDestination destination;

/// URL the GPU Trace document will be captured to.
/// Must be specified when destiation is MTLCaptureDestinationGPUTraceDocument.
@property (nonatomic, copy, nullable) NSURL *outputURL;

@end


MTL_EXTERN API_AVAILABLE(macos(10.13), ios(11.0))
@interface MTLCaptureManager : NSObject

/** Retrieves the shared capture manager for this process. There is only one capture manager per process.
    The capture manager allows the user to create capture scopes and trigger captures from code.
    When a capture has been completed, it will be displayed in Xcode and the application will be paused.
    @remarks: only MTLCommandBuffers created after starting a capture and committed before stopping it are captured. 
 */
+ (MTLCaptureManager*)sharedCaptureManager;

// Use +[sharedCaptureManager]
- (instancetype)init API_UNAVAILABLE(macos, ios);

// Creates a new capture scope for the given capture device
- (id<MTLCaptureScope>)newCaptureScopeWithDevice:(id<MTLDevice>)device;
// Creates a new capture scope for the given command queue
- (id<MTLCaptureScope>)newCaptureScopeWithCommandQueue:(id<MTLCommandQueue>)commandQueue;
// Creates a new capture scope for the given Metal 4 command queue
- (id<MTLCaptureScope>)newCaptureScopeWithMTL4CommandQueue:(id<MTL4CommandQueue>)commandQueue;

// Checks if a given capture destination is supported.
- (BOOL)supportsDestination:(MTLCaptureDestination)destination API_AVAILABLE(macos(10.15), ios(13.0));

/// Start capturing until stopCapture is called.
/// @param descriptor MTLCaptureDescriptor specifies the parameters.
/// @param error Optional error output to check why a capture could not be started.
/// @return true if the capture was successfully started, otherwise false.
/// @remarks Only MTLCommandBuffer​s created after starting and committed before stopping it are captured.
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(__autoreleasing NSError **)error API_AVAILABLE(macos(10.15), ios(13.0));

// Starts capturing, for all queues of the given device, new MTLCommandBuffer's until -[stopCapture] or Xcode’s stop capture button is pressed
- (void)startCaptureWithDevice:(id<MTLDevice>)device API_DEPRECATED("Use startCaptureWithDescriptor:error: instead", macos(10.13, 10.15), ios(11.0, 13.0));
// Starts capturing, for the given command queue, command buffers that are created after invoking this method and committed before invoking -[stopCapture] or clicking Xcode’s stop capture button.
- (void)startCaptureWithCommandQueue:(id<MTLCommandQueue>)commandQueue API_DEPRECATED("Use startCaptureWithDescriptor:error: instead", macos(10.13, 10.15), ios(11.0, 13.0));
// Start a capture with the given scope: from the scope's begin until its end, restricting the capture to the scope's device(s) and, if selected, the scope's command queue
- (void)startCaptureWithScope:(id<MTLCaptureScope>)captureScope API_DEPRECATED("Use startCaptureWithDescriptor:error: instead", macos(10.13, 10.15), ios(11.0, 13.0));

// Stops a capture started from startCaptureWithDevice:/startCaptureWithCommandQueue:/startCaptureWithScope: or from Xcode’s capture button
- (void)stopCapture;

// Default scope to be captured when a capture is initiated from Xcode’s capture button. When nil, it’ll fall back to presentDrawable:, presentDrawable:atTime:, presentDrawable:afterMinimumDuration: in MTLCommandBuffer or present:, present:atTime:, present:afterMinimumDuration: in MTLDrawable.
@property (nullable, readwrite, strong, atomic) id<MTLCaptureScope> defaultCaptureScope;

// Query if a capture is currently in progress
@property (readonly) BOOL isCapturing;

@end

NS_ASSUME_NONNULL_END

